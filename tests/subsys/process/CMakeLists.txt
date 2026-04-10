# Copyright (c) 2026 The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_process LANGUAGES C)

# ──────────────────────────────────────────────────
# Extension binaries used by the test suite
# ──────────────────────────────────────────────────

# basic – prints a message and returns
set(ext_basic_src ${PROJECT_SOURCE_DIR}/ext/proc_basic_ext.c)
set(ext_basic_bin ${PROJECT_BINARY_DIR}/llext/proc_basic.llext)
set(ext_basic_inc ${ZEPHYR_BINARY_DIR}/include/generated/proc_basic.inc)
add_llext_target(proc_basic_ext OUTPUT ${ext_basic_bin} SOURCES ${ext_basic_src})
generate_inc_file_for_target(app ${ext_basic_bin} ${ext_basic_inc})

# args – echoes the void* arg into a result variable via shared memory
set(ext_args_src ${PROJECT_SOURCE_DIR}/ext/proc_args_ext.c)
set(ext_args_bin ${PROJECT_BINARY_DIR}/llext/proc_args.llext)
set(ext_args_inc ${ZEPHYR_BINARY_DIR}/include/generated/proc_args.inc)
add_llext_target(proc_args_ext OUTPUT ${ext_args_bin} SOURCES ${ext_args_src})
generate_inc_file_for_target(app ${ext_args_bin} ${ext_args_inc})

# counter – increments a semaphore-protected counter
set(ext_ctr_src ${PROJECT_SOURCE_DIR}/ext/proc_counter_ext.c)
set(ext_ctr_bin ${PROJECT_BINARY_DIR}/llext/proc_counter.llext)
set(ext_ctr_inc ${ZEPHYR_BINARY_DIR}/include/generated/proc_counter.inc)
add_llext_target(proc_counter_ext OUTPUT ${ext_ctr_bin} SOURCES ${ext_ctr_src})
generate_inc_file_for_target(app ${ext_ctr_bin} ${ext_ctr_inc})

# ──────────────────────────────────────────────────
# Test runner
# ──────────────────────────────────────────────────
target_sources(app PRIVATE src/main.c)

target_include_directories(app PRIVATE
  ${ZEPHYR_BASE}/include
)
