/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * Trackball Gesture Emulation Engine + ZMK Input Processor
 * =========================================================
 *
 * This file is the heart of the zmk-trackball-gestures module.  It:
 *
 *   1.  Maintains virtual finger contacts on a logical touchpad surface
 *       (0 … TP_MAX in both axes).
 *
 *   2.  Translates accumulated trackball deltas into per-gesture contact
 *       position updates (pinch spread, scroll pan, multi-finger swipe).
 *
 *   3.  Fires periodic HID touchpad reports via a Zephyr timer so the
 *       host sees smooth, continuous gesture motion.
 *
 *   4.  Exposes a ZMK input processor that intercepts REL_X / REL_Y
 *       events when a gesture is active, preventing them from moving
 *       the mouse pointer.
 *
 * Thread-safety note
 * ------------------
 * accumulated_dx / accumulated_dy are written from input-event context
 * (which may be ISR or system workqueue) and read from the timer
 * callback.  We protect them with a lightweight spinlock.
 */

#define DT_DRV_COMPAT zmk_input_processor_gesture

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#include <drivers/input_processor.h>
#include <zmk/trackball_gestures.h>
#include <zmk/virtual_touchpad.h>

LOG_MODULE_REGISTER(trackball_gestures, CONFIG_ZMK_LOG_LEVEL);

/* ================================================================== */
/* Compile-time configuration (Kconfig)                                */
/* ================================================================== */

/** Sensitivity multiplier applied to raw trackball deltas. */
#ifndef CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY
#define CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY 3
#endif

/** Base half-distance between the two pinch contacts (in touchpad units). */
#ifndef CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE
#define CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE 800
#endif

/** Interval between periodic HID reports (milliseconds). */
#ifndef CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS
#define CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS 8
#endif

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */

/** Logical bounds of the virtual trackpad (0 to TP_MAX). */
#define TP_MAX           CONFIG_ZMK_TRACKBALL_GESTURES_TP_LOGICAL_MAX
/** Centre coordinate of the virtual trackpad. */
#define TP_CENTER        (TP_MAX / 2)

/** Spacing between adjacent fingers in multi-finger swipe gestures. */
#define FINGER_SPACING   CONFIG_ZMK_TRACKBALL_GESTURES_FINGER_SPACING

/** Pinch offset lower clamp – keeps contacts well inside the surface. */
#define PINCH_OFFSET_MIN 100
/** Pinch offset upper clamp. */
#define PINCH_OFFSET_MAX (TP_CENTER - 100)


/* ================================================================== */
/* Module state                                                        */
/* ================================================================== */

/** Currently active gesture mode (GESTURE_NONE when idle). */
static enum gesture_mode current_mode = GESTURE_NONE;

/**
 * Accumulated trackball deltas since last timer tick.
 * Protected by @c delta_lock.
 */
static int32_t accumulated_dx;
static int32_t accumulated_dy;

/** True while virtual fingers are "down" on the touchpad. */
static bool gesture_active;

/** Spinlock protecting accumulated_dx / accumulated_dy. */
static struct k_spinlock delta_lock;

/** Periodic report timer. */
static struct k_timer gesture_timer;

/** Work item to defer USB processing to the system workqueue. */
static struct k_work gesture_work;

/* ------------------------------------------------------------------ */
/* Per-gesture running state                                           */
/* ------------------------------------------------------------------ */

enum swipe_locked_axis {
    AXIS_UNLOCKED = 0,
    AXIS_X,
    AXIS_Y
};

/** Dominant axis lock for swipe gestures. */
static enum swipe_locked_axis current_locked_axis = AXIS_UNLOCKED;

/**
 * For PINCH: current half-distance between the two contacts.
 * Starts at CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE.
 */
static int32_t pinch_offset;

/**
 * For SCROLL and SWIPE*: cumulative X/Y translation of the finger
 * cluster, relative to the touchpad centre.
 */
static int32_t cluster_x;
static int32_t cluster_y;

/** Timestamp of the last non-zero trackball movement (ms). */
static uint32_t last_movement_time;

/** True if the virtual fingers are currently lifted due to inactivity. */
static bool fingers_lifted_idle;

/** Base ID used to generate unique contact tracking IDs. */
static uint8_t base_contact_id = 0;

/** State tracking for ensuring exactly one lift report is sent. */
static bool lift_report_sent = false;

/* ================================================================== */
/* Helpers                                                             */
/* ================================================================== */


/**
 * Return the number of contacts for a given gesture mode.
 */
static int contacts_for_mode(enum gesture_mode mode)
{
    int num = 0;
    switch (mode) {
    case GESTURE_SWIPE2: num = 2; break;
    case GESTURE_SWIPE3: num = 3; break;
    case GESTURE_SWIPE4: num = 4; break;
    case GESTURE_SWIPE5: num = 5; break;
    case GESTURE_PINCH2: num = 2; break;
    case GESTURE_PINCH3: num = 3; break;
    case GESTURE_PINCH4: num = 4; break;
    case GESTURE_PINCH5: num = 5; break;
    default:             num = 0; break;
    }
    return MIN(num, CONFIG_ZMK_TRACKBALL_GESTURES_MAX_FINGERS);
}

/* ================================================================== */
/* Report building & sending                                           */
/* ================================================================== */

/**
 * Fill a touchpad_report for a PINCH gesture (2 to 5 contacts).
 *
 * Contacts are placed symmetrically around the touchpad centre.
 * @c pinch_offset controls their separation.
 *
 * @param report        Pointer to the report struct to populate.
 * @param num_contacts  Number of contacts (2 to 5).
 * @param fingers_down  true → active = 1 (contact); false → lift.
 */
static void build_pinch_report(struct touchpad_report *report, int num_contacts, bool fingers_down)
{
    if (!fingers_down) {
        if (lift_report_sent) {
            num_contacts = 0;
        } else {
            lift_report_sent = true;
        }
    } else {
        lift_report_sent = false;
    }

    report->contact_count = (uint8_t)num_contacts;

    int32_t R = pinch_offset;
    struct { int32_t x; int32_t y; } pos[5];

    if (num_contacts == 2) {
        pos[0].x = -R; pos[0].y = 0;
        pos[1].x = R;  pos[1].y = 0;
    } else if (num_contacts == 3) {
        pos[0].x = 0;  pos[0].y = -R;
        pos[1].x = -R; pos[1].y = R;
        pos[2].x = R;  pos[2].y = R;
    } else if (num_contacts == 4) {
        pos[0].x = -R; pos[0].y = -R;
        pos[1].x = R;  pos[1].y = -R;
        pos[2].x = -R; pos[2].y = R;
        pos[3].x = R;  pos[3].y = R;
    } else { /* 5 contacts */
        pos[0].x = 0;  pos[0].y = -R;
        pos[1].x = -R; pos[1].y = 0;
        pos[2].x = R;  pos[2].y = 0;
        pos[3].x = -R; pos[3].y = R;
        pos[4].x = R;  pos[4].y = R;
    }

    for (int i = 0; i < num_contacts; i++) {
        report->contacts[i].active = fingers_down ? 1 : 0;
        report->contacts[i].id = (uint8_t)((base_contact_id + i) & 0x7F);
        report->contacts[i].x = (uint16_t)(TP_CENTER + pos[i].x);
        report->contacts[i].y = (uint16_t)(TP_CENTER + pos[i].y);
    }

    LOG_DBG("pinch%d: offset=%d", num_contacts, pinch_offset);
}

/**
 * Fill a touchpad_report for a multi-finger SWIPE gesture.
 *
 * @p num_contacts contacts are arranged in a horizontal row centred at
 * (TP_CENTER + cluster_x, TP_CENTER + cluster_y), spaced by
 * FINGER_SPACING.
 *
 * @param report        Report struct to populate.
 * @param num_contacts  Number of contacts (3, 4, or 5).
 * @param fingers_down  true for contact, false for lift.
 */
static void build_swipe_report(struct touchpad_report *report,
                               int num_contacts, bool fingers_down)
{
    int32_t cx = TP_CENTER + cluster_x;
    int32_t cy = TP_CENTER + cluster_y;

    if (!fingers_down) {
        if (lift_report_sent) {
            num_contacts = 0;
        } else {
            lift_report_sent = true;
        }
    } else {
        lift_report_sent = false;
    }

    /* The leftmost contact is offset to the left by half the total span. */
    int32_t half_span = (int32_t)(num_contacts - 1) * FINGER_SPACING / 2;

    report->contact_count = (uint8_t)num_contacts;

    for (int i = 0; i < num_contacts; i++) {
        int32_t fx = cx - half_span + i * FINGER_SPACING;
        int32_t fy = cy;

        report->contacts[i].active = fingers_down ? 1 : 0;
        report->contacts[i].id = (uint8_t)((base_contact_id + i) & 0x7F);

        report->contacts[i].x          = (uint16_t)fx;
        report->contacts[i].y          = (uint16_t)fy;
    }

    LOG_DBG("swipe%d: cluster(%d,%d)", num_contacts, cluster_x, cluster_y);
}

/**
 * Build a complete report for the current mode and send it.
 *
 * @param active  true → fingers logically down; false → lift-off.
 */
static void send_gesture_report(bool active)
{
    struct touchpad_report report;
    bool fingers_down = active;

    memset(&report, 0, sizeof(report));

    switch (current_mode) {
    case GESTURE_PINCH2:
    case GESTURE_PINCH3:
    case GESTURE_PINCH4:
    case GESTURE_PINCH5:
        build_pinch_report(&report, contacts_for_mode(current_mode), fingers_down);
        break;
    case GESTURE_SWIPE2:
    case GESTURE_SWIPE3:
    case GESTURE_SWIPE4:
    case GESTURE_SWIPE5:

        build_swipe_report(&report, contacts_for_mode(current_mode), fingers_down);
        break;
    default:
        return;
    }

    virtual_touchpad_send_report(&report);
}

/* ================================================================== */
/* Timer callback                                                      */
/* ================================================================== */

/**
 * Work handler executed on the system workqueue.
 *
 * Reads the accumulated trackball deltas (under spinlock), applies them
 * to the gesture-specific running state, and sends a HID report safely
 * from a thread context.
 */
static void gesture_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    if (!gesture_active) {
        return;
    }

    /* ---- Atomically consume the accumulated deltas ---- */
    int32_t dx, dy;
    {
        k_spinlock_key_t key = k_spin_lock(&delta_lock);
        dx = accumulated_dx;
        dy = accumulated_dy;
        accumulated_dx = 0;
        accumulated_dy = 0;
        k_spin_unlock(&delta_lock, key);
    }

    if (dx != 0 || dy != 0) {
        last_movement_time = k_uptime_get_32();
        if (fingers_lifted_idle) {
            cluster_x = 0;
            cluster_y = 0;
            current_locked_axis = AXIS_UNLOCKED;
            fingers_lifted_idle = false;
        }
    } else {
        if (!fingers_lifted_idle && (k_uptime_get_32() - last_movement_time > CONFIG_ZMK_TRACKBALL_GESTURES_IDLE_TIMEOUT_MS)) {
            fingers_lifted_idle = true;
            
            /* Microsoft PTP spec requires two frames for a clean lift:
             * 1. Tip=0 (fingers lifted)
             * 2. Contact Count=0 (fingers no longer reported)
             */
            send_gesture_report(false);
            send_gesture_report(false);
            return;
        }
    }

    if (fingers_lifted_idle) {
        return;
    }

    /* ---- Apply deltas to gesture-specific state ---- */
    switch (current_mode) {
    case GESTURE_PINCH2:
    case GESTURE_PINCH3:
    case GESTURE_PINCH4:
    case GESTURE_PINCH5:
        /*
         * Trackball Y controls pinch spread:
         *   - Negative dy (up)   → increase offset → zoom in
         *   - Positive dy (down) → decrease offset → zoom out
         * Trackball X is intentionally ignored for pinch.
         */
        pinch_offset -= dy;  /* note: intentional sign flip */
        pinch_offset = CLAMP(pinch_offset, PINCH_OFFSET_MIN, PINCH_OFFSET_MAX);
        break;

    case GESTURE_SWIPE2:
    case GESTURE_SWIPE3:
    case GESTURE_SWIPE4:
    case GESTURE_SWIPE5: {
        int nc = contacts_for_mode(current_mode);

#if IS_ENABLED(CONFIG_ZMK_TRACKBALL_GESTURES_SWIPE_AXIS_LOCK)
        if (current_locked_axis == AXIS_UNLOCKED) {
            int32_t abs_x = abs(cluster_x + dx);
            int32_t abs_y = abs(cluster_y + dy);
            
            /* Wait for 50 logical units of movement to determine dominant axis */
            if (abs_x > 50 || abs_y > 50) {
                if (abs_x > abs_y) {
                    current_locked_axis = AXIS_X;
                    cluster_y = 0;
                    dy = 0;
                } else {
                    current_locked_axis = AXIS_Y;
                    cluster_x = 0;
                    dx = 0;
                }
            }
        } else if (current_locked_axis == AXIS_X) {
            dy = 0;
        } else if (current_locked_axis == AXIS_Y) {
            dx = 0;
        }
#endif

        cluster_x += dx;
        cluster_y += dy;

        /* Clamp to keep the widest contact inside bounds, 
         * preventing integral wind-up against the edges. */
        int32_t half_span = (int32_t)(nc - 1) * FINGER_SPACING / 2;
        cluster_x = CLAMP(cluster_x,
                          -(TP_CENTER - half_span),
                          (TP_MAX - TP_CENTER - half_span));
        cluster_y = CLAMP(cluster_y, -TP_CENTER, (TP_MAX - TP_CENTER));

        break;
    }

    default:
        return;
    }

    /* ---- Send the updated report ---- */
    send_gesture_report(true);
}

/**
 * Periodic timer handler — runs every REPORT_RATE_MS milliseconds.
 *
 * Submits the gesture work item to the system workqueue to defer USB
 * transmission out of the ISR context.
 */
static void gesture_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    if (gesture_active) {
        k_work_submit(&gesture_work);
    }
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void gesture_mode_activate(enum gesture_mode mode)
{
    if (mode == GESTURE_NONE) {
        LOG_WRN("gesture_mode_activate called with GESTURE_NONE — ignoring");
        return;
    }

    if (!virtual_touchpad_is_ready()) {
        LOG_WRN("Virtual touchpad not ready — ignoring gesture activation");
        return;
    }

    /* If already active, deactivate the previous mode first. */
    if (gesture_active) {
        LOG_DBG("Deactivating previous gesture before activating mode %d", mode);
        gesture_mode_deactivate();
    }

    LOG_INF("Activating gesture mode %d", (int)mode);

    current_mode = mode;

    /* Reset accumulated deltas. */
    {
        k_spinlock_key_t key = k_spin_lock(&delta_lock);
        accumulated_dx = 0;
        accumulated_dy = 0;
        k_spin_unlock(&delta_lock, key);
    }

    /* Initialise per-gesture state. */
    pinch_offset = CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE;
    cluster_x = 0;
    cluster_y = 0;
    current_locked_axis = AXIS_UNLOCKED;
    last_movement_time = k_uptime_get_32();
    fingers_lifted_idle = false;
    base_contact_id = (base_contact_id + 10) % 100;

    gesture_active = true;

    /* Send the initial finger-down report. */
    send_gesture_report(true);

    /* Start periodic reporting. */
    k_timer_start(&gesture_timer,
                  K_MSEC(CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS),
                  K_MSEC(CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS));
}

void gesture_mode_deactivate(void)
{
    if (!gesture_active) {
        LOG_DBG("gesture_mode_deactivate called while not active — no-op");
        return;
    }

    LOG_INF("Deactivating gesture mode %d", (int)current_mode);

    /* Stop the periodic timer first to avoid races. */
    k_timer_stop(&gesture_timer);

    /* Microsoft PTP spec requires two frames for a clean lift:
     * 1. Tip=0 (fingers lifted)
     * 2. Contact Count=0 (fingers no longer reported)
     */
    send_gesture_report(false);
    send_gesture_report(false);

    gesture_active = false;
    current_mode = GESTURE_NONE;
}

void gesture_feed_delta(int16_t dx, int16_t dy)
{
    /* Scale by sensitivity. */
    int32_t sdx = (int32_t)dx * CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY;
    int32_t sdy = (int32_t)dy * CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY;

    k_spinlock_key_t key = k_spin_lock(&delta_lock);
    accumulated_dx += sdx;
    accumulated_dy += sdy;
    k_spin_unlock(&delta_lock, key);
}

enum gesture_mode gesture_mode_get(void)
{
    return current_mode;
}

/* ================================================================== */
/* Timer initialisation                                                */
/* ================================================================== */

/**
 * Initialise the gesture timer at boot.  This runs at SYS_INIT level
 * so the timer is ready before any behavior or input processor is used.
 */
static int gesture_engine_init(void)
{
    k_timer_init(&gesture_timer, gesture_timer_handler, NULL);
    k_work_init(&gesture_work, gesture_work_handler);
    LOG_DBG("Gesture engine initialised");
    return 0;
}

SYS_INIT(gesture_engine_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/* ================================================================== */
/* ZMK Input Processor                                                 */
/* ================================================================== */

/**
 * Input processor event handler.
 *
 * When a gesture mode is active, REL_X and REL_Y events from the
 * trackball are consumed (fed into the gesture engine) and not passed
 * further down the processor chain.  All other events, and all events
 * when no gesture is active, are passed through unchanged.
 */
static int gesture_processor_handle_event(
    const struct device *dev,
    struct input_event *event,
    uint32_t param1, uint32_t param2,
    struct zmk_input_processor_state *state)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    /* Pass through when idle. */
    if (current_mode == GESTURE_NONE) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* Only intercept relative axis events. */
    if (event->type == INPUT_EV_REL) {
        if (event->code == INPUT_REL_X) {
            gesture_feed_delta((int16_t)event->value, 0);
            event->type = 0; event->code = 0; event->value = 0; return ZMK_INPUT_PROC_CONTINUE;
        }
        if (event->code == INPUT_REL_Y) {
            gesture_feed_delta(0, (int16_t)event->value);
            event->type = 0; event->code = 0; event->value = 0; return ZMK_INPUT_PROC_CONTINUE;
        }
    }

    /* Everything else passes through (buttons, key events, etc.). */
    return ZMK_INPUT_PROC_CONTINUE;
}

/* ------------------------------------------------------------------ */
/* Driver API and device instantiation                                 */
/* ------------------------------------------------------------------ */

static const struct zmk_input_processor_driver_api gesture_processor_api = {
    .handle_event = gesture_processor_handle_event,
};

#define GESTURE_PROC_INST(n)                                            \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL,                    \
                          POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,          \
                          &gesture_processor_api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_PROC_INST)
