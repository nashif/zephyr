# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(interrupt_latency)

target_sources(app PRIVATE
  src/main.c
  src/stats.c
)

target_sources_ifdef(CONFIG_INT_BENCH_TRIGGER_SW_IRQ app PRIVATE src/trigger_sw_irq.c)
target_sources_ifdef(CONFIG_INT_BENCH_TRIGGER_OFFLOAD app PRIVATE src/trigger_offload.c)

target_sources_ifdef(CONFIG_INT_BENCH_SCENARIO_ENTRY app PRIVATE src/sc_entry.c)
target_sources_ifdef(CONFIG_INT_BENCH_SCENARIO_EXIT app PRIVATE src/sc_exit.c)
target_sources_ifdef(CONFIG_INT_BENCH_SCENARIO_LOCKED app PRIVATE src/sc_locked.c)
target_sources_ifdef(CONFIG_INT_BENCH_SCENARIO_DYNAMIC app PRIVATE src/sc_dynamic.c)
target_sources_ifdef(CONFIG_INT_BENCH_SCENARIO_THROUGHPUT app PRIVATE src/sc_throughput.c)
