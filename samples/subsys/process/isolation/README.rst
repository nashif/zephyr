.. zephyr:code-sample:: process-isolation
   :name: Process Model - Isolation
   :relevant-api: process_apis

   Run two hardware-isolated processes concurrently, each loaded from a
   separate ELF binary.

Overview
********

This sample demonstrates running two independent *processes* side-by-side
using the Zephyr :ref:`process_model` subsystem.

Two extensions are compiled into separate ELF binaries and loaded at runtime:

- **Process A** (:zephyr_file:`samples/subsys/process/isolation/ext/proc_a_ext.c`):
  counts up from 1 to 5, printing each value.
- **Process B** (:zephyr_file:`samples/subsys/process/isolation/ext/proc_b_ext.c`):
  counts down from 5 to 1, printing each value.

Each process lives in its own :c:struct:`k_mem_domain`.  The four LLEXT memory
regions (TEXT, DATA, RODATA, BSS) of each extension are registered as
:c:struct:`k_mem_partition` objects in its dedicated domain.  When
:kconfig:option:`CONFIG_USERSPACE` is enabled, hardware memory protection
prevents either process from accessing the other's TEXT, DATA, RODATA, or BSS
regions.

IPC via a shared message queue
================================

Both processes receive a pointer to a host-allocated :c:struct:`k_msgq` as
their :c:member:`z_process_opts.arg`.  They use :c:func:`k_msgq_put` to post
their counter values.

Because user-mode threads must be granted explicit access to kernel objects,
the host calls :c:func:`k_object_access_grant` for both thread objects before
spawning them:

.. code-block:: c

   k_object_access_grant(&out_q, &proc_a.thread);
   k_object_access_grant(&out_q, &proc_b.thread);

   z_process_spawn(&proc_a, "proc_a", elf_a, sz_a, stack_a, ...);
   z_process_spawn(&proc_b, "proc_b", elf_b, sz_b, stack_b, ...);

This demonstrates the standard pattern for passing kernel primitives to
user-mode processes.

Key Points
**********

- Each process gets its own :c:struct:`k_mem_domain` automatically.
- Hardware isolation is transparent at the process API level.
- IPC goes through kernel primitives with explicit access grants.
- The host joins both processes, checks exit codes, and unloads them.

Requirements
************

Any board with an architecture supported by the :ref:`llext` subsystem,
:kconfig:option:`CONFIG_USERSPACE`, and sufficient RAM for two concurrent LLEXT
heaps.

Virtual boards:

- :zephyr:board:`qemu_cortex_r5 <qemu_cortex_r5>` (ARMv7-R)
- :zephyr:board:`qemu_cortex_a9 <qemu_cortex_a9>` (ARMv7-A)
- :zephyr:board:`mps2/an385 <mps2_an385>` (Cortex-M3, ARMv7-M)
- :zephyr:board:`mps2/an521/cpu0 <mps2_an521>` (Cortex-M33, ARMv8-M)

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/process/isolation
   :board: qemu_cortex_r5
   :goals: build run
   :compact:

Sample Output
*************

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0 ***
   [00:00:00.000,000] <inf> main: Process Model sample: isolation
   Process A: counter=1
   Process B: counter=5
   Process A: counter=2
   Process B: counter=4
   Process A: counter=3
   Process B: counter=3
   Process A: counter=4
   Process B: counter=2
   Process A: counter=5
   Process B: counter=1
   [00:00:00.000,000] <inf> main: All processes finished
   [00:00:00.000,000] <inf> main:   proc_a exit code: 0
   [00:00:00.000,000] <inf> main:   proc_b exit code: 0

.. note::

   The interleaving of Process A and Process B output depends on scheduling
   jitter.  Process A sleeps 100 ms per iteration; Process B sleeps 150 ms per
   iteration, so they are expected to interleave but the exact order may vary.

.. note::

   :kconfig:option:`CONFIG_MAX_DOMAIN_PARTITIONS` must be at least ``8`` when
   running two concurrent processes (4 partitions each).  The default ``prj.conf``
   for this sample already sets this value.
