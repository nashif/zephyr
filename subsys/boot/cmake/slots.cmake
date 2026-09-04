# Copyright (c) 2023-2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0

# Slot geometry checks and the image trailer size estimate. The estimate is
# exported as cache variables which sysbuild forwards to the chain-loaded
# application as ROM_END_OFFSET / MCUBOOT_UPDATE_FOOTER_SIZE.

function(align_up num align result)
  math(EXPR out "(((${num}) + ((${align}) - 1)) & ~((${align}) - 1))")
  set(${result} "${out}" PARENT_SCOPE)
endfunction()

# Takes a node path to a partition and goes up until a parent with the soc-nv-flash compatible
# is found, then reads the erase and write block sizes from it
function(dt_get_erase_write_block_sizes node erase_block_size write_block_size)
  string(FIND "${node}" "/" node_first_slash)
  string(FIND "${node}" "/" node_final_slash REVERSE)
  string(SUBSTRING "${node}" 0 ${node_final_slash} current_path)

  while(${node_final_slash} GREATER ${node_first_slash})
    set(current_compatible)
    dt_prop(current_compatible PATH "${current_path}" PROPERTY "compatible")

    if("soc-nv-flash" IN_LIST current_compatible)
      dt_prop(erase_size PATH "${current_path}" PROPERTY "erase-block-size")
      dt_prop(write_size PATH "${current_path}" PROPERTY "write-block-size")
      set(${erase_block_size} ${erase_size} PARENT_SCOPE)
      set(${write_block_size} ${write_size} PARENT_SCOPE)
      break()
    endif()

    string(FIND "${current_path}" "/" node_final_slash REVERSE)
    string(SUBSTRING "${current_path}" 0 ${node_final_slash} current_path)
  endwhile()
endfunction()

# Takes a node path to a partition and goes up until a parent with the soc-nv-flash compatible
# is found, then returns the path for that device
function(dt_get_nvm_device node nvm_device)
  string(FIND "${node}" "/" node_first_slash)
  string(FIND "${node}" "/" node_final_slash REVERSE)
  string(SUBSTRING "${node}" 0 ${node_final_slash} current_path)

  while(${node_final_slash} GREATER ${node_first_slash})
    set(current_compatible)
    dt_prop(current_compatible PATH "${current_path}" PROPERTY "compatible")

    if("soc-nv-flash" IN_LIST current_compatible)
      set(${nvm_device} ${current_path} PARENT_SCOPE)
      break()
    elseif("fixed-partitions" IN_LIST current_compatible)
      string(FIND "${current_path}" "/" node_final_slash REVERSE)
      string(SUBSTRING "${current_path}" 0 ${node_final_slash} current_path)
      set(${nvm_device} ${current_path} PARENT_SCOPE)
      break()
    endif()

    string(FIND "${current_path}" "/" node_final_slash REVERSE)
    string(SUBSTRING "${current_path}" 0 ${node_final_slash} current_path)
  endwhile()
endfunction()

# Calculate erase/write sizes and provide definitions for them as well as calculating the maximum
# sectors (if the feature is enabled)
set(image 0)
set(auto_min_sectors 0)

while(${image} LESS ${CONFIG_UPDATEABLE_IMAGE_NUMBER})
  set(slot1_flash)
  set(slot1_size)
  set(erase_size_slot1)
  set(write_size_slot1)
  math(EXPR primary_slot "${image} * 2")

  dt_nodelabel(slot0_flash NODELABEL "slot${primary_slot}_partition" REQUIRED)
  dt_prop(slot0_size PATH "${slot0_flash}" PROPERTY "reg" INDEX 1)
  dt_get_erase_write_block_sizes(${slot0_flash} erase_size_slot0 write_size_slot0)

  if(CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET)
    if(DEFINED erase_size_slot0)
      zephyr_compile_definitions("MCUBOOT_SLOT${primary_slot}_EXPECTED_ERASE_SIZE=${erase_size_slot0}")
    endif()

    if(DEFINED write_size_slot0)
      zephyr_compile_definitions("MCUBOOT_SLOT${primary_slot}_EXPECTED_WRITE_SIZE=${write_size_slot0}")
    endif()
  endif()

  if(CONFIG_BOOT_MAX_IMG_SECTORS_AUTO)
    if(DEFINED slot0_size AND DEFINED erase_size_slot0)
      math(EXPR slot_min_sectors "${slot0_size} / ${erase_size_slot0}")

      if(${slot_min_sectors} GREATER ${auto_min_sectors})
        set(auto_min_sectors ${slot_min_sectors})

        if(${image} EQUAL 0)
          set(image_0_min_sectors ${slot_min_sectors})
        endif()
      endif()
    else()
      message(WARNING "Unable to determine erase size/total size of slot${primary_slot} partition")
    endif()
  endif()

  if(NOT CONFIG_SINGLE_APPLICATION_SLOT AND NOT CONFIG_SINGLE_APPLICATION_SLOT_RAM_LOAD)
    set(slot1_flash)
    set(slot1_size)
    set(erase_size_slot1)
    set(write_size_slot1)
    math(EXPR secondary_slot "${primary_slot} + 1")

    dt_nodelabel(slot1_flash NODELABEL "slot${secondary_slot}_partition" REQUIRED)
    dt_prop(slot1_size PATH "${slot1_flash}" PROPERTY "reg" INDEX 1)
    dt_get_erase_write_block_sizes(${slot1_flash} erase_size_slot1 write_size_slot1)

    if(CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET)
      if(DEFINED erase_size_slot1)
        zephyr_compile_definitions("MCUBOOT_SLOT${secondary_slot}_EXPECTED_ERASE_SIZE=${erase_size_slot1}")
      endif()

      if(DEFINED write_size_slot1)
        zephyr_compile_definitions("MCUBOOT_SLOT${secondary_slot}_EXPECTED_WRITE_SIZE=${write_size_slot1}")
      endif()
    endif()

    if(CONFIG_BOOT_MAX_IMG_SECTORS_AUTO)
      if(DEFINED slot1_size AND DEFINED erase_size_slot1)
        math(EXPR slot_min_sectors "${slot1_size} / ${erase_size_slot1}")

        if(${slot_min_sectors} GREATER ${auto_min_sectors})
          set(auto_min_sectors ${slot_min_sectors})

          if(${image} EQUAL 0)
            set(image_0_min_sectors ${slot_min_sectors})
          endif()
        endif()
      else()
        message(WARNING "Unable to determine erase size/total size of slot${secondary_slot} partition")
      endif()
    endif()
  endif()

  math(EXPR image "${image} + 1")
endwhile()

if(CONFIG_BOOT_MAX_IMG_SECTORS_AUTO)
  if("${auto_min_sectors}" GREATER "0")
    zephyr_compile_definitions("MIN_SECTOR_COUNT=${auto_min_sectors}")
    message("Calculated maximum number of sectors: ${auto_min_sectors}")
  else()
    message(WARNING "Unable to calculate minimum number of sector sizes, falling back to 128 sector default. Please disable CONFIG_BOOT_MAX_IMG_SECTORS_AUTO and set CONFIG_BOOT_MAX_IMG_SECTORS to the required value")
  endif()
endif()

if((CONFIG_BOOT_SWAP_USING_SCRATCH OR CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET) AND (DEFINED write_size_slot0 OR DEFINED write_size_slot1))
  zephyr_library_sources(${BOOT_DIR}/src/flash_check.c)
endif()

if(SYSBUILD)
  if(CONFIG_SINGLE_APPLICATION_SLOT OR CONFIG_BOOT_FIRMWARE_LOADER OR CONFIG_BOOT_SWAP_USING_SCRATCH OR CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET OR CONFIG_BOOT_UPGRADE_ONLY OR CONFIG_BOOT_DIRECT_XIP OR CONFIG_BOOT_RAM_LOAD)
    # TODO: RAM LOAD support
    dt_nodelabel(slot0_flash NODELABEL "slot0_partition" REQUIRED)
    dt_get_nvm_device(${slot0_flash} slot0_device)

    if(NOT CONFIG_SINGLE_APPLICATION_SLOT)
      dt_nodelabel(slot1_flash NODELABEL "slot1_partition" REQUIRED)
      dt_get_nvm_device(${slot1_flash} slot1_device)

      if(NOT "${slot0_device}" STREQUAL "${slot1_device}")
        # Check both slots for the one with the largest write/erase block size
        dt_prop(erase_size_slot0 PATH "${slot0_device}" PROPERTY "erase-block-size")
        dt_prop(write_size_slot0 PATH "${slot0_device}" PROPERTY "write-block-size")
        dt_prop(erase_size_slot1 PATH "${slot1_device}" PROPERTY "erase-block-size")
        dt_prop(write_size_slot1 PATH "${slot1_device}" PROPERTY "write-block-size")

        if(DEFINED erase_size_slot0 AND DEFINED erase_size_slot1)
          if(${erase_size_slot0} GREATER ${erase_size_slot1})
            set(erase_size ${erase_size_slot0})
          else()
            set(erase_size ${erase_size_slot1})
          endif()
        elseif(DEFINED erase_size_slot0)
          set(erase_size ${erase_size_slot0})
        elseif(DEFINED erase_size_slot1)
          set(erase_size ${erase_size_slot1})
        endif()

        if(DEFINED write_size_slot0 AND DEFINED write_size_slot1)
          if(${write_size_slot0} GREATER ${write_size_slot1})
            set(write_size ${write_size_slot0})
          else()
            set(write_size ${write_size_slot1})
          endif()
        elseif(DEFINED write_size_slot0)
          set(write_size ${write_size_slot0})
        elseif(DEFINED write_size_slot1)
          set(write_size ${write_size_slot1})
        endif()
      else()
        dt_prop(erase_size PATH "${slot0_device}" PROPERTY "erase-block-size")
        dt_prop(write_size PATH "${slot0_device}" PROPERTY "write-block-size")
      endif()
    else()
      dt_prop(erase_size PATH "${slot0_device}" PROPERTY "erase-block-size")
      dt_prop(write_size PATH "${slot0_device}" PROPERTY "write-block-size")
    endif()

    if(NOT DEFINED erase_size)
      message(WARNING "Unable to determine erase size of slot0 or slot1 partition, setting to 1 (this is probably wrong)")
      set(erase_size 1)
    endif()

    if(NOT DEFINED write_size)
      message(WARNING "Unable to determine write size of slot0 or slot1 partition, setting to 8 (this is probably wrong)")
      set(write_size 8)
    endif()

    if(${write_size} LESS 8)
      set(max_align_size 8)
    else()
      set(max_align_size ${write_size})
    endif()

    set(key_size 0)

    # Boot trailer magic size
    set(boot_magic_size 16)

    # Estimates for trailer TLV data size, this was taken from hello world builds for nrf52840dk
    if(CONFIG_BOOT_SIGNATURE_TYPE_RSA)
      if(CONFIG_BOOT_SIGNATURE_TYPE_RSA_LEN EQUAL 3072)
        set(boot_tlv_estimate 464)
      else()
        set(boot_tlv_estimate 336)
      endif()
    elseif(CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256)
      set(boot_tlv_estimate 150)
    elseif(CONFIG_BOOT_SIGNATURE_TYPE_ED25519)
      set(boot_tlv_estimate 144)
    else()
      set(boot_tlv_estimate 40)
    endif()

    if(CONFIG_BOOT_ENCRYPT_RSA OR CONFIG_BOOT_ENCRYPT_EC256 OR CONFIG_BOOT_ENCRYPT_X25519)
      # 128-bit AES key size
      set(boot_enc_key_size 16)

      if(CONFIG_BOOT_SWAP_SAVE_ENCTLV)
        if(CONFIG_BOOT_ENCRYPT_RSA)
          set(key_size 256)
        elseif(CONFIG_BOOT_ENCRYPT_EC256)
          math(EXPR key_size "65 + 32 + ${boot_enc_key_size}")
        elseif(CONFIG_BOOT_ENCRYPT_X25519)
          math(EXPR key_size "32 + 32 + ${boot_enc_key_size}")
        endif()
      else()
        set(key_size "${boot_enc_key_size}")
      endif()

      align_up(${key_size} ${max_align_size} key_size)
      math(EXPR key_size "${key_size} * 2")
    endif()

    align_up(${boot_magic_size} ${write_size} boot_magic_size)

    if(CONFIG_SINGLE_APPLICATION_SLOT OR CONFIG_BOOT_FIRMWARE_LOADER)
      set(boot_swap_data_size 0)
    else()
      math(EXPR boot_swap_data_size "${max_align_size} * 4")
    endif()

    if(CONFIG_BOOT_SWAP_USING_SCRATCH OR CONFIG_BOOT_SWAP_USING_MOVE)
      if(CONFIG_BOOT_MAX_IMG_SECTORS_AUTO AND DEFINED image_0_min_sectors AND "${image_0_min_sectors}" GREATER "0")
        math(EXPR boot_status_data_size "${image_0_min_sectors} * (3 * ${write_size})")
      else()
        if(CONFIG_BOOT_MAX_IMG_SECTORS)
          math(EXPR boot_status_data_size "${CONFIG_BOOT_MAX_IMG_SECTORS} * (3 * ${write_size})")
        else()
          message(WARNING "CONFIG_BOOT_MAX_IMG_SECTORS is not defined, falling back to 128 sector default. Please set CONFIG_BOOT_MAX_IMG_SECTORS to the required value")
          math(EXPR boot_status_data_size "128 * (3 * ${write_size})")
        endif()
      endif()
    elseif(CONFIG_BOOT_SWAP_USING_OFFSET)
      if(CONFIG_BOOT_MAX_IMG_SECTORS_AUTO AND DEFINED image_0_min_sectors AND "${image_0_min_sectors}" GREATER "0")
        math(EXPR boot_status_data_size "${image_0_min_sectors} * (2 * ${write_size})")
      else()
        if(CONFIG_BOOT_MAX_IMG_SECTORS)
          math(EXPR boot_status_data_size "${CONFIG_BOOT_MAX_IMG_SECTORS} * (2 * ${write_size})")
        else()
          message(WARNING "CONFIG_BOOT_MAX_IMG_SECTORS is not defined, falling back to 128 sector default. Please set CONFIG_BOOT_MAX_IMG_SECTORS to the required value")
          math(EXPR boot_status_data_size "128 * (2 * ${write_size})")
        endif()
      endif()
    else()
      set(boot_status_data_size 0)
    endif()

    math(EXPR trailer_size "${key_size} + ${boot_magic_size} + ${boot_swap_data_size} + ${boot_status_data_size}")

    if(CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET)
      align_up(${trailer_size} ${erase_size} trailer_size)
    endif()

    math(EXPR required_size "${trailer_size} + ${boot_tlv_estimate}")

    if(CONFIG_SINGLE_APPLICATION_SLOT OR CONFIG_BOOT_FIRMWARE_LOADER)
      set(required_upgrade_size "0")
    else()
      math(EXPR required_upgrade_size "${boot_magic_size} + ${boot_swap_data_size} + ${boot_status_data_size}")

      if(CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET)
        align_up(${required_upgrade_size} ${erase_size} required_upgrade_size)
      endif()
    endif()

    # For swap methods, check if the extra sector has been included in the images, if not then
    # reduce the available image size by a sector to account for this
    if((CONFIG_BOOT_SWAP_USING_MOVE OR CONFIG_BOOT_SWAP_USING_OFFSET) AND erase_size_slot0 AND erase_size_slot1)
      dt_prop(slot0_size PATH "${slot0_flash}" PROPERTY "reg" INDEX 1)
      dt_prop(slot1_size PATH "${slot1_flash}" PROPERTY "reg" INDEX 1)

      if(${slot0_size} EQUAL ${slot1_size})
        math(EXPR required_size "${required_size} + ${erase_size}")
        math(EXPR required_upgrade_size "${required_upgrade_size} + ${erase_size}")
      endif()
    endif()
  else()
    set(required_size 0)
    set(required_upgrade_size 0)
  endif()

  set(mcuboot_image_footer_size ${required_size} CACHE INTERNAL "Estimated MCUboot image trailer size" FORCE)
  set(mcuboot_image_upgrade_footer_size ${required_upgrade_size} CACHE INTERNAL "Estimated MCUboot update image trailer size" FORCE)
endif()
