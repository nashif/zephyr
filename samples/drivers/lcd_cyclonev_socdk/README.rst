.. zephyr:code-sample:: lcd-cyclonev-socdk
   :name: Cyclone V SoC development kit LCD
   :relevant-api: i2c_interface

   Write characters and strings to the LCD of a Cyclone V SoC FPGA development
   kit over I2C.

Overview
********

This sample drives the character LCD of the Intel Cyclone V SoC FPGA
development kit over I2C.

The display is addressed directly, without a display driver: the sample writes
the bytes the panel expects, prefixing command bytes with the ``0xFE`` escape
the controller uses to tell commands from character data. It writes a single
character, moves to the next line, and then writes a string, so both paths
through the panel's protocol are exercised. The commands the panel understands
are listed in :zephyr_file:`samples/drivers/lcd_cyclonev_socdk/src/commands.h`.

Each I2C transfer reports on the console whether it succeeded, so the sample can
be followed without looking at the panel.

Requirements
************

An Intel Cyclone V SoC FPGA development kit with its character LCD connected to
``i2c0`` at address ``0x28``.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/lcd_cyclonev_socdk
   :board: cyclonev_socdk
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   Hello World! cyclonev_socdk
   i2c is ready
   Success!
   Success!
   Success!

The LCD shows ``A`` on the first line and ``Hello world!`` on the second.
