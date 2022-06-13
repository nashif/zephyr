/*
 * Copyright (c) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <device.h>
#include <devicetree.h>
#include <zephyr/drivers/counter.h>
#include <soc.h>
#include <ace_v1x-regs.h>
#include <counter/counter_ace_v1x_rtc_regs.h>

#if defined(__XCC__)
#include <xtensa/config/core.h>
#if XCHAL_HAVE_HIFI4 && (defined(__ALDERLAKE__) \
	|| defined(__METEORLAKE__) || defined(__METEORLAKE_S__))
#include <xtensa/tie/xt_hifi4.h>
#elif XCHAL_HAVE_HIFI3
#include <xtensa/tie/xt_hifi3.h>
#endif
#endif

static int counter_ace_v1x_rtc_get_value(const struct device *dev,
		int64_t *value)
{
	ARG_UNUSED(dev);
#if defined(__XCC__) /* use Xtensa coprocessor for atomic read of 64 bit value */
	*value = *((volatile ae_int64 * const)ACE_RTCWC);
	return 0;
#else
	while (1) {
		uint64_t rtc1 = *((volatile uint64_t * const)(ACE_RTCWC));
		uint32_t rtc2 = ((volatile union DfRTCWC*)ACE_RTCWC)->part.high;

		if ((rtc1 >> 32) == rtc2) {
			*value = rtc1;
			return 0;
		}
	}
#endif /* __XCC__ */
}

int counter_ace_v1x_rtc_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static const struct counter_driver_api ace_v1x_rtc_counter_apis = {
	.get_value_64 = counter_ace_v1x_rtc_get_value
};

DEVICE_DT_DEFINE(DT_NODELABEL(ace_rtc_counter), counter_ace_v1x_rtc_init, NULL, NULL, NULL,
		 PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		 &ace_v1x_rtc_counter_apis);
