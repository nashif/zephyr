/*
 * {% copyright %}
 */

#ifndef __QM_COMPARATOR_H__
#define __QM_COMPARATOR_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Analog Comparator.
 *
 * @defgroup groupAC Analog Comparator
 * @{
 */

/**
 * Analog Comparator configuration type.
 *
 * Each bit in the registers controls a single Analog Comparator pin.
 */
typedef struct {
	uint32_t int_en;    /**< Interrupt enable. */
	uint32_t reference; /**< Reference voltage, 1b: VREF; 0b: AR_PIN. */
	uint32_t polarity;  /**< 0b: input>ref; 1b: input<ref */
	uint32_t power;     /**< 1b: Normal mode; 0b:Power-down/Shutdown mode */

	/**
	 * Transfer callback.
	 *
	 * @param[in] data Callback user data.
	 * @param[in] status Comparator interrupt status.
	 */
	void (*callback)(void *data, uint32_t int_status);
	void *callback_data; /**< Callback user data. */
} qm_ac_config_t;

/**
 * Set Analog Comparator configuration.
 *
 * @param[in] config Analog Comparator configuration. This must not be NULL.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int qm_ac_set_config(const qm_ac_config_t *const config);

/**
 * @}
 */

#endif /* __QM_COMPARATOR_H__ */
