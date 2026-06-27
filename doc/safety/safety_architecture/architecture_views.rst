.. _safety_arch_views:

Architecture Views
##################

.. note::

   **Status: skeleton.** This document collects the 42010-style views of the
   safety scope (see :ref:`safety_arch_methodology`). Each view addresses a
   defined set of stakeholder concerns and is expanded using the
   ``safety-architecture-docs`` skill. Large views may be split into their own
   files as they grow.

.. contents::
   :local:
   :depth: 1

System Context View
******************

Shows Zephyr in relation to the safety application above it, and the
platform/BSP/drivers/HAL and hardware below it, marking each boundary with its
safety concern.

::

   +-------------------------------------------------------------+
   |                  Safety Application                         |
   |  safety functions | diagnostics | product-specific logic    |
   +--------------------------+----------------------------------+
                              |
   +--------------------------v----------------------------------+
   |                     Zephyr RTOS                             |
   |  Kernel | arch port | syscall layer | object model | timers |
   |  IPC    | memory domains | device model | selected services |
   +--------------------------+----------------------------------+
                              |
   +--------------------------v----------------------------------+
   |             Platform / BSP / Drivers / HALs                 |
   +--------------------------+----------------------------------+
                              |
   +--------------------------v----------------------------------+
   |                         Hardware                            |
   +-------------------------------------------------------------+

.. list-table:: Boundary concerns
   :header-rows: 1
   :widths: 35 65

   * - Boundary
     - Safety concern
   * - Application ↔ RTOS
     - API contracts, timing behavior, error propagation
   * - RTOS ↔ drivers
     - Initialization, interrupt behavior, device readiness
   * - RTOS ↔ hardware
     - Privilege level, memory protection, exception model
   * - Build system ↔ image
     - Configuration correctness, feature inclusion/exclusion
   * - External modules ↔ Zephyr
     - HAL assumptions, versioning, qualification responsibility

Logical Decomposition View
************************

Breaks the safety scope into architectural units and their safety-relevant
responsibilities. The device model deserves explicit treatment: device
availability, initialization order, failure handling and API behavior are
architectural contracts.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Unit
     - Safety-relevant responsibilities
   * - Scheduler
     - Thread state, priority handling, preemption, SMP rules if enabled
   * - Thread lifecycle
     - Creation, stack assignment, termination, permissions
   * - Interrupt management
     - ISR entry/exit, nesting, priority, deferred work
   * - Time management
     - System tick, timeouts, delays, timer services
   * - Synchronization
     - Mutexes, semaphores, condition variables, wait queues
   * - IPC
     - Queues, FIFOs, pipes, message queues, mailboxes (if in scope)
   * - Memory management
     - Stack protection, heaps, memory domains, userspace objects
   * - System calls
     - User/kernel boundary, validation, permission checks
   * - Object model
     - Kernel object metadata, access control, lifecycle rules
   * - Device model
     - Initialization ordering, device readiness, driver API binding
   * - Architecture port
     - Context switch, exception handling, privilege transitions
   * - Build/configuration
     - Kconfig, devicetree, generated headers, linker layout
   * - Diagnostics
     - Asserts, fatal errors, logging (if permitted in safety scope)

Runtime Behavior View
*******************

Describes the main control flows and ties each to architectural requirements
(see :ref:`safety_arch_traceability`).

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Scenario
     - What to document
   * - Boot
     - Reset, early arch init, kernel init, device init, application start
   * - Thread scheduling
     - Ready queue behavior, preemption, cooperative threads, priority
       inversion handling
   * - Interrupt handling
     - ISR entry, nested interrupts, offloaded work, interaction with scheduler
   * - System call path
     - User-mode call, validation, object permission check, kernel execution,
       return
   * - Fault handling
     - CPU exception, kernel oops/panic, fatal error policy, safe-state handoff
   * - Device access
     - API call, driver validation, blocking behavior, timeout/error return
   * - Timeouts
     - Tick/timer source, timeout queue, wakeup behavior
   * - Shutdown/restart
     - If supported or integrator-defined

Example architectural requirements:

* ``ARCH-BOOT-001`` — The kernel shall initialize all in-scope kernel objects
  before application code is scheduled.
* ``ARCH-INT-002`` — Interrupt handlers shall not invoke blocking APIs.
* ``ARCH-SCHED-003`` — A higher-priority ready thread shall preempt a
  lower-priority preemptible thread according to the scheduling policy.
* ``ARCH-FAULT-004`` — Unrecoverable kernel faults shall invoke the configured
  fatal error policy.

Memory and Privilege View
**********************

*TODO: privilege model, user/kernel separation, memory domains, MPU/MMU
configuration, stack protection regions, kernel object access from user mode.*

Interrupt and Scheduling View
*************************

*TODO: interrupt model, priorities and nesting, deferred work, scheduler
interaction, bounded critical sections, SMP rules if enabled.*

Device Model and Driver Architecture
*******************************

*TODO: init levels and ordering, device readiness checks, generic driver API
contracts (e.g. UART/SPI/I2C), error returns for uninitialized/unavailable
devices.*
