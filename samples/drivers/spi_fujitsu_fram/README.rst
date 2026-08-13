.. zephyr:code-sample:: spi-fujitsu-fram
   :name: Fujitsu MB85RS64V FRAM
   :relevant-api: spi_interface

   Read and write a Fujitsu MB85RS64V FRAM over SPI.

Overview
********

This sample drives a Fujitsu MB85RS64V ferroelectric RAM directly over SPI,
without a dedicated driver: it builds the FRAM's command frames itself using the
SPI API.

It first reads the device's ID and checks it against the expected value, which
confirms that wiring and SPI configuration are correct. It then exercises the
memory in two passes: two single bytes written to consecutive addresses and read
back, followed by a 1024 byte buffer written and read back and compared byte for
byte, reporting the offset of the first mismatch if there is one.

Requirements
************

An MB85RS64V FRAM connected to an SPI bus that the board exposes as the
``spi-1`` devicetree alias, with a GPIO available for chip select.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/spi_fujitsu_fram
   :board: <board to use>
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   fujitsu FRAM example application
   Wrote 0xAE to address 0x00.
   Wrote 0x86 to address 0x01.
   Read 0xAE from address 0x00.
   Read 0x86 from address 0x01.
   Wrote 1024 bytes to address 0x00.
   Read 1024 bytes from address 0x00.
   Data comparison successful.
