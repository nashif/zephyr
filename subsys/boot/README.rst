.. SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
.. SPDX-License-Identifier: Apache-2.0

Zephyr bootloader
#################

``subsys/boot`` is the in-tree bootloader. It boots, validates, swaps and
chain-loads images in the MCUboot image format, so images signed with
``imgtool`` and applications built with ``CONFIG_BOOTLOADER_MCUBOOT`` work
unchanged. Unlike the ``mcuboot`` module it needs no external repository:
the bootloader, the application-side ``bootutil`` helpers and the debug
signing keys all live in the tree.

Layout
******

``bootutil/``
   The bootloader core (image format, TLVs, swap algorithms, trailer
   handling), imported from MCUboot ``boot/bootutil`` at revision
   ``7ad67106c3253d03ae4bd8ab48e6bdf7bc46f43e``. ``bootutil/CMakeLists.txt``
   builds the application-side subset (``bootutil_public.c``) for
   ``CONFIG_MCUBOOT_BOOTUTIL_LIB`` when the ``mcuboot`` module is absent.

``src/``, ``include/``
   The Zephyr integration formerly kept in MCUboot ``boot/zephyr``: the
   ``main()`` sequence, flash map and OS glue, key tables, watchdog,
   chain-load code per architecture and the ``mcuboot_config.h`` mapping from
   Kconfig to bootutil options.

``keys/``
   Debug signing keys. They are public and must never be used for products.

``cmake/slots.cmake``
   Slot geometry checks and the image trailer size estimate that sysbuild
   forwards to the chain-loaded application.

The buildable image lives in ``boot/`` at the tree root and only contains
``prj.conf`` and per-board configuration; everything else is selected by
``CONFIG_ZEPHYR_BOOTLOADER``.

Differences from the MCUboot Zephyr port
****************************************

* Only Zephyr-native crypto: mbedTLS and PSA through the ``mbedtls`` module.
  TinyCrypt, the fiat curve25519 copy, the bundled ASN.1 parser and the nRF
  CC310 glue are not carried over. RSA uses the mbedTLS legacy API and
  ECDSA P-256 uses PSA. ed25519 needs pure EdDSA, which the mbedTLS PSA
  core does not implement, so it is only offered when a PSA backend
  selects ``BOOT_PSA_HAS_PURE_EDDSA``.
* Serial recovery (``boot_serial`` and the vendored zcbor) is not carried
  over; it is meant to be replaced by the mcumgr SMP server.
* The other operating system ports and the simulator are not included.

Building
********

Standalone::

   west build -b nrf52840dk/nrf52840 boot

With an application through sysbuild::

   west build -b nrf52840dk/nrf52840 --sysbuild samples/hello_world \
       -DSB_CONFIG_BOOTLOADER_ZEPHYR=y

Mode, signature type and key file are the same sysbuild options used for the
``mcuboot`` module (``SB_CONFIG_MCUBOOT_MODE_*``,
``SB_CONFIG_BOOT_SIGNATURE_TYPE_*``, ``SB_CONFIG_BOOT_SIGNATURE_KEY_FILE``).

Running in QEMU
***************

``qemu_cortex_m0`` emulates the nRF51 flash controller and has the usual
slot layout, so the whole flow can be exercised without hardware. Its 16 KiB
of RAM only fit the hash-only configuration with mbedTLS 4::

   west build -b qemu_cortex_m0 --sysbuild samples/hello_world -d build \
       -DSB_CONFIG_BOOTLOADER_ZEPHYR=y -DSB_CONFIG_BOOT_SIGNATURE_TYPE_NONE=y \
       -DSB_CONFIG_MERGED_HEX_FILES=y
   qemu-system-arm -cpu cortex-m0 -machine microbit -nographic \
       -device loader,file=build/merged_qemu_cortex_m0_nrf51822.hex

QEMU refuses the merged hex through ``-kernel``; load it with the generic
loader as above.
