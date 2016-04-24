/*
 * {% copyright %}
 */

#include "qm_version.h"

uint32_t qm_ver_rom(void)
{
	volatile uint32_t *ver_pointer;

	ver_pointer = (uint32_t*)ROM_VERSION_ADDRESS;

	return *ver_pointer;
}
