# Changelog

## 0.1.0

Initial release.

- Native RED4ext C++ plugin (`gwheel.dll`) that reads Logitech G-series wheels via DirectInput 8.
- Full Logitech G-series hardware table: WingMan Formula Force / Formula Force GP, Driving Force / DF Pro / DF GT, Momo Force / Momo Racing, G25, G27, G29, G920, G923 (Xbox/PS/PC variants), Formula Vibration Feedback.
- Force feedback (`GUID_ConstantForce`, `GUID_Damper`, `GUID_Sine`) for collision and surface-texture effects. Gracefully no-ops on wheels without a motor.
- Vehicle input injection via redscript `@wrapMethod` on `VehicleComponent.GetInputValueFloat`, backed by a native `GWheel_MaybeOverrideFloat` function.
- In-game settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings). No Cyber Engine Tweaks dependency.
- **Respects G HUB by default.** Non-exclusive cooperative level, no `SetProperty(DIPROP_RANGE)` calls, no plugin-owned centering spring unless the user enables Advanced → Override G HUB.
- JSON config at `red4ext/plugins/gwheel/config.json` as persistence fallback.
- FOMOD installer with dependency-check step for RED4ext / ArchiveXL / Mod Settings.
