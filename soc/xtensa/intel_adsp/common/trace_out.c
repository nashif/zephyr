/* Copyright (c) 2021 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <soc.h>
#include <cavs-mem.h>
#include <sys/winstream.h>

struct k_spinlock trace_lock;
struct k_spinlock log_lock;

static struct sys_winstream *winstream_tr;
static struct sys_winstream *winstream_lg;


void intel_adsp_log_out(int8_t *str, size_t len)
{
	if (len == 0) {
		return;
	}

#ifdef CONFIG_ADSP_TRACE_SIMCALL
	register int a2 __asm__("a2") = 4; /* SYS_write */
	register int a3 __asm__("a3") = 1; /* fd 1 == stdout */
	register int a4 __asm__("a4") = (int)str;
	register int a5 __asm__("a5") = len;

	__asm__ volatile("simcall" : "+r"(a2), "+r"(a3)
			 : "r"(a4), "r"(a5)  : "memory");
#endif

	k_spinlock_key_t key = k_spin_lock(&log_lock);

	sys_winstream_write(winstream_lg, str, len);
	k_spin_unlock(&log_lock, key);
}

void intel_adsp_trace_out(int8_t *str, size_t len)
{
	if (len == 0) {
		return;
	}

#ifdef CONFIG_ADSP_TRACE_SIMCALL
	register int a2 __asm__("a2") = 4; /* SYS_write */
	register int a3 __asm__("a3") = 1; /* fd 1 == stdout */
	register int a4 __asm__("a4") = (int)str;
	register int a5 __asm__("a5") = len;

	__asm__ volatile("simcall" : "+r"(a2), "+r"(a3)
			 : "r"(a4), "r"(a5)  : "memory");
#endif

	k_spinlock_key_t key = k_spin_lock(&trace_lock);

	sys_winstream_write(winstream_tr, str, len);
	k_spin_unlock(&trace_lock, key);
}



int arch_printk_char_out(int c)
{
	int8_t s = c;

	intel_adsp_log_out(&s, 1);
	return 0;
}

void soc_trace_init(void)
{
	void *tr_buf = z_soc_uncached_ptr((void *)HP_SRAM_WIN2_BASE);
	void *lg_buf = z_soc_uncached_ptr((void *)HP_SRAM_WIN3_BASE);

	winstream_tr = sys_winstream_init(tr_buf, HP_SRAM_WIN2_SIZE);
	winstream_lg = sys_winstream_init(lg_buf, HP_SRAM_WIN3_SIZE);
}
