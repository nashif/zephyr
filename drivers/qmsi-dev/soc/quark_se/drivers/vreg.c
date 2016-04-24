/*
 * {% copyright %}
 */

#include "vreg.h"

typedef enum { AON_VR = 0, PLAT3P3_VR, PLAT1P8_VR, HOST_VR, VREG_NUM } vreg_t;

QM_RW uint32_t *vreg[VREG_NUM] = {
    &QM_SCSS_PMU->aon_vr, &QM_SCSS_PMU->plat3p3_vr, &QM_SCSS_PMU->plat1p8_vr,
    &QM_SCSS_PMU->host_vr};

static int vreg_set_mode(const vreg_t id, const vreg_mode_t mode)
{
	QM_CHECK(mode < VREG_MODE_NUM, -EINVAL);
	uint32_t vr;

	vr = *vreg[id];

	switch (mode) {
	case VREG_MODE_SWITCHING:
		vr |= QM_SCSS_VR_EN;
		vr &= ~QM_SCSS_VR_VREG_SEL;
		break;
	case VREG_MODE_LINEAR:
		vr |= QM_SCSS_VR_EN;
		vr |= QM_SCSS_VR_VREG_SEL;
		break;
	case VREG_MODE_SHUTDOWN:
		vr &= ~QM_SCSS_VR_EN;
		break;
	default:
		break;
	}

	*vreg[id] = vr;

	while ((mode == VREG_MODE_SWITCHING) &&
	       (*vreg[id] & QM_SCSS_VR_ROK) == 0) {
	}

	return 0;
}

int vreg_aon_set_mode(const vreg_mode_t mode)
{
	QM_CHECK(mode < VREG_MODE_NUM, -EINVAL);
	QM_CHECK(mode != VREG_MODE_SWITCHING, -EINVAL);

	return vreg_set_mode(AON_VR, mode);
}

int vreg_plat3p3_set_mode(const vreg_mode_t mode)
{
	QM_CHECK(mode < VREG_MODE_NUM, -EINVAL);
	return vreg_set_mode(PLAT3P3_VR, mode);
}

int vreg_plat1p8_set_mode(const vreg_mode_t mode)
{
	QM_CHECK(mode < VREG_MODE_NUM, -EINVAL);
	return vreg_set_mode(PLAT1P8_VR, mode);
}

int vreg_host_set_mode(const vreg_mode_t mode)
{
	QM_CHECK(mode < VREG_MODE_NUM, -EINVAL);
	return vreg_set_mode(HOST_VR, mode);
}
