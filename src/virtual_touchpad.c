/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * Virtual Touchpad — implementation
 *
 * Registers a USB HID device that presents itself as a Windows Precision
 * Touchpad (PTP).  Translates high-level touchpad_report structures into
 * the packed HID input reports expected by the host.
 */

#include <zmk/virtual_touchpad.h>
#include "hid_touchpad_descriptor.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_USB)
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#endif

#include <string.h>

LOG_MODULE_REGISTER(virtual_touchpad, CONFIG_ZMK_LOG_LEVEL);

#if IS_ENABLED(CONFIG_ZMK_USB)

/* ------------------------------------------------------------------ */
/* HID report descriptor instantiation                                 */
/* ------------------------------------------------------------------ */

static const uint8_t touchpad_hid_report_desc[] = {
    TOUCHPAD_HID_REPORT_DESC
};

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

/** Handle to the HID device obtained from device_get_binding(). */
static const struct device *hid_dev;

/** True once the HID interface has been successfully initialized. */
static bool touchpad_ready;

/** Spinlock for thread-safe report sending. */
static struct k_spinlock send_lock;

/** Current input mode set by the host (0 = Mouse, 3 = Touchpad). */
static uint8_t current_input_mode = 0;

/** Current device index set by the host. */
static uint8_t current_device_index = 0;

/** Surface switch state (1 = on). */
static uint8_t surface_switch = 1;

/** Button switch state (1 = on). */
static uint8_t button_switch = 1;

/* ------------------------------------------------------------------ */
/* Feature report data                                                 */
/* ------------------------------------------------------------------ */

/** Max Contact Count, Pad Type = 0 (depressible click-pad). */
static const struct touchpad_feature_max_count feature_max_count = {
    .report_id = TOUCHPAD_FEATURE_MAX_COUNT_ID,
    .max_count = CONFIG_ZMK_TRACKBALL_GESTURES_MAX_FINGERS,
    .pad_type  = 0x00,
};

/**
 * PTPHQA certification blob — report ID + 256 bytes of zeros.
 * The response MUST start with the report ID byte so the kernel's
 * hid-multitouch driver can validate buf[0] == report_id.
 */
static uint8_t ptphqa_response[257] = {
    TOUCHPAD_FEATURE_PTPHQA_ID,
    0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, 0x0d, 0xbe, 0x57, 0x3c, 0xb6, 0x70, 0x09, 0x88, 0x07,
    0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6, 0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2, 0x2e, 0x84,
    0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43, 0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc, 0x47, 0x70, 0x1b,
    0x59, 0x6f, 0x74, 0x43, 0xc4, 0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1, 0x71, 0xc7, 0x95, 0x0e, 0x31,
    0x55, 0x21, 0xd3, 0xb5, 0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19, 0x3e, 0xb3, 0xaf, 0x75, 0x81,
    0x9d, 0x53, 0xb9, 0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c, 0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d,
    0xa7, 0x26, 0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89, 0xd7, 0x3b, 0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0,
    0x2a, 0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8, 0xc0, 0x8f, 0x33, 0x13, 0x03, 0xf1, 0xd3, 0xc1, 0x0b,
    0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7, 0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, 0xe8, 0x8a,
    0x56, 0xf0, 0x8c, 0xaa, 0xfa, 0x35, 0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc, 0x2b, 0x53, 0x5c,
    0x69, 0x52, 0xd5, 0xc8, 0x73, 0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff, 0x05, 0xd8, 0x2b, 0x79,
    0x9a, 0xe2, 0x34, 0x60, 0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc, 0x80, 0xe3, 0x0f, 0xbd, 0x65,
    0x20, 0x08, 0x13, 0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7, 0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe,
    0x31, 0x48, 0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a, 0x89, 0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a,
    0xe4, 0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, 0x24, 0x8b, 0xc4, 0x43, 0xa5, 0xe5, 0x24, 0xc2
};

/** Input mode response buffer: [report_id, mode_byte, index_byte]. */
static struct touchpad_feature_input_mode input_mode_response = {
    .report_id    = TOUCHPAD_FEATURE_CONFIG_ID,
    .input_mode   = 3,
    .device_index = 0,
};

/** Function switch response buffer: [report_id, switch_byte]. */
static uint8_t fn_switch_response[2] = { TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID, 0x03 };

/* ------------------------------------------------------------------ */
/* HID class callbacks                                                 */
/* ------------------------------------------------------------------ */

/**
 * GET_REPORT callback.
 *
 * The host may request feature reports to discover device capabilities
 * (max contacts, pad type), certification status, current input mode,
 * or function-switch states.
 *
 * CRITICAL: Every response MUST begin with the Report ID byte.  The
 * Linux hid-multitouch driver validates buf[0] == report_id and falls
 * back to degraded single-touch mode if the check fails.
 */
static int touchpad_get_report(const struct device *dev,
                               struct usb_setup_packet *setup,
                               int32_t *len, uint8_t **data)
{
    uint8_t report_id = (uint8_t)(setup->wValue & 0xFF);
    uint8_t report_type = (uint8_t)(setup->wValue >> 8);

    /* We only handle Feature reports (type 3). */
    if (report_type != 3) {
        LOG_WRN("Unexpected GET_REPORT type %u for report %u",
                report_type, report_id);
        return -ENOTSUP;
    }

    switch (report_id) {
    case TOUCHPAD_FEATURE_MAX_COUNT_ID:
        *data = (uint8_t *)&feature_max_count;
        *len = sizeof(feature_max_count);
        LOG_DBG("GET_REPORT: max contact count (%d), len=%d", feature_max_count.max_count, *len);
        break;

    case TOUCHPAD_FEATURE_PTPHQA_ID:
        *data = ptphqa_response;
        *len = sizeof(ptphqa_response);
        LOG_DBG("GET_REPORT: PTPHQA blob (%u bytes)", sizeof(ptphqa_response));
        break;

    case TOUCHPAD_FEATURE_CONFIG_ID:
        input_mode_response.input_mode = current_input_mode;
        input_mode_response.device_index = current_device_index;
        *data = (uint8_t *)&input_mode_response;
        *len = sizeof(input_mode_response);
        LOG_DBG("GET_REPORT: input mode (%u)", current_input_mode);
        break;

    case TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID: {
        fn_switch_response[0] = TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID;
        fn_switch_response[1] = (surface_switch & 0x01)
                              | ((button_switch & 0x01) << 1);
        *data = fn_switch_response;
        *len = sizeof(fn_switch_response);
        LOG_DBG("GET_REPORT: function switch (0x%02X)", fn_switch_response[1]);
        break;
    }

    default:
        LOG_WRN("GET_REPORT: unknown report ID %u", report_id);
        return -ENOTSUP;
    }

    return 0;
}

/**
 * SET_REPORT callback.
 *
 * The host sends SET_REPORT to change the input mode (e.g. mouse →
 * touchpad) or to toggle the surface / button switches.
 */
static void touchpad_set_report(const struct device *dev,
                                struct usb_setup_packet *setup,
                                int32_t *len, uint8_t **data)
{
    uint8_t report_id = (uint8_t)(setup->wValue & 0xFF);

    if (*len < 1 || !*data) {
        LOG_WRN("SET_REPORT: empty payload for report %u", report_id);
        return;
    }

    switch (report_id) {
    case TOUCHPAD_FEATURE_CONFIG_ID: {
        int offset = ((*len > 0 && (*data)[0] == report_id) ? 1 : 0);
        if (*len > offset) {
            current_input_mode = (*data)[offset];
        }
        if (*len > offset + 1) {
            current_device_index = (*data)[offset + 1];
        }
        LOG_INF("SET_REPORT: input mode set to %u", current_input_mode);
        break;
    }

    case TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID: {
        int offset = ((*len > 0 && (*data)[0] == report_id) ? 1 : 0);
        if (*len > offset) {
            surface_switch = (*data)[offset] & 0x01;
            button_switch = ((*data)[offset] >> 1) & 0x01;
        }
        LOG_INF("SET_REPORT: surface_switch=%u, button_switch=%u",
                surface_switch, button_switch);
        break;
    }

    default:
        LOG_WRN("SET_REPORT: unhandled report ID %u", report_id);
        break;
    }
}

/**
 * Interrupt-IN endpoint ready callback.
 *
 * Called by the USB stack after a report has been successfully sent
 * on the interrupt-IN pipe.  Currently unused.
 */
static void touchpad_int_in_ready(const struct device *dev)
{
    /* Nothing to do — we send reports on demand. */
}

/* HID operations table. */
static const struct hid_ops touchpad_hid_ops = {
    .get_report   = touchpad_get_report,
    .set_report   = touchpad_set_report,
    .int_in_ready = touchpad_int_in_ready,
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int virtual_touchpad_init(void)
{
    if (touchpad_ready) {
        LOG_WRN("Virtual touchpad already initialized");
        return 0;
    }

    /*
     * Obtain the second HID device instance.  ZMK's keyboard / mouse
     * occupies HID_0; we use HID_1 for the touchpad.
     */
    hid_dev = device_get_binding("HID_1");
    if (!hid_dev) {
        LOG_ERR("Failed to get HID_1 device binding. Touchpad disabled.");
        return 0; // Return 0 so we don't halt Zephyr boot!
    }

    /* Register our report descriptor and callbacks. */
    usb_hid_register_device(hid_dev,
                            touchpad_hid_report_desc,
                            sizeof(touchpad_hid_report_desc),
                            &touchpad_hid_ops);

    /* Force report protocol (as opposed to boot protocol). */
    usb_hid_set_proto_code(hid_dev, 0);

    int ret = usb_hid_init(hid_dev);
    if (ret) {
        LOG_ERR("usb_hid_init failed: %d", ret);
        return ret;
    }

    touchpad_ready = true;

    LOG_INF("Virtual touchpad initialized (descriptor %zu bytes)",
            sizeof(touchpad_hid_report_desc));
    return 0;
}

int virtual_touchpad_send_report(const struct touchpad_report *report)
{
    if (!report) {
        return -EINVAL;
    }

    if (!virtual_touchpad_is_ready()) {
        LOG_WRN("Touchpad not ready — dropping report");
        return -ENODEV;
    }

    k_spinlock_key_t key = k_spin_lock(&send_lock);

    /* Use a simple ring buffer of reports to avoid clobbering a buffer
     * that is currently being transmitted asynchronously by the USB stack. */
    static struct touchpad_input_report hid_reports[4];
    static uint8_t report_idx = 0;
    
    struct touchpad_input_report *hid_report = &hid_reports[report_idx];
    report_idx = (report_idx + 1) % 4;
    
    memset(hid_report, 0, sizeof(*hid_report));

    hid_report->report_id = TOUCHPAD_REPORT_ID;

    /* The HID descriptor statically defines 5 finger collections.
     * We must populate all 5 slots with strictly unique contact IDs,
     * even if the module is configured to use fewer contacts. */
    for (int i = 0; i < 5; i++) {
        struct touchpad_contact_report *dst = &hid_report->contacts[i];

        if (i < report->contact_count) {
            const struct touchpad_contact *src = &report->contacts[i];
            /*
             * Pack the flags byte:
             *   Bit 0 : Confidence (1 for valid/lifting contacts)
             *   Bit 1 : Tip Switch
             */
            dst->flags = (uint8_t)(
                (1U << 0)                        /* confidence */
              | ((src->active ? 1U : 0U) << 1)   /* tip switch */
            );
            dst->x = src->x;
            dst->y = src->y;
            dst->contact_id = src->id;           /* Use uniquely generated ID from gesture logic */
        } else {
            /* Inactive contact to pad the report */
            dst->flags = 0;                      /* confidence = 0, tip switch = 0 */
            dst->x = 0;
            dst->y = 0;
            dst->contact_id = i;                 /* Ignored since confidence = 0 */
        }
    }

    /* Scan time: real elapsed time in 100µs units (unit exponent -4).
     * Using actual uptime ensures libinput computes correct velocities.
     * We strictly enforce monotonicity so rapid down/up events don't
     * get the exact same scan time, which confuses some OS drivers. */
    static uint32_t last_scan_time = 0;
    uint32_t current_scan = k_uptime_get_32() * 10;
    if (current_scan <= last_scan_time) {
        current_scan = last_scan_time + 10; /* force +1ms */
    }
    last_scan_time = current_scan;
    hid_report->scan_time = (uint16_t)(current_scan & 0xFFFF);

    hid_report->contact_count = report->contact_count;
    hid_report->buttons = report->button ? 0x01 : 0x00;

    /* Transmit on the interrupt-IN endpoint.  Skip the report_id byte
     * if the USB HID stack adds it automatically; Zephyr's stack
     * expects the full buffer including the report ID.                */
    int ret = hid_int_ep_write(hid_dev,
                               (const uint8_t *)hid_report,
                               sizeof(*hid_report),
                               NULL);

    k_spin_unlock(&send_lock, key);

    if (ret) {
        LOG_WRN("hid_int_ep_write failed: %d", ret);
        return ret;
    }

    return 0;
}

bool virtual_touchpad_is_ready(void)
{
    return touchpad_ready && (hid_dev != NULL);
}

SYS_INIT(virtual_touchpad_init, APPLICATION, 49);

#else // !IS_ENABLED(CONFIG_ZMK_USB)

int virtual_touchpad_init(void)
{
    return 0;
}

int virtual_touchpad_send_report(const struct touchpad_report *report)
{
    return 0;
}

bool virtual_touchpad_is_ready(void)
{
    return true;
}

#endif // IS_ENABLED(CONFIG_ZMK_USB)
