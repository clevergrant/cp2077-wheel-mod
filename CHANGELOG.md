# Changelog

## 2.31.0

### Unreleased adjustments

- Wheel I/O and FFB now go through the official Logitech Steering Wheel SDK (vendored under `gwheel/vendor/LogitechSDK_unpacked/`). DirectInput enumeration and the ViGEmBus virtual-pad bridge are both removed.
- Vehicle input is delivered via a detour on `vehicle::BaseObject::UpdateVehicleCameraInput`, resolved through RED4ext's `UniversalRelocBase::Resolve(501486464u)`. Function-address drift across game patches is now RED4ext's concern (address database maintained by the RED4ext team, shipped per-patch). No `sigs.h`; no `tools/sigfinder/`.
- Injection is gated on a cached player-vehicle pointer published by `gwheel_mount.reds` via `GWheel_Set/ClearPlayerVehicle`. The detour fires for every visible vehicle per tick; without this gate we'd remote-drive parked cars.
- Added wheel-button -> in-game action bindings. Single native: `GWheel_SetInputBinding(inputId: Int32, action: Int32)` mapping 20 stable PhysicalInput IDs to 39 Action values. Driven by Mod Settings. Per-device layout with G923 Xbox verified (empirical probe via the new `tools/input_probe` tool); G923 PS/PC, G920, G29, G27 fall back to the G923 Xbox mapping until each is probed.
- D-pad + A/B/X/Y default to MenuUp/Down/Left/Right/Confirm so the wheel navigates pause/map/inventory menus out of the box. Bindings are user-overridable; "None" truly disables the input.
- New `sources::` module - hardware-agnostic per-tick `Frame` (axes + digital + connected) plus an `InVehicle()` control-context flag. Sits between the wheel reader and the consumers, so a future RawInput path can merge in without churning consumers.
- New `kbd_hook` low-level keyboard filter: suppresses G HUB's synthetic vehicle-key events while V is on foot, while letting the plugin's own `SendInput` events through (tagged with `dwExtraInfo = 'gWHL'`).
- New dev tool: `tools/input_probe` (empirical button / POV / axis edge logger) for building per-device layouts. Replaces the removed `tools/sigfinder` (obsolete under hash-based relocation).
- Dropped ViGEmBus dependency; end users no longer need to install any driver.

### Initial release

- Native RED4ext C++ plugin (`gwheel.dll`) targeting Logitech G-series wheels.
- Full Logitech G-series hardware table: WingMan Formula Force / Formula Force GP, Driving Force / DF Pro / DF GT, Momo Force / Momo Racing, G25, G27, G29, G920, G923 (Xbox/PS/PC variants), Formula Vibration Feedback.
- Force feedback (constant, damper, spring) for collision and surface-texture effects. Gracefully no-ops on wheels without a motor.
- In-game settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings). No Cyber Engine Tweaks dependency.
- JSON config at `red4ext/plugins/gwheel/config.json` as persistence fallback.
- FOMOD installer with dependency-check step for RED4ext / ArchiveXL / Mod Settings.
