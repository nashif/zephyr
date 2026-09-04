# Copyright (c) 2022-2026 Nordic Semiconductor
# Copyright (c) 2026 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# Include a bootloader image if enabled: either the in-tree Zephyr bootloader
# or MCUboot from the mcuboot module. Both boot MCUboot-format images.
if(SB_CONFIG_BOOTLOADER_ZEPHYR)
  set(image boot)
  set(image_source_dir ${ZEPHYR_BASE}/boot)
elseif(SB_CONFIG_BOOTLOADER_MCUBOOT)
  set(image mcuboot)
  set(image_source_dir ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/zephyr)
endif()

if(SB_CONFIG_BOOTLOADER_MCUBOOT_COMPATIBLE)
  ExternalZephyrProject_Add(
    APPLICATION ${image}
    SOURCE_DIR ${image_source_dir}
    APP_TYPE BOOTLOADER
  )
  # Place the bootloader first in list to ensure it is configured and flashed before other images.
  sysbuild_add_dependencies(CONFIGURE ${DEFAULT_IMAGE} ${image})
  sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} ${image})

  # Link the footer size output of this bootloader build with the main sysbuild image
  set_target_properties(${image} PROPERTIES MCUBOOT_FOOTER_UPDATE_IMAGES ${DEFAULT_IMAGE})

  sysbuild_mcuboot_resolve_signature_key_files(
    resolved_key_files "${SB_CONFIG_BOOT_SIGNATURE_KEY_FILE}"
  )
  set_config_string(${image} CONFIG_BOOT_SIGNATURE_KEY_FILE "${resolved_key_files}")

  if(SB_CONFIG_MCUBOOT_DIRECT_XIP_GENERATE_VARIANT)
    ExternalZephyrVariantProject_Add(
      APPLICATION ${DEFAULT_IMAGE}_slot1_variant
      SOURCE_APP ${DEFAULT_IMAGE}
      SNIPPET slot1-partition
    )
  endif()
endif()

if(SB_CONFIG_BOOTLOADER_ZEPHYR)
  sysbuild_register_hooks(zephyr_boot)
endif()

# Re-run sysbuild when the bootloader configuration changes, as its trailer
# size estimate is passed on to the images it boots.
function(zephyr_boot_pre_cmake)
  cmake_parse_arguments(PRE_CMAKE "" "" "IMAGES" ${ARGN})

  foreach(image ${PRE_CMAKE_IMAGES})
    get_property(footer_update_images TARGET ${image} PROPERTY MCUBOOT_FOOTER_UPDATE_IMAGES)
    if(footer_update_images)
      set_property(
        DIRECTORY APPEND PROPERTY
        CMAKE_CONFIGURE_DEPENDS
        ${CMAKE_BINARY_DIR}/${image}/CMakeCache.txt
        ${CMAKE_BINARY_DIR}/${image}/zephyr/.config
      )
    endif()
  endforeach()
endfunction()

# Forward the trailer size estimate computed by the bootloader build to the
# images it boots, so that they leave room for the image trailer.
function(zephyr_boot_post_image_cmake)
  cmake_parse_arguments(POST_IMAGE_CMAKE "" "IMAGE" "IMAGES" ${ARGN})
  set(image ${POST_IMAGE_CMAKE_IMAGE})

  get_property(footer_update_images TARGET ${image} PROPERTY MCUBOOT_FOOTER_UPDATE_IMAGES)
  if(NOT footer_update_images)
    return()
  endif()

  sysbuild_get(mcuboot_image_footer_size IMAGE ${image} CACHE)
  sysbuild_get(mcuboot_image_upgrade_footer_size IMAGE ${image} CACHE)
  math(EXPR mcuboot_image_footer_size "${mcuboot_image_footer_size}"
       OUTPUT_FORMAT HEXADECIMAL)
  math(EXPR mcuboot_image_upgrade_footer_size "${mcuboot_image_upgrade_footer_size}"
       OUTPUT_FORMAT HEXADECIMAL)

  foreach(update_image ${footer_update_images})
    set_config_int(${update_image} CONFIG_ROM_END_OFFSET ${mcuboot_image_footer_size})
    set_config_int(${update_image} CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE
                   ${mcuboot_image_upgrade_footer_size})
  endforeach()
endfunction()
