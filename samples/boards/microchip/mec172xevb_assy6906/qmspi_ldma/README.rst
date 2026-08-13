.. zephyr:code-sample:: mec172xevb-qmspi-ldma
   :name: MEC172x QMSPI with local DMA
   :relevant-api: spi_interface

   Drive a SPI NOR flash from a MEC172x QMSPI controller using its local DMA.

Overview
********

This sample exercises the QMSPI controller of a Microchip MEC172x through the
SPI API, driving the SPI NOR flash on the evaluation board with the
controller's local DMA.

It reads the flash's JEDEC ID and names the part it recognises, reads back the
status registers and reports what they say about quad-enable, register locking
and suspend state, and then runs an erase, program and read back cycle over the
flash.

Because it talks to the flash with raw opcodes rather than through the flash
API, it covers the parts of the SPI API that a plain transfer does not: extended
(dual and quad) modes, transfers split into separate command and data phases
with distinct SPI configurations, and an asynchronous transfer with a completion
callback. These need :kconfig:option:`CONFIG_SPI_EXTENDED_MODES` and
:kconfig:option:`CONFIG_SPI_ASYNC`, which the sample enables.

Requirements
************

A ``mec172xevb_assy6906`` evaluation board with a SPI NOR flash on ``qspi0``.
The sample recognises the Winbond W25Q128 and W25Q128JV and the Microchip
SST26VF016B, and reports the JEDEC ID of anything else it finds.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/microchip/mec172xevb_assy6906/qmspi_ldma
   :board: mec172xevb_assy6906
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   JEDEC ID = 0x001840ef
   W25Q128 16Mbyte SPI flash
   SPI Flash Status1 = 0x00
   SPI Flash Status2 = 0x02
   Quad-Enable bit is set. WP# and HOLD# are IO[2] and IO[3]
   Transmit SPI flash Write-Enable
   Transmit Erase-Sector at 0x00000000
