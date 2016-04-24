/*
 * {% copyright %}
 */

#ifndef __RAR_H__
#define __RAR_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

#if (HAS_RAR)
/**
 * Retention alternator regulator for Quark D2000.
 *
 * @defgroup groupRAR RAR
 * @{
 */

/**
 * RAR modes type.
 */
typedef enum {
	RAR_NORMAL,   /**< Normal mode = 50 mA. */
	RAR_RETENTION /**< Retention mode = 300 uA. */
} rar_state_t;

/**
 * Change operating mode of RAR.
 *
 * Normal mode is able to source up to 50 mA.
 * Retention mode is able to source up to 300 uA.
 * Care must be taken when entering into retention mode
 * to ensure the overall system draw is less than 300 uA.
 *
 * @param[in] mode Operating mode of the RAR.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int rar_set_mode(const rar_state_t mode);

/**
 * @}
 */
#endif /* HAS_RAR */
#endif /* __RAR_H__ */
