/*
 * {% copyright %}
 */

#include "ss_power_states.h"

/* Sensor Subsystem sleep operand definition.
 * Only a subset applies as internal sensor RTC
 * is not available.
 *
 *  OP | Core | Timers | RTC
 * 000 |    0 |      1 |   1 <-- used for SS1
 * 001 |    0 |      0 |   1
 * 010 |    0 |      1 |   0
 * 011 |    0 |      0 |   0 <-- used for SS2
 * 100 |    0 |      0 |   0
 * 101 |    0 |      0 |   0
 * 110 |    0 |      0 |   0
 * 111 |    0 |      0 |   0
 *
 * sleep opcode argument:
 *  - [7:5] : Sleep Operand
 *  - [4]   : Interrupt enable
 *  - [3:0] : Interrupt threshold value
 */
#define QM_SS_SLEEP_MODE_CORE_OFF (0x0)
#define QM_SS_SLEEP_MODE_CORE_OFF_TIMER_OFF (0x20)
#define QM_SS_SLEEP_MODE_CORE_TIMERS_RTC_OFF (0x60)

/* Enter SS1 :
 * SLEEP + sleep operand
 * __builtin_arc_sleep is not used here as it does not propagate sleep operand.
 */
void ss_power_cpu_ss1(const ss_power_cpu_ss1_mode_t mode)
{
	/* Enter SS1 */
	switch (mode) {
	case SS_POWER_CPU_SS1_TIMER_OFF:
		__asm__ __volatile__(
		    "sleep %0"
		    :
		    : "i"(QM_SS_SLEEP_MODE_CORE_OFF_TIMER_OFF));
		break;
	case SS_POWER_CPU_SS1_TIMER_ON:
	default:
		__asm__ __volatile__("sleep %0"
				     :
				     : "i"(QM_SS_SLEEP_MODE_CORE_OFF));
		break;
	}
}

/* Enter SS2 :
 * SLEEP + sleep operand
 * __builtin_arc_sleep is not used here as it does not propagate sleep operand.
 */
void ss_power_cpu_ss2(void)
{
	/* Enter SS2 */
	__asm__ __volatile__("sleep %0"
			     :
			     : "i"(QM_SS_SLEEP_MODE_CORE_TIMERS_RTC_OFF));
}
