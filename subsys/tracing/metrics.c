/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <tracing_metrics.h>
#include <sys/printk.h>
#include <kernel_internal.h>
#include <ksched.h>

static struct k_thread *current_thread;
uint64_t arch_timing_swap_start;
uint64_t arch_timing_swap_end;

void sys_trace_thread_switched_in(void)
{
	int key = irq_lock();

	current_thread = k_current_get();

	printk(">> switched in %s\n", k_thread_name_get(current_thread));
	irq_unlock(key);
}

void sys_trace_thread_switched_out(void)
{
	int key = irq_lock();

	printk(">> switched out %s\n", k_thread_name_get(k_current_get()));
	irq_unlock(key);
}

void sys_trace_thread_priority_set(struct k_thread *thread)
{
}
void sys_trace_thread_create(struct k_thread *thread)
{
	printk(">> thread created (%p): %s\n", thread,
	       k_thread_name_get(thread));
}

void sys_trace_thread_abort(struct k_thread *thread)
{
	printk(">> thread abort: %s\n", k_thread_name_get(thread));
}

void sys_trace_thread_running(struct k_thread *thread)
{
	printk(">> thread running: %s\n", k_thread_name_get(thread));
}

void sys_trace_thread_suspend(struct k_thread *thread)
{
	printk(">> thread suspend: %s\n", k_thread_name_get(thread));
}

void sys_trace_thread_resume(struct k_thread *thread)
{
	printk(">> thread resume: %s\n", k_thread_name_get(thread));
}

void sys_trace_thread_ready(struct k_thread *thread)
{
	printk(">> thread ready (%p): %s\n", thread, k_thread_name_get(thread));
}

void sys_trace_thread_pend(struct k_thread *thread)
{
	printk(">> thread pend: %s\n", k_thread_name_get(thread));
}

void sys_trace_thread_info(struct k_thread *thread)
{
}

void sys_trace_thread_name_set(struct k_thread *thread)
{
	printk(">> set name of thread %p to %s\n", thread,
	       k_thread_name_get(thread));
}

void sys_trace_isr_enter(void)
{
	printk(">> isr enter\n");
}

void sys_trace_isr_exit(void)
{
	printk(">> isr exit\n");
}

void sys_trace_isr_exit_to_scheduler(void)
{
}

void sys_trace_idle(void)
{
	printk(">> idle thread\n");
}

void sys_trace_void(unsigned int id)
{
	switch (id) {
	case SYS_TRACE_ID_SLEEP:
		printk(">> call to k_sleep\n");
		break;
	default:
		printk(">> unhandled end call: %d\n", id);
		break;
	}
}

void sys_trace_end_call(unsigned int id)
{
	switch (id) {
	case SYS_TRACE_ID_SEMA_INIT:
		printk(">> end call to k_sem_init\n");
		break;
	case SYS_TRACE_ID_SEMA_GIVE:
		printk(">> end call to k_sem_give\n");
		break;
	case SYS_TRACE_ID_SEMA_TAKE:
		printk(">> end call to k_sem_take\n");
		break;
	case SYS_TRACE_ID_MUTEX_UNLOCK:
		printk(">> end call to k_mutex_unlock\n");
		break;
	case SYS_TRACE_ID_MUTEX_LOCK:
		printk(">> end call to k_mutex_lock\n");
		break;
	case SYS_TRACE_ID_SLEEP:
		printk(">> end call to k_sleep\n");
		break;
	default:
		printk(">> unhandled end call: %d\n", id);
		break;
	}
}

void sys_trace_arg1(unsigned int id, void *arg)
{
	struct k_sem *sem;
	struct k_mutex *mutex;

	switch (id) {
	case SYS_TRACE_ID_MUTEX_INIT:
		printk(">> mutex inatilized: %p\n", (struct k_mutex *)arg);
		break;
	case SYS_TRACE_ID_MUTEX_LOCK:
		mutex = (struct k_mutex *)arg;
		printk(">> lock mutex: %p (count: %d)\n", arg, mutex->lock_count);
		break;
	case SYS_TRACE_ID_MUTEX_UNLOCK:
		mutex = (struct k_mutex *)arg;
		printk(">> unlock mutex: %p (count: %d)\n", arg, mutex->lock_count);
		break;
	case SYS_TRACE_ID_SEMA_INIT:
		printk(">> semaphore inatilized: %p\n", (struct k_sem *)arg);
		break;
	case SYS_TRACE_ID_SEMA_GIVE:
		sem = (struct k_sem *)arg;
		printk(">> %s give semaphore(signal): %p (count: %d)\n",
		       k_thread_name_get(k_current_get()), sem, sem->count);
		break;
	case SYS_TRACE_ID_SEMA_TAKE:
		sem = (struct k_sem *)arg;
		printk(">> %s takes semaphore(wait): %p (count: %d)\n",
		       k_thread_name_get(k_current_get()), sem, sem->count);
		break;
	default:
		printk(">> unhandled call: %d\n", id);
		break;
	}
}
