/*
* {% copyright %}
*/

#ifndef __QM_PIC_TIMER_H__
#define __QM_PIC_TIMER_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * PIC timer.
 *
 * @defgroup groupPICTimer PIC Timer
 * @{
 */

/**
 * PIC timer mode type.
 */
typedef enum {
	QM_PIC_TIMER_MODE_ONE_SHOT, /**< One shot mode. */
	QM_PIC_TIMER_MODE_PERIODIC  /**< Periodic mode. */
} qm_pic_timer_mode_t;

/**
 * PIC timer configuration type.
 */
typedef struct {
	qm_pic_timer_mode_t mode; /**< Operation mode. */
	bool int_en;		  /**< Interrupt enable. */
	/**
	 * User callback.
	 *
	 * @param[in] data User defined data.
	 */
	void (*callback)(void *data);
	void *callback_data; /**< Callback user data. */
} qm_pic_timer_config_t;

/**
 * Set the PIC timer configuration.
 *
 * Set the PIC timer configuration.
 * This includes timer mode and if interrupts are enabled. If interrupts are
 * enabled, it will configure the callback function.
 *
 * @param[in] cfg PIC timer configuration. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_pic_timer_set_config(const qm_pic_timer_config_t *const cfg);

/**
 * Set the current count value of the PIC timer.
 *
 * Set the current count value of the PIC timer.
 * A value equal to 0 effectively stops the timer.
 *
 * @param[in] count Value to load the timer with.
 *
 * @return Standard errno return type for QMSI.
 * @retval Always returns 0.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_pic_timer_set(const uint32_t count);

/**
 * Get the current count value of the PIC timer.
 *
 * @param[out] count Pointer to the store the timer count.
 *                   This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_pic_timer_get(uint32_t *const count);

/**
 * @}
 */

#endif /* __QM_PIC_TIMER_H__ */
