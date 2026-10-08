#include <zephyr/kernel.h>
#include <cmsis_core.h>
#include "image_control.h"

__attribute__((naked, noreturn)) static void branch_to_image(uint32_t stack, uint32_t entry)
{
	__asm__ volatile(
		"movs r2, #0\n"
		"msr control, r2\n"
		"msr basepri, r2\n"
		"msr msplim, r2\n"
		"msr psplim, r2\n"
		"isb\n"
		"msr msp, r0\n"
		"dsb\n"
		"isb\n"
		"bx r1\n");
}

FUNC_NORETURN void debugger_handoff(uintptr_t address, size_t size)
{
	const uint32_t *vectors = (const uint32_t *)address;
	uint32_t stack = vectors[0];
	uint32_t entry = vectors[1];
	uintptr_t ram_start = DT_REG_ADDR(DT_CHOSEN(zephyr_sram));
	uintptr_t ram_end = ram_start + DT_REG_SIZE(DT_CHOSEN(zephyr_sram));

	if ((stack & 7) != 0 || stack <= ram_start || stack > ram_end ||
	    !(entry & 1) || (entry & ~1u) < address || (entry & ~1u) >= address + size) {
		k_panic();
	}
	__disable_irq();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	for (size_t index = 0; index < ARRAY_SIZE(NVIC->ICER); index++) {
		NVIC->ICER[index] = UINT32_MAX;
		NVIC->ICPR[index] = UINT32_MAX;
	}
	SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
	SCB->VTOR = address;
	__DSB();
	__ISB();
	branch_to_image(stack, entry);
}