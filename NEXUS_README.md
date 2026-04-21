# Cyberpunk 2077 — Logitech G-series Steering Wheel Mod

**v0.1.0** — the first release of a ground-up rewrite. If you used any earlier version of this mod from this project's old Nexus page, uninstall that one completely first; this is a different architecture (RED4ext C++ plugin instead of CET Lua) and a different set of dependencies.

## What it does

Plays Cyberpunk 2077 with a Logitech G-series steering wheel. Direct DirectInput 8 acquisition, force feedback for collisions and surface texture, in-game settings menu, and — importantly — **it respects Logitech G HUB by default**. If you've tuned sensitivity, rotation range, or centering spring in G HUB, this mod won't undo that unless you explicitly turn on the Override toggle in-game.

## Supported wheels

| Model | FFB |
| --- | --- |
| WingMan Formula Force / Formula Force GP | yes |
| Driving Force | no |
| Momo Force / Momo Racing | yes |
| Driving Force Pro | yes |
| Driving Force GT | yes |
| G25, G27 | yes |
| G29 | yes |
| G920 | yes |
| G923 (Xbox / PS / PC) | yes |
| Formula Vibration Feedback | vibration only |

Non-Logitech wheels (Thrustmaster, Fanatec, Moza) are not supported.

## Requirements

Install these first, in this order:

1. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** — loads the plugin.
2. **[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511)** — compiles the `.reds` files.
3. **[ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/4198)** — dependency of Mod Settings.
4. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** — in-game settings page framework.
5. Install this mod.

**No Cyber Engine Tweaks required.**

Logitech G HUB is recommended for wheel-side tuning (rotation range, sensitivity, centering spring). Make sure G HUB's "Allow game to adjust settings" is **enabled** so game-driven FFB effects reach the wheel.

## Install

Drop the downloaded ZIP on Vortex. The FOMOD prompts you to confirm RED4ext, ArchiveXL, and Mod Settings are present, and installs:

- `<CP2077>/red4ext/plugins/gwheel/gwheel.dll`
- `<CP2077>/r6/scripts/gwheel/*.reds`

## First-time setup walkthrough

1. Plug in the wheel. G HUB picks it up.
2. Launch Cyberpunk. In the main menu, go to **Settings → Mod Settings → G-series Wheel**.
3. You'll see three sections: Input, FFB, Advanced. The Advanced section has an **Override G HUB** toggle, default OFF. Leave it off unless you specifically want this mod to take over from G HUB.
4. Start a game. Get in a car. The wheel should steer it, throttle and brake should respond.
5. Crash into something. The wheel should kick.

## Uninstall

Vortex → Uninstall. Or manually delete `red4ext/plugins/gwheel/` and `r6/scripts/gwheel/`.

## Support

Known limitations in v0.1.0:

- Shifter/pedal accessories on separate USB ports aren't enumerated as part of the wheel.
- Per-vehicle override profiles aren't exposed in the Settings page yet (in-memory schema supports it).
- No CET integration, live telemetry overlay, or ImGui UI.

Report issues with: your wheel model, `red4ext/logs/gwheel-*.log` contents, and the Cyberpunk patch version.
