.. zephyr:code-sample:: clock-monitor-measure-freq
   :name: Clock monitor frequency meter
   :relevant-api: clock_monitor_interface

   Measure a clock once with a clock monitor and print the result in Hz.

Overview
********

This sample configures a clock monitor instance in
``CLOCK_MONITOR_MODE_MEASURE`` and takes a single measurement of the monitored
clock, printing the result in Hz.

It demonstrates the start, callback, wait pattern the API is built around. The
callback gives a semaphore, and ``main()`` waits on it with a timeout it chooses
itself, so timeout handling stays with the application rather than the driver.
On success no explicit stop is needed, because a measurement disarms itself once
it completes; taking repeated samples would mean calling
:c:func:`clock_monitor_start` again from the callback. The sample only calls
:c:func:`clock_monitor_stop` to abort a measurement that did not complete in
time.

The instance used is whichever one the board aliases to ``clock-meter``, and
which physical clock that measures depends on the alias target and the
``clocks`` phandle of its binding. Boards that need their source clocks turned
on first declare them in a node matching the ``clock-monitor-required-clocks``
binding, and the sample enables each of them before configuring the monitor.

:kconfig:option:`CONFIG_SAMPLE_WINDOW_NS` sets the measurement window. Longer
windows resolve finer differences at the cost of taking longer to complete.

Requirements
************

A board with a clock monitor peripheral, aliased to ``clock-meter``.

Boards with an NXP FREQME peripheral get the alias from
:zephyr_file:`samples/drivers/clock_monitor/clock_monitor.overlay`, which is
shared with the :zephyr:code-sample:`clock-monitor-check-freq` sample. Pass it
with ``EXTRA_DTC_OVERLAY_FILE`` as shown below.

Building and Running
********************

For a board that provides the alias itself:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/clock_monitor/measure_freq
   :board: frdm_mcxe31b
   :goals: build flash
   :compact:

For a FREQME-capable board, add the shared overlay:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/clock_monitor/measure_freq
   :board: frdm_mcxn236
   :goals: build flash
   :gen-args: -DEXTRA_DTC_OVERLAY_FILE="../clock_monitor.overlay"
   :compact:

Sample Output
=============

.. code-block:: console

   [00:00:00.001,000] <inf> sample: freqme0 configured in MEASURE mode, window = 1000000 ns
   [00:00:00.002,000] <inf> sample: Measured frequency = 96000000 Hz

If the measurement does not complete within the timeout the application
chose, it is aborted and reported instead:

.. code-block:: console

   [00:00:00.052,000] <wrn> sample: measurement timed out
