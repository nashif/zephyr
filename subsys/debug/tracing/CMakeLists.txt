zephyr_include_directories(include)

zephyr_sources_ifdef(
  CONFIG_SEGGER_SYSTEMVIEW
  SEGGER_SYSVIEW_Config_Zephyr.c
  SEGGER_SYSVIEW_Zephyr.c
  )
