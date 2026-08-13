.. zephyr:code-sample:: tfm_config_build
   :name: TF-M build configuration

   Check that TF-M builds in configurations that would otherwise go untested.

Overview
********

This sample exists to be built rather than to be read. Its application is a
single ``printf``, so that what is being exercised is the TF-M build itself:
whether Zephyr and TF-M still build and boot together in configurations that no
other sample covers.

The configurations come from the sample's twister entries rather than from its
:file:`prj.conf`, which is deliberately empty:

* ``sample.config_build.no_bl2`` builds with :kconfig:option:`CONFIG_TFM_BL2`
  disabled, so TF-M is built without its own second stage bootloader.
* ``sample.config_build.single_image`` builds with
  :kconfig:option:`CONFIG_TFM_MCUBOOT_IMAGE_NUMBER` set to 1, so the secure and
  non-secure images are combined into a single MCUboot image rather than signed
  separately. It covers the platforms that cannot build the ``no_bl2``
  configuration.

Because the point is coverage of the build, the two run on different sets of
platforms, and adding a platform to the lists in
:zephyr_file:`samples/tfm_integration/config_build/tests.yaml` is what brings it
into this check.

Requirements
************

A board with a TF-M enabled non-secure board target.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/tfm_integration/config_build
   :board: nrf5340dk/nrf5340/cpuapp/ns
   :goals: build flash
   :gen-args: -DCONFIG_TFM_BL2=n
   :compact:

Sample Output
=============

.. code-block:: console

   Hello World! nrf5340dk/nrf5340/cpuapp/ns

Reaching this line is the result being checked: TF-M booted and handed control
to the non-secure application.
