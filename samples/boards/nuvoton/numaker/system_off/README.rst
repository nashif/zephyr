.. zephyr:code-sample:: numaker-system-off
   :name: NuMaker system off
   :relevant-api: sys_poweroff rtc_interface

   Power off a Nuvoton NuMaker board and wake it again from an RTC alarm.

Overview
********

This sample powers a Nuvoton NuMaker board down with :c:func:`sys_poweroff` and
arranges for it to come back up ten seconds later.

Before powering off it sets the RTC time, then sets an RTC alarm matched on the
hour and minute only, so that the alarm falls ten seconds after the time it just
set. It also disables the digital path on PF.4 through PF.11, which is what
keeps those pins from drawing current while the board is powered down.

Waking from the alarm restarts the application, so the board cycles through
power off and wake up for as long as it is left running.

Requirements
************

A Nuvoton NuMaker board with an RTC exposed through the ``rtc`` devicetree
alias, and a power rail that the RTC alarm can bring back up.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/nuvoton/numaker/system_off
   :board: <NuMaker board to use>
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   Wake-up alarm set for 10 seconds.
   Disable digital path pins
   Powering off ....................

Ten seconds later the board wakes and prints the same three lines again.
