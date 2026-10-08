#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <string.h>
#include "workload.h"

#ifndef DEBUGGER_IMAGE_ID
#define DEBUGGER_IMAGE_ID 1
#endif

const char debugger_image_name[] = "debugger-image-" STRINGIFY(DEBUGGER_IMAGE_ID);
struct debugger_state debugger_globals = {
	.image_name = debugger_image_name,
	.image_id = DEBUGGER_IMAGE_ID,
};
static K_MUTEX_DEFINE(state_lock);
K_MSGQ_DEFINE(samples, sizeof(struct debugger_sample), 4, 4);
static K_SEM_DEFINE(start_gate, 0, 3);

__attribute__((noinline)) int64_t debugger_leaf(struct debugger_sample *sample)
{
	volatile int32_t local_values[8];
	int64_t local_sum = 0;

	for (size_t index = 0; index < ARRAY_SIZE(local_values); index++) {
		local_values[index] = sample->values[index] * DEBUGGER_IMAGE_ID;
		local_sum += local_values[index];
	}
	k_sleep(K_MSEC(20));
	return local_sum;
}

__attribute__((noinline)) int64_t debugger_nested(struct debugger_sample *sample)
{
	struct debugger_sample local_copy = *sample;
	volatile uint32_t local_cookie = 0xd06b0000 | DEBUGGER_IMAGE_ID;
	int64_t result = debugger_leaf(&local_copy);

	return result + (local_cookie & 0xff);
}

static void producer(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);
	uint32_t sequence = 0;

	k_sem_take(&start_gate, K_FOREVER);
	for (;;) {
		struct debugger_sample local_sample = {.sequence = ++sequence};

		memcpy(local_sample.label, "stack-sample", sizeof("stack-sample"));
		for (size_t index = 0; index < ARRAY_SIZE(local_sample.values); index++) {
			local_sample.values[index] = (sequence & 0x7fff) + index;
		}
		k_mutex_lock(&state_lock, K_FOREVER);
		if (k_msgq_put(&samples, &local_sample, K_NO_WAIT) == 0) {
			debugger_globals.produced = sequence;
		}
		k_mutex_unlock(&state_lock);
		k_sleep(K_MSEC(250));
	}
}

static void consumer(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);
	k_sem_take(&start_gate, K_FOREVER);
	for (;;) {
		struct debugger_sample local_sample;

		if (k_msgq_get(&samples, &local_sample, K_FOREVER) == 0) {
			int64_t local_result = debugger_nested(&local_sample);

			k_mutex_lock(&state_lock, K_FOREVER);
			debugger_globals.last_sample = local_sample;
			debugger_globals.checksum = local_result;
			debugger_globals.consumed++;
			k_mutex_unlock(&state_lock);
		}
	}
}

static void observer(void *unused1, void *unused2, void *unused3)
{
	ARG_UNUSED(unused1);
	ARG_UNUSED(unused2);
	ARG_UNUSED(unused3);
	k_sem_take(&start_gate, K_FOREVER);
	for (;;) {
		struct debugger_state local_snapshot;

		k_mutex_lock(&state_lock, K_FOREVER);
		debugger_globals.heartbeat++;
		local_snapshot = debugger_globals;
		k_mutex_unlock(&state_lock);
		__ASSERT(local_snapshot.consumed <= local_snapshot.produced, "queue invariant");
		k_sleep(K_SECONDS(1));
	}
}

K_THREAD_DEFINE(producer_thread, 2048, producer, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(consumer_thread, 2048, consumer, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(observer_thread, 2048, observer, NULL, NULL, NULL, 7, 0, 0);

int debugger_workload_start(void)
{
	int result = k_thread_name_set(producer_thread, "producer");

	if (result == 0) {
		result = k_thread_name_set(consumer_thread, "consumer");
	}
	if (result == 0) {
		result = k_thread_name_set(observer_thread, "observer");
	}
	if (result != 0) {
		return result;
	}
	for (size_t index = 0; index < 3; index++) {
		k_sem_give(&start_gate);
	}
	return 0;
}