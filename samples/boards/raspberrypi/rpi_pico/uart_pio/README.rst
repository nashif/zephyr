.. zephyr:code-sample:: rpi-pico-uart-pio
   :name: Raspberry Pi Pico PIO UART
   :relevant-api: uart_interface

   Echo characters on two UARTs implemented in the RP2040 and RP2350 PIO
   blocks.

Overview
********

The RP2040 and RP2350 have a small number of hardware UARTs, but their
programmable I/O (PIO) blocks can implement more. This sample brings up two such
PIO UARTs and echoes back every character each of them receives.

Nothing in the application code is specific to PIO: both devices are obtained
with :c:macro:`DEVICE_DT_GET` and driven with :c:func:`uart_poll_in` and
:c:func:`uart_poll_out`, exactly as a hardware UART would be. What makes them
PIO UARTs is the devicetree, where each node uses the
``raspberrypi,pico-uart-pio`` compatible and picks its own pins through
pinctrl. The sample is therefore mostly a worked example of that devicetree
configuration.

The supplied overlays run both UARTs at 115200 baud on PIO1, with the first on
GP0 and GP1 and the second on GP2 and GP3.

Requirements
************

A Raspberry Pi Pico or Pico 2. Overlays are provided for
:zephyr:board:`rpi_pico` and for both cores of :zephyr:board:`rpi_pico2`.

Wire a serial adapter to the pins the overlay assigns, or wire the two PIO
UARTs to each other, and send characters to see them echoed back.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/raspberrypi/rpi_pico/uart_pio
   :board: rpi_pico
   :goals: build flash
   :compact:

The sample prints nothing of its own: every character sent to either PIO UART
is echoed back on that same UART.
