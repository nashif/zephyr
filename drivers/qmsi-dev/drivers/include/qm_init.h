/*
 * {% copyright %}
 */

#ifndef __QM_INIT_H__
#define __QM_INIT_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Initialisation and reset.
 *
 * @defgroup groupInit Initialisation
 * @{
 */

/**
 * Reset Mode type.
 */
typedef enum {
	QM_WARM_RESET = BIT(1), /**< Warm reset. */
	QM_COLD_RESET = BIT(3), /**< Cold reset. */
} qm_soc_reset_t;

/**
 * Reset the SoC.
 *
 * This can either be a cold reset or a warm reset.
 *
 * @param [in] reset_type Selects the type of reset to perform.
 */
void qm_soc_reset(qm_soc_reset_t reset_type);

/**
 * @}
 */

#endif /* __QM_INIT_H__ */
