# SPDX-License-Identifier: MIT

if(CONFIG_SOFLE_LOCK_STATUS)
  target_sources(app PRIVATE src/lock_status_sync.c)
  target_include_directories(app PRIVATE include)
endif()

if(CONFIG_SOFLE_RIGHT_LOCK_DISPLAY)
  if(CONFIG_NICE_VIEW_WIDGET_STATUS)
    message(FATAL_ERROR "Disable NICE_VIEW_WIDGET_STATUS for the right lock display")
  endif()
  # Use the same drawing/rotation helpers as this ZMK version's nice!view screen.
  set(nice_view_widgets ${CMAKE_SOURCE_DIR}/boards/shields/nice_view/widgets)
  target_include_directories(app PRIVATE ${nice_view_widgets})
  target_sources(app PRIVATE
    src/right_lock_screen.c
    ${nice_view_widgets}/util.c
    ${nice_view_widgets}/bolt.c
  )
endif()
