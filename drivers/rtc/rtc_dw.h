/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _RTC_DW_H_
#define _RTC_DW_H_

#include <board.h>
#include <device.h>
#include <rtc.h>

#define RTC_DRV_NAME		"rtc"

/**
 * RTC Register block type.
 */
typedef struct {
	volatile uint32_t rtc_ccvr;         /**< Current Counter Value Register */
	volatile uint32_t rtc_cmr;          /**< Current Match Register */
	volatile uint32_t rtc_clr;          /**< Counter Load Register */
	volatile uint32_t rtc_ccr;          /**< Counter Control Register */
	volatile uint32_t rtc_stat;         /**< Interrupt Status Register */
	volatile uint32_t rtc_rstat;        /**< Interrupt Raw Status Register */
	volatile uint32_t rtc_eoi;          /**< End of Interrupt Register */
	volatile uint32_t rtc_comp_version; /**< End of Interrupt Register */
} rtc_dw_t;


/** RTC register block */
#define RTC_DW ((rtc_dw_t *)RTC_BASE_ADDR)


#define RTC_INTERRUPT_ENABLE        (1 << 0)
#define RTC_INTERRUPT_MASK          (1 << 1)
#define RTC_ENABLE                  (1 << 2)
#define RTC_WRAP_ENABLE             (1 << 3)

#define RTC_CLK_DIV_EN     	    (1 << 2)
#define RTC_CLK_DIV_MASK            (0xF << 3)
#define RTC_CLK_DIV_1_HZ            (0xF << 3)
#define RTC_CLK_DIV_32768_HZ        (0x0 << 3)
#define RTC_CLK_DIV_8192_HZ         (0x2 << 3)
#define RTC_CLK_DIV_4096_HZ         (0x3 << 3)

struct rtc_dw_dev_config {
	uint32_t        base_address;
};

int rtc_dw_init(struct device* dev);

#endif
