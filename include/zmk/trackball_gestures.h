/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * Public API for the ZMK trackball gesture emulation engine.
 *
 * This module translates trackball (relative input) movement into
 * multi-finger touchpad gestures that the host OS interprets as
 * pinch-to-zoom, two-finger scroll, or multi-finger swipes.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Gesture modes — these values match the behavior parameter
 *        passed from the keymap binding (e.g. &gesture GESTURE_PINCH).
 */
enum gesture_mode {
    GESTURE_NONE   = 0,  /**< No gesture active; trackball works normally */
    GESTURE_SWIPE2 = 1,  /**< Two-finger swipe                            */
    GESTURE_SWIPE3 = 2,  /**< Three-finger swipe                          */
    GESTURE_SWIPE4 = 3,  /**< Four-finger swipe                           */
    GESTURE_SWIPE5 = 4,  /**< Five-finger swipe                           */
    GESTURE_PINCH2 = 5,  /**< Two-finger pinch (zoom in / zoom out)       */
    GESTURE_PINCH3 = 6,  /**< Three-finger pinch                          */
    GESTURE_PINCH4 = 7,  /**< Four-finger pinch                           */
    GESTURE_PINCH5 = 8,  /**< Five-finger pinch                           */

};

/**
 * @brief Activate a gesture mode.
 *
 * Places the virtual finger contacts on the touchpad surface and starts
 * the periodic HID report timer.  If a mode is already active it is
 * deactivated first.
 *
 * @param mode  The gesture mode to activate.
 */
void gesture_mode_activate(enum gesture_mode mode);

/**
 * @brief Deactivate the current gesture mode.
 *
 * Sends a finger-up (tip_switch = 0) report for every contact, stops the
 * periodic timer, and returns the trackball to normal operation.
 */
void gesture_mode_deactivate(void);

/**
 * @brief Feed a relative trackball delta into the active gesture.
 *
 * Called from the input processor whenever a REL_X or REL_Y event
 * arrives while a gesture is active.  The delta is scaled by the
 * configured sensitivity and accumulated for the next timer tick.
 *
 * This function is safe to call from interrupt / input-event context.
 *
 * @param dx  Horizontal delta (positive = right).
 * @param dy  Vertical delta   (positive = down).
 */
void gesture_feed_delta(int16_t dx, int16_t dy);

/**
 * @brief Return the currently active gesture mode.
 *
 * @return The current gesture_mode value, or GESTURE_NONE if idle.
 */
enum gesture_mode gesture_mode_get(void);
