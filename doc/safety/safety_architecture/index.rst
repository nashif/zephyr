.. _safety_architecture:

Zephyr Safety Architecture Description
######################################

.. important::

   This is a **controlled architecture description** intended for use as a
   functional safety work product. It is **not** a general-purpose RTOS
   overview. The high-level block diagram of "application on kernel on drivers
   on hardware" found elsewhere in the documentation is an introductory aid and
   is **not** sufficient for certification.

   The certifiable architecture is **not "all of Zephyr"**, but a controlled,
   justified and traceable subset of Zephyr with defined configuration
   boundaries, platform assumptions, interface contracts, safety mechanisms and
   integrator obligations.

.. contents::
   :local:
   :depth: 2

Purpose
*******

This document describes the architecture of the Zephyr RTOS *as it is bounded
for functional safety certification*. It defines:

* **what is in scope** for the safety effort and what is explicitly excluded;
* **how the RTOS is decomposed** into safety-relevant architectural units;
* **what assumptions** the integrator is required to satisfy;
* **how configuration** (Kconfig and devicetree) changes the certified
  architecture and how that variability is bounded;
* **how safety-relevant behavior is verified and traced** from safety
  requirements down to verification evidence.

This document is the **top-level entry point**. The detailed views are
maintained as separate, individually controlled documents and are referenced
from the :ref:`document map <safety_arch_document_map>` below. The presence of a
subsystem in the upstream Zephyr tree does **not** imply it is part of the
safety-certified architecture; only what is enumerated in
:ref:`safety_arch_scope` is in scope.

This document complements, and must remain consistent with, the
:ref:`safety_overview` (process and certification goals) and the
:ref:`safety_requirements` (requirements management). Where this document states
an architectural requirement, that requirement is expected to be traceable to a
safety requirement managed in the
`requirement repository <https://github.com/zephyrproject-rtos/reqmgmt>`__.

Applicable Standards and Safety Target
**************************************

The Zephyr safety effort targets `IEC 61508
<https://en.wikipedia.org/wiki/IEC_61508>`__, **SIL 3 / Systematic Capability
(SC) 3**, for a limited source scope, using the **route 3s/1s** approach for
pre-existing code (see :ref:`general_safety_scope`). Because IEC 61508 is the
basic functional safety standard from which sector-specific standards derive, a
mapping to those standards (for example ISO 26262 for automotive) is an
integrator responsibility and is **not** asserted by this document.

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Attribute
     - Definition
   * - Standard target
     - IEC 61508 SIL 3 / SC 3 (route 3s/1s for existing sources)
   * - Item type
     - RTOS software element assessed as a reusable component (SEooC-like /
       SOUP evidence package); not a product-specific OS
   * - Baseline release
     - *To be fixed per safety release* — Zephyr release tag, commit SHA,
       branch and safety baseline identifier
   * - Cross-standard mapping
     - Integrator responsibility (e.g. ISO 26262); not asserted here

.. _safety_arch_methodology:

Architecture Description Methodology
************************************

This description follows the spirit of **ISO/IEC/IEEE 42010** (architecture
description): it identifies **stakeholders**, their **concerns**, the
**viewpoints** that frame those concerns, and the **views** that realize the
viewpoints. Each view in the :ref:`document map <safety_arch_document_map>`
addresses a defined set of concerns and is kept consistent with the others.

A deliberate separation is maintained between **architecture** and **design**:

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Level
     - Content
   * - Architecture
     - Components, interfaces, responsibilities, safety mechanisms,
       constraints (this document and its referenced views)
   * - High-level design
     - Internal algorithms, data structures, state machines
   * - Low-level design
     - Function-level behavior, API contracts, Doxygen, implementation notes
   * - Code
     - Source implementation
   * - Verification
     - Tests, reviews, analysis, coverage, traceability

This document and its views stay at the **architecture** level: they say *what*
a unit is responsible for and *what constraints* apply, not *how* the algorithm
is implemented.

Stakeholders and Concerns
*************************

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Stakeholder
     - Primary concerns
   * - Safety integrator / product team
     - Assumptions of use, configuration envelope, safety manual obligations,
       safe-state integration points
   * - Safety assessor / certification body
     - Scope definition, traceability, verification evidence, justification of
       excluded code
   * - Safety architect / Safety Committee
     - Architectural integrity of the safety scope, change impact, repercussion
       freedom between safety and non-safety components
   * - Maintainers / contributors
     - Which components are in scope, what constraints apply to changes,
       minimal disruption to daily work
   * - Application developers
     - API context rules, blocking behavior, error returns, object lifetime

.. _safety_arch_scope:

Certification Scope (Summary)
****************************

The full, controlled scope statement is maintained in
:ref:`safety_arch_certification_scope`. In summary:

**In scope (subject to the configuration envelope):**
kernel services (scheduling, threads, synchronization, IPC, time management),
the selected architecture port(s), the system call / userspace boundary, the
kernel object model, the device model and an enumerated set of drivers, memory
protection and selected system services.

**Out of scope (unless separately analyzed and listed):**
networking, Bluetooth, Wi-Fi, filesystems, shell, logging used inside a safety
function, debug features, samples and demos, dynamic loading / extensions,
vendor HALs, experimental APIs, board overlays beyond the controlled baseline,
the compiler/toolchain, target hardware and SoC errata handling, and all
application-level safety logic.

.. note::

   Target hardware, devices and the compiler are **not** in the Zephyr project
   certification scope. The integrator owns platform and tool qualification and
   the application-level safety argument. These obligations are collected in
   :ref:`safety_arch_safety_manual`.

.. _safety_arch_document_map:

Document Map
************

The detailed architecture is decomposed into the following controlled views and
sections. Each is expanded in its own document so it can be reviewed, baselined
and changed independently.

.. toctree::
   :maxdepth: 1

   certification_scope.rst
   architecture_views.rst
   configuration_architecture.rst
   safety_mechanisms.rst
   interface_contracts.rst
   traceability.rst
   safety_manual.rst

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - View / section
     - Concerns addressed
   * - :ref:`safety_arch_certification_scope`
     - Certification item, in/out-of-scope inventory, assumptions of use
   * - :ref:`safety_arch_views`
     - System context, logical decomposition, runtime behavior, memory and
       privilege, interrupt and scheduling, device model
   * - :ref:`safety_arch_configuration`
     - Configuration and variability model, build architecture, Kconfig /
       devicetree conformance matrix
   * - :ref:`safety_arch_mechanisms`
     - Hazard / failure classes mapped to architectural mitigations, error
       handling and fault management
   * - :ref:`safety_arch_interfaces`
     - API and interface contracts, architectural constraints
   * - :ref:`safety_arch_traceability`
     - Traceability chain and verification strategy
   * - :ref:`safety_arch_safety_manual`
     - Integrator obligations, known limitations, open assumptions, change
       impact rules

Architectural Requirement Identifiers
************************************

Architectural requirements stated in this description use stable identifiers of
the form ``ARCH-<AREA>-<NNN>`` so they can be traced to safety requirements and
to verification evidence (see :ref:`safety_arch_traceability`). Examples:

* ``ARCH-BOOT-001`` — The kernel shall initialize all in-scope kernel objects
  before application code is scheduled.
* ``ARCH-INT-002`` — Interrupt handlers shall not invoke blocking APIs.
* ``ARCH-SCHED-003`` — A higher-priority ready thread shall preempt a
  lower-priority preemptible thread according to the configured scheduling
  policy.
* ``ARCH-FAULT-004`` — Unrecoverable kernel faults shall invoke the configured
  fatal error policy.

Identifiers are allocated within the view that owns the corresponding behavior
and must not be reused once retired.

Change Impact Rules
******************

Any change touching the safety scope must assess its impact on this
architecture description. The detailed rules are maintained in
:ref:`safety_arch_safety_manual`; at minimum a change must state whether it
affects scope, the configuration envelope, an architectural requirement, an
interface contract, or a safety mechanism, and must update the affected view and
its traceability.

Document Maintenance
******************

This is a living document maintained under the process described in
:ref:`safety_overview`. Changes are submitted via pull request, reviewed by the
Safety Committee, and baselined per safety release. Each referenced view records
its own review and baseline status.
