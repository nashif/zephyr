# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mctp)

target_sources(app PRIVATE
  src/test_core.c
  src/test-utils.c
  src/test_eid.c
  src/test_seq.c
  #src/test_serial.c
)
