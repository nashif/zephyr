/*
 * {% copyright %}
 */

#ifndef __VREG_H__
#define __VREG_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

typedef enum {
	VREG_MODE_SWITCHING = 0,
	VREG_MODE_LINEAR,
	VREG_MODE_SHUTDOWN,
	VREG_MODE_NUM,
} vreg_mode_t;

/**
 * Voltage Regulators Control.
 *
 * @defgroup groupVREG Quark SE Voltage Regulators
 * @{
 */

/**
 * Set AON Voltage Regulator mode.
 *
 * The AON Voltage Regulator is not a
 * switching regulator and only acts as
 * a linear regulator.
 * VREG_SWITCHING_MODE is not a value mode
 * for the AON Voltage Regulator.
 *
 * @param[in] mode Voltage Regulator mode.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int vreg_aon_set_mode(const vreg_mode_t mode);

/**
 * Set Platform 3P3 Voltage Regulator mode.
 *
 * @param[in] mode Voltage Regulator mode.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int vreg_plat3p3_set_mode(const vreg_mode_t mode);

/**
 * Set Platform 1P8 Voltage Regulator mode.
 *
 * @param[in] mode Voltage Regulator mode.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int vreg_plat1p8_set_mode(const vreg_mode_t mode);

/**
 * Set Host Voltage Regulator mode.
 *
 * @param[in] mode Voltage Regulator mode.
 *
 * @return Standard errno return type for QMSI.
 * @retval 0 on success.
 * @retval Negative @ref errno for possible error codes.
 */
int vreg_host_set_mode(const vreg_mode_t mode);

/**
 * @}
 */

#endif /* __VREG_H__ */
