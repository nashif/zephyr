/*
 * {% copyright %}
 */

#ifndef __QM_IDENTIFICATION_H__
#define __QM_IDENTIFICATION_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Identification functions for Quark Microcontrollers.
 *
 * @defgroup groupIdentification Identification
 * @{
 */

/**
 * Get Quark SoC identification number
 *
 * @return uint32_t SoC identifier number.
 */
uint32_t qm_soc_id(void);

/**
 * Get Quark SoC version number
 *
 * @return uint32_t SoC version number.
 */
uint32_t qm_soc_version(void);

/**
 * @}
 */

#endif /* __QM_IDENTIFICATION_H__ */
