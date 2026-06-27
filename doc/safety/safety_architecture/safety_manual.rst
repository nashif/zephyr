.. _safety_arch_safety_manual:

Safety Manual Implications, Limitations and Change Impact
########################################################

.. note::

   **Status: skeleton.** This view collects the obligations transferred to the
   integrator, the known limitations and open assumptions, and the rules for
   assessing the impact of changes on the safety architecture. Expanded using
   the ``safety-architecture-docs`` skill.

.. contents::
   :local:

Integrator Obligations
*******************

Because target hardware, devices and the compiler are not in the Zephyr project
certification scope, the integrator owns the following (see also
:ref:`safety_arch_certification_scope`):

* *TODO: platform/SoC qualification, errata handling.*
* *TODO: compiler/toolchain qualification and option set.*
* *TODO: external HAL qualification.*
* *TODO: application-level safety functions and the safe-state argument.*
* *TODO: applying and reviewing the configuration safety profile
  (:ref:`safety_arch_configuration`).*
* *TODO: providing safe-state hooks / fatal error policy
  (:ref:`safety_arch_mechanisms`).*

Known Limitations and Open Assumptions
*******************************

* *TODO: behaviors Zephyr cannot guarantee alone (e.g. reaching a product safe
  state).*
* *TODO: open assumptions pending closure for the baseline.*

Change Impact Rules
****************

Every change touching the safety scope must state its impact and update the
affected views. A change must declare whether it affects:

#. **Scope** — adds/removes an in-scope component
   (:ref:`safety_arch_certification_scope`).
#. **Configuration envelope** — changes an allowed Kconfig/devicetree choice
   (:ref:`safety_arch_configuration`).
#. **Architectural requirement** — adds/modifies/retires an ``ARCH-*``
   requirement.
#. **Interface contract** — changes context rules, blocking behavior, error
   returns or object lifetime (:ref:`safety_arch_interfaces`).
#. **Safety mechanism** — changes a hazard mitigation
   (:ref:`safety_arch_mechanisms`).

Any "yes" requires updating the affected view **and** its traceability
(:ref:`safety_arch_traceability`), and review by the Safety Committee per
:ref:`safety_overview`.
