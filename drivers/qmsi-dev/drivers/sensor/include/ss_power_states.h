/*
 * {% copyright %}
 */

#ifndef __QM_SS_POWER_STATES_H__
#define __QM_SS_POWER_STATES_H__

#include "qm_common.h"
#include "qm_sensor_regs.h"

/**
 * SS Power mode control for Quark SE Microcontrollers.
 *
 * @defgroup groupSSPower SS Power states
 * @{
 */

/**
 * Sensor Subsystem SS1 Timers mode type.
 */
typedef enum {
	SS_POWER_CPU_SS1_TIMER_OFF = 0, /**< Disable SS Timers in SS1. */
	SS_POWER_CPU_SS1_TIMER_ON       /**< Keep SS Timers enabled in SS1. */
} ss_power_cpu_ss1_mode_t;

/**
 * Enter Sensor SS1 state.
 *
 * Put the Sensor Subsystem into SS1.<BR>
 * Processor Clock is gated in this state.
 *
 * A wake event causes the Sensor Subsystem to transition to SS0.<BR>
 * A wake event is a sensor subsystem interrupt.
 *
 * According to the mode selected, Sensor Subsystem Timers can be disabled.
 *
 * @param[in] mode Mode selection for SS1 state.
 */
void ss_power_cpu_ss1(const ss_power_cpu_ss1_mode_t mode);

/**
 * Enter Sensor SS2 state or SoC LPSS state.
 *
 * Put the Sensor Subsystem into SS2.<BR>
 * Sensor Complex Clock is gated in this state.<BR>
 * Sensor Peripherals are gated in this state.<BR>
 *
 * This enables entry in LPSS if:
 *  - Sensor Subsystem is in SS2
 *  - Lakemont is in C2 or C2LP
 *  - LPSS entry is enabled
 *
 * A wake event causes the Sensor Subsystem to transition to SS0.<BR>
 * There are two kinds of wake event depending on the Sensor Subsystem
 * and SoC state:
 *  - SS2: a wake event is a Sensor Subsystem interrupt
 *  - LPSS: a wake event is a Sensor Subsystem interrupt or a Lakemont interrupt
 *
 * LPSS wake events apply if LPSS is entered.
 * If Host wakes the SoC from LPSS,
 * Sensor also transitions back to SS0.
 */
void ss_power_cpu_ss2(void);

/**
 * @}
 */

#endif /* __QM_SS_POWER_STATES_H__ */
