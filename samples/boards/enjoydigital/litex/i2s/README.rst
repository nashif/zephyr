.. zephyr:code-sample:: litex-i2s
   :name: LiteX I2S
   :relevant-api: i2s_interface

   Receive and play back stereo audio on a LiteX SoC using the I2S API.

Overview
********

This is a simple I2S audio transceiver example. You can plug any source of music and listen to it.

Audio Format
************

The driver requires and provides Audio data with the following parameters:

* 44100 Hz sample rate
* Signed 24 bit PCM
* Stereo
* Little endian

Building
********

.. code-block::

   mkdir build && cd build
   cmake -DBOARD=litex_vexriscv ..
   make

Known issues
************

It seems that after a few minutes some music delay occurs, this is because the sound driver is not able to send data as fast as it receives it.
