# DualSense Gamepad Support

**Version:** 1.1.0
**Engine:** Unreal Engine 5.8
**Platform:** Windows 64-bit
**Category:** Input

---

## Overview

DualSense Gamepad Support adds **native PlayStation controller support** to Unreal Engine on Windows, powered by SDL3.

Unreal Engine only recognizes Xbox (XInput) controllers out of the box on Windows. This plugin makes PlayStation controllers work exactly like a native gamepad: they drive the standard `Gamepad_*` keys, so they work with Enhanced Input and **CommonUI** (focus navigation and button clicks) with no Blueprint setup and no project settings to change.

---

## Features

**Native PlayStation Controllers**
Supported types:
- PlayStation 3
- PlayStation 4 (DualShock 4)
- PlayStation 5 (DualSense)

Xbox controllers (Xbox 360, Xbox One, Xbox Series) are handled by the plugin too, so every controller behaves the same way. All other controller types are filtered out.

**CommonUI Compatible — One Input Path per Controller**
While the plugin is active it replaces Unreal's built-in XInput device, so each controller reaches the engine through exactly one input path. CommonUI buttons receive press and release from the same device and clicks complete reliably, and plugging in a controller never creates an extra local player. If SDL3 cannot be loaded, Unreal's built-in Xbox support is left untouched.

**Hot-Swap Detection**
Automatically detects when the player switches between controllers (PlayStation or Xbox) mid-session, using an activity-based heuristic with debounce protection. All controllers drive player 1.

**Force Feedback**
Fully compatible with Unreal Engine's native Play Force Feedback nodes. No custom rumble nodes required.

**Async Hotplug (No Hitches)**
SDL open/close operations run on a dedicated worker thread. The game thread is never blocked during controller plug/unplug.

**Always Active**
Works in editor viewport, PIE, and packaged builds. No settings or toggles needed.

---

## Requirements

- Unreal Engine 5.4
- Windows 64-bit target
- SDL3 binaries are included in the plugin (no separate installation required)

---

## Installation

1. Install the plugin from Fab, or copy the `DualSenseGamepadSupport` folder into your project's `Plugins/` directory
2. Enable the plugin in: **Edit → Plugins → Input → DualSense Gamepad Support**
3. Restart the editor

> SDL3.dll is included and staged automatically in packaged builds. No manual DLL copying required.

---

## How It Works

The plugin registers itself as an `IInputDeviceModule`. UE calls `CreateInputDevice` once at startup, which initializes SDL3 and starts a background worker thread for hotplug handling.

From that point, every frame:
1. Any pending rebuild results from the worker are applied (microseconds)
2. SDL events are pumped to detect plug/unplug
3. The active controller is polled at 120 Hz and button/axis events are forwarded to UE's message handler

Input is routed using standard UE keys (`Gamepad_FaceButton_Bottom`, `Gamepad_LeftX`, etc.) and works with any Input Mapping Context out of the box.

---

## Hardcoded Settings

There are no Project Settings. All values are compile-time constants:

| Setting | Value | Description |
|---|---|---|
| Poll Rate | 120 Hz | Input polling frequency |
| Axis Noise Threshold | 0.01 | Thumbstick jitter filtering |
| Hotplug Cooldown | 0.20s | Debounce time after plug/unplug |
| Controller Switch Threshold | 0.15 | Axis threshold for hot-swap |
| Active Hold Seconds | 0.35s | Lock duration before next hot-swap |
| Max Tracked Controllers | 4 | Simultaneous controllers |

---

## Notes

**Deadzones**
Raw axis values are passed to UE. Configure deadzones in your Enhanced Input actions as usual.

**Hotplug Performance**
SDL_OpenGamepad can block 300–400 ms on USB/HID/BT driver init on Windows. This plugin runs that call on a worker thread — the game thread is never stalled during hotplug.

**IMC Key Capture in Editor**
SDL captures controller input in the viewport at all times. If you need to assign gamepad buttons in an Input Mapping Context, disconnect the physical controller first.

---

## Troubleshooting

**Controller Not Detected**
- Verify the controller is a PlayStation or Xbox controller (other types are filtered by design)
- Confirm `SDL3.dll` exists in `Source/ThirdParty/SDL3/lib/Win64/` (the plugin loads it from there first; `Binaries/Win64/` is only a build artifact)
- Check `[DGS]` entries in the Output Log for detailed hotplug and scan info

**No Input in Game**
- Make sure your Input Mapping Context is active and assigned to the correct player
- Check that your Enhanced Input actions use the standard `Gamepad_*` keys

---

## Support

For bug reports and questions: dxmasdesign@gmail.com

---

## License

This plugin uses **SDL3**, licensed under the zlib license.
A copy of the SDL3 license is included at `Source/ThirdParty/SDL3/SDL3_License.txt`.

---

*DualSense Gamepad Support — David Sanchez*
