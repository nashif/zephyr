# Copyright (c) 2020-2025 Nordic Semiconductor ASA
# Copyright (c) 2026 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# Application-side bootutil: the swap-state and trailer helpers a
# chain-loaded application needs (image confirmation, test requests,
# swap type queries), used by dfu, mcumgr img_mgmt and retention.
#
# While the mcuboot module is in the workspace its copy of this library
# takes precedence, so that nothing changes for existing users.
if(ZEPHYR_MCUBOOT_MODULE_DIR)
  return()
endif()

zephyr_interface_library_named(MCUBOOT_BOOTUTIL)

target_include_directories(MCUBOOT_BOOTUTIL INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

zephyr_library_named(mcuboot_util)
zephyr_library_sources(src/bootutil_public.c)

zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
target_link_libraries(MCUBOOT_BOOTUTIL INTERFACE zephyr_interface)

if(CONFIG_BOOT_USE_PSA_CRYPTO)
  target_include_directories(MCUBOOT_BOOTUTIL INTERFACE ${ZEPHYR_MBEDTLS_MODULE_DIR}/include)
endif()

if(CONFIG_BOOT_USE_MBEDTLS OR CONFIG_BOOT_PSA_CRYPTO_BACKEND_MBEDTLS)
  zephyr_link_libraries_ifdef(CONFIG_MBEDTLS_BUILTIN mbedTLS)
endif()
