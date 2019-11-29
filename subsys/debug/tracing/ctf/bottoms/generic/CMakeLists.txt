# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(include)

zephyr_sources(
  ctf_bottom.c
  tracing_core.c
  tracing_format.c
  tracing_packet.c
)

zephyr_sources_ifdef(
  CONFIG_TRACING_BACKEND_UART
  tracing_backend_uart.c
)
