/*
 * {% copyright %}
 */

#ifndef __POWER_STATES_H__
#define __POWER_STATES_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Power mode control for Quark D2000 Microcontrollers.
 *
 * @defgroup groupD2000Power Quark D2000 Power states
 * @{
 */

/**
 * Put CPU in halt state.
 *
 * Halts the CPU until next interrupt or reset.
 */
void cpu_halt(void);

/**
 * Put SoC to sleep.
 *
 * Enter into sleep mode. The hybrid oscillator is disabled, most peripherals
 * are disabled and the voltage regulator is set into retention mode.
 * The following peripherals are disabled in this mode:
 *  - I2C
 *  - SPI
 *  - GPIO debouncing
 *  - Watchdog timer
 *  - PWM / Timers
 *  - UART
 *
 * The SoC operates from the 32 kHz clock source and the following peripherals
 * may bring the SoC back into an active state:
 *
 *  - GPIO interrupts
 *  - AON Timers
 *  - RTC
 *  - Low power comparators
 */
void soc_sleep();

/**
 * Put SoC to deep sleep.
 *
 * Enter into deep sleep mode. All clocks are gated. The only way to return
 * from this is to have an interrupt trigger on the low power comparators.
 */
void soc_deep_sleep();

/**
 * @}
 */

#endif /* __POWER_STATES_H__ */
