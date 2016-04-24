/*
 * {% copyright %}
 */

#include "qm_identification.h"

/* Quark D2000 ID is 1.1 */
#define QUARK_D2000_SOC_ID (0x11)

uint32_t qm_soc_id(void)
{
#if (QUARK_D2000)
	return QUARK_D2000_SOC_ID;
#elif(QUARK_SE)
	return QM_SCSS_GP->id;
#else
#error "Unsupported / unspecified processor detected."
#endif
}

uint32_t qm_soc_version(void)
{
#if (QUARK_D2000)
	return (QUARK_D2000_SOC_ID << 8) | QM_SCSS_INFO->rev;
#elif(QUARK_SE)
	return (QM_SCSS_GP->id << 8) | QM_SCSS_GP->rev;
#endif
}
