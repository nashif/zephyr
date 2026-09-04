# Copyright (c) 2026 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# The Zephyr bootloader image. Everything lives in subsys/boot; this
# directory only exists so that the bootloader can be built and flashed
# like any other image:
#
#   west build -b <board> boot
#   west build -b <board> --sysbuild <app> -DSB_CONFIG_BOOTLOADER_ZEPHYR=y

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_boot)

# The bootloader is entirely provided by subsys/boot; the application
# target only needs a placeholder source.
target_sources(app PRIVATE ${ZEPHYR_BASE}/misc/empty_file.c)
