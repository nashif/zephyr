.. zephyr:code-sample:: clock-monitor-check-freq
   :name: Clock monitor frequency check
   :relevant-api: clock_monitor_interface

   Watch a clock against programmable thresholds and report when it drifts out
   of window.

Overview
********

This sample configures a clock monitor instance in ``CLOCK_MONITOR_MODE_WINDOW``
to watch a clock against an expected frequency. A callback is installed and is
the only event delivery path: it logs an error whenever the monitored clock
crosses the upper or lower threshold, or is lost altogether. The main loop only
prints a periodic heartbeat, so any output between heartbeats is a
threshold-crossing event.

The instance used is whichever one the board aliases to ``clock-monitor``.
Boards that need their source clocks turned on first declare them in a node
matching the ``clock-monitor-required-clocks`` binding, and the sample enables
each of them before configuring the monitor.

The window is tunable through Kconfig:

* :kconfig:option:`CONFIG_SAMPLE_EXPECTED_HZ` sets the centre of the window. The
  default of 0 makes the driver derive it from the monitored clock through the
  clock control API.
* :kconfig:option:`CONFIG_SAMPLE_TOLERANCE_PPM` sets the accepted deviation. The
  default of 50000 ppm, that is +/- 5 %, is loose enough to ride out crystal
  warm-up on a typical evaluation board.
* :kconfig:option:`CONFIG_SAMPLE_WINDOW_NS` sets the measurement window. Longer
  windows resolve finer ppm differences but react to faults more slowly.

Requirements
************

A board with a clock monitor peripheral, aliased to ``clock-monitor``.

Boards with an NXP FREQME peripheral get the alias from
:zephyr_file:`samples/drivers/clock_monitor/clock_monitor.overlay`, which is
shared with the :zephyr:code-sample:`clock-monitor-measure-freq` sample. Pass it
with ``EXTRA_DTC_OVERLAY_FILE`` as shown below.

Building and Running
********************

For a board that provides the alias itself:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/clock_monitor/check_freq
   :board: frdm_mcxe31b
   :goals: build flash
   :compact:

For a FREQME-capable board, add the shared overlay:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/clock_monitor/check_freq
   :board: frdm_mcxn236
   :goals: build flash
   :gen-args: -DEXTRA_DTC_OVERLAY_FILE="../clock_monitor.overlay"
   :compact:

Sample Output
=============

.. code-block:: console

   [00:00:00.001,000] <inf> sample: clock check running (expected ~0 Hz, tolerance +/-50000 ppm, window 1000000 ns)
   [00:00:05.001,000] <inf> sample: heartbeat: clock check running
   [00:00:10.001,000] <inf> sample: heartbeat: clock check running

If the monitored clock leaves the window, an error is logged between
heartbeats:

.. code-block:: console

   [00:00:12.345,000] <err> sample: [freqme0] monitored clock below lower threshold (or lost)
