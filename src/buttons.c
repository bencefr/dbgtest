#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include "image_control.h"

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
#include <nrf53_cpunet_mgmt.h>
#else
#include <hal/nrf_vpr.h>
#endif

struct switch_button {
	struct gpio_dt_spec gpio;
	struct gpio_callback callback;
	struct k_work_delayable debounce;
	bool secondary;
};

static struct switch_button buttons[] = {
	{.gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios), .secondary = false},
	{.gpio = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios), .secondary = true},
};

static void switch_image(struct k_work *work)
{
	struct switch_button *button = CONTAINER_OF(k_work_delayable_from_work(work),
						   struct switch_button, debounce);
	int pressed = gpio_pin_get_dt(&button->gpio);

	if (pressed < 0) {
		k_panic();
	}
	if (pressed == 0) {
		return;
	}
#if defined(CONFIG_SOC_NRF5340_CPUAPP)
	nrf53_cpunet_enable(false);
#else
	nrf_vpr_cpurun_set((NRF_VPR_Type *)DT_REG_ADDR(DT_NODELABEL(cpuflpr_vpr)), false);
#endif
	volatile struct debugger_selection *selection = debugger_selection_get();

	if (button->secondary) {
		selection->secondary ^= 1;
	} else {
		selection->app ^= 1;
	}
	sys_reboot(SYS_REBOOT_WARM);
}

static void button_pressed(const struct device *device, struct gpio_callback *callback,
			   gpio_port_pins_t pins)
{
	ARG_UNUSED(device);
	ARG_UNUSED(pins);
	struct switch_button *button = CONTAINER_OF(callback, struct switch_button, callback);

	if (k_work_reschedule(&button->debounce, K_MSEC(40)) < 0) {
		k_panic();
	}
}

int debugger_buttons_init(void)
{
	for (size_t index = 0; index < ARRAY_SIZE(buttons); index++) {
		struct switch_button *button = &buttons[index];

		if (!gpio_is_ready_dt(&button->gpio)) {
			return -ENODEV;
		}
		k_work_init_delayable(&button->debounce, switch_image);
		int result = gpio_pin_configure_dt(&button->gpio, GPIO_INPUT);

		if (result != 0) {
			return result;
		}
		gpio_init_callback(&button->callback, button_pressed, BIT(button->gpio.pin));
		result = gpio_add_callback(button->gpio.port, &button->callback);
		if (result != 0) {
			return result;
		}
		result = gpio_pin_interrupt_configure_dt(&button->gpio, GPIO_INT_EDGE_TO_ACTIVE);
		if (result != 0) {
			return result;
		}
	}
	return 0;
}