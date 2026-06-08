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

/** Monotonically increasing scan-time counter (wraps at UINT16_MAX). */
static uint16_t scan_time_counter;

/** True once the HID interface has been successfully initialized. */
static bool touchpad_ready;

/** Current input mode set by the host (3 = Touchpad). */
static uint8_t current_input_mode = 3;

/** Surface switch state (1 = on). */
static uint8_t surface_switch = 1;

/** Button switch state (1 = on). */
static uint8_t button_switch = 1;

/* ------------------------------------------------------------------ */
/* Feature report data                                                 */
/* ------------------------------------------------------------------ */

/** Max Contact Count = 5, Pad Type = 0 (depressible click-pad). */
static const struct touchpad_feature_max_count feature_max_count = {
    .report_id = TOUCHPAD_FEATURE_MAX_COUNT_ID,
    .max_count_and_pad_type = 0x05, /* lower nibble = 5, upper = 0 */
};

/**
 * PTPHQA certification blob — report ID + 256 bytes of zeros.
 * The response MUST start with the report ID byte so the kernel's
 * hid-multitouch driver can validate buf[0] == report_id.
 */
static uint8_t ptphqa_response[257] = { TOUCHPAD_FEATURE_PTPHQA_ID };

/** Input mode response buffer: [report_id, mode_byte]. */
static uint8_t input_mode_response[2] = { TOUCHPAD_FEATURE_CONFIG_ID, 3 };

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
        /* Return full struct: { report_id=0x02, max_count_and_pad_type=0x05 } */
        *data = (uint8_t *)&feature_max_count;
        *len = sizeof(feature_max_count);
        LOG_DBG("GET_REPORT: max contact count (5), len=%d", *len);
        break;

    case TOUCHPAD_FEATURE_PTPHQA_ID:
        /* Return report_id + 256 zero bytes */
        *data = ptphqa_response;
        *len = sizeof(ptphqa_response);
        LOG_DBG("GET_REPORT: PTPHQA blob (%u bytes)", sizeof(ptphqa_response));
        break;

    case TOUCHPAD_FEATURE_CONFIG_ID:
        /* Return { report_id=0x04, input_mode } */
        input_mode_response[0] = TOUCHPAD_FEATURE_CONFIG_ID;
        input_mode_response[1] = current_input_mode;
        *data = input_mode_response;
        *len = sizeof(input_mode_response);
        LOG_DBG("GET_REPORT: input mode (%u)", current_input_mode);
        break;

    case TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID: {
        /* Return { report_id=0x05, switch_bits } */
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
    case TOUCHPAD_FEATURE_CONFIG_ID:
        current_input_mode = (*data)[0];
        LOG_INF("SET_REPORT: input mode set to %u", current_input_mode);
        break;

    case TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID:
        surface_switch = (*data)[0] & 0x01;
        button_switch = ((*data)[0] >> 1) & 0x01;
        LOG_INF("SET_REPORT: surface_switch=%u, button_switch=%u",
                surface_switch, button_switch);
        break;

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

    scan_time_counter = 0;
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

    if (!touchpad_ready || !hid_dev) {
        LOG_WRN("Touchpad not ready — dropping report");
        return -ENODEV;
    }

    /* Build the packed HID input report.
     * Must be static because hid_int_ep_write transmits asynchronously
     * and will read this buffer after the function returns! */
    static struct touchpad_input_report hid_report;
    memset(&hid_report, 0, sizeof(hid_report));

    hid_report.report_id = TOUCHPAD_REPORT_ID;

    for (int i = 0; i < TOUCHPAD_MAX_CONTACTS; i++) {
        const struct touchpad_contact *src = &report->contacts[i];
        struct touchpad_contact_report *dst = &hid_report.contacts[i];

        /*
         * Pack the flags byte:
         *   Bit 0    : Tip Switch
         *   Bit 1    : Confidence (always 1 for valid contacts)
         *   Bits 2-5 : Contact Identifier
         *   Bits 6-7 : Padding (0)
         */
        dst->flags = (uint8_t)(
            ((src->active ? 1U : 0U) << 0)   /* tip switch  */
          | ((src->active ? 1U : 0U) << 1)   /* confidence  */
          | ((i & 0x0FU) << 2)               /* strictly unique contact ID */
        );

        dst->x = src->x;
        dst->y = src->y;
    }

    /* Scan time: real elapsed time in 100µs units (unit exponent -4).
     * Using actual uptime ensures libinput computes correct velocities. */
    uint32_t now_ms = k_uptime_get_32();
    hid_report.scan_time = (uint16_t)((now_ms * 10) & 0xFFFF);

    hid_report.contact_count = report->contact_count;
    hid_report.buttons = report->button ? 0x01 : 0x00;

    /* Transmit on the interrupt-IN endpoint.  Skip the report_id byte
     * if the USB HID stack adds it automatically; Zephyr's stack
     * expects the full buffer including the report ID.                */
    int ret = hid_int_ep_write(hid_dev,
                               (const uint8_t *)&hid_report,
                               sizeof(hid_report),
                               NULL);
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
