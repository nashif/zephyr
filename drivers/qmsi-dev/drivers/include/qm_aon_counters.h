/*
 * {% copyright %}
 */

#ifndef __QM_AON_COUNTERS_H__
#define __QM_AON_COUNTERS_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Always-on Counters.
 *
 * @defgroup groupAONC Always-on Counters
 * @{
 */

/**
 * Always-on Periodic Timer configuration type.
 */
typedef struct {
	uint32_t count; /**< Time to count down from in clock cycles.*/
	bool int_en;    /**< Enable/disable the interrupts. */

	/**
	* User callback.
	*
	* @param[in] data User defined data.
	*/
	void (*callback)(void *data);
	void *callback_data; /**< Callback data. */
} qm_aonpt_config_t;

/**
 * Enable the Always-on Counter.
 *
 * @param[in] aonc Always-on counter to read.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
*/
int qm_aonc_enable(const qm_scss_aon_t aonc);

/**
 * Disable the Always-on Counter.
 *
 * @param[in] aonc Always-on counter to read.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonc_disable(const qm_scss_aon_t aonc);

/**
 * Get the current value of the Always-on Counter.
 *
 * Returns a 32-bit value which represents the number of clock cycles
 * since the counter was first enabled.
 *
 * @param[in] aonc Always-on counter to read.
 * @param[out] val Value of the counter. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonc_get_value(const qm_scss_aon_t aonc, uint32_t *const val);

/**
 * Set the Always-on Periodic Timer configuration.
 *
 * This includes the initial value of the Always-on Periodic Timer,
 * the interrupt enable and the callback function that will be run
 * when the timer expiers and an interrupt is triggered.
 * The Periodic Timer is disabled if the counter is set to 0.
 *
 * @param[in] aonc Always-on counter to read.
 * @param[in] cfg New configuration for the Always-on Periodic Timer.
 * This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonpt_set_config(const qm_scss_aon_t aonc,
			const qm_aonpt_config_t *const cfg);

/**
 * Get the current value of the Always-on Periodic Timer.
 *
 * Returns a 32-bit value which represents the number of clock cycles
 * remaining before the timer fires.
 * This is the initial configured number minus the number of cycles that have
 * passed.
 *
 * @param[in] aonc Always-on counter to read.
 * @param[out] val Value of the Always-on Periodic Timer.
 * This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonpt_get_value(const qm_scss_aon_t aonc, uint32_t *const val);

/**
 * Get the current status of the Always-on Periodic Timer.
 *
 *  Returns true if the timer has expired. This will continue to return true
 *  until it is cleared with qm_aonpt_clear().
 *
 * @param[in] aonc Always-on counter to read.
 * @param[out] status Status of the Always-on Periodic Timer.
 * This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonpt_get_status(const qm_scss_aon_t aonc, bool *const status);

/**
 * Clear the status of the Always-on Periodic Timer.
 *
 * The status must be clear before the Always-on Periodic Timer can trigger
 * another interrupt.
 *
 * @param[in] aonc Always-on counter to read.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonpt_clear(const qm_scss_aon_t aonc);

/**
 * Reset the Always-on Periodic Timer back to the configured value.
 *
 * @param[in] aonc Always-on counter to read.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_aonpt_reset(const qm_scss_aon_t aonc);

/**
 * @}
 */
#endif /* __QM_AON_COUNTERS_H__ */
