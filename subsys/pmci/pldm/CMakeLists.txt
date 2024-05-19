# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

#add_subdirectory(transport)
#add_subdirectory(requester)

zephyr_sources(
  base.c
  pdr.c
  platform.c
  responder.c
  utils.c
)
