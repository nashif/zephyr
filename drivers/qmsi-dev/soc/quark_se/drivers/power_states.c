/*
 * {% copyright %}
 */

#include "power_states.h"
#include "vreg.h"

#if (QM_SENSOR)
#include "qm_sensor_regs.h"
#endif

void power_soc_lpss_enable()
{
	QM_SCSS_CCU->ccu_lp_clk_ctl |= QM_SCSS_CCU_SS_LPS_EN;
}

void power_soc_lpss_disable()
{
	QM_SCSS_CCU->ccu_lp_clk_ctl &= ~QM_SCSS_CCU_SS_LPS_EN;
}

void power_soc_sleep()
{
	/* Go to sleep */
	QM_SCSS_PMU->slp_cfg &= ~QM_SCSS_SLP_CFG_LPMODE_EN;
	QM_SCSS_PMU->pm1c |= QM_SCSS_PM1C_SLPEN;
}

void power_soc_deep_sleep()
{
	/* Switch to linear regulators */
	vreg_plat1p8_set_mode(VREG_MODE_LINEAR);
	vreg_plat3p3_set_mode(VREG_MODE_LINEAR);

	/* Enable low power sleep mode */
	QM_SCSS_PMU->slp_cfg |= QM_SCSS_SLP_CFG_LPMODE_EN;
	QM_SCSS_PMU->pm1c |= QM_SCSS_PM1C_SLPEN;
}

#if (!QM_SENSOR)
void power_cpu_c1()
{
	__asm__ __volatile__("hlt");
}

void power_cpu_c2()
{
	QM_SCSS_CCU->ccu_lp_clk_ctl &= ~QM_SCSS_CCU_C2_LP_EN;
	/* Read P_LVL2 to trigger a C2 request */
	QM_SCSS_PMU->p_lvl2;
}

void power_cpu_c2lp()
{
	QM_SCSS_CCU->ccu_lp_clk_ctl |= QM_SCSS_CCU_C2_LP_EN;
	/* Read P_LVL2 to trigger a C2 request */
	QM_SCSS_PMU->p_lvl2;
}
#endif
