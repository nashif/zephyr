/*
 * {% copyright %}
 */

#include "power_states.h"
#include "clk.h"
#include "qm_comparator.h"

#include "qm_adc.h"

#include "rar.h"

void cpu_halt(void)
{
	__asm__ __volatile__("hlt");
}

static void clear_all_pending_interrupts(void)
{
	/* Clear comparator interrupts. */
	QM_SCSS_CMP->cmp_stat_clr = -1;

	/* Clear RTC interrupts. */
	QM_RTC->rtc_eoi;

	/* Clear timers interrupt flag. */
	QM_PWM->timerseoi;

	/* Clear GPIO interrupts. */
	QM_GPIO[QM_GPIO_0]->gpio_porta_eoi = -1;
}

void soc_sleep(void)
{
	/* Variables to save register values. */
	uint32_t ac_power_save;
	uint32_t clk_gate_save = QM_SCSS_CCU->ccu_periph_clk_gate_ctl;
	uint32_t sys_clk_ctl_save = QM_SCSS_CCU->ccu_sys_clk_ctl;
	uint32_t osc0_cfg_save = QM_SCSS_CCU->osc0_cfg1;
	uint32_t adc_mode_save = QM_ADC->adc_op_mode;

	/* Clear any pending interrupts. */
	clear_all_pending_interrupts();

	qm_adc_set_mode(QM_ADC_0, QM_ADC_MODE_PWR_DOWN);

	/* Turn off high power comparators. */
	ac_power_save = QM_SCSS_CMP->cmp_pwr;
	QM_SCSS_CMP->cmp_pwr &= QM_AC_HP_COMPARATORS_MASK;

	/*
	 * Program WAKE_MASK.WAKE_MASK[31:0],
	 * CCU_LP_CLK_CTL.WAKE_PROBE_MODE_MASK registers identical to Interrupt
	 * Mask registers.
	 */
	QM_SCSS_CCU->ccu_lp_clk_ctl &= ~QM_WAKE_PROBE_MODE_MASK;

	/*
	 * Ensure that powering down of oscillators is delayed by hardware until
	 * core  executes HALT instruction.
	 */
	/* HYB_OSC_PD_LATCH_EN = 0, RTC_OSC_PD_LATCH_EN=0 */
	QM_SCSS_CCU->ccu_lp_clk_ctl &=
	    ~(QM_HYB_OSC_PD_LATCH_EN | QM_RTC_OSC_PD_LATCH_EN);

	/* Ensure that at exit, hardware will switch system clock to Hybrid
	 * oscillator clock so as to minimize exit latency by running at higher
	 * frequency than RTC clock.
	 */
	/* CCU_LP_CLK_CTL.CCU_EXIT_TO_HYBOSC */
	QM_SCSS_CCU->ccu_lp_clk_ctl |= QM_CCU_EXIT_TO_HYBOSC;

	/* Power down hybrid oscillator after HALT instruction is executed. */
	QM_SCSS_CCU->osc0_cfg1 |= QM_OSC0_PD;

	/*
	 * Only the following peripherals can be used as a wakeup source:
	 *  - GPIO Interrupts
	 *  - AON timers
	 *  - RTC
	 *  - low power comparators
	 */
	clk_periph_disable(
	    CLK_PERIPH_I2C_M0 | CLK_PERIPH_SPI_S | CLK_PERIPH_SPI_M0 |
	    CLK_PERIPH_GPIO_DB | CLK_PERIPH_WDT_REGISTER |
	    CLK_PERIPH_PWM_REGISTER | CLK_PERIPH_GPIO_REGISTER |
	    CLK_PERIPH_SPI_M0_REGISTER | CLK_PERIPH_SPI_S_REGISTER |
	    CLK_PERIPH_UARTA_REGISTER | CLK_PERIPH_UARTB_REGISTER |
	    CLK_PERIPH_I2C_M0_REGISTER);

	/* Set system clock source to hyb osc, 4 MHz, scaled to 512 kHz. */
	clk_sys_set_mode(CLK_SYS_HYB_OSC_4MHZ, CLK_SYS_DIV_8);

	/* Set the RAR to retention mode. */
	rar_set_mode(RAR_RETENTION);

	/*
	 * If wake source is any of AON Timer, RTC, GPIO interrupt, program
	 * CCU_SYS_CLK_CTL.CCU_SYS_CLK_SEL to RTC Oscillator.
	 */
	/* Enter SoC sleep mode. */
	cpu_halt();

	/* From here on, restore the SoC to an active state. */
	/* Set the RAR to normal mode. */
	rar_set_mode(RAR_NORMAL);
	/* Restore all previous values. */
	QM_SCSS_CCU->ccu_sys_clk_ctl = sys_clk_ctl_save;
	/* Re-apply clock divider values. DIV_EN must go 0 -> 1. */
	QM_SCSS_CCU->ccu_sys_clk_ctl &=
	    ~(QM_CCU_SYS_CLK_DIV_EN | QM_CCU_RTC_CLK_DIV_EN);
	QM_SCSS_CCU->ccu_sys_clk_ctl |=
	    QM_CCU_SYS_CLK_DIV_EN | QM_CCU_RTC_CLK_DIV_EN;

	/* Wait for the XTAL or SI oscillator to stabilise. */
	while (!(QM_SCSS_CCU->osc0_stat1 &
		 (QM_OSC0_LOCK_SI | QM_OSC0_LOCK_XTAL))) {
	};

	/* Restore original clocking, ADC, analog comparator states. */
	QM_SCSS_CCU->osc0_cfg1 = osc0_cfg_save;
	QM_SCSS_CCU->ccu_periph_clk_gate_ctl = clk_gate_save;

	QM_SCSS_CMP->cmp_pwr = ac_power_save;
	QM_ADC->adc_op_mode = adc_mode_save;
}

void soc_deep_sleep(void)
{
	/* Variables to save register values. */
	uint32_t ac_power_save;
	uint32_t clk_gate_save = QM_SCSS_CCU->ccu_periph_clk_gate_ctl;
	uint32_t sys_clk_ctl_save = QM_SCSS_CCU->ccu_sys_clk_ctl;
	uint32_t osc0_cfg_save = QM_SCSS_CCU->osc0_cfg1;
	uint32_t osc1_cfg_save = QM_SCSS_CCU->osc1_cfg0;
	uint32_t adc_mode_save = QM_ADC->adc_op_mode;
	uint32_t aon_vr_save = QM_SCSS_PMU->aon_vr;
	uint32_t ext_clock_save;
	uint32_t lp_clk_save, pmux_slew_save;

	pmux_slew_save = QM_SCSS_PMUX->pmux_slew[0];
	ext_clock_save = QM_SCSS_CCU->ccu_ext_clock_ctl;
	lp_clk_save = QM_SCSS_CCU->ccu_lp_clk_ctl;

	/* Clear any pending interrupts. */
	clear_all_pending_interrupts();

	/* Only clear the comparator wake mask bit. */
	QM_SCSS_CCU->wake_mask =
	    SET_ALL_BITS & ~QM_CCU_WAKE_MASK_COMPARATOR_BIT;
	QM_SCSS_GP->gps1 |= QM_SCSS_GP_POWER_STATE_DEEP_SLEEP;

	qm_adc_set_mode(QM_ADC_0, QM_ADC_MODE_DEEP_PWR_DOWN);

	/* Turn off high power comparators. */
	ac_power_save = QM_SCSS_CMP->cmp_pwr;
	QM_SCSS_CMP->cmp_pwr &= QM_AC_HP_COMPARATORS_MASK;

	/* Disable all peripheral clocks. */
	clk_periph_disable(CLK_PERIPH_REGISTER);

	/* Disable external clocks. */
	QM_SCSS_CCU->ccu_ext_clock_ctl = 0;

	/* Set slew rate of all pins to 12mA. */
	QM_SCSS_PMUX->pmux_slew[0] = 0;

	/* Disable RTC. */
	QM_SCSS_CCU->osc1_cfg0 &= ~QM_OSC1_PD;

	/* Set system clock source to hyb osc, 4 MHz, scaled down to 32 kHz. */
	clk_sys_set_mode(CLK_SYS_HYB_OSC_4MHZ, CLK_SYS_DIV_128);

	/* Power down the oscillator after the halt instruction is executed. */
	QM_SCSS_CCU->ccu_lp_clk_ctl &= ~QM_HYB_OSC_PD_LATCH_EN;
	/*
	 * Enable memory halt and CPU halt. When exiting sleep mode, use hybrid
	 * oscillator.
	 */
	QM_SCSS_CCU->ccu_lp_clk_ctl |=
	    QM_CCU_EXIT_TO_HYBOSC | QM_CCU_MEM_HALT_EN | QM_CCU_CPU_HALT_EN;

	/* Power down hybrid oscillator. */
	QM_SCSS_CCU->osc0_cfg1 |= QM_OSC0_PD;

	/* Disable gpio debounce clocking. */
	QM_SCSS_CCU->ccu_gpio_db_clk_ctl &= ~QM_CCU_GPIO_DB_CLK_EN;
	/* Set retention voltage to 1.35V. */
	/* SCSS.OSC0_CFG0.OSC0_HYB_SET_REG1.OSC0_CFG0[0]  = 1; */
	QM_SCSS_CCU->osc0_cfg0 |= QM_SI_OSC_1V2_MODE;

	/* Enable low voltage mode for flash controller. */
	/* FlashCtrl.CTRL.LVE_MODE = 1; */
	QM_FLASH[QM_FLASH_0]->ctrl |= QM_FLASH_LVE_MODE;

	/* Select 1.35V for voltage regulator. */
	/* SCSS.AON_VR.VSEL = 0xB; */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | (aon_vr_save & QM_AON_VR_VSEL_MASK) |
	     QM_AON_VR_VSEL_1V35);
	/* SCSS.AON_VR.ROK_BUF_VREG_MASK = 1; */
	QM_SCSS_PMU->aon_vr = (QM_AON_VR_PASS_CODE | QM_SCSS_PMU->aon_vr |
			       QM_AON_VR_ROK_BUF_VREG_MASK);

	/* SCSS.AON_VR.VSEL_STROBE = 1;  */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | QM_SCSS_PMU->aon_vr | QM_AON_VR_VSTRB);

	/* Wait >= 1 usec, at 256 kHz this is 1 cycle. */
	__asm__("nop");

	/* SCSS.AON_VR.VSEL_STROBE = 0; */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | (QM_SCSS_PMU->aon_vr & ~QM_AON_VR_VSTRB));

	/* Wait >= 2 usec, at 256 kHz this is 1 cycle. */
	__asm__("nop");

	/* Set the RAR to retention mode. */
	rar_set_mode(RAR_RETENTION);

	/* Enter SoC deep sleep mode. */
	cpu_halt();

	/* We are now exiting from deep sleep mode. */
	/* Set the RAR to normal mode. */
	rar_set_mode(RAR_NORMAL);

	/* Restore operating voltage to 1.8V. */
	/* SCSS.AON_VR.VSEL = 0x10; */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | (QM_SCSS_PMU->aon_vr & QM_AON_VR_VSEL_MASK) |
	     QM_AON_VR_VSEL_1V8 | QM_AON_VR_ROK_BUF_VREG_MASK);

	/* SCSS.AON_VR.VSEL_STROBE = 1;  */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | QM_SCSS_PMU->aon_vr | QM_AON_VR_VSTRB);

	/* Wait >= 1 usec, at 256 kHz this is 1 cycle. */
	__asm__("nop");

	/* SCSS.AON_VR.VSEL_STROBE = 0; */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE | (QM_SCSS_PMU->aon_vr & ~QM_AON_VR_VSTRB));

	/* Wait >= 2 usec, at 256 kHz this is 1 cycle. */
	__asm__("nop");

	/* SCSS.AON_VR.ROK_BUF_VREG_MASK = 0;  */
	QM_SCSS_PMU->aon_vr =
	    (QM_AON_VR_PASS_CODE |
	     (QM_SCSS_PMU->aon_vr & ~QM_AON_VR_ROK_BUF_VREG_MASK));

	/* Wait >= 1 usec, at 256 kHz this is 1 cycle. */
	__asm__("nop");

	/* SCSS.OSC0_CFG0.OSC0_HYB_SET_REG1.OSC0_CFG0[0]  = 0; */
	QM_SCSS_CCU->osc0_cfg0 &= ~QM_SI_OSC_1V2_MODE;

	/* FlashCtrl.CTRL.LVE_MODE = 0; */
	QM_FLASH[QM_FLASH_0]->ctrl &= ~QM_FLASH_LVE_MODE;

	/* Restore all previous values. */
	QM_SCSS_CCU->ccu_sys_clk_ctl = sys_clk_ctl_save;
	/* Re-apply clock divider values. DIV_EN must go 0 -> 1. */
	QM_SCSS_CCU->ccu_sys_clk_ctl &=
	    ~(QM_CCU_SYS_CLK_DIV_EN | QM_CCU_RTC_CLK_DIV_EN);
	QM_SCSS_CCU->ccu_sys_clk_ctl |=
	    QM_CCU_SYS_CLK_DIV_EN | QM_CCU_RTC_CLK_DIV_EN;

	/* Wait for the XTAL or SI oscillator to stabilise. */
	while (!(QM_SCSS_CCU->osc0_stat1 &
		 (QM_OSC0_LOCK_SI | QM_OSC0_LOCK_XTAL))) {
	};

	/* Re-enable clocks. */
	clk_periph_enable(CLK_PERIPH_REGISTER);

	/* Re-enable gpio debounce clocking. */
	QM_SCSS_CCU->ccu_gpio_db_clk_ctl |= QM_CCU_GPIO_DB_CLK_EN;

	/* Restore original clocking, ADC, analog comparator states. */
	QM_SCSS_CCU->osc0_cfg1 = osc0_cfg_save;
	QM_SCSS_CCU->ccu_periph_clk_gate_ctl = clk_gate_save;
	QM_SCSS_CCU->osc1_cfg0 = osc1_cfg_save;

	QM_SCSS_CMP->cmp_pwr = ac_power_save;
	QM_ADC->adc_op_mode = adc_mode_save;

	QM_SCSS_PMUX->pmux_slew[0] = pmux_slew_save;
	QM_SCSS_CCU->ccu_ext_clock_ctl = ext_clock_save;
	QM_SCSS_CCU->ccu_lp_clk_ctl = lp_clk_save;

	QM_SCSS_CCU->wake_mask = SET_ALL_BITS;
	QM_SCSS_GP->gps1 &= ~QM_SCSS_GP_POWER_STATE_DEEP_SLEEP;
}
