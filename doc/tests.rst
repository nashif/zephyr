Integration Tests
=================


.. item:: test_mutex_lock_unlock test lock unlock
    :validates: ZEP-5

    :c:func:`test_mutex_lock_unlock`

.. item:: test_mutex_recursive Test recursive mutex.
    :validates:

    :c:func:`test_mutex_recursive`

.. item:: test_mutex_priority_inheritance Test mutex's priority inheritance mechanism.
    :validates:

    :c:func:`test_mutex_priority_inheritance`

.. item:: test_mutex_timeout_race_during_priority_inversion Test fix for subtle race during priority inversion.
    :validates:

    :c:func:`test_mutex_timeout_race_during_priority_inversion`

.. item:: test_mutex_init_null Test initializing mutex with a NULL pointer.
    :validates:

    :c:func:`test_mutex_init_null`

.. item:: test_mutex_init_invalid_obj Test initialize mutex with a invalid kernel object.
    :validates:

    :c:func:`test_mutex_init_invalid_obj`

.. item:: test_mutex_lock_null Test locking mutex with a NULL pointer.
    :validates:

    :c:func:`test_mutex_lock_null`

.. item:: test_mutex_lock_invalid_obj Test locking mutex with a invalid kernel object.
    :validates:

    :c:func:`test_mutex_lock_invalid_obj`

.. item:: test_mutex_unlock_null Test unlocking mutex with a NULL pointer.
    :validates:

    :c:func:`test_mutex_unlock_null`

.. item:: test_mutex_unlock_invalid_obj Test unlocking mutex with a invalid kernel object.
    :validates:

    :c:func:`test_mutex_unlock_invalid_obj`

.. item:: test_k_sem_define Test semaphore defined at compile time.
    :validates: ZEP-12001 ZEP-12005

    :c:func:`test_k_sem_define`

.. item:: test_sem_thread2thread Test synchronization of threads with semaphore.
    :validates: ZEP-12002

    :c:func:`test_sem_thread2thread`

.. item:: test_sem_thread2isr Test synchronization between thread and irq.
    :validates:

    :c:func:`test_sem_thread2isr`

.. item:: test_k_sem_init Test semaphore initialization at runtime.
    :validates: ZEP-12002 ZEP-12006 ZEP-12025

    :c:func:`test_k_sem_init`

.. item:: test_k_sem_init_max Test semaphore initialization with maximum limit.
    :validates: ZEP-12003

    :c:func:`test_k_sem_init_max`

.. item:: test_sem_reset Test semaphore reset interface.
    :validates: ZEP-12023

    :c:func:`test_sem_reset`

.. item:: test_sem_reset_waiting Test aborting all semaphore takes after semaphore reset.
    :validates: ZEP-12024

    :c:func:`test_sem_reset_waiting`

.. item:: test_sem_count_get Test k_sem_count_get()
    :validates: ZEP-12015

    :c:func:`test_sem_count_get`

.. item:: test_sem_give_from_isr Test whether a semaphore can be given by an ISR.
    :validates: ZEP-12012

    :c:func:`test_sem_give_from_isr`

.. item:: test_sem_give_from_thread Test semaphore count when given by thread.
    :validates: ZEP-12012 ZEP-12013 ZEP-12023

    :c:func:`test_sem_give_from_thread`

.. item:: test_sem_take_no_wait Test semaphore count decreases on semaphore acquisition.
    :validates: ZEP-12007 ZEP-12011

    :c:func:`test_sem_take_no_wait`

.. item:: test_sem_take_no_wait_fails Test when there is no semaphore to take.
    :validates: ZEP-12011

    :c:func:`test_sem_take_no_wait_fails`

.. item:: test_sem_take_timeout_fails Test a semaphore take operation with an unavailable semaphore.
    :validates: ZEP-12008 ZEP-12010

    :c:func:`test_sem_take_timeout_fails`

.. item:: test_sem_take_timeout Test the semaphore take operation with specified timeout.
    :validates: ZEP-12006 ZEP-12010

    :c:func:`test_sem_take_timeout`

.. item:: test_sem_take_timeout_forever Test the semaphore take operation with forever wait.
    :validates: ZEP-12006

    :c:func:`test_sem_take_timeout_forever`

.. item:: test_sem_take_timeout_isr Test k_sem_take()
    :validates: ZEP-12006

    :c:func:`test_sem_take_timeout_isr`

.. item:: test_sem_take_multiple Test semaphore take operation by multiple threads.
    :validates: ZEP-12006 ZEP-12014

    :c:func:`test_sem_take_multiple`

.. item:: test_k_sem_correct_count_limit Test the max value a semaphore can be given and taken.
    :validates: ZEP-12004 ZEP-12009

    :c:func:`test_k_sem_correct_count_limit`

.. item:: test_sem_give_take_from_isr Test semaphore give and take and its count from ISR.
    :validates:

    :c:func:`test_sem_give_take_from_isr`

.. item:: test_sem_multiple_threads_wait Test multiple semaphore take and give with wait.
    :validates:

    :c:func:`test_sem_multiple_threads_wait`

.. item:: test_sem_measure_timeouts Test semaphore timeout period.
    :validates: ZEP-12009 ZEP-12010 ZEP-12011

    :c:func:`test_sem_measure_timeouts`

.. item:: test_sem_measure_timeout_from_thread Test timeout of semaphore from thread.
    :validates: ZEP-12009

    :c:func:`test_sem_measure_timeout_from_thread`

.. item:: test_sem_multiple_take_and_timeouts Test multiple semaphore take with timeouts.
    :validates: ZEP-12009

    :c:func:`test_sem_multiple_take_and_timeouts`

.. item:: test_sem_multi_take_timeout_diff_sem Test sequence of multiple semaphore timeouts.
    :validates:

    :c:func:`test_sem_multi_take_timeout_diff_sem`

.. item:: test_sem_queue_mutual_exclusion Test thread mutual exclusion using a semaphore.
    :validates: ZEP-99

    :c:func:`test_sem_queue_mutual_exclusion`

.. item:: test_sem_give_null Test k_sem_give()
    :validates:

    :c:func:`test_sem_give_null`

.. item:: test_sem_init_null Test k_sem_init()
    :validates:

    :c:func:`test_sem_init_null`

.. item:: test_sem_take_null Test k_sem_take()
    :validates:

    :c:func:`test_sem_take_null`

.. item:: test_sem_reset_null Test k_sem_reset()
    :validates:

    :c:func:`test_sem_reset_null`

.. item:: test_sem_count_get_null Test k_sem_count_get()
    :validates:

    :c:func:`test_sem_count_get_null`

.. item:: test_multiple_thread_sem_usage Test semaphore usage with multiple thread.
    :validates:

    :c:func:`test_multiple_thread_sem_usage`

.. item:: test_multi_thread_sem_limit Test max semaphore can be give and take with multiple thread.
    :validates:

    :c:func:`test_multi_thread_sem_limit`

.. item:: test_dyn_thread_perms Test object permission on dynamic user thread when index is reused.
    :validates:

    :c:func:`test_dyn_thread_perms`

.. item:: test_kernel_create_dyn_user_thread Test creation of dynamic user thread under kernel thread.
    :validates:

    :c:func:`test_kernel_create_dyn_user_thread`

.. item:: test_user_create_dyn_user_thread Test creation of dynamic user thread under user thread.
    :validates:

    :c:func:`test_user_create_dyn_user_thread`

.. item:: test_systhreads_main Verify main thread.
    :validates:

    :c:func:`test_systhreads_main`

.. item:: test_systhreads_idle Verify idle thread.
    :validates:

    :c:func:`test_systhreads_idle`

.. item:: test_customdata_get_set_coop test thread custom data get/set from coop thread
    :validates:

    :c:func:`test_customdata_get_set_coop`

.. item:: test_thread_name_get_set test thread name get/set from supervisor thread
    :validates:

    :c:func:`test_thread_name_get_set`

.. item:: test_thread_name_user_get_set test thread name get/set from user thread
    :validates:

    :c:func:`test_thread_name_user_get_set`

.. item:: test_customdata_get_set_preempt test thread custom data get/set from preempt thread
    :validates:

    :c:func:`test_customdata_get_set_preempt`

.. item:: test_essential_thread_operation Test to validate essential flag set/clear.
    :validates:

    :c:func:`test_essential_thread_operation`

.. item:: test_essential_thread_abort Abort an essential thread.
    :validates:

    :c:func:`test_essential_thread_abort`

.. item:: test_k_thread_foreach Test k_thread_foreach API.
    :validates:

    :c:func:`test_k_thread_foreach`

.. item:: test_k_thread_foreach_unlocked Test k_thread_foreach_unlock API.
    :validates:

    :c:func:`test_k_thread_foreach_unlocked`

.. item:: test_k_thread_foreach_null_cb Test k_thread_foreach API with null callback.
    :validates:

    :c:func:`test_k_thread_foreach_null_cb`

.. item:: test_k_thread_foreach_unlocked_null_cb Test k_thread_foreach_unlocked API with null callback.
    :validates:

    :c:func:`test_k_thread_foreach_unlocked_null_cb`

.. item:: test_k_thread_state_str Test k_thread_state_str API with null callback.
    :validates:

    :c:func:`test_k_thread_state_str`

.. item:: test_threads_abort_self Validate k_thread_abort()
    :validates:

    :c:func:`test_threads_abort_self`

.. item:: test_threads_abort_others Validate k_thread_abort()
    :validates:

    :c:func:`test_threads_abort_others`

.. item:: test_threads_abort_repeat Test abort on a terminated thread.
    :validates:

    :c:func:`test_threads_abort_repeat`

.. item:: test_delayed_thread_abort Test abort on delayed thread before it has started execution.
    :validates:

    :c:func:`test_delayed_thread_abort`

.. item:: test_abort_from_isr Show that threads can be aborted from interrupt context by itself.
    :validates:

    :c:func:`test_abort_from_isr`

.. item:: test_abort_from_isr_not_self Show that threads can be aborted from interrupt context.
    :validates:

    :c:func:`test_abort_from_isr_not_self`

.. item:: test_threads_priority_set Test the k_thread_priority_set()
    :validates:

    :c:func:`test_threads_priority_set`

.. item:: test_threads_spawn_params Check the parameters passed to thread entry function.
    :validates:

    :c:func:`test_threads_spawn_params`

.. item:: test_threads_spawn_priority Spawn thread with higher priority.
    :validates:

    :c:func:`test_threads_spawn_priority`

.. item:: test_threads_spawn_delay Spawn thread with a delay.
    :validates:

    :c:func:`test_threads_spawn_delay`

.. item:: test_threads_spawn_forever Spawn thread with forever delay and highest priority.
    :validates:

    :c:func:`test_threads_spawn_forever`

.. item:: test_thread_start Validate behavior of multiple calls to k_thread_start()
    :validates:

    :c:func:`test_thread_start`

.. item:: test_threads_suspend_resume_cooperative Check the suspend and resume functionality in a cooperative thread.
    :validates: ZEP-109

    :c:func:`test_threads_suspend_resume_cooperative`

.. item:: test_threads_suspend_resume_preemptible Check the suspend and resume functionality in preemptive thread.
    :validates: ZEP-109

    :c:func:`test_threads_suspend_resume_preemptible`

.. item:: test_threads_suspend Check that k_thread_suspend()
    :validates: ZEP-109

    :c:func:`test_threads_suspend`

.. item:: test_threads_suspend_timeout Check that k_thread_suspend()
    :validates: ZEP-109

    :c:func:`test_threads_suspend_timeout`

.. item:: test_resume_unsuspend_thread Check resume an unsuspend thread.
    :validates: ZEP-109

    :c:func:`test_resume_unsuspend_thread`

.. item:: test_kdefine_preempt_thread test preempt thread initialization via K_THREAD_DEFINE
    :validates: ZEP-23

    :c:func:`test_kdefine_preempt_thread`

.. item:: test_kdefine_coop_thread test coop thread initialization via K_THREAD_DEFINE
    :validates: ZEP-23

    :c:func:`test_kdefine_coop_thread`
