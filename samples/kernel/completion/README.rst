.. zephyr:code-sample:: completion
   :name: Completion sample
   :relevant-api: completion_apis

   Use k_completion to synchronise threads.

Overview
********

This sample demonstrates two typical usage patterns for
:ref:`completion objects <completion_v2>`.

**Pattern 1 – one-shot synchronisation**

A worker thread performs a time-consuming computation and signals the
completion when it finishes.  The main thread blocks on
:c:func:`k_completion_wait` until the result is ready.

**Pattern 2 – broadcast fan-out**

Three worker threads each pend on a shared *start* completion.  A
coordinator calls :c:func:`k_completion_complete_all` to release all
workers simultaneously.  Each worker signals a *done* completion when it
finishes; the main thread waits three times (once per worker).

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/synchronization/completion
   :board: qemu_x86
   :goals: run

Sample Output
*************

.. code-block:: console

   worker: starting long computation
   worker: computation done, signalling completion
   main: computation result ready: 42
   worker: task 1 done
   worker: task 2 done
   worker: task 3 done
   main: all tasks done
