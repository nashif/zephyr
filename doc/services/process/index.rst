.. _process_model:

Process Model
#############

The **Process Model** subsystem provides a single, user-friendly API for creating
isolated, dynamically-loaded applications in Zephyr without having to manually
interact with :ref:`llext`, :ref:`memory domains <memory_domain>`, or the
:ref:`userspace API <usermode_api>`.

.. toctree::
   :maxdepth: 1

   overview
   build
   api

.. note::

   The Process Model depends on the :ref:`llext` subsystem and therefore shares
   the same architecture-specific support requirements: it is available on
   RISC-V, ARM, ARM64, ARC, x86, and Xtensa cores.

   Full memory isolation between processes requires :kconfig:option:`CONFIG_USERSPACE`.
   The subsystem works without it, but all threads then run in supervisor mode
   and memory protection is not enforced between processes.
