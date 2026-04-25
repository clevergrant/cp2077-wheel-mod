# Cyberpunk 2077 — Logitech G-series Steering Wheel Mod

A RED4ext plugin that gives Cyberpunk 2077 first-class support for Logitech G-series steering wheels. Reads the wheel through the official Logitech Steering Wheel SDK, drives force feedback back to the wheel, injects vehicle input into `Cyberpunk2077.exe` via a direct function detour (no virtual gamepad, no driver install), and exposes settings in the game's own menu.

Version: **2.31.0** — first release for game patch 2.31, ground-up rewrite. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design.

## Features

- Any Logitech G-series wheel (see table below), auto-detected by USB VID/PID.
- Steering, throttle, brake, and clutch routed directly from the wheel to Cyberpunk's vehicle input.
- Force-feedback effects for collisions and surface texture. FFB is skipped gracefully on wheels without a motor.
- Settings page in the game's own menu (via [Mod Settings](https://github.com/jackhumbert/mod_settings)) — deadzones, FFB strength, per-vehicle response curves, and an opt-in Override G HUB section.
- **Respects Logitech G HUB.** Rotation range, sensitivity curve, and centering spring stay with G HUB unless you explicitly turn on Override.

## Supported wheels

| Model | Force feedback |
| --- | --- |
| WingMan Formula Force | yes |
| WingMan Formula Force GP | yes |
| Driving Force | no |
| Momo Force | yes |
| Driving Force Pro | yes |
| G25 Racing Wheel | yes |
| Driving Force GT | yes |
| G27 Racing Wheel | yes |
| G29 Driving Force | yes |
| G920 Driving Force | yes |
| G923 (Xbox / PS / PC) | yes |
| Momo Racing | yes |
| Formula Vibration Feedback | vibration only |

Non-Logitech wheels are not supported. If your wheel is plugged in and this mod doesn't claim it, the mod exits cleanly and stays out of the way.

## Requirements

- **Cyberpunk 2077** 2.0 or later (tested against current patch).
- **[RED4ext](https://github.com/WopsS/RED4ext)** — loads the plugin.
- **[redscript](https://github.com/jac3km4/redscript)** — compiles the `.reds` files.
- **[Mod Settings](https://github.com/jackhumbert/mod_settings)** — in-game settings page framework.
- **[ArchiveXL](https://github.com/psiberx/cp2077-archive-xl)** — required by Mod Settings.
- **Logitech G HUB** (recommended) — Logitech's own wheel manager. This mod plays nicely with it.

No Cyber Engine Tweaks dependency.

## Install

### Via Vortex

1. Install the dependencies above (each has its own Nexus listing and FOMOD).
2. Download the latest release ZIP from Nexus.
3. Drop it on Vortex; accept the FOMOD prompts. The installer warns if RED4ext, ArchiveXL, or Mod Settings isn't present.

### Manually

Extract the release ZIP into your Cyberpunk 2077 install directory. The expected final layout:

```text
<CP2077>/red4ext/plugins/gwheel/gwheel.dll
<CP2077>/r6/scripts/gwheel/gwheel_natives.reds
<CP2077>/r6/scripts/gwheel/gwheel_settings.reds
<CP2077>/r6/scripts/gwheel/gwheel_mount.reds
<CP2077>/r6/scripts/gwheel/gwheel_menu.reds
```

## First run

1. Plug the wheel in and let G HUB pick it up.
2. Launch the game. The plugin logs to `red4ext/logs/gwheel-*.log`; look for `[gwheel] loaded v2.31.0` and a line naming the detected wheel.
3. Open the game's Settings menu → **Mod Settings** → **G-series Wheel**.
4. In the Advanced section, the first-time tip reads: "Sensitivity, rotation range, and centering spring are managed by G HUB. Enable Advanced → Override G HUB only if you want this mod to take control."
5. Get in any car. The wheel should steer it.

## Configuration

All settings live in the game's Settings menu. Four groups:

- **Wheel — Input.** Master toggle, per-axis deadzones, per-vehicle response curve.
- **Wheel — FFB.** Enable, strength (scales collision/texture effects), debug logging.
- **Wheel — Advanced.** **Override G HUB** toggle (default OFF). Only when ON: sensitivity, operating range, centering spring. Leaving this OFF preserves whatever you've tuned in G HUB.
- **Wheel — Button Bindings.** 20 physical wheel controls (paddles, D-pad, A/B/X/Y, Start/Select, LSB/RSB, +/-, scroll, Xbox button) each bindable to one of 39 in-game actions (horn, headlights, handbrake, camera, weapons, map, pause, etc.). D-pad and A/B/X/Y auto-swap to arrow-keys / Enter / Escape while any pause menu is open so the wheel navigates menus like a controller.

Values are persisted by Mod Settings across game runs. A backup copy is written to `<CP2077>/red4ext/plugins/gwheel/config.json` — safe to copy/edit between installs, but the game-side Settings page is authoritative while the game is running.

## Troubleshooting

**Plugin doesn't load.** Check `<CP2077>/red4ext/logs/` for entries tagged `[gwheel]`. A missing entry means RED4ext didn't load it — verify `red4ext/plugins/gwheel/gwheel.dll` exists and is unblocked (right-click → Properties → Unblock if Windows marked it).

**Settings page doesn't appear.** Mod Settings + ArchiveXL need to be installed correctly. The wheel still works without them; you just lose in-game tuning. Edit `config.json` directly as a fallback.

**Wheel detected but nothing happens in the car.** Axis injection goes through a C++ detour on `vehicle::BaseObject::UpdateVehicleCameraInput`, resolved via a hash in RED4ext's address database. If RED4ext itself is behind the current game patch, it terminates the game with its own message box at launch — update RED4ext first. If the game launches but the wheel still doesn't steer, check `red4ext/logs/gwheel-*.log` for `[gwheel:hook] UpdateVehicleCameraInput fired for the first time` (live-hook signal) and call `GWheel_GetDeviceInfo` from a debug console to see the fire counter. Missing or stuck at zero means the mount gate isn't tracking your vehicle — check that `gwheel_mount.reds` compiled (see `r6/cache/modded/final.redscripts.log`).

**FFB never fires.** Confirm the wheel has a motor (see the supported-wheels table). Confirm `GWheel_HasFFB()` returns true in the mod's log output. In G HUB, make sure "Allow game to adjust settings" is enabled; without it the game's force commands won't reach the wheel.

## Uninstall

Delete:

- `<CP2077>/red4ext/plugins/gwheel/`
- `<CP2077>/r6/scripts/gwheel/`

(Vortex handles both automatically on uninstall.)

## Contributing

See [ARCHITECTURE.md](ARCHITECTURE.md) for the internals. PRs welcome, especially for:

- Additional Logitech wheel PIDs not in the table
- Additional per-vehicle response profiles
- Verified per-device button layouts for G920 / G29 / G27 (run `tools/input_probe` against your wheel, diff against `input_bindings.cpp`'s `kG923XboxLayout`)

## License

TBD — v2.31.0 is unlicensed. A license will be added before the first public Nexus release.
