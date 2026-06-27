.. _safety_arch_configuration:

Configuration and Variability Architecture
##########################################

.. note::

   **Status: skeleton.** Zephyr is configured by **Kconfig** (software included
   in the image) and **devicetree** (hardware description and initial
   configuration). For safety this variability must be reduced to a controlled
   envelope. Expanded using the ``safety-architecture-docs`` skill.

.. contents::
   :local:

Principle
********

The safety baseline does not state "Zephyr supports X, Y and Z". It states:

   The safety baseline permits **only** the following configuration choices.
   All other options are out of scope unless separately analyzed.

Variability Model
***************

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Variability area
     - Acceptable safety treatment
   * - Architectures
     - Define allowed architecture ports and CPU modes
   * - SMP
     - Excluded unless separately analyzed
   * - Userspace
     - Required / optional with constraints / excluded (decide per baseline)
   * - Dynamic allocation
     - Prohibited, restricted, or justified
   * - Device drivers
     - Only listed drivers/configurations in scope
   * - HALs
     - External; qualified separately or excluded
   * - Logging / shell / debug
     - Disabled or constrained
   * - Assertions
     - Define production behavior vs development diagnostics
   * - Devicetree overlays
     - Controlled inputs, reviewed and baselined
   * - Kconfig fragments
     - Controlled safety profile, not arbitrary application choice

Configuration Conformance Matrix
****************************

The authoritative list of Kconfig symbols and their required state for the
safety profile. *TODO: fix each value for the baseline and justify.*

.. list-table::
   :header-rows: 1
   :widths: 45 30 25

   * - Kconfig symbol
     - Safety profile
     - Justification ref
   * - ``CONFIG_MULTITHREADING``
     - Required
     - *TODO*
   * - ``CONFIG_USERSPACE``
     - Required / Optional / Excluded
     - *TODO*
   * - ``CONFIG_SMP``
     - Excluded unless analyzed
     - *TODO*
   * - ``CONFIG_ASSERT``
     - Defined per safety profile
     - *TODO*
   * - ``CONFIG_LOG``
     - Excluded from safety function
     - *TODO*
   * - ``CONFIG_DYNAMIC_OBJECTS``
     - Excluded unless justified
     - *TODO*
   * - ``CONFIG_HEAP_MEM_POOL_SIZE``
     - 0 or bounded/justified
     - *TODO*
   * - ``CONFIG_HW_STACK_PROTECTION``
     - Required where supported
     - *TODO*
   * - ``CONFIG_STACK_SENTINEL``
     - Required/optional depending on HW
     - *TODO*

Build and Configuration Architecture
*******************************

How configuration becomes the certified image, and which artifacts are part of
the controlled baseline.

* *TODO: Kconfig symbols, devicetree bindings and overlays, generated headers,
  linker layout / sections.*
* *TODO: rule — generated configuration inputs are part of the controlled
  baseline and reviewed (see :ref:`safety_arch_interfaces`).*
* *TODO: how a configuration outside the envelope is detected/prevented.*
