/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * Virtual Touchpad — public API
 *
 * Provides an interface for submitting multi-finger touchpad reports
 * over USB HID, emulating a Windows Precision Touchpad.
 */

#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/** Maximum number of simultaneous finger contacts. */
#define TOUCHPAD_MAX_CONTACTS CONFIG_ZMK_TRACKBALL_GESTURES_MAX_FINGERS

/**
 * A single finger contact.
 */
struct touchpad_contact {
    bool active;     /**< Tip switch — finger is touching the surface. */
    uint8_t id;      /**< Contact identifier (0–4). */
    uint16_t x;      /**< Absolute X position. */
    uint16_t y;      /**< Absolute Y position. */
};

/**
 * Full touchpad report data.
 *
 * Populate the contacts array and set @p contact_count to the number
 * of contacts being reported in this frame.  The scan time stamp is
 * managed internally and does not need to be provided by the caller.
 */
struct touchpad_report {
    struct touchpad_contact contacts[TOUCHPAD_MAX_CONTACTS];
    uint8_t contact_count;
    bool button; /**< Primary button state (left-click). */
};

/**
 * Initialize the virtual touchpad HID interface.
 *
 * This must be called once at startup, after USB has been initialized.
 *
 * @return 0 on success, negative errno on failure.
 */
int virtual_touchpad_init(void);

/**
 * Send a touchpad report to the host.
 *
 * The scan time is auto-managed: an internal counter is incremented on
 * every call and wraps at 65535.
 *
 * @param report  Pointer to a filled-in touchpad report.
 * @return 0 on success, negative errno on failure.
 */
int virtual_touchpad_send_report(const struct touchpad_report *report);

/**
 * Check whether the virtual touchpad is ready to accept reports.
 *
 * @return true if the HID interface is initialized and the USB host
 *         has configured the device.
 */
bool virtual_touchpad_is_ready(void);
