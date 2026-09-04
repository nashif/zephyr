# Copyright (c) 2017 Open Source Foundries Limited
# Copyright (c) 2023-2026 Nordic Semiconductor ASA
# Copyright (c) 2026 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# The in-tree bootloader: MCUboot's bootutil core plus the Zephyr
# integration, built into the kernel image when ZEPHYR_BOOTLOADER=y.

set(BOOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BOOTUTIL_DIR ${BOOT_DIR}/bootutil)
set(BOOT_KEYS_DIR ${BOOT_DIR}/keys)

zephyr_library_named(zephyr_boot)

zephyr_include_directories(
  ${BOOT_DIR}/include
  ${BOOTUTIL_DIR}/include
)

if(CONFIG_MBEDTLS)
  zephyr_include_directories(
    ${ZEPHYR_MBEDTLS_MODULE_DIR}/include
    ${ZEPHYR_MBEDTLS_MODULE_DIR}/library
  )
endif()

zephyr_library_sources(
  src/main.c
  src/io.c
  src/flash_map_extended.c
  src/os.c
  src/keys.c
  src/watchdog.c
)

if(CONFIG_ARM)
  zephyr_library_sources(src/arch/arm.c)
elseif(CONFIG_SOC_FAMILY_ESPRESSIF_ESP32)
  zephyr_library_sources(src/arch/esp32.c)
elseif(CONFIG_XTENSA)
  zephyr_library_sources(src/arch/xtensa.c)
elseif(CONFIG_ARC)
  zephyr_library_sources(src/arch/arc.c)
else()
  zephyr_library_sources(src/arch/default.c)
endif()

zephyr_library_sources_ifdef(CONFIG_BOOT_SHARE_BACKEND_RETENTION src/shared_data.c)

zephyr_library_sources(
  ${BOOTUTIL_DIR}/src/image_validate.c
  ${BOOTUTIL_DIR}/src/bootutil_find_key.c
  ${BOOTUTIL_DIR}/src/bootutil_img_hash.c
  ${BOOTUTIL_DIR}/src/bootutil_img_security_cnt.c
  ${BOOTUTIL_DIR}/src/tlv.c
  ${BOOTUTIL_DIR}/src/encrypted.c
  ${BOOTUTIL_DIR}/src/image_rsa.c
  ${BOOTUTIL_DIR}/src/image_ecdsa.c
  ${BOOTUTIL_DIR}/src/image_ed25519.c
  ${BOOTUTIL_DIR}/src/bootutil_misc.c
  ${BOOTUTIL_DIR}/src/bootutil_area.c
  ${BOOTUTIL_DIR}/src/bootutil_loader.c
  ${BOOTUTIL_DIR}/src/bootutil_public.c
  ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
)

if((CONFIG_BOOT_ENCRYPT_X25519 AND CONFIG_BOOT_ED25519_PSA)
  OR (CONFIG_BOOT_ENCRYPT_EC256 AND CONFIG_BOOT_ECDSA_PSA))
  zephyr_library_sources(${BOOTUTIL_DIR}/src/encrypted_psa.c)
endif()

if(CONFIG_MEASURED_BOOT OR CONFIG_BOOT_SHARE_DATA)
  zephyr_library_sources(${BOOTUTIL_DIR}/src/boot_record.c)
  set_source_files_properties(${BOOTUTIL_DIR}/src/boot_record.c
    PROPERTIES COMPILE_FLAGS -DZEPHYR_VER_INCLUDE=1
  )
endif()

if(CONFIG_BOOT_FIH_PROFILE_HIGH)
  if(CONFIG_BOOT_USE_PSA_CRYPTO)
    zephyr_library_sources(${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_psa.c)
  else()
    zephyr_library_sources(${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_mbedtls.c)
  endif()
endif()

if(CONFIG_SINGLE_APPLICATION_SLOT_RAM_LOAD)
  zephyr_library_sources(src/single_loader.c ${BOOTUTIL_DIR}/src/ram_load.c)
  zephyr_library_include_directories(${BOOTUTIL_DIR}/src)
elseif(CONFIG_SINGLE_APPLICATION_SLOT)
  zephyr_library_sources(src/single_loader.c)
  zephyr_library_include_directories(${BOOTUTIL_DIR}/src)
elseif(CONFIG_BOOT_FIRMWARE_LOADER)
  zephyr_library_sources(src/firmware_loader.c)
  zephyr_library_include_directories(${BOOTUTIL_DIR}/src)
else()
  zephyr_library_sources(
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/caps.c
  )

  if(CONFIG_BOOT_SWAP_USING_MOVE)
    zephyr_library_sources(${BOOTUTIL_DIR}/src/swap_move.c)
  elseif(CONFIG_BOOT_SWAP_USING_OFFSET)
    zephyr_library_sources(${BOOTUTIL_DIR}/src/swap_offset.c)
  else()
    zephyr_library_sources(${BOOTUTIL_DIR}/src/swap_scratch.c)
    zephyr_library_sources_ifdef(CONFIG_BOOT_RAM_LOAD ${BOOTUTIL_DIR}/src/ram_load.c)
  endif()
endif()

if(CONFIG_BOOT_SIGNATURE_TYPE_ED25519 AND CONFIG_BOOT_ED25519_PSA)
  zephyr_library_sources(${BOOTUTIL_DIR}/src/ed25519_psa.c)
endif()

if(CONFIG_BOOT_SIGNATURE_TYPE_RSA AND CONFIG_BOOT_ENCRYPT_RSA)
  set_source_files_properties(${BOOTUTIL_DIR}/src/encrypted.c
    PROPERTIES INCLUDE_DIRECTORIES ${ZEPHYR_MBEDTLS_MODULE_DIR}/library
  )
endif()

# Verification keys: every entry of BOOT_SIGNATURE_KEY_FILE is turned into a
# public-key C source by imgtool.
if(NOT CONFIG_BOOT_SIGNATURE_TYPE_NONE AND NOT CONFIG_BOOT_BUILTIN_KEY AND NOT
  CONFIG_BOOT_SIGNATURE_KEY_FILE STREQUAL "")

  if(NOT IMGTOOL)
    message(FATAL_ERROR "imgtool is required to build the bootloader: install it with "
                        "'pip install imgtool' or add the mcuboot repository to the workspace.")
  endif()

  file(GLOB boot_debug_key_files ${BOOT_KEYS_DIR}/*.pem)

  string(REPLACE "," ";" boot_key_files "${CONFIG_BOOT_SIGNATURE_KEY_FILE}")
  list(LENGTH boot_key_files boot_sign_key_count)

  set(key_index 0)
  foreach(raw_key_path IN LISTS boot_key_files)
    string(CONFIGURE "${raw_key_path}" key_path)

    if(IS_ABSOLUTE ${key_path})
      set(resolved_key_path ${key_path})
    elseif(EXISTS ${APPLICATION_CONFIG_DIR}/${key_path})
      set(resolved_key_path ${APPLICATION_CONFIG_DIR}/${key_path})
    else()
      set(resolved_key_path ${BOOT_KEYS_DIR}/${key_path})
    endif()

    if(key_index EQUAL 0)
      set(name_suffix_arg "")
      set(generated_pubkey ${ZEPHYR_BINARY_DIR}/autogen-pubkey.c)
      set(keyinfo_check_command "")
      message("Bootloader key file: ${resolved_key_path}")
    else()
      set(name_suffix_arg "--name-suffix" "_${key_index}")
      set(generated_pubkey ${ZEPHYR_BINARY_DIR}/autogen-pubkey-${key_index}.c)
      set(keyinfo_check_command
        COMMAND ${IMGTOOL} keyinfo --key ${resolved_key_path} --require public
      )
      message("Bootloader verification key file #${key_index}: ${resolved_key_path}")
    endif()
    if(${resolved_key_path} IN_LIST boot_debug_key_files)
      message(WARNING "Using a default debug signing key (${resolved_key_path}); "
                      "this key is public and not secure.")
    endif()

    add_custom_command(
      OUTPUT ${generated_pubkey}
      ${keyinfo_check_command}
      COMMAND ${IMGTOOL} getpub -k ${resolved_key_path} ${name_suffix_arg} > ${generated_pubkey}
      DEPENDS ${resolved_key_path}
    )
    zephyr_library_sources(${generated_pubkey})
    math(EXPR key_index "${key_index} + 1")
  endforeach()
  zephyr_library_compile_definitions(MCUBOOT_SIGN_KEY_COUNT=${boot_sign_key_count})
elseif(CONFIG_BOOT_BUILTIN_KEY)
  zephyr_library_compile_definitions(MCUBOOT_SIGN_KEY_COUNT=1)
endif()

if(CONFIG_BOOT_ENCRYPTION_KEY_FILE AND NOT CONFIG_BOOT_ENCRYPTION_KEY_FILE STREQUAL "")
  string(CONFIGURE "${CONFIG_BOOT_ENCRYPTION_KEY_FILE}" key_file)

  if(IS_ABSOLUTE ${key_file})
    set(encryption_key_file ${key_file})
  elseif(EXISTS ${APPLICATION_CONFIG_DIR}/${key_file})
    set(encryption_key_file ${APPLICATION_CONFIG_DIR}/${key_file})
  else()
    set(encryption_key_file ${BOOT_KEYS_DIR}/${key_file})
  endif()
  message("Bootloader encryption key file: ${encryption_key_file}")

  set(generated_enckey ${ZEPHYR_BINARY_DIR}/autogen-enckey.c)
  add_custom_command(
    OUTPUT ${generated_enckey}
    COMMAND ${IMGTOOL} getpriv -k ${encryption_key_file} > ${generated_enckey}
    DEPENDS ${encryption_key_file}
  )
  zephyr_library_sources(${generated_enckey})
endif()

if(CONFIG_MCUBOOT_CLEANUP_ARM_CORE)
  zephyr_library_sources_ifdef(CONFIG_CPU_CORTEX_M src/cleanup/arm_cortex_m.c)
  zephyr_library_sources_ifdef(CONFIG_ARMV7_R src/cleanup/arm_cortex_r.c)
endif()

zephyr_library_sources_ifdef(CONFIG_MCUBOOT_BOOT_BANNER src/kernel/banner.c)

zephyr_library_link_libraries_ifdef(CONFIG_MBEDTLS mbedTLS)

include(${BOOT_DIR}/cmake/slots.cmake)
