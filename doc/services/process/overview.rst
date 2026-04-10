.. _process_model_overview:

Overview
########

The Process Model subsystem provides a high-level abstraction on top of three
existing Zephyr subsystems:

.. list-table::
   :widths: 25 75
   :header-rows: 1

   * - Underlying Subsystem
     - Role in the Process Model
   * - :ref:`llext`
     - Loads and links ELF binaries at runtime.  Each process is an ordinary
       ELF relocatable or shared-library object that is compiled separately
       from the main Zephyr image.
   * - :ref:`Memory Domains <memory_domain>`
     - Each process lives in a dedicated :c:struct:`k_mem_domain`.  The LLEXT
       memory regions (TEXT, DATA, RODATA, BSS) of the loaded extension are
       registered as :c:struct:`k_mem_partition` objects in that domain, giving
       every process hardware-enforced isolation from other processes and from
       the rest of the kernel.
   * - :ref:`Userspace <usermode_api>`
     - When :kconfig:option:`CONFIG_USERSPACE` is enabled the process thread
       drops to unprivileged mode via :c:func:`k_thread_user_mode_enter` before
       running the application code.  The thread can only access its own stack
       and the partitions registered in its domain; all privileged operations go
       through the syscall interface.

Without this subsystem, an application that needs isolated, loadable extensions
must manually orchestrate all three layers.  The code typically looks like::

   /* LLEXT load */
   struct llext_buf_loader loader = LLEXT_TEMPORARY_BUF_LOADER(buf, len);
   llext_load(&loader.loader, "myext", &ext, &params);

   /* Domain setup */
   k_mem_domain_init(&domain, 0, NULL);
   llext_add_domain(ext, &domain);

   /* Thread creation */
   k_thread_create(&t, stack, stack_size, trampoline, ext, NULL, NULL, prio, 0, K_FOREVER);
   k_mem_domain_add_thread(&domain, &t);
   k_thread_start(&t);

   /* Wait and tear down */
   k_thread_join(&t, K_FOREVER);
   llext_teardown(ext);
   k_mem_domain_deinit(&domain);
   llext_unload(&ext);

This is error-prone, especially when the teardown path is reached from multiple
call sites.  The Process Model reduces all of the above to::

   z_process_spawn(&proc, "myext", buf, len, stack, stack_size, NULL);
   z_process_join(&proc, K_FOREVER);
   z_process_unload(&proc);

Concepts
********

Process
=======

A *process* is a self-contained unit of execution composed of:

- An ELF binary loaded at runtime (the *extension*)
- A dedicated :c:struct:`k_mem_domain` containing the extension's memory
  regions as partitions
- A Zephyr thread that runs the extension's entry function

Processes are described by a :c:struct:`z_process` *descriptor* that must be
allocated by the caller (as a global, static, or heap variable) and kept valid
for the lifetime of the process.

Extension
=========

An *extension* is the loadable ELF binary that contains the process
application code.  It is compiled separately from the main Zephyr image using
the :ref:`LLEXT native CMake helpers <llext_build_native>`.

The extension must export an entry function using :c:macro:`LL_EXTENSION_SYMBOL`::

   #include <zephyr/kernel.h>
   #include <zephyr/llext/symbol.h>

   void process_main(void *arg)
   {
       /* application code */
   }
   LL_EXTENSION_SYMBOL(process_main);

The name of the entry symbol defaults to ``process_main`` and can be changed
via :c:member:`z_process_opts.entry_sym`.

Lifecycle
=========

A process moves through the following states:

.. code-block:: none

   UNLOADED ──z_process_load()──► LOADED ──z_process_start()──► RUNNING
                                                                     │
                                   ◄──────z_process_unload()──── DEAD
                                                                  ▲
                                                        z_process_join() / z_process_kill()

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - State
     - Meaning
   * - ``UNLOADED``
     - Descriptor is zeroed or has been reset by :c:func:`z_process_unload`.
       No LLEXT heap memory is held.
   * - ``LOADED``
     - Extension is loaded and linked.  The domain and entry function are
       resolved.  Thread has not been scheduled yet.
   * - ``RUNNING``
     - Thread is active.  The extension's ``.init_array`` has been called.
   * - ``DEAD``
     - Thread has exited normally or was killed.  LLEXT memory is still
       allocated until :c:func:`z_process_unload` is called.

Memory Layout
=============

After loading, the process has the following memory layout:

.. code-block:: none

   ┌─────────────────────────────────────────────────────┐
   │  Kernel heap (LLEXT heap)                           │
   │   ┌─────────────────────────────────────────────┐   │
   │   │ Extension TEXT  (partition: P_RX_U_RX)     │   │
   │   │ Extension DATA  (partition: P_RW_U_RW)     │   │
   │   │ Extension RODATA(partition: P_RO_U_RO)     │   │
   │   │ Extension BSS   (partition: P_RW_U_RW)     │   │
   │   └─────────────────────────────────────────────┘   │
   └─────────────────────────────────────────────────────┘
   ┌─────────────────────────────────────────────────────┐
   │  Process thread stack (caller-provided)             │
   └─────────────────────────────────────────────────────┘

When running in user mode, the process thread may only access:

1. Its own stack
2. The four LLEXT partitions listed above

All other memory — including the :c:struct:`z_process` descriptor itself,
kernel data structures, and other processes' extension regions — is
inaccessible from user mode and any attempt to access it triggers a memory
protection fault.

User Mode Execution
===================

When :kconfig:option:`CONFIG_USERSPACE` is enabled (the default when it is
available), the supervisor trampoline :c:func:`process_thread_fn` runs
``.init_array`` in privileged mode and then calls
:c:func:`k_thread_user_mode_enter` to switch to user mode before invoking the
extension's entry function.  This means:

- The ``.init_array`` (C++ constructors, ``__attribute__((constructor))``
  functions) always run in supervisor mode.
- The application code in the entry function runs in user mode.
- The ``.fini_array`` (C++ destructors) is called by
  :c:func:`z_process_unload` in supervisor mode after the thread exits.

This arrangement ensures that module initialisation and teardown have the
privileges they need while the application code itself is properly sandboxed.

Inter-Process Communication
===========================

Two isolated processes cannot share writable memory directly; each process has
its own :c:struct:`k_mem_domain` and cannot see the other's partitions.

IPC must go through kernel primitives (message queues, pipes, shared memory
partitions with explicit grants, etc.).  The host supervisor code may pass a
pointer to a kernel object as :c:member:`z_process_opts.arg`, but user-mode
threads need an explicit :c:func:`k_object_access_grant` before they can use
it::

   /* Host: grant access before spawning */
   k_object_access_grant(&my_msgq, &proc.thread);

   struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;
   opts.arg = &my_msgq;

   z_process_spawn(&proc, "worker", elf, sz, stack, stack_sz, &opts);
