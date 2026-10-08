#include <zephyr/init.h>
#include "image_control.h"

static int network_select(void)
{
#if DEBUGGER_IMAGE_ID == 3
	volatile struct debugger_selection *selection = debugger_selection_get();

	if (debugger_selection_valid(selection) && selection->secondary != 0) {
		uintptr_t address = DEBUGGER_PARTITION_ADDRESS(DT_NODELABEL(app_d_partition));

		debugger_handoff(address, DT_REG_SIZE(DT_NODELABEL(app_d_partition)));
	}
#endif
	return 0;
}

SYS_INIT(network_select, EARLY, 0);