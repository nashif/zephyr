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
#include <counter/counter_ace_v1x_art_regs.h>

#if defined(__XCC__)
#include <xtensa/config/core.h>
#if XCHAL_HAVE_HIFI4 && (defined(__ALDERLAKE__) \
	|| defined(__METEORLAKE__) || defined(__METEORLAKE_S__))
#include <xtensa/tie/xt_hifi4.h>
#elif XCHAL_HAVE_HIFI3
#include <xtensa/tie/xt_hifi3.h>
#endif
#endif

static struct k_spinlock lock;

#define ACE_TSCTRL	(0x72040)

static void counter_ace_v1x_art_ionte_set(bool new_timestamp_enable)
{
	struct DfTSCTRL val;

	val.u.full                      = ((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full;
	val.u.part.ionte                = new_timestamp_enable;
	((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full = val.u.full;
}

static void counter_ace_v1x_art_cdmas_set(uint32_t cdmas)
{
	struct DfTSCTRL val;

	val.u.full                      = ((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full;
	val.u.part.cdmas                = cdmas;
	((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full = val.u.full;
}

static void counter_ace_v1x_art_ntk_set(bool new_timestamp_taken)
{
	struct DfTSCTRL val;

	val.u.full                      = ((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full;
	val.u.part.ntk                = new_timestamp_taken;
	((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full = val.u.full;
}

static uint32_t counter_ace_v1x_art_ntk_get(void)
{
	return ((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.part.ntk;
}

static void counter_ace_v1x_art_hhtse_set(bool enable)
{
	struct DfTSCTRL val;

	val.u.full                      = ((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full;
	val.u.part.hhtse                = enable;
	((volatile struct DfTSCTRL*)(ACE_TSCTRL))->u.full = val.u.full;
}

static uint64_t counter_ace_v1x_art_counter_get(void)
{
#if defined(__XCC__) /* use Xtensa coprocessor for atomic read of 64 bit value */
	return *((volatile ae_int64 * const)ACE_ARTCS);
#else
	while (1) {
		uint64_t artc1 = ((volatile union DfARTCS*)ACE_ARTCS)->full;
		uint32_t artc2 = ((volatile union DfARTCS*)ACE_ARTCS)->part.high;

		if ((artc1 >> 32) == artc2)
			return artc1;
	}
#endif /* __XCC__ */
}

int counter_ace_v1x_art_get_value(const struct device *dev, uint64_t *value)
{
	ARG_UNUSED(dev);

	k_spinlock_key_t key = k_spin_lock(&lock);

	counter_ace_v1x_art_ionte_set(1);
	counter_ace_v1x_art_cdmas_set(1);

	if (counter_ace_v1x_art_ntk_get()) {
		counter_ace_v1x_art_ntk_set(1);
		while (counter_ace_v1x_art_ntk_get()) {
			for (uint32_t idx = 0; idx < 12; ++idx)
				__asm volatile("nop");
		}
	}

	counter_ace_v1x_art_hhtse_set(1);

	while (!counter_ace_v1x_art_ntk_get()) {
		for (uint32_t idx = 0; idx < 12; ++idx)
			__asm volatile("nop");
	}

	*value = counter_ace_v1x_art_counter_get();

	counter_ace_v1x_art_ntk_set(1);
	k_spin_unlock(&lock, key);

	return 0;
}

int counter_ace_v1x_art_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static const struct counter_driver_api ace_v1x_art_counter_apis = {
	.get_value_64 = counter_ace_v1x_art_get_value
};

DEVICE_DT_DEFINE(DT_NODELABEL(ace_art_counter), counter_ace_v1x_art_init, NULL, NULL, NULL,
		 PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		 &ace_v1x_art_counter_apis);
