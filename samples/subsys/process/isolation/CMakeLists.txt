# Copyright (c) 2026 The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(process_isolation LANGUAGES C)

# ──────────────────────────────────────────────────
# Extension A
# ──────────────────────────────────────────────────
set(ext_a_src  ${PROJECT_SOURCE_DIR}/ext/proc_a_ext.c)
set(ext_a_bin  ${PROJECT_BINARY_DIR}/llext/proc_a.llext)
set(ext_a_inc  ${ZEPHYR_BINARY_DIR}/include/generated/proc_a.inc)

add_llext_target(proc_a_ext
  OUTPUT  ${ext_a_bin}
  SOURCES ${ext_a_src}
)
generate_inc_file_for_target(app ${ext_a_bin} ${ext_a_inc})

# ──────────────────────────────────────────────────
# Extension B
# ──────────────────────────────────────────────────
set(ext_b_src  ${PROJECT_SOURCE_DIR}/ext/proc_b_ext.c)
set(ext_b_bin  ${PROJECT_BINARY_DIR}/llext/proc_b.llext)
set(ext_b_inc  ${ZEPHYR_BINARY_DIR}/include/generated/proc_b.inc)

add_llext_target(proc_b_ext
  OUTPUT  ${ext_b_bin}
  SOURCES ${ext_b_src}
)
generate_inc_file_for_target(app ${ext_b_bin} ${ext_b_inc})

# ──────────────────────────────────────────────────
# Host application
# ──────────────────────────────────────────────────
target_sources(app PRIVATE src/main.c)
