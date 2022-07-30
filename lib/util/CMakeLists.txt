# SPDX-License-Identifier: Apache-2.0

add_subdirectory_ifdef(CONFIG_FNMATCH		fnmatch)
zephyr_sources_ifdef(CONFIG_BASE64 base64.c)
zephyr_sources_ifdef(CONFIG_UTF8 utf8.c)
zephyr_sources(
  timeutil.c
  crc32c_sw.c
  crc32_sw.c
  crc16_sw.c
  crc8_sw.c
  crc7_sw.c
  dec.c
  hex.c
  bitarray.c
)
