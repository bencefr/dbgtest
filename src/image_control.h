#ifndef DBGTEST_IMAGE_CONTROL_H
#define DBGTEST_IMAGE_CONTROL_H

#include <zephyr/devicetree.h>
#include <zephyr/toolchain.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DEBUGGER_SELECTION_MAGIC 0x44424754u
#define DEBUGGER_PARTITION_ADDRESS(node) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(node, zephyr_mapped_partition), \
		    (DT_REG_ADDR(node)), \
		    (DT_REG_ADDR(DT_GPARENT(node)) + DT_REG_ADDR(node)))

struct debugger_selection {
	uint32_t magic;
	uint32_t inverse_magic;
	uint32_t app;
	uint32_t secondary;
	uint32_t boots;
};

static inline volatile struct debugger_selection *debugger_selection_get(void)
{
	return (volatile struct debugger_selection *)DT_REG_ADDR(DT_NODELABEL(debugger_retained));
}

static inline bool debugger_selection_valid(volatile struct debugger_selection *selection)
{
	return selection->magic == DEBUGGER_SELECTION_MAGIC &&
	       selection->inverse_magic == ~DEBUGGER_SELECTION_MAGIC &&
	       selection->app < 2 && selection->secondary < 2;
}

FUNC_NORETURN void debugger_handoff(uintptr_t address, size_t size);
int debugger_buttons_init(void);
void debugger_secondary_start(void);

#endif