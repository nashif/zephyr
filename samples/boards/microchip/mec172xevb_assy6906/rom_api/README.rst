.. zephyr:code-sample:: mec172xevb-rom-api
   :name: MEC172x ROM API hashing
   :relevant-api: crypto

   Compute SHA digests on a MEC172x using the hash engine exposed by its boot
   ROM.

Overview
********

This sample uses the crypto API to compute SHA-224, SHA-256, SHA-384 and
SHA-512 digests on a Microchip MEC172x, where the hashing is performed by
routines held in the chip's boot ROM rather than by a driver in the image.

It first prints the sizes the ROM API requires for its hash state and context
structures, which is the practical thing to know when budgeting memory for it.
It then checks that the driver advertises the capabilities it is about to use,
and computes digests for a table of test messages, comparing each result
against the known digest for that message.

The digests are computed several times over, feeding the same messages in
differently sized chunks: whole blocks plus a remainder first, then arbitrary
chunk sizes. This is what the sample is really testing, since the ROM has to
carry hash state across calls for a chunked message to come out the same as a
contiguous one.

Requirements
************

A ``mec172xevb_assy6906`` evaluation board.

Building and Running
********************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/microchip/mec172xevb_assy6906/rom_api
   :board: mec172xevb_assy6906
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   It lives! mec172xevb_assy6906
   ROM API say GIVE MEMORY, MORE MEMORY!
   Size of MEC172x ROM API hash state save memory is 256 bytes!!!!

   Test Zephyr crypto hash API for multiblock plus remainder
   Test Zephyr crypto hash API for multiblock plus remainder returned 0

   Test Zephyr crypto arbitrary chunk size = 0
   Test Zephyr crypto arbitrary chunk size returned 0

   Test Zephyr crypto arbitrary chunk size = 8
   Test Zephyr crypto arbitrary chunk size returned 0

   Test Zephyr crypto arbitrary chunk size = 23
   Test Zephyr crypto arbitrary chunk size returned 0
   Application done

A returned value of 0 means every digest in the table matched.
