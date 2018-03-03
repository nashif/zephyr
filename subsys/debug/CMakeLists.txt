zephyr_sources_ifdef(
  CONFIG_OPENOCD_SUPPORT
  openocd.c
  )

zephyr_sources_ifdef(
  CONFIG_SEGGER_SYSTEMVIEW
  sysview.c
  )
