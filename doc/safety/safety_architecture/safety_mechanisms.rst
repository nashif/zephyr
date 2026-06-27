.. _safety_arch_mechanisms:

Error Handling and Safety Mechanisms
####################################

.. note::

   **Status: skeleton.** This view connects the architecture to fault control:
   it maps hazard / failure classes to architectural mitigations and describes
   the error-handling and fault-management architecture. Expanded using the
   ``safety-architecture-docs`` skill.

.. contents::
   :local:

Scope of Mitigation
*****************

This section must be explicit about **what Zephyr can mitigate and what it
cannot**. Zephyr provides primitives for isolation, scheduling and error
handling; it cannot by itself prove that a product reaches a safe state after a
sensor fault. Reaching a safe state belongs to the product safety architecture
(see :ref:`safety_arch_safety_manual`).

Hazard / Failure Class to Mitigation
*******************************

.. list-table::
   :header-rows: 1
   :widths: 32 68

   * - Hazard / failure class
     - Architectural mitigation
   * - Stack overflow
     - Static stack sizing, guard region, MPU/MMU guard, stack sentinel
   * - Invalid user access
     - Userspace, syscall validation, object permissions
   * - Priority inversion
     - Mutex priority inheritance, design constraints
   * - Deadlock
     - API usage constraints, timeout support, application design rules
   * - Invalid device state
     - Device readiness checks, init levels, error returns
   * - Memory corruption
     - Memory domains, MPU/MMU configuration, no arbitrary kernel access from
       user mode
   * - Timing failure
     - Bounded critical sections, documented ISR constraints, timeout behavior
   * - Unhandled exception
     - Fatal error path, safe-state integration point
   * - Incorrect configuration
     - Controlled Kconfig/devicetree profile, generated configuration review
       (see :ref:`safety_arch_configuration`)
   * - External HAL defect
     - Qualification boundary, restricted use, wrapper contracts, safety manual
       note

Error Handling and Fault Management Architecture
*****************************************

* *TODO: assertion architecture — development diagnostics vs production
  behavior.*
* *TODO: fatal error / kernel oops / panic path and the configured fatal error
  policy.*
* *TODO: safe-state handoff — the integration point where the product safety
  architecture takes over.*
* *TODO: error-return discipline across in-scope APIs (see
  :ref:`safety_arch_interfaces`).*
