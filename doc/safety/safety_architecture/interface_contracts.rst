.. _safety_arch_interfaces:

Interfaces, API Contracts and Architectural Constraints
#######################################################

.. note::

   **Status: skeleton.** A safety-suitable architecture defines its
   **interfaces**, not just its components. This view captures interface
   contracts and the constraints that bound correct use. Expanded using the
   ``safety-architecture-docs`` skill.

.. contents::
   :local:

Interface Contracts
*****************

For each interface, the required information must be specified.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Interface
     - Required information
   * - Kernel APIs
     - Allowed context, blocking behavior, error returns, object lifetime
   * - Driver APIs
     - Sync/async behavior, timeout behavior, initialization requirements
   * - System calls
     - Validation, permissions, user/kernel memory transfer
   * - Architecture hooks
     - Context switch, ISR entry/exit, fatal error, privilege transition
   * - Build interfaces
     - Kconfig symbols, devicetree bindings, linker sections
   * - Application integration
     - Startup entry point, thread creation model, safe-state hooks
   * - External dependencies
     - Compiler, libc, HALs, generated files, scripts

Architectural Constraints (Usage Rules)
*********************************

These constraints are architectural requirements (``ARCH-*``) and must be
traced (see :ref:`safety_arch_traceability`).

* APIs callable from ISR context shall be explicitly identified.
* Blocking APIs shall not be callable from ISR context.
* Kernel object lifetime shall be defined before use.
* Driver APIs shall return defined errors for uninitialized or unavailable
  devices.
* Generated configuration inputs shall be part of the controlled baseline.

External Dependencies
*****************

The qualification boundary for things Zephyr depends on but does not certify.

* *TODO: compiler and libc — integrator qualification responsibility.*
* *TODO: vendor HALs — external dependency, restricted use, wrapper contracts.*
* *TODO: generated files and scripts — controlled baseline membership.*

See :ref:`safety_arch_safety_manual` for the obligations transferred to the
integrator.
