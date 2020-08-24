#include <zephyr.h>
#include <arch/cpu.h>
#include <sys/arch_interface.h>
#include <sys/atomic.h>

#define NUM_THREADS 3
#define TCOUNT 10
#define COUNT_LIMIT 12

static int state;
enum power_state {
	ACTIVE = 0,
	SUSPENDING = 1,
	SUSPENDED = 2,
	RESUMING = 3,
	OFF = 4
};

atomic_t usage_count = 0;

K_MUTEX_DEFINE(suspend_mutex);
K_MUTEX_DEFINE(resume_mutex);

K_CONDVAR_DEFINE(suspend_cv);
K_CONDVAR_DEFINE(resume_cv);

#define STACK_SIZE (1024)

K_THREAD_STACK_EXTERN(tstack);
K_THREAD_STACK_ARRAY_DEFINE(tstacks, NUM_THREADS, STACK_SIZE);

static struct k_thread t[NUM_THREADS];


void resume(long thread_id)
{
	printk("Resuming by thread %ld..\n", thread_id);
	state = RESUMING;
	k_sleep(K_MSEC(500));
	state = ACTIVE;
	printk("Resumed by thread %ld...\n", thread_id);
	k_condvar_broadcast(&resume_cv);
}


void suspend(long thread_id)
{
	printk("Suspending by thread %ld..\n", thread_id);
	state = SUSPENDING;
	k_sleep(K_MSEC(500));
	state = SUSPENDED;
	printk("Suspended by thread %ld..\n", thread_id);
	k_condvar_broadcast(&resume_cv);
}

int release(long thread_id)
{

	atomic_dec(&usage_count);

	if (usage_count > 0 || state == SUSPENDED) {
		printk("usage_count: %d\n", usage_count);
		return 0;
	}

	k_mutex_lock(&resume_mutex, K_FOREVER);

	while (state == SUSPENDING || state == RESUMING ) {

		printk("release(): thread %ld Status= %d. Going into wait...\n",
			thread_id, state);
		k_condvar_wait(&resume_cv, &resume_mutex, K_FOREVER);

		printk("release(): thread %ld Condition signal received. State= %d\n",
			thread_id, state);
	}
	k_mutex_unlock(&resume_mutex);

	suspend(thread_id);

	return 0;
}

int claim(long thread_id)
{

	atomic_inc(&usage_count);

	if (state == ACTIVE) {
		return 0;
	}

	k_mutex_lock(&resume_mutex, K_FOREVER);
	while (state == RESUMING || state == SUSPENDING ) {

		printk("claim(): thread %ld Status= %d. Going into wait...\n",
			thread_id, state);
		k_condvar_wait(&resume_cv, &resume_mutex, K_FOREVER);

		printk("claim(): thread %ld Condition signal received. State= %d\n",
			thread_id, state);
	}
	k_mutex_unlock(&resume_mutex);

	if (state == SUSPENDED) {
		resume(thread_id);
	}

	return 0;
}

void worker(void *p1, void *p2, void *p3)
{
	long my_id = (long)p1;
	int rc;

	printk("Starting worker(): thread %ld\n", my_id);


	rc = claim(my_id);
	printk("Thread %ld returned after claiming. Status=%d\n", my_id, state);
	/* do some work */
	k_busy_wait(USEC_PER_MSEC * 500);

	rc = release(my_id);
	printk("Thread %ld returned after releasing. Status=%d\n", my_id, state);

}

void main(void)
{
	long t1 = 1, t2 = 2, t3 = 3;
	int i;

	state = SUSPENDED;

	k_thread_create(&t[0], tstacks[0], STACK_SIZE, worker,
			INT_TO_POINTER(t1), NULL, NULL, K_PRIO_PREEMPT(10), 0,
			K_NO_WAIT);

	k_thread_create(&t[1], tstacks[1], STACK_SIZE, worker,
			INT_TO_POINTER(t2), NULL, NULL, K_PRIO_PREEMPT(11), 0,
			K_NO_WAIT);

	k_thread_create(&t[2], tstacks[2], STACK_SIZE, worker,
			INT_TO_POINTER(t3), NULL, NULL, K_PRIO_PREEMPT(12), 0,
			K_NO_WAIT);

	/* Wait for all threads to complete */
	for (i = 0; i < NUM_THREADS; i++) {
		k_thread_join(&t[i], K_FOREVER);
	}

	printk("Main(): Waited and joined with %d threads. Final value of state = %d. Done.\n",
	       NUM_THREADS, state);
}
