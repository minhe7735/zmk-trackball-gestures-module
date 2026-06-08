# ZMK Trackball Gestures Module

Emulate a **multi-touch touchpad** using your keyboard's trackball and key combos. This ZMK firmware module intercepts trackball movement while a gesture key is held, then translates the motion into multi-finger HID touchpad reports — giving your operating system real pinch-to-zoom, multi-finger swipes, and more, all from a trackball.

## Features

| Gesture | Description |
|---------|-------------|
| **2-Finger Swipe** | Hold gesture key → trackball movement becomes two-finger swipe |
| **3-Finger Swipe** | Hold gesture key → trackball movement becomes three-finger swipe (workspace switching, app exposé) |
| **4-Finger Swipe** | Hold gesture key → trackball movement becomes four-finger swipe (desktop switching, action center) |
| **5-Finger Swipe** | Hold gesture key → trackball movement becomes five-finger swipe |
| **2 to 5-Finger Pinch** | Hold gesture key → trackball up/down maps to 2 to 5-finger pinch in/out |

All gestures produce **real multi-contact touchpad reports** — the host OS sees native trackpad input, not synthetic scroll events or keystrokes.

## Prerequisites

- [ZMK Firmware](https://zmk.dev) with pointing device support enabled
- A trackball sensor wired to your keyboard (e.g., PMW3610, PMW3389, ADNS-9800)
- USB or BLE connection to a host that supports multi-touch touchpad input

## Installation

Add this module to your ZMK `config/west.yml` manifest:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: minhe7735
      url-base: https://github.com/minhe7735
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-trackball-gestures-module
      remote: minhe7735
      revision: master
  self:
    path: config
```

Then rebuild your firmware. The module is enabled by default once added.

## Configuration

### Keymap Setup

The module provides a `&gesture` behavior that you bind in your keymap. Each binding specifies the gesture type to activate while the key is held:

```dts
#include <dt-bindings/zmk/gesture.h>

/ {
    keymap {
        compatible = "zmk,keymap";

        default_layer {
            bindings = <
                // ... your normal keys ...
                &gesture GESTURE_PINCH2      // 2-finger pinch
                &gesture GESTURE_SWIPE2      // 2-finger swipe
                &gesture GESTURE_SWIPE3      // 3-finger swipe
                &gesture GESTURE_SWIPE4      // 4-finger swipe
            >;
        };
    };
};
```

While a gesture key is held, trackball movement is captured and translated into the corresponding multi-finger touchpad report. When the key is released, virtual fingers are lifted and normal trackball operation resumes.

### Kconfig Options

All options can be set in your board's `.conf` file or `prj.conf`:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_ZMK_TRACKBALL_GESTURES` | bool | `y` | Master enable for the module |
| `CONFIG_ZMK_TRACKBALL_GESTURES_MAX_FINGERS` | int | `5` | Maximum virtual finger contacts (2–5) |
| `CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS` | int | `8` | Report interval in ms (8 = 125 Hz) |
| `CONFIG_ZMK_TRACKBALL_GESTURES_TOUCHPAD_WIDTH` | int | `4095` | Logical X maximum of virtual touchpad |
| `CONFIG_ZMK_TRACKBALL_GESTURES_TOUCHPAD_HEIGHT` | int | `4095` | Logical Y maximum of virtual touchpad |
| `CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY` | int | `10` | Trackball delta multiplier |
| `CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE` | int | `800` | Initial finger distance for pinch (logical units) |
| `CONFIG_ZMK_TRACKBALL_GESTURES_FINGER_SPACING` | int | `200` | Spacing between fingers in multi-finger swipes |

Example `.conf`:

```ini
CONFIG_ZMK_TRACKBALL_GESTURES=y
CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY=15
CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS=4
```

## How It Works

```
┌─────────────┐     ┌───────────────────┐     ┌──────────────────┐
│  Trackball   │────▶│ trackball_gestures│────▶│ virtual_touchpad │
│  Sensor      │     │ (input listener)  │     │ (HID report gen) │
└─────────────┘     └───────────────────┘     └──────────────────┘
                            │                          │
                    ┌───────┴───────┐                  │
                    │ gesture_behavior │                │
                    │ (keymap &gesture)│                │
                    └───────────────┘                  ▼
                                              ┌──────────────────┐
                                              │   Host OS (PTP)  │
                                              └──────────────────┘
```

1. **`gesture_behavior.c`** — Implements the `&gesture` ZMK behavior. When a gesture key is pressed, it activates the corresponding gesture mode and begins intercepting trackball input.

2. **`trackball_gestures.c`** — Registers as a ZMK input listener on the trackball's input device. When a gesture is active, it captures `REL_X` / `REL_Y` deltas and forwards them to the virtual touchpad module instead of the normal pointer pipeline.

3. **`virtual_touchpad.c`** — Maintains the state of virtual finger contacts and generates standard multi-touch HID reports. It manages finger placement, movement, and lift-off to produce gesture patterns that the host OS recognizes natively.

## Compatibility

| OS | Status | Notes |
|----|--------|-------|
| **Linux** | ✅ Full support | libinput recognizes multi-touch reports; works with GNOME, KDE, Sway, Niri, etc. |
| **Windows 10/11** | ❓ Untested | Needs real-world testing to confirm native gesture support |
| **macOS** | ❓ Untested | macOS does not natively support multi-touch HID over USB/BLE; may require third-party drivers |

## License

This project is licensed under the [MIT License](LICENSE).

---

**Questions or issues?** Open an issue on the GitHub repository.
