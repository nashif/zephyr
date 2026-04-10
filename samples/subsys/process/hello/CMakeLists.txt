# Copyright (c) 2026 The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(process_hello LANGUAGES C)

# ──────────────────────────────────────────────────
# Build the loadable extension (hello_ext.c → hello.llext)
# and embed it as a generated C header (hello.inc).
# ──────────────────────────────────────────────────
set(ext_src  ${PROJECT_SOURCE_DIR}/ext/hello_ext.c)
set(ext_bin  ${PROJECT_BINARY_DIR}/llext/hello.llext)
set(ext_inc  ${ZEPHYR_BINARY_DIR}/include/generated/hello.inc)

add_llext_target(hello_ext
  OUTPUT  ${ext_bin}
  SOURCES ${ext_src}
)
generate_inc_file_for_target(app ${ext_bin} ${ext_inc})

# ──────────────────────────────────────────────────
# Host application
# ──────────────────────────────────────────────────
target_sources(app PRIVATE src/main.c)
