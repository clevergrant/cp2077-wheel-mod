# Cyberpunk 2077 — Logitech G-series Steering Wheel Mod

A RED4ext plugin that gives Cyberpunk 2077 first-class support for Logitech G-series steering wheels. Reads the wheel through the official Logitech Steering Wheel SDK, drives physics-aware force feedback (centering, cornering, surface texture, collision jolts, slip-angle countersteer) back to the wheel, and injects vehicle input through a hash-resolved detour on `vehicle::BaseObject::UpdateVehicleCameraInput`. Rev-strip LEDs follow real engine RPM and switch to a music visualizer when the in-car radio is on. Wheel buttons are bindable to in-game actions through Mod Settings. No virtual gamepad, no driver install, no XInput shim.

Version: **2.31.0**, ground-up rewrite for game patch 2.31. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design.

User-facing documentation for the Nexus mod page lives in [NEXUS_README.md](NEXUS_README.md).

## Supported wheels

| Model | FFB | Rev LEDs | Lower cluster |
| --- | --- | --- | --- |
| WingMan Formula Force / GP | yes | no | no |
| Driving Force | no | no | no |
| Momo Force / Momo Racing | yes | no | no |
| Driving Force Pro / Driving Force GT | yes | no | no |
| G25, G27 | yes | yes | yes |
| G29 | yes | yes | yes |
| G920 | yes | yes | no |
| G923 (Xbox / PS / PC) | yes | yes | yes |
| Formula Vibration Feedback | vibration | no | no |

Hardware capability detection (FFB, rev-strip LEDs, lower-cluster buttons) drives which sections appear in the in-game Settings UI.

## Requirements

- **Cyberpunk 2077** patch 2.31 (build 5294808). Earlier patches may work but are untested.
- **[RED4ext](https://github.com/WopsS/RED4ext)** — loads the plugin.
- **[redscript](https://github.com/jac3km4/redscript)** — compiles the `.reds` files.
- **[ArchiveXL](https://github.com/psiberx/cp2077-archive-xl)** — required by Mod Settings.
- **[Mod Settings](https://github.com/jackhumbert/mod_settings)** v0.2.21+ — in-game settings page framework. Required, not optional. The release zip bundles a patched build of the Mod Settings DLL that adds a `ModSettings.hidden` runtime property; the patch is API-compatible with upstream.
- **Logitech G HUB** — provides the user-mode service the Logitech SDK talks to.

No Cyber Engine Tweaks. No ViGEmBus. No drivers.

## Install

End users install via Vortex. The Nexus "Mod Manager Download" deep-link is the supported path; see [NEXUS_README.md](NEXUS_README.md) for the user-facing instructions.

For dev iteration, skip Vortex with direct deploy:

```powershell
.\deploy.ps1 -Game "S:\SteamLibrary\steamapps\common\Cyberpunk 2077"
```

This rebuilds the DLL if sources are newer, copies the DLL + `.reds` files into the game install, copies the patched `mod_settings.dll` over the user's Mod Settings install, and invalidates the redscript cache. See [deploy.ps1](deploy.ps1) for details.

## Settings (in-game)

All settings live in the game's **Mod Settings → G-series Wheel** page. Categories:

- **Wheel input** — master enable, "treat clutch as brake".
- **Force feedback** — enable, FFB strength, cornering feedback, active torque, stationary threshold.
- **Rev-strip LEDs** — enable, music visualizer while music is playing.
- **Button bindings** — 15 controls present on every G-series wheel.
- **Lower-cluster bindings** — 5 controls on G25/G27/G29/G923 (absent on G920).
- **Startup** — Pon pon shi greeting on/off.
- **Debug** — debug logging.

Hardware-capability sections auto-hide on wheels without the relevant components, driven by capability flags written to `red4ext/plugins/mod_settings/user.ini` from the C++ side at plugin load (see `gwheel/src/mod_settings_seed.cpp`).

Settings are persisted by Mod Settings and mirrored to `red4ext/plugins/gwheel/config.json`.

## Troubleshooting

- **Plugin doesn't load.** Check `red4ext/logs/` for `[gwheel]` lines. Missing entries usually mean RED4ext didn't load the DLL: confirm `red4ext/plugins/gwheel/gwheel.dll` exists and isn't blocked by Windows (right-click → Properties → Unblock).
- **Settings page is missing.** Mod Settings + ArchiveXL must both be installed. Without them the mod runs on defaults from `config.json` but has no in-game tuning.
- **Wheel detected but the car doesn't move.** Confirm `[gwheel:hook] UpdateVehicleCameraInput fired for the first time` in the log after mounting a vehicle. If absent, redscript didn't compile the mount wrap; check `r6/cache/modded/final.redscripts.log`.
- **Three orphan toggles named `hasFfbHardware` / `hasRevLeds` / `hasRightCluster`.** Vortex installed stock Mod Settings on top of the patched build. Re-deploy and let this mod win the file conflict on `mod_settings.dll`.

## Uninstall

Vortex → Uninstall. Or manually delete `<CP2077>/red4ext/plugins/gwheel/` and `<CP2077>/r6/scripts/gwheel/`. To revert to stock Mod Settings, reinstall upstream Mod Settings to overwrite the patched DLL.

## Contributing

See [ARCHITECTURE.md](ARCHITECTURE.md) for the internals. PRs welcome, especially:

- Additional Logitech wheel PIDs not in `gwheel/src/device_table.cpp`.
- Verified per-device button layouts for G920 / G29 / G27 (run `tools/input_probe` against your wheel, diff against `kG923XboxLayout` in `gwheel/src/input_bindings.cpp`).
- Surface-material FFB tuning if you can capture dirt / sand / gravel CName transitions through the existing `gwheel_surface.reds` raycast (see `gwheel/src/wheel.cpp` for the existing baseline-by-material lookup).

## License

[MIT](LICENSE). Bundled patched mod_settings.dll is also MIT (forked from jackhumbert/mod_settings).
