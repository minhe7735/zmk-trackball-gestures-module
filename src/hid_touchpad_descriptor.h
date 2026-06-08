/*
 * Copyright (c) 2024
 * SPDX-License-Identifier: MIT
 *
 * HID Report Descriptor for Windows Precision Touchpad (PTP)
 *
 * This descriptor follows the Microsoft PTP specification for a 5-finger
 * capacitive touchpad with absolute coordinate reporting.
 */

#ifndef HID_TOUCHPAD_DESCRIPTOR_H
#define HID_TOUCHPAD_DESCRIPTOR_H

#include <stdint.h>

/* Report IDs */
#define TOUCHPAD_REPORT_ID                    0x01
#define TOUCHPAD_FEATURE_MAX_COUNT_ID         0x02
#define TOUCHPAD_FEATURE_PTPHQA_ID            0x03
#define TOUCHPAD_FEATURE_CONFIG_ID            0x04
#define TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID   0x05

/*
 * Per-finger logical collection descriptor fragment.
 * Each finger reports: confidence (1 bit), tip switch (1 bit),
 * contact ID (4 bits), padding (2 bits), X (16 bits), Y (16 bits).
 */
#define TOUCHPAD_FINGER_COLLECTION                                             \
    0xA1, 0x02,             /* Collection (Logical)                         */ \
    0x09, 0x42,             /*   Usage (Tip Switch)                         */ \
    0x09, 0x47,             /*   Usage (Confidence)                         */ \
    0x15, 0x00,             /*   Logical Minimum (0)                        */ \
    0x25, 0x01,             /*   Logical Maximum (1)                        */ \
    0x75, 0x01,             /*   Report Size (1)                            */ \
    0x95, 0x02,             /*   Report Count (2)                           */ \
    0x81, 0x02,             /*   Input (Data, Variable, Absolute)           */ \
    0x09, 0x51,             /*   Usage (Contact Identifier)                 */ \
    0x25, 0x0F,             /*   Logical Maximum (15)                       */ \
    0x75, 0x04,             /*   Report Size (4)                            */ \
    0x95, 0x01,             /*   Report Count (1)                           */ \
    0x81, 0x02,             /*   Input (Data, Variable, Absolute)           */ \
    0x75, 0x02,             /*   Report Size (2)                            */ \
    0x95, 0x01,             /*   Report Count (1)                           */ \
    0x81, 0x03,             /*   Input (Constant, Variable, Absolute)       */ \
    0x05, 0x01,             /*   Usage Page (Generic Desktop)               */ \
    0x09, 0x30,             /*   Usage (X)                                  */ \
    0x15, 0x00,             /*   Logical Minimum (0)                        */ \
    0x26, 0xFF, 0x0F,       /*   Logical Maximum (4095)                     */ \
    0x35, 0x00,             /*   Physical Minimum (0)                       */ \
    0x46, 0x90, 0x01,       /*   Physical Maximum (400)                     */ \
    0x55, 0x0E,             /*   Unit Exponent (-2)                         */ \
    0x65, 0x13,             /*   Unit (Inch, English Linear)                */ \
    0x75, 0x10,             /*   Report Size (16)                           */ \
    0x95, 0x01,             /*   Report Count (1)                           */ \
    0x81, 0x02,             /*   Input (Data, Variable, Absolute)           */ \
    0x09, 0x31,             /*   Usage (Y)                                  */ \
    0x46, 0x13, 0x01,       /*   Physical Maximum (275)                     */ \
    0x81, 0x02,             /*   Input (Data, Variable, Absolute)           */ \
    0xC0                    /* End Collection (Logical)                     */

/*
 * Full Windows Precision Touchpad HID Report Descriptor.
 *
 * Structure:
 *   TLC 1 — Touch Pad (Usage Page 0x0D, Usage 0x05)
 *     - 5x finger collections (input report, ID 0x01)
 *     - Scan Time, Contact Count, Button
 *     - Feature: Max Contact Count + Pad Type (ID 0x02)
 *     - Feature: PTPHQA certification blob (ID 0x03)
 *   TLC 2 — Configuration (Usage Page 0x0D, Usage 0x0E)
 *     - Feature: Input Mode (ID 0x04)
 *     - Feature: Function Switch (Surface Switch + Button Switch, ID 0x05)
 */
#define TOUCHPAD_HID_REPORT_DESC                                               \
    /* ============================================================== */       \
    /* TLC 1: Touch Pad                                               */       \
    /* ============================================================== */       \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x05,             /* Usage (Touch Pad)                            */ \
    0xA1, 0x01,             /* Collection (Application)                     */ \
    0x85, TOUCHPAD_REPORT_ID, /* Report ID (0x01)                           */ \
                                                                               \
    /* ----- Finger 1 ----- */                                                 \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 2 ----- */                                                 \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 3 ----- */                                                 \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 4 ----- */                                                 \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 5 ----- */                                                 \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Scan Time (16 bits) ----- */                                      \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x56,             /* Usage (Scan Time)                            */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x26, 0xFF, 0xFF,       /* Logical Maximum (65535)                      */ \
    0x35, 0x00,             /* Physical Minimum (0)                         */ \
    0x46, 0xFF, 0xFF,       /* Physical Maximum (65535)                     */ \
    0x55, 0x0C,             /* Unit Exponent (-4)                           */ \
    0x66, 0x01, 0x10,       /* Unit (Seconds, SI Linear)                    */ \
    0x75, 0x10,             /* Report Size (16)                             */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute)             */ \
                                                                               \
    /* ----- Contact Count (8 bits) ----- */                                   \
    0x09, 0x54,             /* Usage (Contact Count)                        */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x25, 0x7F,             /* Logical Maximum (127)                        */ \
    0x35, 0x00,             /* Physical Minimum (0)                         */ \
    0x45, 0x00,             /* Physical Maximum (0)  — reset                */ \
    0x55, 0x00,             /* Unit Exponent (0)     — reset                */ \
    0x65, 0x00,             /* Unit (None)           — reset                */ \
    0x75, 0x08,             /* Report Size (8)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute)             */ \
                                                                               \
    /* ----- Button (1 bit + 7 padding) ----- */                               \
    0x05, 0x09,             /* Usage Page (Button)                          */ \
    0x09, 0x01,             /* Usage (Button 1)                             */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x25, 0x01,             /* Logical Maximum (1)                          */ \
    0x75, 0x01,             /* Report Size (1)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute)             */ \
    0x75, 0x07,             /* Report Size (7)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0x81, 0x03,             /* Input (Constant, Variable, Absolute)         */ \
                                                                               \
    /* ----- Feature: Max Contact Count + Pad Type (ID 0x02) ----- */          \
    0x85, TOUCHPAD_FEATURE_MAX_COUNT_ID, /* Report ID (0x02)                */ \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x55,             /* Usage (Contact Count Maximum)                */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x25, 0x0F,             /* Logical Maximum (15)                         */ \
    0x75, 0x04,             /* Report Size (4)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0xB1, 0x02,             /* Feature (Data, Variable, Absolute)           */ \
    0x09, 0x59,             /* Usage (Pad Type)                             */ \
    0x25, 0x0F,             /* Logical Maximum (15)                         */ \
    0x75, 0x04,             /* Report Size (4)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0xB1, 0x02,             /* Feature (Data, Variable, Absolute)           */ \
                                                                               \
    /* ----- Feature: PTPHQA Certification Blob (ID 0x03) ----- */             \
    0x85, TOUCHPAD_FEATURE_PTPHQA_ID, /* Report ID (0x03)                   */ \
    0x06, 0x00, 0xFF,       /* Usage Page (Vendor Defined 0xFF00)           */ \
    0x09, 0xC5,             /* Usage (Vendor Usage 0xC5)                    */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x26, 0xFF, 0x00,       /* Logical Maximum (255)                        */ \
    0x75, 0x08,             /* Report Size (8)                              */ \
    0x96, 0x00, 0x01,       /* Report Count (256)                           */ \
    0xB1, 0x02,             /* Feature (Data, Variable, Absolute)           */ \
                                                                               \
    0xC0,                   /* End Collection (Application — TLC 1)         */ \
                                                                               \
    /* ============================================================== */       \
    /* TLC 2: Configuration                                           */       \
    /* ============================================================== */       \
    0x05, 0x0D,             /* Usage Page (Digitizer)                       */ \
    0x09, 0x0E,             /* Usage (Configuration)                        */ \
    0xA1, 0x01,             /* Collection (Application)                     */ \
                                                                               \
    /* ----- Feature: Input Mode (ID 0x04) ----- */                            \
    0x85, TOUCHPAD_FEATURE_CONFIG_ID, /* Report ID (0x04)                   */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    0xA1, 0x02,             /* Collection (Logical)                         */ \
    0x09, 0x52,             /* Usage (Input Mode)                           */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x25, 0x0A,             /* Logical Maximum (10)                         */ \
    0x75, 0x08,             /* Report Size (8)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0xB1, 0x02,             /* Feature (Data, Variable, Absolute)           */ \
    0xC0,                   /* End Collection (Logical)                     */ \
                                                                               \
    /* ----- Feature: Function Switch (ID 0x05) ----- */                       \
    0x85, TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID, /* Report ID (0x05)          */ \
    0x09, 0x22,             /* Usage (Finger)                               */ \
    0xA1, 0x02,             /* Collection (Logical)                         */ \
    0x09, 0x57,             /* Usage (Surface Switch)                       */ \
    0x09, 0x58,             /* Usage (Button Switch)                        */ \
    0x15, 0x00,             /* Logical Minimum (0)                          */ \
    0x25, 0x01,             /* Logical Maximum (1)                          */ \
    0x75, 0x01,             /* Report Size (1)                              */ \
    0x95, 0x02,             /* Report Count (2)                             */ \
    0xB1, 0x02,             /* Feature (Data, Variable, Absolute)           */ \
    0x75, 0x06,             /* Report Size (6)                              */ \
    0x95, 0x01,             /* Report Count (1)                             */ \
    0xB1, 0x03,             /* Feature (Constant, Variable, Absolute)       */ \
    0xC0,                   /* End Collection (Logical)                     */ \
                                                                               \
    0xC0                    /* End Collection (Application — TLC 2)         */

/* ------------------------------------------------------------------ */
/* Report data structures (packed, matching the descriptor layout)     */
/* ------------------------------------------------------------------ */

/**
 * Per-finger contact in the input report.
 *
 * Bit layout of `flags`:
 *   Bit 0    : Tip Switch  (1 = finger touching)
 *   Bit 1    : Confidence  (1 = valid contact)
 *   Bits 2-5 : Contact Identifier (0-15)
 *   Bits 6-7 : Padding (0)
 */
struct touchpad_contact_report {
    uint8_t flags;
    uint16_t x;
    uint16_t y;
} __packed;

/**
 * Full touchpad input report (Report ID 0x01).
 *
 * Total size: 1 (report ID) + 5×5 (contacts) + 2 (scan time)
 *             + 1 (contact count) + 1 (buttons) = 30 bytes.
 */
struct touchpad_input_report {
    uint8_t report_id;
    struct touchpad_contact_report contacts[5];
    uint16_t scan_time;
    uint8_t contact_count;
    uint8_t buttons; /* Bit 0 = Button 1, bits 1-7 = padding */
} __packed;

/**
 * Feature report: Maximum Contact Count + Pad Type (Report ID 0x02).
 *
 * Bit layout of `max_count_and_pad_type`:
 *   Bits 0-3 : Maximum Contact Count (5)
 *   Bits 4-7 : Pad Type (0 = depressible click-pad)
 */
struct touchpad_feature_max_count {
    uint8_t report_id;
    uint8_t max_count_and_pad_type;
} __packed;

#endif /* HID_TOUCHPAD_DESCRIPTOR_H */
