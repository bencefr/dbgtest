#include "image_control.h"

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
#include <nrf53_cpunet_mgmt.h>
#include <hal/nrf_spu.h>
#else
#include <hal/nrf_vpr.h>
#include <hal/nrf_spu.h>
#endif

void debugger_secondary_start(void)
{
#if !defined(CONFIG_SOC_NRF5340_CPUAPP)
	NRF_VPR_Type *vpr = (NRF_VPR_Type *)DT_REG_ADDR(DT_NODELABEL(cpuflpr_vpr));
	volatile struct debugger_selection *selection = debugger_selection_get();
	uintptr_t address;

	if (selection->secondary != 0) {
		address = DEBUGGER_PARTITION_ADDRESS(DT_NODELABEL(app_d_partition));
	} else {
		address = DEBUGGER_PARTITION_ADDRESS(DT_NODELABEL(app_c_partition));
	}
	nrf_spu_periph_perm_secattr_set(NRF_SPU00, nrf_address_slave_get((uint32_t)vpr), true);
	nrf_vpr_initpc_set(vpr, address);
	nrf_vpr_cpurun_set(vpr, true);
#endif
}