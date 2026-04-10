.. _thread_notify_sample:

Direct-to-Thread Notifications Sample
######################################

Overview
********

This sample demonstrates the two primary usage patterns for Zephyr's
direct-to-thread notification API:

1. **Value notification** — a simulated sensor thread measures a
   temperature and delivers it directly to the main thread using
   :c:func:`k_thread_notify` with the
   ``K_THREAD_NOTIFY_SET_VALUE_OVERWRITE`` action.  The main thread
   waits with :c:func:`k_thread_notify_wait`.

2. **Lightweight semaphore** — a producer thread enqueues five work items,
   signalling the consumer after each one via :c:func:`k_thread_notify_give`.
   The consumer drains them with :c:func:`k_thread_notify_take`.

Requirements
************

None beyond a standard Zephyr-capable board.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/kernel/thread_notify
   :board: native_sim
   :goals: build run

Expected Output
***************

.. code-block:: console

   [main] waiting for sensor notification
   [sensor] posting temperature: 23
   [main] received notification: value=0x00000017
   [producer] sending 5 work items
   [consumer] processed 5 work items
   Sample complete
