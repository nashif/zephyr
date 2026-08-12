/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interrupt handling benchmarks, built on the ztest benchmark
 * framework. All scenarios are manually sampled benchmarks
 * (ZTEST_BENCHMARK_MANUAL) because their measured spans have endpoints
 * captured in different execution contexts (thread vs ISR), which
 * framework-timed benchmarks cannot express.
 *
 * The suite and all benchmarks live in one translation unit because
 * ZTEST_BENCHMARK_SUITE() defines the suite object static; individual
 * scenarios are selected with the CONFIG_INT_BENCH_SCENARIO_* options.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/irq.h>
#include <zephyr/timing/timing.h>

#include "load.h"
#include "trigger.h"

#define NUM_ITERATIONS   CONFIG_INT_BENCH_NUM_ITERATIONS
#define WARMUP           CONFIG_INT_BENCH_WARMUP_ITERATIONS
#define TOTAL_ITERATIONS (WARMUP + NUM_ITERATIONS)

/*
 * Record a sample, discarding everything recorded during the warmup
 * phase once it ends.
 *
 * Warmup iterations record like any other and are thrown away
 * afterwards, rather than skipping the recording. Skipping it would
 * leave the first measured iteration as the only one not preceded by
 * the bookkeeping ztest_benchmark_record_sample() performs, which is
 * enough to make it measurably faster than every iteration after it:
 * the reported minimum would then describe a state the scenario is
 * never in again.
 */
static inline void record_after(uint32_t iteration, uint32_t warmup, uint64_t cycles)
{
	ztest_benchmark_record_sample(cycles);

	if ((warmup > 0U) && (iteration == (warmup - 1U))) {
		ztest_benchmark_discard_samples();
	}
}

static inline void record(uint32_t iteration, uint64_t cycles)
{
	record_after(iteration, WARMUP, cycles);
}

#if defined(CONFIG_INT_BENCH_SCENARIO_DIRECT) || defined(CONFIG_INT_BENCH_SCENARIO_ZLI)
static volatile bool alt_fired;
static volatile timing_t alt_timestamp;

/*
 * Dispatched straight from the vector table: no software ISR table
 * lookup and none of the common entry code that wraps a regular ISR.
 * Returning zero tells the architecture layer that no reschedule is
 * needed, which keeps the exit path out of the measurement.
 */
ISR_DIRECT_DECLARE(bench_alt_isr)
{
	alt_timestamp = timing_counter_get();
	alt_fired = true;

	return 0;
}
#endif

static void suite_setup(void)
{
#ifdef CONFIG_INT_BENCH_TRIGGER_SW_IRQ
	printk("Interrupt benchmark: trigger=sw-irq (line %u), %u iterations\n",
	       bench_trigger_irq_line(), NUM_ITERATIONS);
#else
	printk("Interrupt benchmark: trigger=irq_offload, %u iterations\n"
	       "No sw-irq trigger for this architecture: only the exit path\n"
	       "scenarios run (entry latency, critical section, throughput\n"
	       "and dynamic connect need a real asynchronous interrupt)\n",
	       NUM_ITERATIONS);
#endif

	printk("Background load: %s\n", bench_load_description());

	(void)bench_trigger_init();

#ifdef CONFIG_INT_BENCH_SCENARIO_DIRECT
	IRQ_DIRECT_CONNECT(BENCH_IRQ_LINE_ALT, CONFIG_INT_BENCH_IRQ_PRIO, bench_alt_isr, 0);
	irq_enable(BENCH_IRQ_LINE_ALT);
#endif
#ifdef CONFIG_INT_BENCH_SCENARIO_ZLI
	/*
	 * Zero-latency interrupts have to be registered with
	 * IRQ_DIRECT_CONNECT(), and at a priority within the levels
	 * reserved for them.
	 */
	IRQ_DIRECT_CONNECT(BENCH_IRQ_LINE_ALT, 0, bench_alt_isr, IRQ_ZERO_LATENCY);
	irq_enable(BENCH_IRQ_LINE_ALT);
#endif

	bench_load_start();
}

static void suite_teardown(void)
{
	bench_load_stop();
}

ZTEST_BENCHMARK_SUITE(interrupt, suite_setup, suite_teardown);

#if defined(CONFIG_INT_BENCH_SCENARIO_ENTRY) || defined(CONFIG_INT_BENCH_SCENARIO_EXIT) || \
	defined(CONFIG_INT_BENCH_SCENARIO_LOCKED)
static volatile bool fired;
static volatile timing_t isr_timestamp;
#endif

#if defined(CONFIG_INT_BENCH_SCENARIO_ENTRY) || defined(CONFIG_INT_BENCH_SCENARIO_LOCKED)
/* Timestamp as early as possible in the ISR: measures the entry path */
static void entry_handler(void)
{
	isr_timestamp = timing_counter_get();
	fired = true;
}
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_ENTRY
/*
 * Interrupt entry latency: time from the software write that raises
 * the interrupt to the first timestamp taken inside the ISR. Requires
 * the sw-irq trigger backend (a real asynchronous interrupt).
 */
ZTEST_BENCHMARK_MANUAL(interrupt, entry_trigger_to_isr, NULL, NULL)
{
	timing_t start;
	timing_t finish;

	bench_trigger_set_handler(entry_handler);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		fired = false;
		bench_load_pollute();

		start = timing_counter_get();
		bench_trigger();

		while (!fired) {
		}

		finish = isr_timestamp;
		record(i, timing_cycles_get(&start, &finish));
	}

	bench_trigger_set_handler(NULL);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_ENTRY */

#if defined(CONFIG_INT_BENCH_SCENARIO_EXIT) || defined(CONFIG_INT_BENCH_SCENARIO_END_TO_END)
#define WAITER_STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

static K_SEM_DEFINE(wake_sem, 0, 1);
static K_SEM_DEFINE(sync_sem, 0, 1);
static K_THREAD_STACK_DEFINE(waiter_stack, WAITER_STACK_SIZE);
static struct k_thread waiter_thread;
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_EXIT
/* Timestamp as the last operation in the ISR: measures the exit path */
static void exit_handler(void)
{
	fired = true;
	isr_timestamp = timing_counter_get();
}

/*
 * Interrupt exit latency: time from the last instruction of the ISR
 * body back to the interrupted thread. Works with both trigger
 * backends; with irq_offload() it measures the offload trap exit path.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, exit_resume_interrupted, NULL, NULL)
{
	timing_t start;
	timing_t finish;

	bench_trigger_set_handler(exit_handler);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		fired = false;
		bench_load_pollute();

		bench_trigger();

		while (!fired) {
		}

		finish = timing_counter_get();
		start = isr_timestamp;
		record(i, timing_cycles_get(&start, &finish));
	}

	bench_trigger_set_handler(NULL);
}

static void resched_handler(void)
{
	k_sem_give(&wake_sem);
	isr_timestamp = timing_counter_get();
}

static void waiter_entry(void *p1, void *p2, void *p3)
{
	timing_t start;
	timing_t finish;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		k_sem_take(&wake_sem, K_FOREVER);

		finish = timing_counter_get();
		start = isr_timestamp;
		record(i, timing_cycles_get(&start, &finish));

		k_sem_give(&sync_sem);
	}
}

/*
 * Interrupt exit with rescheduling: time from the last instruction of
 * an ISR that wakes a higher priority thread to that thread running
 * (interrupt exit plus context switch).
 */
ZTEST_BENCHMARK_MANUAL(interrupt, exit_reschedule, NULL, NULL)
{
	int priority = k_thread_priority_get(k_current_get());

	bench_trigger_set_handler(resched_handler);

	k_thread_create(&waiter_thread, waiter_stack, K_THREAD_STACK_SIZEOF(waiter_stack),
			waiter_entry, NULL, NULL, NULL, priority - 1, 0, K_NO_WAIT);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		bench_load_pollute();

		bench_trigger();
		k_sem_take(&sync_sem, K_FOREVER);
	}

	k_thread_join(&waiter_thread, K_FOREVER);
	bench_trigger_set_handler(NULL);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_EXIT */

#ifdef CONFIG_INT_BENCH_SCENARIO_LOCKED
/*
 * Entry latency after a critical section: the interrupt is raised
 * while interrupts are locked, kept pending for
 * CONFIG_INT_BENCH_LOCK_HOLD_US, then the time from irq_unlock() to
 * ISR entry is measured. Requires the sw-irq trigger backend.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, locked_unlock_to_isr, NULL, NULL)
{
	timing_t start;
	timing_t finish;
	unsigned int key;

	bench_trigger_set_handler(entry_handler);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		fired = false;

		key = irq_lock();

		bench_trigger();
		k_busy_wait(CONFIG_INT_BENCH_LOCK_HOLD_US);

		/*
		 * Pollute from inside the critical section, so that the
		 * interrupt is unmasked with the caches in the state a
		 * critical section doing real work would leave them in.
		 * This lengthens the hold time beyond the configured
		 * value.
		 */
		bench_load_pollute();

		start = timing_counter_get();
		irq_unlock(key);

		while (!fired) {
		}

		finish = isr_timestamp;
		record(i, timing_cycles_get(&start, &finish));
	}

	bench_trigger_set_handler(NULL);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_LOCKED */

#ifdef CONFIG_INT_BENCH_SCENARIO_THROUGHPUT
#define THROUGHPUT_BURSTS 10U

/*
 * One burst is plenty of warmup here: each of them serves
 * NUM_ITERATIONS interrupts back to back.
 */
#define THROUGHPUT_WARMUP 1U

static volatile bool done;
static volatile uint32_t isr_count;
static volatile timing_t end_timestamp;

static void throughput_handler(void)
{
	isr_count++;

	if (isr_count < NUM_ITERATIONS) {
		bench_trigger();
	} else {
		end_timestamp = timing_counter_get();
		done = true;
	}
}

/*
 * Sustained interrupt round-trip cost: the ISR re-triggers the
 * interrupt from within its own handler so interrupts are serviced
 * back to back (tail-chained where the hardware supports it). Each
 * sample is the average cost of one entry + ISR body + exit round trip
 * over a burst of NUM_ITERATIONS interrupts; its inverse is the
 * maximum sustainable interrupt rate. Requires the sw-irq backend.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, throughput_round_trip, NULL, NULL)
{
	timing_t start;
	timing_t finish;

	bench_trigger_set_handler(throughput_handler);

	for (uint32_t burst = 0U; burst < THROUGHPUT_BURSTS + THROUGHPUT_WARMUP; burst++) {
		isr_count = 0U;
		done = false;
		bench_load_pollute();

		start = timing_counter_get();
		bench_trigger();

		while (!done) {
		}

		finish = end_timestamp;
		record_after(burst, THROUGHPUT_WARMUP,
			     timing_cycles_get(&start, &finish) / NUM_ITERATIONS);
	}

	bench_trigger_set_handler(NULL);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_THROUGHPUT */

#ifdef CONFIG_INT_BENCH_SCENARIO_DYNAMIC
/*
 * Cost of installing an interrupt handler at runtime with
 * irq_connect_dynamic(). The line is disabled while the handler is
 * (re)installed because z_isr_install() requires the line to be
 * disabled on most architectures.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, dynamic_connect, NULL, NULL)
{
	timing_t start;
	timing_t finish;
	unsigned int line = bench_trigger_irq_line();

	irq_disable(line);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		bench_load_pollute();

		start = timing_counter_get();
		(void)irq_connect_dynamic(line, CONFIG_INT_BENCH_IRQ_PRIO,
					  bench_trigger_isr, NULL, 0);
		finish = timing_counter_get();

		record(i, timing_cycles_get(&start, &finish));
	}

	irq_enable(line);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_DYNAMIC */

#ifdef CONFIG_INT_BENCH_SCENARIO_MASKING
/*
 * Delay inflicted on a periodic interrupt.
 *
 * The other scenarios measure interrupts the benchmark raises itself.
 * This one measures what the rest of the system does to an interrupt
 * that was already scheduled: a periodic timer runs at the tick rate
 * while the benchmark exercises kernel primitives, and each sample is
 * how much later than its period the timer interrupt was actually
 * served.
 *
 * That delay is what an application experiences when the kernel or
 * another driver masks interrupts, but it is a sampling technique and
 * cannot separate the causes. A sample is only taken at a tick
 * boundary, so a masked window that does not overlap one is never
 * seen, and the reported delay also contains time spent in interrupts
 * that were served before this one. Treat the maximum as a sampled
 * lower bound on the longest interrupt-disabled window, not as proof
 * of one.
 *
 * Set CONFIG_INT_BENCH_MASK_INJECT_US to mask interrupts for a known
 * time in the work loop; the reported maximum should then be at least
 * that long, which validates the measurement on a new platform.
 */
static uint32_t mask_samples[NUM_ITERATIONS];
static volatile uint32_t mask_count;
static timing_t mask_prev;

static K_SEM_DEFINE(mask_sem, 0, 1);
static K_MUTEX_DEFINE(mask_mutex);

static void mask_timer_handler(struct k_timer *timer)
{
	timing_t now = timing_counter_get();
	timing_t prev = mask_prev;

	ARG_UNUSED(timer);

	if (prev != 0 && mask_count < NUM_ITERATIONS) {
		mask_samples[mask_count] = (uint32_t)timing_cycles_get(&prev, &now);
		mask_count++;
	}

	mask_prev = now;
}

static K_TIMER_DEFINE(mask_timer, mask_timer_handler, NULL);

static void sort_samples(uint32_t *samples, uint32_t count)
{
	for (uint32_t gap = count / 2U; gap > 0U; gap /= 2U) {
		for (uint32_t i = gap; i < count; i++) {
			uint32_t value = samples[i];
			uint32_t j = i;

			while ((j >= gap) && (samples[j - gap] > value)) {
				samples[j] = samples[j - gap];
				j -= gap;
			}
			samples[j] = value;
		}
	}
}

/* Kernel primitives that take a spinlock, and so mask interrupts */
static void mask_kernel_work(void)
{
	k_sem_give(&mask_sem);
	(void)k_sem_take(&mask_sem, K_NO_WAIT);
	(void)k_mutex_lock(&mask_mutex, K_NO_WAIT);
	(void)k_mutex_unlock(&mask_mutex);
	(void)k_uptime_get();
}

ZTEST_BENCHMARK_MANUAL(interrupt, periodic_isr_delay, NULL, NULL)
{
	uint32_t baseline;
	int64_t deadline;

	mask_count = 0U;
	mask_prev = 0;

	k_timer_start(&mask_timer, K_TICKS(1), K_TICKS(1));

	/*
	 * Give up quickly if the timer is not delivering at all, rather
	 * than spinning out the full collection window. Emulated
	 * platforms can take minutes of wall clock to run out even a
	 * short window of guest time.
	 */
	deadline = k_uptime_get() + 100;

	while (mask_count == 0U && k_uptime_get() < deadline) {
		mask_kernel_work();
	}

	if (mask_count == 0U) {
		k_timer_stop(&mask_timer);
		printk("periodic_isr_delay: timer did not deliver, skipping\n");
		return;
	}

	/*
	 * Bound the collection: if the timer delivers more slowly than
	 * the tick rate suggests this would otherwise spin for a long
	 * time. Allow four times the nominal duration.
	 */
	deadline = k_uptime_get() + (4 * MSEC_PER_SEC * NUM_ITERATIONS) /
		   CONFIG_SYS_CLOCK_TICKS_PER_SEC + 100;

	while (mask_count < NUM_ITERATIONS && k_uptime_get() < deadline) {
		mask_kernel_work();

		if (CONFIG_INT_BENCH_MASK_INJECT_US > 0) {
			unsigned int key = irq_lock();

			k_busy_wait(CONFIG_INT_BENCH_MASK_INJECT_US);
			irq_unlock(key);
		}
	}

	k_timer_stop(&mask_timer);

	if (mask_count < NUM_ITERATIONS) {
		/*
		 * Too few intervals to say anything. Recording no
		 * samples makes the framework report the benchmark as
		 * inconclusive, which is the honest outcome.
		 */
		printk("periodic_isr_delay: only %u of %u intervals sampled\n",
		       mask_count, NUM_ITERATIONS);
		return;
	}

	/*
	 * Establish the undelayed interval empirically rather than from
	 * the configured tick rate: the timing counter does not
	 * necessarily run at timing_freq_get() on every platform, and a
	 * baseline that is off by a constant would either hide every
	 * delay or turn every sample into one. The median interval is
	 * the undelayed period as long as most intervals are undelayed,
	 * and unlike the minimum it is not skewed by the short interval
	 * that follows a late one.
	 */
	sort_samples(mask_samples, NUM_ITERATIONS);
	baseline = mask_samples[NUM_ITERATIONS / 2U];

	for (uint32_t i = 0U; i < NUM_ITERATIONS; i++) {
		uint32_t delta = mask_samples[i];

		ztest_benchmark_record_sample((delta > baseline) ? (delta - baseline) : 0U);
	}
}
#endif /* CONFIG_INT_BENCH_SCENARIO_MASKING */

#if defined(CONFIG_INT_BENCH_SCENARIO_DIRECT) || defined(CONFIG_INT_BENCH_SCENARIO_ZLI)
/*
 * Bound on the spin waiting for the second line's ISR. A direct or
 * zero-latency interrupt that never arrives would otherwise hang the
 * run, and in the zero-latency case the wait happens with interrupts
 * locked, so nothing else could break the deadlock.
 */
#define ALT_SPIN_LIMIT 10000000U

static bool alt_wait(void)
{
	for (uint32_t spin = 0U; spin < ALT_SPIN_LIMIT; spin++) {
		if (alt_fired) {
			return true;
		}
	}

	return false;
}
#endif

/*
 * Entry latency of the second line, measured exactly as
 * entry_trigger_to_isr measures the first. What differs is only how
 * the line is connected, so the two are directly comparable.
 */
static void alt_entry_measure(const char *name)
{
	timing_t start;
	timing_t finish;

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		alt_fired = false;
		bench_load_pollute();

		start = timing_counter_get();
		bench_trigger_alt();

		if (!alt_wait()) {
			printk("%s: ISR did not run, skipping\n", name);
			return;
		}

		finish = alt_timestamp;
		record(i, timing_cycles_get(&start, &finish));
	}
}

#ifdef CONFIG_INT_BENCH_SCENARIO_DIRECT
/*
 * A directly connected ISR is dispatched from the vector table, so the
 * difference from entry_trigger_to_isr is the software ISR table
 * dispatch and the common entry code a regular ISR goes through.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, entry_direct_isr, NULL, NULL)
{
	alt_entry_measure("entry_direct_isr");
}
#endif /* CONFIG_INT_BENCH_SCENARIO_DIRECT */

#ifdef CONFIG_INT_BENCH_SCENARIO_ZLI
/*
 * Entry latency of a zero-latency interrupt raised while interrupts
 * are locked. A zero-latency interrupt runs above the priority the
 * kernel masks with, so unlike the interrupt in locked_unlock_to_isr
 * it is served inside the critical section rather than after it. The
 * two scenarios together show what a critical section costs an
 * interrupt, and what escaping it buys.
 */
/*
 * Entry latency of a zero-latency interrupt with interrupts enabled.
 * This is the lowest latency Zephyr offers, and being measured the
 * same way as entry_trigger_to_isr and entry_direct_isr it can be
 * compared with them; zli_entry_while_locked below answers the
 * different question of what happens inside a critical section.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, zli_entry_trigger_to_isr, NULL, NULL)
{
	alt_entry_measure("zli_entry_trigger_to_isr");
}

ZTEST_BENCHMARK_MANUAL(interrupt, zli_entry_while_locked, NULL, NULL)
{
	timing_t start;
	timing_t finish;
	unsigned int key;
	bool served;

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		alt_fired = false;
		bench_load_pollute();

		key = irq_lock();

		start = timing_counter_get();
		bench_trigger_alt();

		served = alt_wait();

		irq_unlock(key);

		if (!served) {
			printk("zli_entry_while_locked: not served while locked, skipping\n");
			return;
		}

		finish = alt_timestamp;
		record(i, timing_cycles_get(&start, &finish));
	}
}
#endif /* CONFIG_INT_BENCH_SCENARIO_ZLI */

#ifdef CONFIG_INT_BENCH_SCENARIO_END_TO_END
static volatile timing_t e2e_start;

static void e2e_handler(void)
{
	k_sem_give(&wake_sem);
}

static void e2e_waiter_entry(void *p1, void *p2, void *p3)
{
	timing_t start;
	timing_t finish;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		k_sem_take(&wake_sem, K_FOREVER);

		finish = timing_counter_get();
		start = e2e_start;
		record(i, timing_cycles_get(&start, &finish));

		k_sem_give(&sync_sem);
	}
}

/*
 * The whole path an application actually waits on: from raising the
 * interrupt to the high priority thread it wakes being on the CPU.
 *
 * The other scenarios measure the pieces of this span, but the pieces
 * cannot simply be added: entry latency and the rescheduling exit are
 * measured in separate runs and neither includes the ISR body or the
 * handoff between them. This is the figure to quote for how quickly an
 * application can respond to an event.
 */
ZTEST_BENCHMARK_MANUAL(interrupt, irq_to_thread, NULL, NULL)
{
	int priority = k_thread_priority_get(k_current_get());

	bench_trigger_set_handler(e2e_handler);

	k_thread_create(&waiter_thread, waiter_stack, K_THREAD_STACK_SIZEOF(waiter_stack),
			e2e_waiter_entry, NULL, NULL, NULL, priority - 1, 0, K_NO_WAIT);

	for (uint32_t i = 0U; i < TOTAL_ITERATIONS; i++) {
		bench_load_pollute();

		e2e_start = timing_counter_get();
		bench_trigger();

		k_sem_take(&sync_sem, K_FOREVER);
	}

	k_thread_join(&waiter_thread, K_FOREVER);
	bench_trigger_set_handler(NULL);
}
#endif /* CONFIG_INT_BENCH_SCENARIO_END_TO_END */
