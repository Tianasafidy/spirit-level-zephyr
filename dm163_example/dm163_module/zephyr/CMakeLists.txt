if(CONFIG_DM163_DRIVER)
  # Unused for now as we do not have any .h, but we might need to add
  # .h files later for this driver
  zephyr_include_directories(.)

  zephyr_library()
  zephyr_library_sources(dm163.c)
endif()