/*
 * {% copyright %}
 */

#ifndef __POWER_STATES_H__
#define __POWER_STATES_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * SoC Power mode control for Quark SE Microcontrollers.
 *
 * @defgroup groupSoCPower Quark SE SoC Power states
 * @{
 */

/**
 * Enter SoC sleep state.
 *
 * Put the SoC into sleep state until next SoC wake event.
 *
 * - Core well is turned off
 * - Always on well is on
 * - Hybrid Clock is off
 * - RTC Clock is on
 *
 * Possible SoC wake events are:
 * 	- Low Power Comparator Interrupt
 * 	- AON GPIO Interrupt
 * 	- AON Timer Interrupt
 * 	- RTC Interrupt
 */
void power_soc_sleep(void);

/**
 * Enter SoC deep sleep state.
 *
 * Put the SoC into deep sleep state until next SoC wake event.
 *
 * - Core well is turned off
 * - Always on well is on
 * - Hybrid Clock is off
 * - RTC Clock is on
 *
 * Possible SoC wake events are:
 * 	- Low Power Comparator Interrupt
 * 	- AON GPIO Interrupt
 * 	- AON Timer Interrupt
 * 	- RTC Interrupt
 *
 * This function puts 1P8V regulators and 3P3V into Linear Mode.
 */
void power_soc_deep_sleep(void);

/**
 * Enable LPSS state entry.
 *
 * Put the SoC into LPSS on next C2/C2LP and SS2 state combination.<BR>
 * SoC Hybrid Clock is gated in this state.<BR>
 * Core Well Clocks are gated.<BR>
 * RTC is the only clock remaining running.
 *
 * Possible SoC wake events are:
 * 	- Low Power Comparator Interrupt
 * 	- AON GPIO Interrupt
 * 	- AON Timer Interrupt
 * 	- RTC Interrupt
 */
void power_soc_lpss_enable(void);

/**
 * Disable LPSS state entry.
 *
 * Clear LPSS enable flag.<BR>
 * This will prevent entry in LPSS when cores are in C2/C2LP and SS2 states.
 */
void power_soc_lpss_disable(void);

/**
 * @}
 */

#if (!QM_SENSOR)
/**
 * Host Power mode control for Quark SE Microcontrollers.<BR>
 * These functions cannot be called from the Sensor Subsystem.
 *
 * @defgroup groupSEPower Quark SE Host Power states
 * @{
 */

/**
 * Enter Host C1 state.
 *
 * Put the Host into C1.<BR>
 * Processor Clock is gated in this state.<BR>
 * Nothing is turned off in this state.
 *
 * A wake event causes the Host to transition to C0.<BR>
 * A wake event is a host interrupt.
 */
void power_cpu_c1(void);

/**
 * Enter Host C2 state or SoC LPSS state.
 *
 * Put the Host into C2.
 * Processor Clock is gated in this state.
 * All rails are supplied.
 *
 * This enables entry in LPSS if:
 *  - Sensor Subsystem is in SS2.
 *  - LPSS entry is enabled.
 *
 * If C2 is entered:
 *  - A wake event causes the Host to transition to C0.
 *  - A wake event is a host interrupt.
 *
 * If LPSS is entered:
 *  - LPSS wake events applies.
 *  - If the Sensor Subsystem wakes the SoC from LPSS, Host is back in C2.
 */
void power_cpu_c2(void);

/**
 * Enter Host C2LP state or SoC LPSS state.
 *
 * Put the Host into C2LP.
 * Processor Complex Clock is gated in this state.
 * All rails are supplied.
 *
 * This enables entry in LPSS if:
 *  - Sensor Subsystem is in SS2.
 *  - LPSS is allowed.
 *
 * If C2LP is entered:
 *  - A wake event causes the Host to transition to C0.
 *  - A wake event is a Host interrupt.
 *
 * If LPSS is entered:
 *  - LPSS wake events apply if LPSS is entered.
 *  - If the Sensor Subsystem wakes the SoC from LPSS,
 *    Host transitions back to C2LP.
 */
void power_cpu_c2lp(void);
#endif

/**
 * @}
 */

#endif /* __POWER_STATES_H__ */
