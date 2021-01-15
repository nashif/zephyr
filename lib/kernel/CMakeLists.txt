# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  rb.c
  sem.c
  thread_entry.c
  work_q.c
  heap.c
  heap-validate.c
  )

zephyr_sources_ifdef(CONFIG_USERSPACE mutex.c)

zephyr_sources_ifdef(CONFIG_SCHED_DEADLINE p4wq.c)

zephyr_library_include_directories(
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/${ARCH}/include
)
