/*
 * Parts derived from tests/kernel/fatal/src/main.c, which has the
 * following copyright and license:
 *
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <ztest.h>
#include <kernel_structs.h>
#include <string.h>
#include <stdlib.h>

extern void inherit_test_1(void);
extern void mem_domain_ztest_1(void);
extern void mem_domain_ztest_2(void);
extern void mem_domain_ztest_3(void);
extern void mem_domain_ztest_4(void);
extern void mem_domain_ztest_5(void);
extern void mem_domain_ztest_6(void);
extern void mem_domain_ztest_7(void);
extern void kobject_test_1(void);
extern void kobject_test_2(void);
extern void kobject_test_3(void);
extern void kobject_test_4(void);
extern void kobject_test_5(void);
extern void kobject_test_6(void);
extern void kobject_test_7(void);
extern void kobject_test_8(void);
extern void kobject_test_9(void);
extern void kobject_test_10(void);
extern void kobject_test_11(void);
extern void kobject_test_12(void);
extern void kobject_test_13(void);
extern void kobject_test_14(void);
extern void kobject_test_15(void);
extern void kobject_test_16(void);
extern void kobject_test_17(void);
extern void kobject_test_18(void);
extern void kobject_test_19(void);
extern void kobject_test_20(void);
extern void kobject_test_21(void);
extern void kobject_test_22(void);
extern void kobject_test_23(void);
extern void kobject_test_24(void);
extern void kobject_test_25(void);
extern void kobject_test_26(void);

void test_main(void)
{
	k_thread_priority_set(k_current_get(), -1);
	ztest_test_suite(memory_protection_test_suite,
			 ztest_unit_test(inherit_test_1),
			 ztest_unit_test(mem_domain_ztest_1),
			 ztest_unit_test(mem_domain_ztest_2),
			 ztest_unit_test(mem_domain_ztest_3),
			 ztest_unit_test(mem_domain_ztest_4),
			 ztest_unit_test(mem_domain_ztest_5),
			 ztest_unit_test(mem_domain_ztest_6),
			 ztest_unit_test(mem_domain_ztest_7),
			 ztest_unit_test(kobject_test_1),
			 ztest_unit_test(kobject_test_2),
			 ztest_unit_test(kobject_test_3),
			 ztest_unit_test(kobject_test_4),
			 ztest_unit_test(kobject_test_5),
			 ztest_unit_test(kobject_test_6),
			 ztest_unit_test(kobject_test_7),
			 ztest_unit_test(kobject_test_8),
			 ztest_unit_test(kobject_test_9),
			 ztest_unit_test(kobject_test_10),
			 ztest_user_unit_test(kobject_test_11),
			 ztest_user_unit_test(kobject_test_12),
			 ztest_unit_test(kobject_test_13),
			 ztest_unit_test(kobject_test_14),
			 ztest_unit_test(kobject_test_15),
			 ztest_unit_test(kobject_test_16),
			 ztest_unit_test(kobject_test_17),
			 ztest_unit_test(kobject_test_18),
			 ztest_unit_test(kobject_test_19),
			 ztest_unit_test(kobject_test_20),
			 ztest_unit_test(kobject_test_21),
			 ztest_unit_test(kobject_test_22),
			 ztest_unit_test(kobject_test_23),
			 ztest_unit_test(kobject_test_24),
			 ztest_unit_test(kobject_test_25),
			 ztest_unit_test(kobject_test_26));
	ztest_run_test_suite(memory_protection_test_suite);
}
