# Cyberpunk 2077 - Logitech G-series Steering Wheel Mod

**v0.1.0** - ground-up rewrite as a RED4ext C++ plugin. If you used any earlier version of this mod from this project's old Nexus page, uninstall that one completely first; this is a different architecture and a different set of dependencies.

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
- `<CP2077>/r6/scripts/gwheel/*.reds`

## Compatibility note: byte-pattern signatures

The plugin reaches into `Cyberpunk2077.exe` through hard-coded byte-pattern signatures (stored in `gwheel/src/sigs.h`). When CDPR ships a game patch, those signatures drift and the vehicle-input detour goes inactive - you'll see `[gwheel:hook] vehicle-input: pattern not configured` or `no match` in the log. The maintainer regenerates the signatures with the `tools/sigfinder/` Python tool and re-publishes. If your game updates and the mod stops steering, check back on Nexus for an update.

The `CHANGELOG.md` lists the game version each release was tested against.

## First-time setup walkthrough

1. Plug in the wheel. G HUB picks it up.
2. Launch Cyberpunk. In the main menu, go to **Settings -> Mod Settings -> G-series Wheel**.
3. You'll see three sections: Input, FFB, Advanced. The Advanced section has an **Override G HUB** toggle, default OFF. Leave it off unless you specifically want this mod to take over from G HUB.
4. Start a game. Get in a car. The wheel should steer, throttle and brake should respond.
5. Crash into something. The wheel should kick.

## Wheel buttons -> in-game actions

The plugin exposes redscript natives that let you bind wheel buttons to in-game actions:

```swift
GWheel_SetButtonBinding(buttonIndex: Int32, action: String)
GWheel_ClearButtonBinding(buttonIndex: Int32)
GWheel_GetButtonBinding(buttonIndex: Int32) -> String
GWheel_IsButtonPressed(buttonIndex: Int32) -> Bool
GWheel_GetLastPressedButton() -> Int32
```

Bindings persist to `red4ext/plugins/gwheel/config.json`. Until the action-dispatch signature in `sigs.h` is populated, bound presses are logged but not yet routed into the game's action system.

## Uninstall

Vortex -> Uninstall. Or manually delete `red4ext/plugins/gwheel/` and `r6/scripts/gwheel/`.

## Support

Report issues with: your wheel model, `red4ext/logs/gwheel-*.log` contents, and the Cyberpunk patch version.
