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
#define TOUCHPAD_MOUSE_REPORT_ID              0x06

/* Byte extraction macros for Kconfig parameters */
#define HID_LSB_16(x) ((x) & 0xFF)
#define HID_MSB_16(x) (((x) >> 8) & 0xFF)

/*
 * Per-finger logical collection descriptor fragment.
 * Each finger reports: confidence (1 bit), tip switch (1 bit),
 * padding (6 bits), contact ID (8 bits), X (16 bits), Y (16 bits).
 * To match the MS Sample Descriptor format.
 */
#define TOUCHPAD_FINGER_COLLECTION                                             \
    0xA1, 0x02,             /* COLLECTION (Logical)                         */ \
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0)                        */ \
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1)                        */ \
    0x35, 0x00,             /*   PHYSICAL_MINIMUM (0)                       */ \
    0x45, 0x00,             /*   PHYSICAL_MAXIMUM (0)                       */ \
    0x55, 0x00,             /*   UNIT_EXPONENT (0)                          */ \
    0x65, 0x00,             /*   UNIT (None)                                */ \
    0x09, 0x47,             /*   USAGE (Confidence)                         */ \
    0x09, 0x42,             /*   USAGE (Tip Switch)                         */ \
    0x95, 0x02,             /*   REPORT_COUNT (2)                           */ \
    0x75, 0x01,             /*   REPORT_SIZE (1)                            */ \
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)                       */ \
    0x95, 0x01,             /*   REPORT_COUNT (1)                           */ \
    0x75, 0x06,             /*   REPORT_SIZE (6)                            */ \
    0x81, 0x03,             /*   INPUT (Cnst,Var,Abs)                       */ \
    0x75, 0x08,             /*   REPORT_SIZE (8)                            */ \
    0x25, 0x0F,             /*   LOGICAL_MAXIMUM (15)                       */ \
    0x09, 0x51,             /*   USAGE (Contact Identifier)                 */ \
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)                       */ \
    0x05, 0x01,             /*   USAGE_PAGE (Generic Desktop)               */ \
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0)                        */ \
    0x26, HID_LSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_LOGICAL_MAX), HID_MSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_LOGICAL_MAX), /*   LOGICAL_MAXIMUM */ \
    0x75, 0x10,             /*   REPORT_SIZE (16)                           */ \
    0x55, 0x0E,             /*   UNIT_EXPONENT (-2)                         */ \
    0x65, 0x13,             /*   UNIT (Inch, English Linear)                */ \
    0x09, 0x30,             /*   USAGE (X)                                  */ \
    0x35, 0x00,             /*   PHYSICAL_MINIMUM (0)                       */ \
    0x46, HID_LSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_PHYSICAL_MAX), HID_MSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_PHYSICAL_MAX), /*   PHYSICAL_MAXIMUM */ \
    0x95, 0x01,             /*   REPORT_COUNT (1)                           */ \
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)                       */ \
    0x46, HID_LSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_PHYSICAL_MAX), HID_MSB_16(CONFIG_ZMK_TRACKBALL_GESTURES_TP_PHYSICAL_MAX), /*   PHYSICAL_MAXIMUM */ \
    0x09, 0x31,             /*   USAGE (Y)                                  */ \
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)                       */ \
    0xC0                    /* END_COLLECTION                               */

/*
 * Full Windows Precision Touchpad HID Report Descriptor.
 * Matches Microsoft PTP Sample Descriptor structure carefully.
 */
#define TOUCHPAD_HID_REPORT_DESC                                               \
    /* ============================================================== */       \
    /* TLC 1: Touch Pad                                               */       \
    /* ============================================================== */       \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x05,             /* USAGE (Touch Pad)                            */ \
    0xA1, 0x01,             /* COLLECTION (Application)                     */ \
    0x85, TOUCHPAD_REPORT_ID, /* REPORT_ID (0x01)                           */ \
                                                                               \
    /* ----- Finger 1 ----- */                                                 \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 2 ----- */                                                 \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 3 ----- */                                                 \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 4 ----- */                                                 \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Finger 5 ----- */                                                 \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    TOUCHPAD_FINGER_COLLECTION,                                                \
                                                                               \
    /* ----- Scan Time (16 bits) ----- */                                      \
    0x55, 0x0C,             /* UNIT_EXPONENT (-4)                           */ \
    0x66, 0x01, 0x10,       /* UNIT (Seconds)                               */ \
    0x47, 0xFF, 0xFF, 0x00, 0x00, /* PHYSICAL_MAXIMUM (65535)               */ \
    0x27, 0xFF, 0xFF, 0x00, 0x00, /* LOGICAL_MAXIMUM (65535)                */ \
    0x75, 0x10,             /* REPORT_SIZE (16)                             */ \
    0x95, 0x01,             /* REPORT_COUNT (1)                             */ \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x56,             /* USAGE (Scan Time)                            */ \
    0x81, 0x02,             /* INPUT (Data,Var,Abs)                         */ \
                                                                               \
    /* ----- Contact Count (8 bits) ----- */                                   \
    0x09, 0x54,             /* USAGE (Contact Count)                        */ \
    0x25, 0x7F,             /* LOGICAL_MAXIMUM (127)                        */ \
    0x35, 0x00,             /* PHYSICAL_MINIMUM (0)                         */ \
    0x45, 0x00,             /* PHYSICAL_MAXIMUM (0)                         */ \
    0x55, 0x00,             /* UNIT_EXPONENT (0)                            */ \
    0x65, 0x00,             /* UNIT (None)                                  */ \
    0x95, 0x01,             /* REPORT_COUNT (1)                             */ \
    0x75, 0x08,             /* REPORT_SIZE (8)                              */ \
    0x81, 0x02,             /* INPUT (Data,Var,Abs)                         */ \
                                                                               \
    /* ----- Button (1 bit + 7 padding) ----- */                               \
    0x05, 0x09,             /* USAGE_PAGE (Button)                          */ \
    0x09, 0x01,             /* USAGE (Button 1)                             */ \
    0x25, 0x01,             /* LOGICAL_MAXIMUM (1)                          */ \
    0x75, 0x01,             /* REPORT_SIZE (1)                              */ \
    0x95, 0x01,             /* REPORT_COUNT (1)                             */ \
    0x81, 0x02,             /* INPUT (Data,Var,Abs)                         */ \
    0x95, 0x07,             /* REPORT_COUNT (7)                             */ \
    0x81, 0x03,             /* INPUT (Cnst,Var,Abs)                         */ \
    /* ----- Feature: Max Contact Count + Pad Type ----- */                    \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x85, TOUCHPAD_FEATURE_MAX_COUNT_ID, /* REPORT_ID                       */ \
    0x09, 0x55,             /* USAGE (Contact Count Maximum)                */ \
    0x09, 0x59,             /* USAGE (Pad Type)                             */ \
    0x75, 0x04,             /* REPORT_SIZE (4)                              */ \
    0x95, 0x02,             /* REPORT_COUNT (2)                             */ \
    0x25, 0x0F,             /* LOGICAL_MAXIMUM (15)                         */ \
    0xB1, 0x02,             /* FEATURE (Data,Var,Abs)                       */ \
                                                                               \
    /* ============================================================== */       \
    /* PTPHQA Certification Blob (Still inside TLC 1)                 */       \
    /* ============================================================== */       \
    0x06, 0x00, 0xFF,       /* USAGE_PAGE (Vendor Defined)                  */ \
    0x85, TOUCHPAD_FEATURE_PTPHQA_ID, /* REPORT_ID                          */ \
    0x09, 0xC5,             /* USAGE (Vendor Usage 0xC5)                    */ \
    0x15, 0x00,             /* LOGICAL_MINIMUM (0)                          */ \
    0x26, 0xFF, 0x00,       /* LOGICAL_MAXIMUM (255)                        */ \
    0x75, 0x08,             /* REPORT_SIZE (8)                              */ \
    0x96, 0x00, 0x01,       /* REPORT_COUNT (256)                           */ \
    0xB1, 0x02,             /* FEATURE (Data,Var,Abs)                       */ \
    0xC0,                   /* END_COLLECTION (TLC 1)                       */ \
                                                                               \
    /* ============================================================== */       \
    /* TLC 2: Device Configuration (Features)                         */       \
    /* ============================================================== */       \
    0x05, 0x0D,             /* USAGE_PAGE (Digitizer)                       */ \
    0x09, 0x0E,             /* USAGE (Device Configuration)                 */ \
    0xA1, 0x01,             /* COLLECTION (Application)                     */ \
                                                                               \
    /* ----- Feature: Input Mode ----- */                                      \
    0x85, TOUCHPAD_FEATURE_CONFIG_ID,    /* REPORT_ID                       */ \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    0xA1, 0x02,             /* COLLECTION (Logical)                         */ \
    0x09, 0x52,             /* USAGE (Input Mode)                           */ \
    0x09, 0x53,             /* USAGE (Device Index)                         */ \
    0x15, 0x00,             /* LOGICAL_MINIMUM (0)                          */ \
    0x25, 0x0A,             /* LOGICAL_MAXIMUM (10)                         */ \
    0x75, 0x08,             /* REPORT_SIZE (8)                              */ \
    0x95, 0x02,             /* REPORT_COUNT (2)                             */ \
    0xB1, 0x02,             /* FEATURE (Data,Var,Abs)                       */ \
    0xC0,                   /* END_COLLECTION                               */ \
                                                                               \
    /* ----- Feature: Function Switch ----- */                                 \
    0x09, 0x22,             /* USAGE (Finger)                               */ \
    0xA1, 0x00,             /* COLLECTION (Physical)                        */ \
    0x85, TOUCHPAD_FEATURE_FUNCTION_SWITCH_ID, /* REPORT_ID                 */ \
    0x09, 0x57,             /* USAGE (Surface Switch)                       */ \
    0x09, 0x58,             /* USAGE (Button Switch)                        */ \
    0x75, 0x01,             /* REPORT_SIZE (1)                              */ \
    0x95, 0x02,             /* REPORT_COUNT (2)                             */ \
    0x25, 0x01,             /* LOGICAL_MAXIMUM (1)                          */ \
    0xB1, 0x02,             /* FEATURE (Data,Var,Abs)                       */ \
    0x95, 0x06,             /* REPORT_COUNT (6)                             */ \
    0xB1, 0x03,             /* FEATURE (Cnst,Var,Abs)                       */ \
    0xC0,                   /* END_COLLECTION                               */ \
    0xC0,                   /* END_COLLECTION (TLC 2)                       */ \
                                                                               \
    /* ============================================================== */       \
    /* TLC 4: Mouse Collection (Required for Default Compatibility)   */       \
    /* ============================================================== */       \
    0x05, 0x01,             /* USAGE_PAGE (Generic Desktop)                 */ \
    0x09, 0x02,             /* USAGE (Mouse)                                */ \
    0xA1, 0x01,             /* COLLECTION (Application)                     */ \
    0x85, TOUCHPAD_MOUSE_REPORT_ID, /* REPORT_ID (0x06)                     */ \
    0x09, 0x01,             /* USAGE (Pointer)                              */ \
    0xA1, 0x00,             /* COLLECTION (Physical)                        */ \
    /* ----- Buttons ----- */                                                  \
    0x05, 0x09,             /*   USAGE_PAGE (Button)                        */ \
    0x19, 0x01,             /*   USAGE_MINIMUM (Button 1)                   */ \
    0x29, 0x02,             /*   USAGE_MAXIMUM (Button 2)                   */ \
    0x25, 0x01,             /*   LOGICAL_MAXIMUM (1)                        */ \
    0x75, 0x01,             /*   REPORT_SIZE (1)                            */ \
    0x95, 0x02,             /*   REPORT_COUNT (2)                           */ \
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)                       */ \
    0x95, 0x06,             /*   REPORT_COUNT (6)                           */ \
    0x81, 0x03,             /*   INPUT (Cnst,Var,Abs)                       */ \
    /* ----- X/Y Movement ----- */                                             \
    0x05, 0x01,             /*   USAGE_PAGE (Generic Desktop)               */ \
    0x09, 0x30,             /*   USAGE (X)                                  */ \
    0x09, 0x31,             /*   USAGE (Y)                                  */ \
    0x75, 0x10,             /*   REPORT_SIZE (16)                           */ \
    0x95, 0x02,             /*   REPORT_COUNT (2)                           */ \
    0x16, 0x01, 0x80,       /*   LOGICAL_MINIMUM (-32767)                   */ \
    0x26, 0xFF, 0x7F,       /*   LOGICAL_MAXIMUM (32767)                    */ \
    0x81, 0x06,             /*   INPUT (Data,Var,Rel)                       */ \
    0xC0,                   /* END_COLLECTION                               */ \
    0xC0                    /* END_COLLECTION (TLC 4)                       */

/* ------------------------------------------------------------------ */
/* Report data structures (packed, matching the descriptor layout)     */
/* ------------------------------------------------------------------ */

/**
 * Per-finger contact in the input report.
 */
struct touchpad_contact_report {
    uint8_t flags; /* Bit 0: Confidence, Bit 1: Tip Switch, Bits 2-7: padding */
    uint8_t contact_id;
    uint16_t x;
    uint16_t y;
} __packed;

/**
 * Full touchpad input report (Report ID 0x01).
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
 */
struct touchpad_feature_max_count {
    uint8_t report_id;
    uint8_t max_count : 4;
    uint8_t pad_type  : 4;
} __packed;

/**
 * Feature report: Input Mode (Report ID 0x04).
 */
struct touchpad_feature_input_mode {
    uint8_t report_id;
    uint8_t input_mode;
    uint8_t device_index;
} __packed;

#endif /* HID_TOUCHPAD_DESCRIPTOR_H */
