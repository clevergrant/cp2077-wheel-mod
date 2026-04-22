# Changelog

## 0.1.0

### Unreleased adjustments

- Wheel I/O and FFB now go through the official Logitech Steering Wheel SDK (vendored under `gwheel/vendor/LogitechSDK_unpacked/`). DirectInput enumeration and the ViGEmBus virtual-pad bridge are both removed.
- Vehicle input is delivered via a direct `Cyberpunk2077.exe` function detour. Target function addresses are resolved from byte-pattern signatures in `gwheel/src/sigs.h`; signatures are produced by the developer-side `tools/sigfinder/` Python tool and hard-coded into the source before release.
- Added wheel-button -> in-game action bindings. Redscript natives `GWheel_SetButtonBinding`, `GWheel_ClearButtonBinding`, `GWheel_GetButtonBinding`, `GWheel_IsButtonPressed`, `GWheel_GetLastPressedButton`, `GWheel_GetButtonBindingsJson`. Bindings persist to `config.json`.
- Dropped ViGEmBus dependency; end users no longer need to install any driver.

### Initial release

- Native RED4ext C++ plugin (`gwheel.dll`) targeting Logitech G-series wheels.
- Full Logitech G-series hardware table: WingMan Formula Force / Formula Force GP, Driving Force / DF Pro / DF GT, Momo Force / Momo Racing, G25, G27, G29, G920, G923 (Xbox/PS/PC variants), Formula Vibration Feedback.
- Force feedback (constant, damper, spring) for collision and surface-texture effects. Gracefully no-ops on wheels without a motor.
- In-game settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings). No Cyber Engine Tweaks dependency.
- JSON config at `red4ext/plugins/gwheel/config.json` as persistence fallback.
- FOMOD installer with dependency-check step for RED4ext / ArchiveXL / Mod Settings.
