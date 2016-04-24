/*
* {% copyright %}
*/

#ifndef __QM_SS_TIMER_H__
#define __QM_SS_TIMER_H__

#include "qm_common.h"
#include "qm_sensor_regs.h"

/**
 * Timer driver for Sensor Subsystem.
 *
 * @defgroup groupSSTimer SS Timer
 * @{
 */

/**
 * Sensor Subsystem Timer Configuration Type.
 */
typedef struct {
	bool watchdog_mode;     /**< Watchdog mode. */

	/**
	 * Increments in run state only.
	 *
	 * If this field is set to 0, the timer will count
	 * in both halt state and running state.
	 * When set to 1, this will only increment in
	 * running state.
	 */
	bool inc_run_only;
	bool int_en;            /**< Interrupt enable. */
	uint32_t count;         /**< Final count value. */
	/**
	 * User callback.
	 *
	 * Called for any interrupt on the Sensor Subsystem Timer.
	 *
	 * @param[in] data The callback user data.
	 */
	void (*callback)(void *data);
	void *callback_data; /**< Callback user data. */
} qm_ss_timer_config_t;

/**
 * Set the SS timer configuration.
 *
 * This includes final count value, timer mode and if interrupts are enabled.
 * If interrupts are enabled, it will configure the callback function.
 *
 * @param[in] timer Which SS timer to configure.
 * @param[in] cfg SS timer configuration. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_ss_timer_set_config(const qm_ss_timer_t timer,
			   const qm_ss_timer_config_t *const cfg);

/**
 * Set SS timer count value.
 *
 * Set the current count value of the SS timer.
 *
 * @param[in] timer Which SS timer to set the count of.
 * @param[in] count Value to load the timer with.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_ss_timer_set(const qm_ss_timer_t timer, const uint32_t count);

/**
 * Get SS timer count value.
 *
 * Get the current count value of the SS timer.
 *
 * @param[in] timer Which SS timer to get the count of.
 * @param[out] count Current value of timer. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_ss_timer_get(const qm_ss_timer_t timer, uint32_t *const count);

/**
 * @}
 */

#endif /* __QM_SS_TIMER_H__ */
