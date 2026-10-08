#ifndef DBGTEST_WORKLOAD_H
#define DBGTEST_WORKLOAD_H

#include <stdint.h>

struct debugger_sample {
	uint32_t sequence;
	int32_t values[8];
	char label[16];
};

struct debugger_state {
	const char *image_name;
	uint32_t image_id;
	uint32_t produced;
	uint32_t consumed;
	uint32_t heartbeat;
	int64_t checksum;
	struct debugger_sample last_sample;
};

extern struct debugger_state debugger_globals;
int debugger_workload_start(void);

#endif