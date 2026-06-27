.. _safety_arch_certification_scope:

Certification Scope and Assumptions of Use
##########################################

.. note::

   **Status: skeleton.** This view defines the certification item and its
   boundaries. It is expanded using the ``safety-architecture-docs`` skill.
   Every entry below must be made concrete for the fixed safety baseline before
   this document can be used as a safety work product.

.. contents::
   :local:

Certification Item
******************

Defines the item being certified for the fixed baseline (see
:ref:`safety_architecture`): release tag, commit SHA, branch, safety baseline
identifier, and the item type (RTOS software element assessed as a reusable
component).

Included Scope
*************

Enumerate the in-scope architectural units. Each entry must name the concrete
components, source locations and configuration under which they are in scope.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - In-scope area
     - Concrete inclusion (components / source / config)
   * - Kernel services
     - *TODO: scheduling, threads, synchronization, IPC, time management*
   * - Architecture port(s)
     - *TODO: enumerate allowed ports and CPU modes*
   * - System call / userspace boundary
     - *TODO*
   * - Kernel object model
     - *TODO*
   * - Device model + selected drivers
     - *TODO: enumerate drivers and configurations in scope*
   * - Memory protection
     - *TODO: MPU/MMU usage, memory domains*
   * - Selected system services
     - *TODO*

Excluded Scope
*************

State explicitly what is excluded. The presence of a subsystem in the upstream
tree does not imply inclusion.

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Area
     - Treatment
   * - Networking / Bluetooth / Wi-Fi
     - Out of scope (separate safety analysis if ever required)
   * - Filesystems
     - Out of scope / separate qualification
   * - Shell
     - Out of scope or diagnostic-only
   * - Logging
     - Non-safety unless explicitly justified (see :ref:`safety_arch_configuration`)
   * - Samples and demos
     - Out of scope
   * - Dynamic loading / extensions
     - Excluded unless specifically analyzed
   * - Vendor HALs
     - External dependency, qualified separately (see :ref:`safety_arch_interfaces`)
   * - Board overlays beyond baseline
     - Integrator-controlled
   * - Toolchain / compiler
     - Integrator qualification responsibility
   * - Target hardware / SoC errata
     - Integrator responsibility
   * - Application safety logic
     - Product-level responsibility

Assumptions of Use
*****************

Obligations the integrator must satisfy for the architecture to hold. These are
the inputs to the safety manual (:ref:`safety_arch_safety_manual`).

* *TODO: platform assumptions — CPU architecture, privilege model, interrupt
  model, timer source, memory protection availability.*
* *TODO: configuration assumptions — the safety profile of
  :ref:`safety_arch_configuration` is applied and reviewed.*
* *TODO: integration assumptions — startup entry point, thread creation model,
  safe-state hooks, fatal error policy provided by the integrator.*
* *TODO: tool assumptions — compiler and options qualified by the integrator.*
