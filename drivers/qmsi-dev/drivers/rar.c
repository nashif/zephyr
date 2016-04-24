/*
 * {% copyright %}
 */

#include "rar.h"

#if (HAS_RAR)
int rar_set_mode(const rar_state_t mode)
{
	QM_CHECK(mode <= RAR_RETENTION, -EINVAL);
	volatile uint32_t i = 32;
	volatile uint32_t reg;

	switch (mode) {
	case RAR_RETENTION:
		QM_SCSS_PMU->aon_vr |=
		    (QM_AON_VR_PASS_CODE | QM_AON_VR_ROK_BUF_VREG_MASK);
		QM_SCSS_PMU->aon_vr |=
		    (QM_AON_VR_PASS_CODE | QM_AON_VR_VREG_SEL);
		break;

	case RAR_NORMAL:
		reg = QM_SCSS_PMU->aon_vr & ~QM_AON_VR_VREG_SEL;
		QM_SCSS_PMU->aon_vr = QM_AON_VR_PASS_CODE | reg;
		/* Wait for >= 2usec, at most 64 clock cycles. */
		while (i--) {
			__asm__ __volatile__("nop");
		}
		reg = QM_SCSS_PMU->aon_vr & ~QM_AON_VR_ROK_BUF_VREG_MASK;
		QM_SCSS_PMU->aon_vr = QM_AON_VR_PASS_CODE | reg;
		break;
	}
	return 0;
}
#endif
