/*
 * {% copyright %}
 */

#include "qm_init.h"

void qm_soc_reset(qm_soc_reset_t reset_type)
{
	QM_SCSS_PMU->rstc |= reset_type;
}
