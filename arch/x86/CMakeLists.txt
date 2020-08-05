# Copyright (c) 2019 Intel Corp.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_X86_64)
  include(intel64.cmake)
else()
  include(ia32.cmake)
endif()

zephyr_library_sources_ifdef(CONFIG_TIMING_FUNCTIONS timing.c)
