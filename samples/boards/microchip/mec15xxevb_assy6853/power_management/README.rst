.. zephyr:code-sample:: mec15xx-power-management
   :name: MEC15xx power management
   :relevant-api: subsys_pm

   Enter light and deep sleep on a Microchip MEC15xx based board.

Overview
********

This sample demonstrates power management features on MEC15xx-based boards.
It showcase simple app that allows to enter into light and deep sleep.

Building and Running
********************

The sample can be built and executed on boards using west.
No pins configurations, except GPIO014 is used as indicator for entry/exit.


Sample output
=============

.. code-block:: console

   Wake from Light Sleep
   Wake from Deep Sleep
   ResumeBBBAA
   Wake from Light Sleep
   Suspend...
   Wake from Deep Sleep
   ResumeBBBAA

note:: The values shown above might differ.
