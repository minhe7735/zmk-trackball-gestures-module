# Copyright (c) 2024 ZMK Trackball Gestures Contributors
# SPDX-License-Identifier: MIT

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_TRACKBALL_GESTURES
  src/trackball_gestures.c
  src/virtual_touchpad.c
  src/gesture_behavior.c
)

zephyr_include_directories_ifdef(CONFIG_ZMK_TRACKBALL_GESTURES include)
