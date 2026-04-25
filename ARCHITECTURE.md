# gwheel — Architecture

This document is the contract the rest of the implementation is built against. It covers: plugin layout, the startup sequence, the hardware-to-game data path (Logitech SDK → `sources::` seam → vehicle detour + button dispatch), the config JSON schema, the redscript-facing native API, the button-binding model, and the G HUB coexistence model.

## One-line summary

A Cyberpunk 2077 RED4ext plugin that reads a Logitech G-series wheel via the official Logitech Steering Wheel SDK, injects its axes into the game through a hash-resolved detour on `vehicle::BaseObject::UpdateVehicleCameraInput`, dispatches wheel-button presses to in-game actions via `SendInput`, drives force-feedback effects back out, and exposes a flat set of global native functions that four `.reds` files use to render a Settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings) and to track player-vehicle / menu lifecycle.

## Components

```text
Logitech G-series wheel ──USB HID──▶ Logitech Steering Wheel SDK (via G HUB / LGS)
                                                     │
                                                     ▼
                           gwheel.dll  (red4ext plugin, 250 Hz pump)
                           ┌──────────────────────────────────────────┐
                           │   wheel::  ─────▶  sources::Frame        │
                           │                      │         │          │
                           │                      ▼         ▼          │
                           │      vehicle_hook (detour)   input_bindings │
                           │          │ inject axes          │ SendInput │
                           │          ▼                      ▼          │
                           │   vehicle::BaseObject     kbd_hook filters │
                           │                           G HUB ghosts     │
                           └──────────┬──────────────────┬──────────────┘
                                      │                  │
                                      ▼                  ▼
                                game vehicle        game input queue
                                                         │
                           redscript (r6/scripts/gwheel/*.reds)
                                      │
                                      ▼
                       Main Menu → Mod Settings → G-series Wheel
```

| File | Role |
| --- | --- |
| `gwheel/src/dllmain.cpp` | RED4ext `Main` / `Query` entry points. |
| `gwheel/src/plugin.{h,cpp}` | Lifecycle: 6-step `OnLoad` (config → rtti → wheel → vehicle_hook → kbd_hook → pump thread); owns the 250 Hz pump loop. |
| `gwheel/src/logging.{h,cpp}` | Thin wrapper over the RED4ext logger + optional local log file. |
| `gwheel/src/device_table.{h,cpp}` | Logitech G-series VID/PID table. |
| `gwheel/src/wheel.{h,cpp}` | Logitech SDK wrapper: init, `Pump`, `Snapshot` (steer/throttle/brake/clutch/buttons/POV), and FFB dispatch (`PlayConstant`/`PlayDamper`/`PlaySpring` + global strength). |
| `gwheel/src/sources.{h,cpp}` | Hardware-agnostic publish/read seam. `sources::Frame` = axes + digital + connected. Also carries `InVehicle()` control context (set by reds mount wrappers). |
| `gwheel/src/input_bindings.{h,cpp}` | Physical-input enum, per-device layout, edge detection, `SendInput` dispatch. Entry point: `OnTick(const sources::Frame&)`. Every input falls through to the user's Mod Settings binding; menu-nav is the default for D-pad + A but is user-overridable. |
| `gwheel/src/vehicle_hook.{h,cpp}` | RED4ext hash-resolved detour on `vehicle::BaseObject::UpdateVehicleCameraInput`. Gated on a cached player-vehicle pointer so we don't remote-drive parked cars. |
| `gwheel/src/kbd_hook.{h,cpp}` | Low-level `WH_KEYBOARD_LL` hook that suppresses G HUB's synthetic vehicle-key presses while on foot. Our own `SendInput` events tag `dwExtraInfo = kExtraInfoTag` ('gWHL') so the hook passes them through. |
| `gwheel/src/config.{h,cpp}` | Config struct, `Load()`, `ReadAsJson()`, per-field setters. Atomic double-buffered snapshot; every setter writes back to disk. |
| `gwheel/src/rtti.{h,cpp}` | Registers 19 global native functions exposed to redscript in `PostRegisterTypes`. |
| `gwheel/src/rtti_dump.{h,cpp}` | Debug-only RTTI dumper (disabled this build during a bisect). |
| `gwheel_reds/gwheel_natives.reds` | Declarations only — kept in lockstep with `rtti.cpp::PostRegisterTypes`. |
| `gwheel_reds/gwheel_settings.reds` | `GWheelSettings` Mod Settings class + `GWheelAction` enum; pushes values to plugin on every change. |
| `gwheel_reds/gwheel_mount.reds` | Wraps `VehicleComponent::OnVehicleFinishedMountingEvent` / `OnUnmountingEvent`; notifies plugin of the player's current vehicle pointer. |
| `gwheel_reds/gwheel_events.reds` | Wraps `VehicleObject::OnVehicleBumpEvent`; queues a transient FFB jolt (in player-frame world-right) on collision. |
| `gwheel_reds/gwheel_surface.reds` | 20 Hz downward raycast from chassis; pushes ground material CName transitions to the plugin (FFB mapping currently dormant). |
| `gwheel_reds/gwheel_vehicle_signals.reds` | Subscribes to vehicle Blackboard for RPMValue + VehRadioState; pushes normalized RPM and radio state to the plugin so the LED rev-strip / music-visualizer reflect real game state. |

## Supported hardware (device_table)

Only wheels under Logitech VID `0x046D` with PIDs in the table below are accepted. Anything else is refused with a log line.

```text
VID = 0x046D, entries in (PID, model_id, name, ffb_default, steering_range_deg):

0xC291  WINGMAN_FORMULA_FORCE       "WingMan Formula Force"        yes  240
0xC293  WINGMAN_FORMULA_FORCE_GP    "WingMan Formula Force GP"     yes  240
0xC294  DRIVING_FORCE               "Driving Force"                no   240
0xC295  MOMO_FORCE                  "Momo Force"                   yes  270
0xC298  DRIVING_FORCE_PRO           "Driving Force Pro"            yes  900
0xC299  G25                         "G25 Racing Wheel"             yes  900
0xC29A  DRIVING_FORCE_GT            "Driving Force GT"             yes  900
0xC29B  G27                         "G27 Racing Wheel"             yes  900
0xC24F  G29_NATIVE                  "G29 Driving Force"            yes  900
0xC260  G29_PS                      "G29 Driving Force (PS mode)"  yes  900
0xC261  G920_VARIANT                "G920 Driving Force"           yes  900
0xC262  G920                        "G920 Driving Force"           yes  900
0xC266  G923_XBOX                   "G923 (Xbox)"                  yes  900
0xC267  G923_PS_PC                  "G923 (PS/PC)"                 yes  900
0xC26D  G923_PS                     "G923 (PS mode)"               yes  900
0xC26E  G923                        "G923 (PC/USB)"                yes  900
0xCA03  MOMO_RACING                 "Momo Racing"                  yes  270
0xCA04  FORMULA_VFB                 "Formula Vibration Feedback"   yes  240
```

`ffb_default` is informational only. The authoritative check is the `hasFFB` flag on `wheel::Caps`, derived from `LogiHasForceFeedback` at bind time. This drives the `GWheel_HasFFB` native, which in turn disables the FFB section of the Settings UI for motorless wheels.

## Startup sequence

`plugin::OnLoad` runs six steps in order (see [plugin.cpp:63-95](gwheel/src/plugin.cpp#L63-L95)):

1. `config::Load()` — read `config.json` from the plugin install dir; fall back to defaults if missing / unparseable. Pushes initial bindings into `input_bindings::ReplaceAll`.
2. `rtti::Register()` — queue pre- and post-register callbacks with `CRTTISystem`; natives are registered in `PostRegisterTypes` once RTTI is built.
3. `wheel::Init()` — validate the Logitech SDK version and schedule a deferred `LogiSteeringInitialize` (the SDK refuses to bind until G HUB has enumerated the device).
4. `vehicle_hook::Init()` — resolve the UpdateVehicleCameraInput hash via RED4ext's `UniversalRelocBase::Resolve` and install the detour.
5. `kbd_hook::Install()` — install the low-level keyboard filter.
6. Start the 250 Hz pump thread.

## Wheel I/O (Logitech SDK) + sources seam

The pump thread (`plugin.cpp::PumpLoop`) runs at 250 Hz. Each tick:

1. `wheel::Pump()` → `LogiUpdate` + `LogiGetC` → `wheel::Snapshot`.
2. `BuildFrame(snapshot)` → `sources::Frame` (axes + digital + connected).
3. `sources::Publish(frame)`.
4. If `frame.connected`, `input_bindings::OnTick(frame)` — detects button/POV edges and dispatches bound actions via `SendInput`.

The `sources::` module is a hardware-agnostic seam. Today only `wheel.cpp` publishes into it; if we ever move digital input to RawInput (or merge readers), `BuildFrame` becomes the merge point and consumers don't change.

`sources::` also carries a control-context flag:

- `sources::InVehicle()` — flipped on/off by the redscript mount-event wrappers via `GWheel_Set/ClearPlayerVehicle`. `input_bindings::Dispatch` consults this to suppress vehicle-only actions (Handbrake, Headlights, Horn, camera cycles, etc.) while V is walking — the same keyboard keys mean different things on foot and we don't want the wheel firing Jump / Interact because a paddle is mapped to Handbrake.

The vehicle hook doesn't go through `sources::`; the detour fires on the game thread and reads `wheel::CurrentSnapshot()` directly to minimize latency into the input path.

## Vehicle input hook

Single target: `vehicle::BaseObject::UpdateVehicleCameraInput(self)`. Resolved via `RED4ext::UniversalRelocBase::Resolve(501486464u)` (hash sourced from Let There Be Flight, maintained in RED4ext's address database and updated per game patch by the RED4ext maintainers).

If the hash doesn't resolve — meaning RED4ext itself is behind the current game build — RED4ext terminates the process with its own MessageBox at load. The game won't launch at all until the RED4ext address database catches up.

The detour fires per-vehicle per-tick (not just for the player's car). A cached `g_playerVehicle` pointer, set by the redscript mount/unmount wrappers through `GWheel_SetPlayerVehicle` / `GWheel_ClearPlayerVehicle`, gates injection to the player's currently-mounted vehicle only. Without this gate, our writes would propagate to parked cars, AI traffic, and anything else with active camera updates.

Struct field offsets (CP2077 v2.31, build 5294808, found empirically 2026-04-21 — LTBF's SDK labels did not match):

| Offset | Field | Type | Range |
| --- | --- | --- | --- |
| `0x264` | throttle | `float` | 0..1 |
| `0x268` | brake | `float` | 0..1 (also drives reverse while stationary) |
| `0x278` | steer | `float` | -1..+1, + = right |

Re-probe if the game patches the `vehicle::BaseObject` struct. The detour also merges with vanilla keyboard/gamepad input (max-magnitude wins), so keyboard steering still works when the wheel isn't moving.

`vehicle_hook::FireCount()` is a monotonic tick counter surfaced through `GWheel_GetDeviceInfo` for live-hook confirmation.

## Native function surface (RTTI)

Registered as global static native functions (not a class; no `IScriptable` parent) in [rtti.cpp:170-243](gwheel/src/rtti.cpp#L170-L243) via `RED4ext::CGlobalFunction::Create(name, name, fn)` + `AddParam` + `SetReturnType` + `rtti->RegisterFunction(func)`.

Each function uses the canonical stack-frame shape:

```cpp
void Fn(RED4ext::IScriptable* aContext,
        RED4ext::CStackFrame* aFrame,
        OutT* aOut,
        int64_t /*unused*/);
```

Parameters are read sequentially with `RED4ext::GetParameter(aFrame, &arg)`, followed by `aFrame->code++;`. Return value is assigned via `if (aOut) *aOut = …;`.

### Read-only

| Native | Params | Returns | Purpose |
| --- | --- | --- | --- |
| `GWheel_GetVersion` | — | `String` | Plugin version string (`kVersionString`). |
| `GWheel_IsPluginReady` | — | `Bool` | `true` iff the Logitech SDK bound a device. |
| `GWheel_GetDeviceInfo` | — | `String` | Human-readable summary: product name, operating range, FFB flag, SDK version, hook state, fire count. |
| `GWheel_HasFFB` | — | `Bool` | `true` iff a device is bound and advertises force feedback. |
| `GWheel_ReadConfig` | — | `String` | Current config serialized as JSON (same schema as `config.json`). |

### Config setters

Each setter atomically swaps the live snapshot and writes `config.json` to disk. All return `Bool` (`true` on accept).

| Native | Params |
| --- | --- |
| `GWheel_SetInputEnabled` | `v: Bool` |
| `GWheel_SetClutchAsBrake` | `v: Bool` |
| `GWheel_SetFfbEnabled` | `v: Bool` |
| `GWheel_SetFfbStrengthPct` | `pct: Int32` (0–100) |
| `GWheel_SetFfbDebugLogging` | `v: Bool` |
| `GWheel_SetOverrideEnabled` | `v: Bool` |
| `GWheel_SetOverrideSensitivity` | `v: Float` (0.25–2.0) |
| `GWheel_SetOverrideRangeDeg` | `deg: Int32` (40–900) |
| `GWheel_SetOverrideCenteringSpringPct` | `pct: Int32` (0–100) |

### Bindings

| Native | Params | Purpose |
| --- | --- | --- |
| `GWheel_SetInputBinding` | `inputId: Int32, action: Int32` | Map a PhysicalInput (0–19) to an Action (0–38). |

### Player-vehicle lifecycle

| Native | Params | Purpose |
| --- | --- | --- |
| `GWheel_SetPlayerVehicle` | `v: ref<VehicleObject>` | Cache on mount (also sets `sources::InVehicle(true)`). |
| `GWheel_ClearPlayerVehicle` | — | Clear on dismount (also `sources::InVehicle(false)`). |

## Config JSON schema

Path: `<CP2077>/red4ext/plugins/gwheel/config.json`. Loaded at plugin load, written back on every setter, saved on unload.

```json
{
  "version": 3,

  "input": {
    "enabled": true,
    "clutchAsBrake": false,
    "responseCurve": "default"
  },

  "ffb": {
    "enabled": true,
    "strengthPct": 80,
    "debugLogging": false
  },

  "override": {
    "enabled": false,
    "sensitivity": 1.0,
    "rangeDeg": 90,
    "centeringSpringPct": 50
  },

  "perVehicle": {
    "car":        { "steeringMultiplier": 1.0, "responseDelayMs": 20 },
    "motorcycle": { "steeringMultiplier": 1.2, "responseDelayMs": 10 },
    "truck":      { "steeringMultiplier": 0.8, "responseDelayMs": 40 },
    "van":        { "steeringMultiplier": 0.9, "responseDelayMs": 30 }
  },

  "bindings": [3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 30, 20, 4, 5, 6, 7, 1, 14, 15, 0]
}
```

**Defaults** (constants in `config.h`):

| Field | Default | Range | Notes |
| --- | --- | --- | --- |
| `input.enabled` | `true` | — | Master toggle for wheel input. |
| `input.clutchAsBrake` | `false` | — | When true, `BuildFrame` publishes `brake = max(brake, clutch)` so the clutch pedal brakes alongside the brake pedal. |
| `input.responseCurve` | `"default"` | `"default"` \| `"subdued"` \| `"sharp"` | Shapes axis response pre-game. |
| `ffb.enabled` | `true` | — | Plugin-generated effects (collision / texture) only. |
| `ffb.strengthPct` | `80` | 0 – 100 | Scales plugin effect magnitudes; does not affect G HUB spring. |
| `ffb.debugLogging` | `false` | — | Verbose log of every effect start/stop. |
| `override.enabled` | `false` | — | Gates every field below. |
| `override.sensitivity` | `1.0` | 0.25 – 2.00 | Only when `override.enabled`. |
| `override.rangeDeg` | `90` | 40 – 900 | CP2077's virtual-wheel cap is ~90°; raise for more physical travel per on-screen degree. |
| `override.centeringSpringPct` | `50` | 0 – 100 | Only when `override.enabled`; plugin-driven centering spring. |
| `perVehicle.*` | as shown | positive floats / ints | Per-vehicle tuning. |
| `bindings` | 20-element array | `Action` integers | `PhysicalInput` index → `Action` value. See "Button bindings" below. |

Validation is per-setter inside `config::Set*`: each setter clamps/rejects out-of-range values before committing. There is no monolithic `ApplyConfig`.

## Button bindings

Wheel buttons map to in-game actions through a table driven by two enums that must stay in lockstep:

- `PhysicalInput` ([input_bindings.h:14-37](gwheel/src/input_bindings.h#L14-L37)) — 20 stable IDs for the controls on a modern G923-class wheel: `PaddleLeft`, `PaddleRight`, `DpadUp..Right`, `ButtonA..Y`, `Start`, `Select`, `LSB`, `RSB`, `Plus`, `Minus`, `ScrollClick`, `ScrollCW`, `ScrollCCW`, `Xbox`. Order is locked by the config.json `bindings` array and by the field order in `gwheel_settings.reds` — do not renumber; new controls append.
- `Action` ([input_bindings.h:48-90](gwheel/src/input_bindings.h#L48-L90)) — 39 values covering horn, headlights, handbrake, autodrive, exit, camera cycles, zoom, weapon slots, map / journal / inventory / phone / perks / crafting, quick save, radio, consumable, iconic cyberware, pause, tag, call vehicle, menu nav (Confirm / Cancel / Up / Down / Left / Right). Indices must match `GWheelAction` in [gwheel_settings.reds:268-308](gwheel_reds/gwheel_settings.reds#L268-L308) so Mod Settings dropdown indices round-trip.

Each `Action` dispatches to a specific Windows virtual-key or mouse event via `SendInput`. Tap actions fire DOWN+UP on rising edge; Hold actions mirror the physical state.

Per-device layouts pick which DirectInput button index / POV value maps to each `PhysicalInput`:

| Device | Status |
| --- | --- |
| G923 Xbox | **Verified** empirically 2026-04-21 via `tools/input_probe`. Ground truth. |
| G923 PS / PC, G920, G29, G27 | Unverified — use G923 Xbox layout as best-guess fallback. Re-run `tools/input_probe` and update `input_bindings.cpp` when possible. |

Dispatch lifecycle each pump tick (`input_bindings::OnTick`):

1. For each of the 20 physical inputs, read `IsPhysicallyPressed(layout, input, buttons, pov)`.
2. Compare against the previous tick → rising / falling edge.
3. If an edge fired, `Dispatch(bindings[input], rising)`:
   - Suppress if the action is `VehicleOnly` and `sources::InVehicle()` is false.
   - Otherwise `SendInput` with `dwExtraInfo = kbd_hook::kExtraInfoTag` so our own LL keyboard hook doesn't filter the event.
4. There is no menu-state-aware override. An earlier design hard-overrode D-pad + A/B/X/Y to arrow keys / Enter / Escape while any menu was open, but CP2077's arrow keys are secondary vehicle controls (Up/Down = accelerate/decelerate, Left/Right = steer), so the override drove the car when the user pressed the D-pad even with binding=None. Menu nav is now just the user-visible default for D-pad + A in `gwheel_settings.reds`; the user can rebind to None to fully disable.

## FFB effect model

Three effects: `GUID_ConstantForce`, `GUID_Damper`, `GUID_Spring` (via the Logitech SDK, not DirectInput). Created at bind time, reparameterized per event via `LogiPlay*Force*`, stopped via `LogiStop*`. The spring is only active when `override.enabled && override.centeringSpringPct > 0`; otherwise centering stays with G HUB.

Every effect magnitude is scaled by `ffb.strengthPct / 100` via `wheel::SetGlobalStrength`. If the bound wheel reports no FFB support, every FFB call becomes a no-op — callers do not need to branch.

## G HUB coexistence

| Knob | Default owner | Mod owner when `override.enabled = true` |
| --- | --- | --- |
| Rotation range (°) | G HUB | Mod (via Logitech SDK controller-properties update) |
| Sensitivity curve | G HUB | Mod (pre-game shaping) |
| Centering spring | G HUB | Mod (plugin-created spring effect) |
| Collision FFB | Mod | Mod |
| Surface-texture FFB | Mod | Mod |
| Per-vehicle response | Mod | Mod |
| Wheel button → keyboard (for bound controls) | Mod (via `input_bindings` + `kbd_hook`) | Mod |

The `kbd_hook` layer is what makes this peaceful. G HUB's own Cyberpunk profile also synthesizes keyboard events; without filtering, bound controls would double-fire (wheel → `SendInput(Handbrake)` **and** G HUB → `Space`). The LL keyboard hook drops any non-tagged synthetic event matching a vehicle-only key while the player is on foot, and our events carry the `'gWHL'` `dwExtraInfo` tag to pass through.

## Developer tools

- [tools/input_probe/](tools/input_probe/) — standalone console tool. Enumerates connected wheels via the Logi SDK, polls at 16 ms, logs every button / POV edge and significant axis delta. Used to build per-device layout tables for `input_bindings.cpp`. Requires G HUB (or LGS) running and the wheel not claimed by another session (game closed).
- [tools/wheel_reset/](tools/wheel_reset/) — one-shot utility that pushes G HUB's current controller properties back to the wheel (or forces a specific operating-range override). Useful when firmware is stuck in an SDK-managed state and G HUB's GUI changes no longer reach the hardware.

## Build / deploy

- Build: `powershell -ExecutionPolicy Bypass -File build.ps1 -Config Release` (wraps CMake + MSVC; VS2022 + CMake 3.21+ required). Output at `build/gwheel/Release/gwheel.dll`.
- Deploy: `powershell -ExecutionPolicy Bypass -File deploy.ps1 [-Game <path>]`. Zip mode (default) produces `dist/gwheel-<version>.zip` laid out for the FOMOD installer. Direct mode (`-Game`) copies the DLL + four `.reds` files into `<CP2077>/red4ext/plugins/gwheel/` and `<CP2077>/r6/scripts/gwheel/` and invalidates the redscript cache.
- Runtime deps: [RED4ext](https://github.com/WopsS/RED4ext), [redscript](https://github.com/jac3km4/redscript), [Mod Settings](https://github.com/jackhumbert/mod_settings), [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl).

## Sources

- [RED4ext documentation](https://docs.red4ext.com)
- [RED4ext & RED4ext.SDK on GitHub](https://github.com/WopsS/RED4ext)
- [Let There Be Flight (hash source for UpdateVehicleCameraInput)](https://github.com/jackhumbert/let_there_be_flight)
- [Mod Settings](https://github.com/jackhumbert/mod_settings) / [Nexus listing](https://www.nexusmods.com/cyberpunk2077/mods/4885)
- [redscript](https://github.com/jac3km4/redscript) / [redscript wiki](https://wiki.redmodding.org/redscript)
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl)
- [Logitech Gaming Software SDK — Steering Wheel](https://www.logitechg.com/en-us/innovation/developer-lab.html)
- [the-sz USB ID database — Logitech 046D](https://the-sz.com/products/usbid/index.php?v=0x046D)
- [FOMOD ModuleConfig schema](https://fomod-docs.readthedocs.io/en/latest/_static/ModuleConfig.html)
