.. zephyr:code-sample:: process-hello
   :name: Process Model - Hello World
   :relevant-api: process_apis

   Load and run an isolated process from an embedded ELF binary.

Overview
********

This sample demonstrates the simplest possible use of the Zephyr
:ref:`process_model` subsystem.

A single loadable extension
(:zephyr_file:`samples/subsys/process/hello/ext/hello_ext.c`) is compiled into
a separate ELF binary at build time, embedded in the firmware image as a C
array, and loaded at runtime with :c:func:`z_process_spawn`.

The extension receives the integer ``42`` as its argument and prints:

.. code-block:: none

   Hello from process 'hello' (arg=42)

When :kconfig:option:`CONFIG_USERSPACE` is enabled (the default), the process
thread runs in unprivileged mode.  The extension code can only access its own
stack and the TEXT/DATA/RODATA/BSS partitions of the loaded ELF; any access to
kernel memory or other processes' regions would trigger a memory protection
fault.

Extension source
================

The extension is defined in
:zephyr_file:`samples/subsys/process/hello/ext/hello_ext.c`.  It exports its
entry function with :c:macro:`LL_EXTENSION_SYMBOL`:

.. code-block:: c

   void process_main(void *arg)
   {
       printk("Hello from process 'hello' (arg=%lu)\n", (unsigned long)arg);
   }
   LL_EXTENSION_SYMBOL(process_main);

Host source
===========

The host application in
:zephyr_file:`samples/subsys/process/hello/src/main.c`:

1. Embeds the compiled extension as a C array via ``#include <hello.inc>``.
2. Calls :c:func:`z_process_spawn` with the embedded ELF and ``arg=42``.
3. Waits with :c:func:`z_process_join`.
4. Releases resources with :c:func:`z_process_unload`.

Requirements
************

Any board with an architecture supported by the :ref:`llext` subsystem and
sufficient RAM.  This sample can be run in QEMU on the following virtual
boards:

- :zephyr:board:`qemu_cortex_r5 <qemu_cortex_r5>` (ARMv7-R)
- :zephyr:board:`qemu_cortex_a9 <qemu_cortex_a9>` (ARMv7-A)
- :zephyr:board:`mps2/an385 <mps2_an385>` (Cortex-M3, ARMv7-M)
- :zephyr:board:`mps2/an521/cpu0 <mps2_an521>` (Cortex-M33, ARMv8-M)
- :zephyr:board:`native_sim <native_sim>`

Building and Running
********************

With memory isolation (default, requires :kconfig:option:`CONFIG_USERSPACE`):

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/process/hello
   :board: qemu_cortex_r5
   :goals: build run
   :compact:

Without memory isolation (supervisor mode only):

.. zephyr-app-commands::
   :zephyr-app: samples/subsys/process/hello
   :board: qemu_cortex_r5
   :goals: build run
   :gen-args: -DCONFIG_USERSPACE=n
   :compact:

Sample Output
*************

.. code-block:: console

   *** Booting Zephyr OS build v4.4.0 ***
   [00:00:00.000,000] <inf> main: Process Model sample: hello world
   Hello from process 'hello' (arg=42)
   [00:00:00.000,000] <inf> main: Process exited with code 0
   [00:00:00.000,000] <inf> main: Done

.. note::

   On ARMv7-M platforms with MPU regions requiring power-of-two alignment,
   :kconfig:option:`CONFIG_LLEXT_HEAP_SIZE` may need to be increased to
   ``16`` KB or more.  See :ref:`process_model_build` for guidance.
