#include <zephyr/kernel.h>
#include "image_control.h"
#include "workload.h"

int main(void)
{
#if DEBUGGER_IMAGE_ID < 3
	int result = debugger_buttons_init();

	if (result != 0) {
		k_panic();
	}
	debugger_secondary_start();
#endif
	if (debugger_workload_start() != 0) {
		k_panic();
	}
	return 0;
}