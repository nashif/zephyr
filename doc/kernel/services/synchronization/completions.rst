.. _completion_v2:

Completions
###########

A :dfn:`completion` is a kernel synchronisation primitive that enables one or
more threads to wait until a specific operation—signalled by another thread or
an ISR—has been completed.

It is modelled after the Linux kernel completion API.

.. contents::
    :local:
    :depth: 2

Concepts
********

Any number of completion objects can be defined (limited only by available
RAM). Each completion is referenced by its memory address.

A completion has the following key properties:

* A **done counter** that indicates how many times the completion has been
  signalled without a corresponding waiter consuming the signal.  A value of
  zero means the completion is pending.
* A special saturated state where ``done == UINT_MAX``, reached after a call to
  :c:func:`k_completion_complete_all`.  In this state every subsequent waiter
  passes through immediately without decrementing the counter.

A completion must be initialised before it can be used. Its initial done
counter is zero.

A completion may be signalled by a thread or an ISR:

* :c:func:`k_completion_complete` wakes **one** waiting thread. If no thread
  is waiting, the done counter is incremented (capped at ``UINT_MAX - 1``), so
  that the next :c:func:`k_completion_wait` call returns immediately.
* :c:func:`k_completion_complete_all` wakes **all** waiting threads and sets
  ``done = UINT_MAX``, making the completion permanently signalled until it is
  reset.

A completion may be waited on by a thread:

* If ``done > 0`` the call returns immediately.  The counter is decremented by
  one unless its value is ``UINT_MAX``.
* If ``done == 0`` the calling thread pends until the completion is signalled
  or the timeout expires.

A completion may be **reset** by :c:func:`k_completion_reset`, which clears
the done counter and aborts any currently pending threads with ``-EAGAIN``.
This allows the completion to be reused for a new operation.

Comparison to semaphores
========================

A completion and a binary semaphore may appear similar, but completions carry
an explicit "something finished" semantic.  The key differences are:

* :c:func:`k_completion_complete_all` is a one-way latch: it permanently opens
  the completion until :c:func:`k_completion_reset` is called.
* The done counter is consumed (decremented) by waiters, just like a semaphore,
  **except** when ``done == UINT_MAX``.
* :c:func:`k_completion_reset` also aborts current waiters, unlike
  :c:func:`k_sem_reset`.

Implementation
**************

Defining a Completion
=====================

A completion is defined using a variable of type :c:struct:`k_completion`.
It must then be initialised by calling :c:func:`k_completion_init`.

.. code-block:: c

    struct k_completion my_completion;

    k_completion_init(&my_completion);

Alternatively, a completion can be defined and initialised at compile time
using :c:macro:`K_COMPLETION_DEFINE`:

.. code-block:: c

    K_COMPLETION_DEFINE(my_completion);

Signalling a Completion
=======================

Call :c:func:`k_completion_complete` to wake one waiting thread (or increment
the done counter if no thread is waiting):

.. code-block:: c

    k_completion_complete(&my_completion);

Call :c:func:`k_completion_complete_all` to wake all waiting threads and
permanently open the completion:

.. code-block:: c

    k_completion_complete_all(&my_completion);

Both functions are safe to call from an ISR.

Waiting for a Completion
========================

Call :c:func:`k_completion_wait` to wait for the completion to be signalled:

.. code-block:: c

    int rc = k_completion_wait(&my_completion, K_FOREVER);
    if (rc == 0) {
        /* completion received */
    } else if (rc == -EAGAIN) {
        /* timed out */
    }

Use ``K_NO_WAIT`` to check without blocking (returns ``-EBUSY`` if not done).

Resetting a Completion
======================

Call :c:func:`k_completion_reset` to clear the done state and abort pending
threads:

.. code-block:: c

    k_completion_reset(&my_completion);

Suggested Uses
**************

* Signal from an ISR or a worker thread to a higher-level thread that a
  hardware operation (DMA, I2C transfer, ADC conversion, …) has finished.
* Fan-out: use :c:func:`k_completion_complete_all` to release N worker threads
  simultaneously once all prerequisites are in place.
* Reusable gate: combine :c:func:`k_completion_reset` with the signal
  functions to build recurring synchronisation checkpoints.

Configuration Options
*********************

Related configuration options:

* :kconfig:option:`CONFIG_COMPLETION`

API Reference
*************

.. doxygengroup:: completion_apis
