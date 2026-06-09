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
- USB connection to a host that supports multi-touch touchpad input (BLE is currently not tested/supported)

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

### Device Tree Setup

Because this module operates as a native ZMK Input Processor, you must attach it to your trackball's device node in your `.dtsi` or `.keymap` file. This tells ZMK to route the trackball's movement data through the gesture engine.

```dts
&trackball {
    /* Route this specific trackball through the gesture module */
    input-processors = <&gesture_processor>;
};
```

*Note for dual-trackball users: If you have two trackballs, you can attach the processor to just one of them (dedicating it to gestures while the other remains a normal mouse) or attach it to both.*

### Keymap Setup

The module provides a `&gesture` behavior that you bind in your keymap. Each binding specifies the gesture type to activate while the key is held:

```dts
#include <dt-bindings/zmk/trackball_gestures.h>

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

| Option | Type | Default | Range | Description |
|--------|------|---------|-------|-------------|
| `CONFIG_ZMK_TRACKBALL_GESTURES` | bool | `y` | - | Master enable for the module. |
| `CONFIG_USB_HID_DEVICE_COUNT` | int | `2` | - | Global ZMK config. Forced to 2 by this module to allow the virtual touchpad to register alongside the keyboard. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_MAX_FINGERS` | int | `5` | `2 - 5` | Maximum virtual finger contacts. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_SWIPE_AXIS_LOCK` | bool | `y` | - | Dominant axis snapping. Automatically locks multi-finger swipes to a straight vertical or horizontal line to prevent diagonal drift. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_REPORT_RATE_MS` | int | `8` | - | Report interval in ms (8 = 125 Hz). |
| `CONFIG_ZMK_TRACKBALL_GESTURES_TP_LOGICAL_MAX` | int | `32767` | `100 - 32767` | Logical X/Y maximum of the virtual touchpad. Defines the coordinate resolution. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_TP_PHYSICAL_MAX` | int | `3200` | `100 - 32767` | Physical size of the emulated touchpad in 100ths of an inch (3200 = 32 inches). Dictates OS swipe distance scaling. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_IDLE_TIMEOUT_MS` | int | `100` | `50 - 2000` | Trackball idle timeout before virtual fingers lift. Acts as an auto-centering mechanism for consecutive swipes. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY` | int | `10` | `1 - 500` | Multiplies raw hardware ticks. E.g., moving 2 physical ticks with a sensitivity of 10 tells the OS you moved 20 pixels. Increase to make gestures feel faster. |
| `CONFIG_ZMK_TRACKBALL_GESTURES_PINCH_BASE_DISTANCE` | int | `8000` | `1 - 16000` | Distance between virtual fingers when pinch-to-zoom starts. Rolling down pushes them apart (Zoom In), rolling up pulls them closer (Zoom Out). |
| `CONFIG_ZMK_TRACKBALL_GESTURES_FINGER_SPACING` | int | `800` | `1 - 8191` | Distance between virtual fingers in multi-finger swipes. Ensures fingers are spaced widely enough to avoid OS palm-rejection errors. |

Example `.conf`:

```ini
CONFIG_ZMK_TRACKBALL_GESTURES=y
CONFIG_ZMK_TRACKBALL_GESTURES_SENSITIVITY=15
CONFIG_ZMK_TRACKBALL_GESTURES_IDLE_TIMEOUT_MS=150
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
| **Windows 10/11** | ⚠️ Limited testing | Implements Microsoft PTP specs, tested to work with native gestures |
| **macOS** | ❓ Untested | macOS does not natively support multi-touch HID over USB/BLE; may require third-party drivers |

## License

This project is licensed under the [MIT License](LICENSE).

---

**Questions or issues?** Open an issue on the GitHub repository.
