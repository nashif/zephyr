.. _safety_arch_traceability:

Traceability and Verification Strategy
######################################

.. note::

   **Status: skeleton.** This view defines the traceability chain from safety
   goals to verification evidence and the strategy used to verify
   safety-relevant behavior. Expanded using the ``safety-architecture-docs``
   skill, kept consistent with the :ref:`safety_requirements`.

.. contents::
   :local:

Traceability Chain
****************

::

   Safety goals / assumptions
           |
   RTOS safety requirements
           |
   Architecture requirements (ARCH-*)
           |
   Component responsibilities
           |
   Design elements
           |
   Implementation
           |
   Verification evidence
           |
   Safety manual constraints

Architecture requirements use the ``ARCH-<AREA>-<NNN>`` scheme (see
:ref:`safety_architecture`) and link upward to safety requirements managed in
the `requirement repository <https://github.com/zephyrproject-rtos/reqmgmt>`__
and downward to verification evidence.

Traceability Matrix
****************

.. list-table::
   :header-rows: 1
   :widths: 28 24 24 24

   * - Safety requirement
     - Architecture element
     - Design/code
     - Verification
   * - Prevent unauthorized user access to kernel objects
     - Userspace + object permission model
     - Syscall validation, object metadata
     - Syscall negative tests, static analysis
   * - Provide deterministic thread dispatch
     - Scheduler
     - Ready queue, priority rules
     - Scheduler tests, timing analysis
   * - Detect invalid execution context
     - API context rules
     - ISR checks, assertions/error paths
     - API misuse tests
   * - Initialize devices before use
     - Device model
     - Init levels, device readiness
     - Boot/init tests
   * - Handle unrecoverable faults
     - Fatal error architecture
     - Arch exception path, kernel fatal handler
     - Fault injection tests

Verification Strategy
*****************

Per :ref:`safety_overview`, the SIL 3 / SC 3 target requires (among others):

* Structural test coverage (entry points) 100%
* Structural test coverage (statements) 100%
* Structural test coverage (branches) 100%

Where 100% cannot be reached (e.g. defensive code), the part must be described
and justified.

* *TODO: per-view verification approach — tests, reviews, static analysis,
  timing analysis.*
* *TODO: how coverage and traceability are collected and reported per baseline.*
