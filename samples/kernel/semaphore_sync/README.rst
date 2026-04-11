.. zephyr:code-sample:: semaphore_sync
   :name: Semaphore Sync Sample
   :relevant-api: semaphore_apis

   Mirror of :zephyr:code-sample:`completion` implemented with semaphores,
   showing where the two primitives differ.

Overview
********

This sample implements the same two synchronisation patterns as
:zephyr:code-sample:`completion` using :c:struct:`k_sem` instead of
:c:struct:`k_completion`, making it easy to compare the two APIs
side-by-side.

**Pattern 1 – one-shot synchronisation**

Both primitives behave identically here: initialise with count/state 0,
signal after the work is done, wait before consuming the result.

==============================  =====================================
Completion                      Semaphore
==============================  =====================================
``k_completion_init(&c)``       ``k_sem_init(&s, 0, 1)``
``k_completion_complete(&c)``   ``k_sem_give(&s)``
``k_completion_wait(&c, tmo)``  ``k_sem_take(&s, tmo)``
==============================  =====================================

**Pattern 2 – broadcast fan-out**

This is where the two primitives diverge:

* :c:func:`k_completion_complete_all` wakes **all** N waiting threads
  with a single call and permanently opens the gate
  (``done = UINT_MAX``).
* :c:func:`k_sem_give` wakes **one** thread per call.  To release N
  threads, the sender must call it N times.  A late-arriving thread
  that pends *after* the initial wave has consumed all the counts will
  block again, whereas with a completion it would pass through
  immediately.

This difference matters when:

* The number of waiting threads is not known at signal time.
* New threads may arrive at the gate after the first wave of waiters.
* Code simplicity is important (one call vs. a loop).

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/kernel/semaphore_sync
   :board: qemu_x86
   :goals: run

Sample Output
*************

.. code-block:: console

   worker: starting long computation
   worker: computation done, giving semaphore
   main: computation result ready: 42
   worker: task 1 done
   worker: task 2 done
   worker: task 3 done
   main: all tasks done
