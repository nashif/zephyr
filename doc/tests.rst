Integration Tests
=================


.. item:: test_mutex_lock_unlock test lock unlock


    .. doxygendefine:: test_mutex_lock_unlock
        :project: Zephyr

.. item:: test_mutex_recursive Test recursive mutex.


    .. doxygendefine:: test_mutex_recursive
        :project: Zephyr

.. item:: test_mutex_priority_inheritance Test mutex's priority inheritance mechanism.


    .. doxygendefine:: test_mutex_priority_inheritance
        :project: Zephyr

.. item:: test_mutex_timeout_race_during_priority_inversion Test fix for subtle race during priority inversion.


    .. doxygendefine:: test_mutex_timeout_race_during_priority_inversion
        :project: Zephyr

.. item:: test_mutex_init_null Test initializing mutex with a NULL pointer.


    .. doxygendefine:: test_mutex_init_null
        :project: Zephyr

.. item:: test_mutex_init_invalid_obj Test initialize mutex with a invalid kernel object.


    .. doxygendefine:: test_mutex_init_invalid_obj
        :project: Zephyr

.. item:: test_mutex_lock_null Test locking mutex with a NULL pointer.


    .. doxygendefine:: test_mutex_lock_null
        :project: Zephyr

.. item:: test_mutex_lock_invalid_obj Test locking mutex with a invalid kernel object.


    .. doxygendefine:: test_mutex_lock_invalid_obj
        :project: Zephyr

.. item:: test_mutex_unlock_null Test unlocking mutex with a NULL pointer.


    .. doxygendefine:: test_mutex_unlock_null
        :project: Zephyr

.. item:: test_mutex_unlock_invalid_obj Test unlocking mutex with a invalid kernel object.


    .. doxygendefine:: test_mutex_unlock_invalid_obj
        :project: Zephyr

.. item:: test_multiple_thread_sem_usage Test semaphore usage with multiple thread.


    .. doxygendefine:: test_multiple_thread_sem_usage
        :project: Zephyr

.. item:: test_multi_thread_sem_limit Test max semaphore can be give and take with multiple thread.


    .. doxygendefine:: test_multi_thread_sem_limit
        :project: Zephyr

.. item:: test_dyn_thread_perms Test object permission on dynamic user thread when index is reused.


    .. doxygendefine:: test_dyn_thread_perms
        :project: Zephyr

.. item:: test_kernel_create_dyn_user_thread Test creation of dynamic user thread under kernel thread.


    .. doxygendefine:: test_kernel_create_dyn_user_thread
        :project: Zephyr

.. item:: test_user_create_dyn_user_thread Test creation of dynamic user thread under user thread.


    .. doxygendefine:: test_user_create_dyn_user_thread
        :project: Zephyr

.. item:: test_systhreads_main Verify main thread.


    .. doxygendefine:: test_systhreads_main
        :project: Zephyr

.. item:: test_systhreads_idle Verify idle thread.


    .. doxygendefine:: test_systhreads_idle
        :project: Zephyr

.. item:: test_customdata_get_set_coop test thread custom data get/set from coop thread


    .. doxygendefine:: test_customdata_get_set_coop
        :project: Zephyr

.. item:: test_thread_name_get_set test thread name get/set from supervisor thread


    .. doxygendefine:: test_thread_name_get_set
        :project: Zephyr

.. item:: test_thread_name_user_get_set test thread name get/set from user thread


    .. doxygendefine:: test_thread_name_user_get_set
        :project: Zephyr

.. item:: test_customdata_get_set_preempt test thread custom data get/set from preempt thread


    .. doxygendefine:: test_customdata_get_set_preempt
        :project: Zephyr

.. item:: test_essential_thread_operation Test to validate essential flag set/clear.


    .. doxygendefine:: test_essential_thread_operation
        :project: Zephyr

.. item:: test_essential_thread_abort Abort an essential thread.


    .. doxygendefine:: test_essential_thread_abort
        :project: Zephyr

.. item:: test_k_thread_foreach Test k_thread_foreach API.


    .. doxygendefine:: test_k_thread_foreach
        :project: Zephyr

.. item:: test_k_thread_foreach_unlocked Test k_thread_foreach_unlock API.


    .. doxygendefine:: test_k_thread_foreach_unlocked
        :project: Zephyr

.. item:: test_k_thread_foreach_null_cb Test k_thread_foreach API with null callback.


    .. doxygendefine:: test_k_thread_foreach_null_cb
        :project: Zephyr

.. item:: test_k_thread_foreach_unlocked_null_cb Test k_thread_foreach_unlocked API with null callback.


    .. doxygendefine:: test_k_thread_foreach_unlocked_null_cb
        :project: Zephyr

.. item:: test_k_thread_state_str Test k_thread_state_str API with null callback.


    .. doxygendefine:: test_k_thread_state_str
        :project: Zephyr

.. item:: test_threads_abort_self Validate k_thread_abort()


    .. doxygendefine:: test_threads_abort_self
        :project: Zephyr

.. item:: test_threads_abort_others Validate k_thread_abort()


    .. doxygendefine:: test_threads_abort_others
        :project: Zephyr

.. item:: test_threads_abort_repeat Test abort on a terminated thread.


    .. doxygendefine:: test_threads_abort_repeat
        :project: Zephyr

.. item:: test_delayed_thread_abort Test abort on delayed thread before it has started execution.


    .. doxygendefine:: test_delayed_thread_abort
        :project: Zephyr

.. item:: test_abort_from_isr Show that threads can be aborted from interrupt context by itself.


    .. doxygendefine:: test_abort_from_isr
        :project: Zephyr

.. item:: test_abort_from_isr_not_self Show that threads can be aborted from interrupt context.


    .. doxygendefine:: test_abort_from_isr_not_self
        :project: Zephyr

.. item:: test_threads_priority_set Test the k_thread_priority_set()


    .. doxygendefine:: test_threads_priority_set
        :project: Zephyr

.. item:: test_threads_spawn_params Check the parameters passed to thread entry function.


    .. doxygendefine:: test_threads_spawn_params
        :project: Zephyr

.. item:: test_threads_spawn_priority Spawn thread with higher priority.


    .. doxygendefine:: test_threads_spawn_priority
        :project: Zephyr

.. item:: test_threads_spawn_delay Spawn thread with a delay.


    .. doxygendefine:: test_threads_spawn_delay
        :project: Zephyr

.. item:: test_threads_spawn_forever Spawn thread with forever delay and highest priority.


    .. doxygendefine:: test_threads_spawn_forever
        :project: Zephyr

.. item:: test_thread_start Validate behavior of multiple calls to k_thread_start()


    .. doxygendefine:: test_thread_start
        :project: Zephyr

.. item:: test_threads_suspend_resume_cooperative Check the suspend and resume functionality in a cooperative thread.
    :validates: ZEP-109


    .. doxygendefine:: test_threads_suspend_resume_cooperative
        :project: Zephyr

.. item:: test_threads_suspend_resume_preemptible Check the suspend and resume functionality in preemptive thread.
    :validates: ZEP-109


    .. doxygendefine:: test_threads_suspend_resume_preemptible
        :project: Zephyr

.. item:: test_threads_suspend Check that k_thread_suspend()
    :validates: ZEP-109


    .. doxygendefine:: test_threads_suspend
        :project: Zephyr

.. item:: test_threads_suspend_timeout Check that k_thread_suspend()
    :validates: ZEP-109


    .. doxygendefine:: test_threads_suspend_timeout
        :project: Zephyr

.. item:: test_resume_unsuspend_thread Check resume an unsuspend thread.
    :validates: ZEP-109


    .. doxygendefine:: test_resume_unsuspend_thread
        :project: Zephyr

.. item:: test_kdefine_preempt_thread test preempt thread initialization via K_THREAD_DEFINE
    :validates: ZEP-23


    .. doxygendefine:: test_kdefine_preempt_thread
        :project: Zephyr

.. item:: test_kdefine_coop_thread 
    :validates: ZEP-23


    .. doxygendefine:: test_kdefine_coop_thread
        :project: Zephyr

.. item:: test_kinit_preempt_thread test preempt thread initialization via k_thread_create
    :validates: ZEP-23


    .. doxygendefine:: test_kinit_preempt_thread
        :project: Zephyr

.. item:: test_kinit_coop_thread test coop thread initialization via k_thread_create
    :validates: ZEP-23


    .. doxygendefine:: test_kinit_coop_thread
        :project: Zephyr

.. item:: test_kdefine_coop_thread test coop thread initialization via K_THREAD_DEFINE
    :validates: ZEP-23


    .. doxygendefine:: test_kdefine_coop_thread
        :project: Zephyr

.. item:: test_k_sem_init 
    :validates: ZEP-99


    .. doxygendefine:: test_k_sem_init
        :project: Zephyr
