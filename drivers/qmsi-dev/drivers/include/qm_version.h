/*
 * {% copyright %}
 */

#ifndef __QM_VERSION_H__
#define __QM_VERSION_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/**
 * Version number functions for API.
 *
 * @defgroup groupVersion Version
 * @{
 */

/**
 * Create a single version number from the major, minor and patch numbers
 */
#define QM_VER_API_UINT                                                        \
	((QM_VER_API_MAJOR * 10000) + (QM_VER_API_MINOR * 100) +               \
	 QM_VER_API_PATCH)

/**
 * Create a version number string from the major, minor and patch numbers
 */
#define QM_VER_API_STRING                                                      \
	QM_VER_STRINGIFY(QM_VER_API_MAJOR, QM_VER_API_MINOR, QM_VER_API_PATCH)

/**
 * Get the ROM version number.
 *
 * Reads the ROM version information from flash and returns it.
 *
 * @return uint32_t ROM version.
 */
uint32_t qm_ver_rom(void);

/**
 * @}
 */

#endif /* __QM_VERSION_H__ */
