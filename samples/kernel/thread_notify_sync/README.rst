.. zephyr:code-sample:: thread_notify_sync
   :name: Thread Notify Sync Sample
   :relevant-api: thread_notify_apis

   Mirror of :zephyr:code-sample:`completion` and
   :zephyr:code-sample:`semaphore_sync` using direct-to-thread notifications.

Overview
********

This sample implements the same two synchronisation patterns as
:zephyr:code-sample:`completion` and :zephyr:code-sample:`semaphore_sync`
using the :ref:`thread_notify_apis` instead, making it easy to compare all
three side-by-side.

**Pattern 1 – one-shot synchronisation**

The worker signals the main thread by calling
:c:func:`k_thread_notify_give` with **main's TID**.  The main thread blocks
on :c:func:`k_thread_notify_take`.

==============================  =================================  ===================================
Completion                      Semaphore                          Thread notify
==============================  =================================  ===================================
``k_completion_init(&c)``       ``k_sem_init(&s, 0, 1)``           ``k_thread_notify_clear()``
``k_completion_complete(&c)``   ``k_sem_give(&s)``                 ``k_thread_notify_give(main_tid)``
``k_completion_wait(&c, tmo)``  ``k_sem_take(&s, tmo)``            ``k_thread_notify_take(…)``
==============================  =================================  ===================================

**Pattern 2 – broadcast fan-out**

This is where all three primitives diverge most clearly:

* **Completion** — :c:func:`k_completion_complete_all` wakes all N threads
  in one call and permanently opens the gate.
* **Semaphore** — no broadcast; must call :c:func:`k_sem_give` N times.
  Counts are consumed; a late-arriving thread will block again.
* **Thread notify** — same N-calls requirement as semaphore, *plus* the
  sender must know the TID of every target thread.  No shared object is
  involved; each thread's notification state is private to that thread.

Unique properties of thread notify
===================================

* Zero-cost object: the notify state is embedded in :c:struct:`k_thread`.
  No separate kernel object is allocated.
* The sender addresses a **specific thread by TID**; there is no anonymous
  "post to whoever is waiting" semantic.
* :c:func:`k_thread_notify` can carry a 32-bit value and select an action
  (SET_BITS, SET_VALUE, INCREMENT, NO_ACTION), giving richer signalling
  than a binary semaphore or completion.
* Only the *target thread* waits on its own notification; multiple threads
  cannot share one notification object.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/kernel/thread_notify_sync
   :board: qemu_x86
   :goals: run

Sample Output
*************

.. code-block:: console

   worker: starting long computation
   worker: computation done, notifying main
   main: computation result ready: 42
   worker: task 1 done
   worker: task 2 done
   worker: task 3 done
   main: all tasks done
