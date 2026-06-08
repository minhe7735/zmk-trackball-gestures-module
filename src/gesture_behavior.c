/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * ZMK behavior driver for trackball gesture emulation.
 *
 * Usage in a keymap:
 *
 *   #include <behaviors/gesture.dtsi>
 *   ...
 *   bindings = <&gesture GESTURE_PINCH>;   // hold key → pinch mode
 *
 * On key-press the corresponding gesture mode is activated; on key-release
 * the gesture is deactivated (finger-up reports are sent).
 */

#define DT_DRV_COMPAT zmk_behavior_gesture

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/trackball_gestures.h>

LOG_MODULE_REGISTER(behavior_gesture, CONFIG_ZMK_LOG_LEVEL);

/* ------------------------------------------------------------------ */
/* Behavior callbacks                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Called when the bound key is pressed.
 *
 * param1 carries the gesture mode (GESTURE_PINCH, GESTURE_SCROLL, …).
 */
static int on_gesture_binding_pressed(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event)
{
    enum gesture_mode mode = (enum gesture_mode)binding->param1;

    if (mode < GESTURE_NONE || mode > GESTURE_PINCH5) {
        LOG_WRN("Invalid gesture mode %d requested", (int)mode);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    LOG_DBG("Gesture binding pressed, mode=%d", (int)mode);
    gesture_mode_activate(mode);
    return ZMK_BEHAVIOR_OPAQUE;
}

/**
 * @brief Called when the bound key is released.
 *
 * Deactivates the gesture regardless of which mode was active.
 */
static int on_gesture_binding_released(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event)
{
    LOG_DBG("Gesture binding released");
    gesture_mode_deactivate();
    return ZMK_BEHAVIOR_OPAQUE;
}

/* ------------------------------------------------------------------ */
/* Driver API and instantiation                                        */
/* ------------------------------------------------------------------ */

static const struct behavior_driver_api gesture_behavior_api = {
    .binding_pressed  = on_gesture_binding_pressed,
    .binding_released = on_gesture_binding_released,
};

static int gesture_behavior_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define GESTURE_BEHAVIOR_INST(n)                                        \
    BEHAVIOR_DT_INST_DEFINE(n, gesture_behavior_init, NULL, NULL, NULL, \
                            POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,        \
                            &gesture_behavior_api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_BEHAVIOR_INST)
