/*
 * Copyright (c) 1997-2015 Wind River Systems, Inc.
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

/**
 * @file
 * @brief mutex kernel services
 *
 * This module contains routines for handling mutex locking and unlocking.  It
 * also includes routines that force the release of  mutex objects when a task
 * is aborted or unloaded.
 *
 * Mutexes implement a priority inheritance algorithm that boosts the priority
 * level of the owning task to match the priority level of the highest priority
 * task waiting on the mutex.
 *
 * Each mutex that contributes to priority inheritance must be released in the
 * reverse order in which is was acquired.  Furthermore each subsequent mutex
 * that contributes to raising the owning task's priority level must be acquired
 * at a point after the most recent "bumping" of the priority level.
 *
 * For example, if task A has two mutexes contributing to the raising of its
 * priority level, the second mutex M2 must be acquired by task A after task
 * A's priority level was bumped due to owning the first mutex M1.  When
 * releasing the mutex, task A must release M2 before it releases M1.  Failure
 * to follow this nested model may result in tasks running at unexpected priority
 * levels (too high, or too low).
 */

#include <microkernel.h>
#include <micro_private.h>
#include <nano_private.h>

/**
 * @brief Reply to a mutex lock request.
 *
 * This routine replies to a mutex lock request.  This will occur if either
 * the waiting task times out or acquires the mutex lock.
 *
 * @param A k_args
 *
 * @return N/A
 */
void _k_mutex_lock_reply(
	struct k_args *A /* pointer to mutex lock reply request arguments */
	)
{
#ifdef CONFIG_SYS_CLOCK_EXISTS
	struct _k_mutex_struct *Mutex; /* pointer to internal mutex structure */
	struct k_args *PrioChanger; /* used to change a task's priority level */
	struct k_args *FirstWaiter; /* pointer to first task in wait queue */
	kpriority_t newPriority;    /* priority level to which to drop */
	int MutexId;                /* mutex ID obtained from request args */

	if (A->Time.timer) {
		FREETIMER(A->Time.timer);
	}

	if (A->Comm == _K_SVC_MUTEX_LOCK_REPLY_TIMEOUT) {/* Timeout case */

		REMOVE_ELM(A);
		A->Time.rcode = RC_TIME;

		MutexId = A->args.l1.mutex;
		Mutex = (struct _k_mutex_struct *)MutexId;

		FirstWaiter = Mutex->waiters;

		/*
		 * When timing out, there are two cases to consider.
		 * 1. There are no waiting tasks.
		 *    - As there are no waiting tasks, this mutex is no longer
		 *    involved in priority inheritance.  It's current priority
		 *    level should be dropped (if needed) to the original
		 *    priority level.
		 * 2. There is at least one waiting task in a priority ordered
		 *    list.
		 *    - Depending upon the the priority level of the first
		 *    waiting task, the owner task's original priority and
		 *    the ceiling priority, the owner's priority level may
		 *    be dropped but not necessarily to the original priority
		 *    level.
		 */

		newPriority = Mutex->original_owner_priority;

		if (FirstWaiter != NULL) {
			newPriority = (FirstWaiter->priority < newPriority)
					      ? FirstWaiter->priority
					      : newPriority;
			newPriority = (newPriority > CONFIG_PRIORITY_CEILING)
					      ? newPriority
					      : CONFIG_PRIORITY_CEILING;
		}

		if (Mutex->current_owner_priority != newPriority) {
			GETARGS(PrioChanger);
			PrioChanger->alloc = true;
			PrioChanger->Comm = _K_SVC_TASK_PRIORITY_SET;
			PrioChanger->priority = newPriority;
			PrioChanger->args.g1.task = Mutex->owner;
			PrioChanger->args.g1.prio = newPriority;
			SENDARGS(PrioChanger);
			Mutex->current_owner_priority = newPriority;
		}
	} else {/* LOCK_RPL: Reply case */
		A->Time.rcode = RC_OK;
	}
#else
	/* LOCK_RPL: Reply case */
	A->Time.rcode = RC_OK;
#endif

	_k_state_bit_reset(A->Ctxt.task, TF_LOCK);
}

/**
 * @brief Reply to a mutex lock request with timeout.
 *
 * This routine replies to a mutex lock request.  This will occur if either
 * the waiting task times out or acquires the mutex lock.
 *
 * @param A Pointer to a k_args structure.
 *
 * @return N/A
 */
void _k_mutex_lock_reply_timeout(struct k_args *A)
{
	_k_mutex_lock_reply(A);
}

/**
 * @brief Process a mutex lock request
 *
 * This routine processes a mutex lock request (LOCK_REQ).  If the mutex
 * is already locked, and the timeout is non-zero then the priority inheritance
 * algorithm may be applied to prevent priority inversion scenarios.
 *
 * @param A k_args
 *
 * @return N/A
 */
void _k_mutex_lock_request(struct k_args *A /* pointer to mutex lock
					     * request arguments
					     */
				     )
{
	struct _k_mutex_struct *Mutex; /* pointer to internal mutex structure */
	int MutexId;                /* mutex ID obtained from lock request */
	struct k_args *PrioBooster; /* used to change a task's priority level */
	kpriority_t BoostedPrio;    /* new "boosted" priority level */

	MutexId = A->args.l1.mutex;


	Mutex = (struct _k_mutex_struct *)MutexId;
	if (Mutex->level == 0 || Mutex->owner == A->args.l1.task) {
		/* The mutex is either unowned or this is a nested lock. */
#ifdef CONFIG_OBJECT_MONITOR
		Mutex->count++;
#endif

		Mutex->owner = A->args.l1.task;

		/*
		 * Assign the current owner's priority from the priority found
		 * in the current task's task object: the priority stored there
		 * may be more recent than the one stored in struct k_args.
		 */
		Mutex->current_owner_priority = _k_current_task->priority;

		/*
		 * Save the original priority when first acquiring the lock (but
		 * not on nested locks).  The original priority level only
		 * reflects the priority level of the requesting task at the
		 * time the lock is acquired.  Consequently, if the requesting
		 * task is already involved in priority inheritance, this
		 * original priority reflects its "boosted" priority.
		 */
		if (Mutex->level == 0) {
			Mutex->original_owner_priority = Mutex->current_owner_priority;
		}

		Mutex->level++;

		A->Time.rcode = RC_OK;

	} else {
		/* The mutex is owned by another task. */
#ifdef CONFIG_OBJECT_MONITOR
		Mutex->num_conflicts++;
#endif

		if (likely(A->Time.ticks != TICKS_NONE)) {
			/*
			 * A non-zero timeout was specified.  Ensure the
			 * priority saved in the request is up to date
			 */
			A->Ctxt.task = _k_current_task;
			A->priority = _k_current_task->priority;
			_k_state_bit_set(_k_current_task, TF_LOCK);
			/* Note: Mutex->waiters is a priority sorted list */
			INSERT_ELM(Mutex->waiters, A);
#ifdef CONFIG_SYS_CLOCK_EXISTS
			if (A->Time.ticks == TICKS_UNLIMITED) {
				/* Request will not time out */
				A->Time.timer = NULL;
			} else {
				/*
				 * Prepare to call _k_mutex_lock_reply() should
				 * the request time out.
				 */
				A->Comm = _K_SVC_MUTEX_LOCK_REPLY_TIMEOUT;
				_k_timeout_alloc(A);
			}
#endif
			if (A->priority < Mutex->current_owner_priority) {
				/*
				 * The priority level of the owning task is less
				 * than that of the requesting task.  Boost the
				 * priority level of the owning task to match
				 * the priority level of the requesting task.
				 * Note that the boosted priority level is
				 * limited to <K_PrioCeiling>.
				 */
				BoostedPrio = (A->priority > CONFIG_PRIORITY_CEILING)
						      ? A->priority
						      : CONFIG_PRIORITY_CEILING;
				if (BoostedPrio < Mutex->current_owner_priority) {
					/* Boost the priority level */
					GETARGS(PrioBooster);

					PrioBooster->alloc = true;
					PrioBooster->Comm = _K_SVC_TASK_PRIORITY_SET;
					PrioBooster->priority = BoostedPrio;
					PrioBooster->args.g1.task = Mutex->owner;
					PrioBooster->args.g1.prio = BoostedPrio;
					SENDARGS(PrioBooster);
					Mutex->current_owner_priority = BoostedPrio;
				}
			}
		} else {
			/*
			 * ERROR.  The mutex is locked by another task and
			 * this is an immediate lock request (timeout = 0).
			 */
			A->Time.rcode = RC_FAIL;
		}
	}
}

int task_mutex_lock(kmutex_t mutex, int32_t timeout)
{
	struct k_args A; /* argument packet */

	A.Comm = _K_SVC_MUTEX_LOCK_REQUEST;
	A.Time.ticks = timeout;
	A.args.l1.mutex = mutex;
	A.args.l1.task = _k_current_task->id;
	KERNEL_ENTRY(&A);
	return A.Time.rcode;
}

/**
 * @brief Process a mutex unlock request
 *
 * This routine processes a mutex unlock request (UNLOCK).  If the mutex
 * was involved in priority inheritance, then it will change the priority level
 * of the current owner to the priority level it had when it acquired the
 * mutex.
 *
 * @param A pointer to mutex unlock request arguments
 *
 * @return N/A
 */
void _k_mutex_unlock(struct k_args *A)
{
	struct _k_mutex_struct *Mutex; /* pointer internal mutex structure */
	int MutexId;                /* mutex ID obtained from unlock request */
	struct k_args *PrioDowner;  /* used to change a task's priority level */

	MutexId = A->args.l1.mutex;
	Mutex = (struct _k_mutex_struct *)MutexId;
	if (Mutex->owner == A->args.l1.task && --(Mutex->level) == 0) {
		/*
		 * The requesting task owns the mutex and all locks
		 * have been released.
		 */

		struct k_args *X;

#ifdef CONFIG_OBJECT_MONITOR
		Mutex->count++;
#endif

		if (Mutex->current_owner_priority != Mutex->original_owner_priority) {
			/*
			 * This mutex is involved in priority inheritance.
			 * Send a request to revert the priority level of
			 * the owning task back to its priority level when
			 * it first acquired the mutex.
			 */
			GETARGS(PrioDowner);

			PrioDowner->alloc = true;
			PrioDowner->Comm = _K_SVC_TASK_PRIORITY_SET;
			PrioDowner->priority = Mutex->original_owner_priority;
			PrioDowner->args.g1.task = Mutex->owner;
			PrioDowner->args.g1.prio = Mutex->original_owner_priority;
			SENDARGS(PrioDowner);
		}

		X = Mutex->waiters;
		if (X != NULL) {
			/*
			 * At least one task was waiting for the mutex.
			 * Assign the new owner of the task to be the
			 * first in the queue.
			 */

			Mutex->waiters = X->next;
			Mutex->owner = X->args.l1.task;
			Mutex->level = 1;
			Mutex->current_owner_priority = X->priority;
			Mutex->original_owner_priority = X->priority;

#ifdef CONFIG_SYS_CLOCK_EXISTS
			if (X->Time.timer) {
				/*
				 * Trigger a call to _k_mutex_lock_reply()--it
				 * will send a reply with a return code of
				 * RC_OK.
				 */
				_k_timeout_cancel(X);
				X->Comm = _K_SVC_MUTEX_LOCK_REPLY;
			} else {
#endif
				/*
				 * There is no timer to update.
				 * Set the return code.
				 */
				X->Time.rcode = RC_OK;
					_k_state_bit_reset(X->Ctxt.task, TF_LOCK);
#ifdef CONFIG_SYS_CLOCK_EXISTS
			}
#endif
		} else {
			/* No task is waiting in the queue. */
			Mutex->owner = ANYTASK;
			Mutex->level = 0;
		}
	}
}

/**
 * @brief Mutex unlock kernel service
 *
 * This routine is the entry to the mutex unlock kernel service.
 *
 * @param mutex mutex to unlock
 *
 * @return N/A
 */
void _task_mutex_unlock(kmutex_t mutex)
{
	struct k_args A; /* argument packet */

	A.Comm = _K_SVC_MUTEX_UNLOCK;
	A.args.l1.mutex = mutex;
	A.args.l1.task = _k_current_task->id;
	KERNEL_ENTRY(&A);
}
