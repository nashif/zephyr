.. zephyr:code-sample:: soc-flash-nand
   :name: Cadence NAND flash
   :relevant-api: flash_interface

   Erase, write and read back on-SoC NAND flash through the flash API.

Overview
********

This sample exercises the Cadence NAND flash controller through the flash API.

It reports the block and page geometry it reads back from the device, then runs
a full erase, write, read cycle over a range of pages: erasing the range,
writing a pattern, reading it back and comparing it against what was written.
It then erases the range a second time and reads it back again, to confirm that
the erase actually took effect. Every step is checked and any mismatch is
reported rather than being allowed to pass silently.

Requirements
************

A board with a Cadence NAND flash controller whose devicetree exposes it as the
``nand`` alias. The sample is built and run on the
``intel_socfpga_agilex5_socdk`` board, and needs the external flash fixture to
be present.

The sample allocates its page buffers from the heap, so it sets a large
:kconfig:option:`CONFIG_HEAP_MEM_POOL_SIZE`.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/soc_flash_nand
   :board: intel_socfpga_agilex5_socdk
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   Nand flash driver test sample
   Nand flash driver block size 20000
   The Page size of 800
   Nand flash driver data erase successful....
   Nand flash driver write completed....
   Nand flash driver read completed....
   Nand flash driver read verified
   Nand flash driver data erase successful....
   Nand flash driver read verified after erase....
   Nand flash driver test sample completed....
