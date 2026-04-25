# Cyberpunk 2077 - Logitech G-series Steering Wheel Mod

**v2.31.0** - first release for game patch 2.31, ground-up rewrite as a RED4ext C++ plugin. If you used any earlier version of this mod from this project's old Nexus page, uninstall that one completely first; this is a different architecture and a different set of dependencies.

## What it does

Plays Cyberpunk 2077 with a Logitech G-series steering wheel. Wheel input and force feedback go through the **official Logitech Steering Wheel SDK** (the same one every driving sim uses), and vehicle input is injected into `Cyberpunk2077.exe` via a direct function detour. There is no virtual gamepad, no driver install, and no XInput shim.

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

Non-Logitech wheels (Thrustmaster, Fanatec, Moza) are not supported by the Logitech SDK and therefore not supported here.

## Requirements

Install these first, in this order:

1. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** - loads the plugin.
2. **[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511)** - compiles the `.reds` files.
3. **[ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/4198)** - dependency of Mod Settings.
4. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** - in-game settings page framework.
5. **[Logitech G HUB](https://www.logitech.com/innovation/g-hub.html)** - provides the user-mode service the SDK talks to. Already installed if you have a Logitech wheel.
6. Install this mod.

**No Cyber Engine Tweaks required. No ViGEmBus driver required.**

## Install

Drop the downloaded ZIP on Vortex. The FOMOD prompts you to confirm RED4ext, ArchiveXL, and Mod Settings are present, and installs:

- `<CP2077>/red4ext/plugins/gwheel/gwheel.dll`
- Four `.reds` files under `<CP2077>/r6/scripts/gwheel/`: `gwheel_natives.reds`, `gwheel_settings.reds`, `gwheel_mount.reds`, `gwheel_menu.reds`.

## Compatibility note: RED4ext address database

The plugin locates the game's vehicle-input function through an RED4ext hash (`UniversalRelocBase::Resolve`). RED4ext's maintainers ship updated address databases per game patch. If RED4ext itself is behind the current game build, the hash won't resolve and **RED4ext hard-fails the game at launch with its own message box** - this mod is not the culprit, it's the whole RED4ext ecosystem waiting on an update. Check RED4ext's Nexus page first for a refresh, then come back here if it's still broken after that.

There are no byte-pattern signatures to maintain on this side; no `sigs.h`; no per-patch re-release of this mod for hook drift. Button/keyboard dispatch uses `SendInput` (stable across patches).

The `CHANGELOG.md` lists the game version each release was tested against.

## First-time setup walkthrough

1. Plug in the wheel. G HUB picks it up.
2. Launch Cyberpunk. In the main menu, go to **Settings -> Mod Settings -> G-series Wheel**.
3. You'll see three sections: Input, FFB, Advanced. The Advanced section has an **Override G HUB** toggle, default OFF. Leave it off unless you specifically want this mod to take over from G HUB.
4. Start a game. Get in a car. The wheel should steer, throttle and brake should respond.
5. Crash into something. The wheel should kick.

## Wheel buttons -> in-game actions

You get **20 physical wheel controls** (paddles, D-pad, A/B/X/Y, Start/Select, LSB/RSB, +/-, scroll wheel click + CW + CCW, Xbox/Guide) each bindable to one of **39 in-game actions** (horn, headlights, handbrake, autodrive, exit vehicle, camera cycle, zoom, weapon slots, map, journal, inventory, phone, perks, crafting, quick save, radio menu, consumable, iconic cyberware, pause, tag, call vehicle, and menu nav for when the wheel is your menu controller).

Bindings are configured through **Mod Settings** (Settings -> Mod Settings -> G-series Wheel -> Button Bindings). They persist across runs via Mod Settings and are also backed up to `red4ext/plugins/gwheel/config.json`.

**Menu mode.** When any pause menu is open (pause, map, inventory, journal, perks, crafting, phone), the D-pad and A/B/X/Y are **automatically** overridden to arrow keys / Enter / Escape regardless of what you bound them to, so the wheel navigates menus like a controller. Other controls (paddles, +/-, scroll, LSB/RSB) keep their bindings.

**G HUB interaction.** The plugin installs a low-level keyboard filter: G HUB can keep sending keyboard events for controls you also bound here, and you won't get double-fires. But if you want the wheel-driven bindings to be the source of truth, clearing the equivalent controls in your G HUB Cyberpunk profile keeps things simple.

Under the hood there is one binding native, `GWheel_SetInputBinding(inputId: Int32, action: Int32)`, where inputId is a PhysicalInput enum value (0-19) and action is a GWheelAction enum value (0-38). Mod Settings drives it for you; direct calls are only useful for scripted test harnesses.

## Uninstall

Vortex -> Uninstall. Or manually delete `red4ext/plugins/gwheel/` and `r6/scripts/gwheel/`.

## Support

Report issues with: your wheel model, `red4ext/logs/gwheel-*.log` contents, and the Cyberpunk patch version.
