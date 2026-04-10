.. _thread_notifications:

Direct-to-Thread Notifications
###############################

.. contents::
    :local:
    :depth: 2

Concepts
********

Direct-to-thread notifications are a lightweight signalling mechanism
that allows any thread or ISR to send a 32-bit notification directly to
a specific thread without creating a separate kernel object such as a
semaphore or event.

Each thread that has direct-to-thread notifications enabled carries:

* A 32-bit **notification value** — an application-defined bitmask or
  integer that accumulates information about what happened.
* A **pending count** — the number of unconsumed notifications.  When
  greater than zero, the thread has at least one notification waiting to
  be processed.

Sending
=======

A notification is sent with :c:func:`k_thread_notify`, which accepts a
*action* parameter controlling how the sender's ``value`` is applied:

.. list-table:: Notification actions
   :header-rows: 1
   :widths: 30 70

   * - Action constant
     - Behaviour
   * - ``K_THREAD_NOTIFY_SET_BITS``
     - Bitwise-OR ``value`` into the target's notification value and
       increment the pending count.
   * - ``K_THREAD_NOTIFY_SET_VALUE``
     - Overwrite the notification value with ``value``.  Fails with
       ``-EBUSY`` if a notification is already pending (non-overwriting).
   * - ``K_THREAD_NOTIFY_SET_VALUE_OVERWRITE``
     - Unconditionally overwrite the notification value with ``value``
       and increment the pending count.
   * - ``K_THREAD_NOTIFY_INCREMENT``
     - Increment the pending count without touching the notification value
       (lightweight semaphore give).
   * - ``K_THREAD_NOTIFY_NO_ACTION``
     - Wake the target thread without modifying the notification value or
       the pending count.

:c:func:`k_thread_notify_give` is a convenience wrapper for
``K_THREAD_NOTIFY_INCREMENT``.

Receiving
=========

:c:func:`k_thread_notify_wait` suspends the calling thread until a
notification arrives.  Two bit-mask parameters let the receiver control
which bits are cleared before waiting and which bits are cleared after a
notification is consumed.

:c:func:`k_thread_notify_take` acts like a semaphore take: it blocks
until the pending count is greater than zero, then decrements it.  An
optional flag also clears the notification value on successful return.

Both receive functions accept a :c:type:`k_timeout_t` timeout and return
``-EAGAIN`` on timeout.

Thread initialization
=====================

Notification state is initialised to zero automatically when a thread is
created.  No explicit initialiser call is needed.

Comparison with other primitives
=================================

.. list-table::
   :header-rows: 1
   :widths: 25 15 15 15 30

   * - Primitive
     - Object needed
     - ISR-safe send
     - Multiple waiters
     - Notes
   * - :ref:`semaphores_v2`
     - Yes
     - Yes
     - Yes
     - General-purpose counter
   * - :ref:`events`
     - Yes
     - Yes
     - Yes
     - Multi-bit event flags
   * - Thread notification
     - No
     - Yes
     - No (per-thread)
     - 32-bit value + count

Implementation details
======================

All accesses to per-thread notification state are serialised by a
single system-wide spinlock.  The wait queue embedded in each thread's
structure (``notify_wait_q``) is used by receivers blocked inside
:c:func:`k_thread_notify_wait` or :c:func:`k_thread_notify_take`.

Configuration
*************

:kconfig:option:`CONFIG_THREAD_NOTIFY`

   Enable direct-to-thread notifications.  Adds three fields to
   ``struct k_thread`` and provides the API described below.

API Reference
*************

.. doxygengroup:: thread_notify_apis
   :project: Zephyr
