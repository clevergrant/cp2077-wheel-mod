# Cyberpunk 2077 — Steering Wheel & Flight Controller Mod

**v2.31.0** for Cyberpunk 2077 game patch 2.31.

Drive Cyberpunk 2077 with a steering wheel or force feedback flight yoke. Your controller handles steering, throttle, brake, and clutch. Force feedback fires from in-game physics (centering as you turn, cornering load, surface texture, collision jolts, slip when you lose grip). Rev-strip LEDs follow real engine RPM. When the in-car radio is on, the rev strip becomes a music visualizer.

There is no virtual gamepad to install, no XInput shim, no third-party driver. The mod communicates natively via DirectInput and the official Logitech Steering Wheel SDK (the same one driving sims use).

## Supported wheels and controllers

The mod was designed to work with Logitech G Hub compatible wheels as well as DirectInput flight yokes and rudder pedals.

Tested with:

- G923 (Xbox variant)
- G920
- MOZA AY210 Force Feedback Base & Flight Yoke
- Thrustmaster TFRP Rudder Pedals

Supported Logitech wheels:

- WingMan Formula Force, Formula Force GP
- Driving Force, Driving Force Pro, Driving Force GT
- Momo Force, Momo Racing
- G25, G27, G29, G920
- G923 (Xbox, PS, PC variants)
- G PRO Racing Wheel
- Formula Vibration Feedback

Supported DirectInput flight hardware & pedals:

- MOZA Flight Yokes & Bases: AY90, AY210, AB9, MFY
- MOZA Wheelbases: R3, R5, R9, R12, R16, R21
- Thrustmaster TFRP & TPR Rudder Pedals
- Generic DirectInput flight yokes, pedals, and steering wheels

The mod auto-detects what hardware your controller actually has (force feedback motor, rev-strip LEDs, lower-cluster buttons, flight yoke pitch axis, rudder pedals) and only shows the relevant settings.

## Required mods

1. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** (v1.25.0 or later)
1. **[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511)** (v0.5.24 or later)
1. **[ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/4198)** (v1.14.0 or later)
1. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** v0.2.21 or later

## Required drivers

- **[Logitech G HUB](https://www.logitech.com/innovation/g-hub.html)** (for Logitech wheels)
- Controller companion software (e.g. MOZA Cockpit) if required by your hardware

## Install

1. Click the **Vortex** download button on this page (the green button in the Files tab).
1. Vortex will ask about a file conflict on `mod_settings.dll`. Choose this mod to win the conflict (load after Mod Settings). The included file is a small patch on top of upstream Mod Settings.

(Manual Download is also available if you don't use Vortex, but Mod Manager Download is the supported path. If you go manual, you handle the file conflict on `mod_settings.dll` yourself by overwriting upstream Mod Settings' DLL with the one from this zip.)

## G HUB

This mod respects G HUB. Wheel rotation range, sensitivity, and centering spring stay with G HUB; the mod reads the rotation range and scales its force feedback to match. G HUB and this mod can both be running at the same time.

If you have wheel buttons configured in your G HUB Cyberpunk profile that overlap with the bindings here, you may get doubled keypresses. Clear the overlapping bindings from your G HUB Cyberpunk profile.

I recommend starting with an operating range of 180 to start, since the rotation will match the animation you see in 1st person view inside of a car. I tend to like it a bit wider to help mitigate fishtailing.

## Diagnostic Tool (`input_probe`)

A standalone diagnostic tool is included in the build (`tools/input_probe/input_probe.exe`). You can run:
```cmd
input_probe.exe once
```
to verify attached DirectInput devices, detected roles (Flight Yoke, Rudder Pedals, Racing Wheel), axis positions, and force feedback capabilities without launching the game.

## Troubleshooting

- **Wheel detected but the car doesn't move.** Make sure RED4ext is installed and current.
- **Settings page is missing.** Mod Settings or ArchiveXL aren't installed correctly. The wheel will still work on defaults.
- **Three orphan toggles named `hasFfbHardware` / `hasRevLeds` / `hasRightCluster` show up in settings.** Vortex installed stock Mod Settings on top of the patched build that ships with this mod. Re-deploy in Vortex and let this mod win the file conflict.
- **Doubled keypresses on D-pad or A/B/X/Y.** G HUB (or companion app) and this mod are both binding the same keys. Reset the wheel-button entries in your companion profile back to their default bindings.
- **Overlapping inputs with keyboard.** Keyboard WASD can override steering controls if pressed simultaneously.

## Credits

- [RED4ext](https://github.com/WopsS/RED4ext) by WopsS
- [redscript](https://github.com/jac3km4/redscript) by jac3km4
- [Mod Settings](https://github.com/jackhumbert/mod_settings) by jackhumbert
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl) by psiberx

## Reporting issues

Include your wheel/controller model, Cyberpunk patch version, RED4ext version, and your most recent `red4ext/logs/gwheel-*.log` file.
