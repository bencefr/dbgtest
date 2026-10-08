#include <zephyr/kernel.h>
#include "image_control.h"

int main(void)
{
	volatile struct debugger_selection *selection = debugger_selection_get();

	if (!debugger_selection_valid(selection)) {
		selection->magic = 0;
		selection->app = 0;
		selection->secondary = 0;
		selection->boots = 0;
		selection->inverse_magic = ~DEBUGGER_SELECTION_MAGIC;
		selection->magic = DEBUGGER_SELECTION_MAGIC;
	}
	selection->boots++;
	if (selection->app == 0) {
		uintptr_t address = DEBUGGER_PARTITION_ADDRESS(DT_NODELABEL(app_a_partition));

		debugger_handoff(address, DT_REG_SIZE(DT_NODELABEL(app_a_partition)));
	} else {
		uintptr_t address = DEBUGGER_PARTITION_ADDRESS(DT_NODELABEL(app_b_partition));

		debugger_handoff(address, DT_REG_SIZE(DT_NODELABEL(app_b_partition)));
	}
}
