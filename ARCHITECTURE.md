# gwheel — Architecture

This document is the contract the rest of the implementation is built against. It covers: plugin layout, the config JSON schema, the redscript-facing native API, DirectInput acquisition rules, vehicle input hook targets, and the G HUB coexistence model.

## One-line summary

A Cyberpunk 2077 red4ext plugin that reads a Logitech G-series wheel via DirectInput 8, injects its axes into the game's vehicle input path through RTTI hooks, drives game-generated force-feedback effects back out, and exposes a small set of natives that a single `.reds` file uses to render a Settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings).

## Components

```text
Logitech G-series wheel  ──USB HID──▶  dinput8.dll
                                            │
                                            ▼
                              gwheel.dll  (red4ext plugin)
                               │                        │
                  RTTI hook                       redscript natives
                               ▼                        │
                  VehicleComponent input                │
                                                        ▼
                                            gwheel_settings.reds
                                                        │
                                                        ▼
                                   Main Menu → Mod Settings → Wheel
```

| File | Role |
| --- | --- |
| `gwheel/src/dllmain.cpp` | RED4ext `Main` / `Query` entry points |
| `gwheel/src/plugin.{h,cpp}` | Version, logger bootstrap, lifecycle coordination |
| `gwheel/src/device_table.{h,cpp}` | Logitech G-series PID table |
| `gwheel/src/wheel_device.{h,cpp}` | DirectInput 8 enumeration, acquisition, polling thread |
| `gwheel/src/ffb.{h,cpp}` | `IDirectInputEffect` management (collision, texture) |
| `gwheel/src/vehicle_hooks.{h,cpp}` | RTTI hooks on vehicle input methods |
| `gwheel/src/config.{h,cpp}` | JSON load/save; atomic live snapshot |
| `gwheel/src/rtti.{h,cpp}` | Registers native functions exposed to redscript |
| `gwheel/src/logging.h` | Thin wrapper over RED4ext logger + optional local log |
| `gwheel/include/gwheel_abi.h` | Debug-only C ABI header (external tools; not used by the game) |
| `gwheel_reds/gwheel_settings.reds` | Mod Settings class + listener |

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

`ffb_default` is informational only. The authoritative check is `IDirectInputDevice8::GetCapabilities()` — the `DIDC_FORCEFEEDBACK` bit of `DIDEVCAPS.dwFlags` decides whether the FFB subsystem engages.

## DirectInput acquisition rules

1. Create interface with `DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, …, nullptr)`.
2. Enumerate with `EnumDevices(DI8DEVCLASS_GAMECTRL, …, nullptr, DIEDFL_ATTACHEDONLY)`.
3. For each enumerated device, `CreateDevice(instance.guidInstance)`, then `GetDeviceInfo` to read VID/PID. First PID match against `device_table` wins; ignore the rest.
4. `SetDataFormat(&c_dfDIJoystick2)`.
5. Cooperative level depends on config:
   - **Default (`overrideGHub = false`)**: `DISCL_BACKGROUND | DISCL_NONEXCLUSIVE`. No `SetProperty` calls. G HUB remains authoritative for range / sensitivity / centering.
   - **Override on (`overrideGHub = true`)**: `DISCL_FOREGROUND | DISCL_EXCLUSIVE`. Plugin issues `SetProperty(DIPROP_RANGE, …)` with the user-chosen operating-range in ±degrees (converted to DI axis units) and `DIPROP_DEADZONE`/`DIPROP_SATURATION` as needed.
6. `Acquire()`. On `DIERR_NOTACQUIRED` from `Poll` later, `Acquire()` again before re-polling.
7. Dedicated polling thread at 250 Hz writes the latest `DIJOYSTATE2`-derived snapshot into an `std::atomic<snapshot_idx>` double-buffer. Game-thread RTTI hooks read the current index.

## Vehicle input hook targets

Hooked via RED4ext's RTTI hook surface. Targets, in priority order:

1. `VehicleComponent::GetInputValueFloat(CName)` — primary hot path. Intercepted for `"Steer"`, `"Accelerate"`, `"Brake"`; returns the snapshot's derived value. Falls through to the original for any other input name.
2. `VehicleComponent::GetInputValueVector(CName)` — used for `"VehicleMovement"` if the build exposes it. Returns `{ steer, throttle - brake }`.
3. `PlayerPuppet::GetInputValueFloat(CName)` — fallback target. If 1 fails to attach, engage this and mount-check via `GameInstance::GetMountedVehicle(puppet)` before overriding.
4. Vehicle movement blackboard — last-resort fallback if neither of the above is reachable.

If all four targets fail (RED4ext `DetourAttachEx` returning error code 6 or similar), the plugin logs `[gwheel] no vehicle hook attached — input will not reach the game` and continues running: device polling and FFB still work, but the car won't respond. The `IsPluginReady()` native reflects this (returns `false`).

Signatures are confirmed against the current CP2077 RTTI dump at implementation time (use [RED4.RTTIDumper](https://github.com/WopsS/RED4.RTTIDumper) against the installed game).

## Native function surface (RTTI)

Class: `GWheelNative`, parent: `IScriptable`. Registered in `PostRegisterTypes()` using `CClassFunction::Create(&cls, "Name", "Name", &fn, { .isNative = true })` + `AddParam("TypeName","argName")` + `SetReturnType("ReturnType")` + `cls.RegisterFunction(fn)`.

Each function uses the canonical shape:

```cpp
void Fn(RED4ext::IScriptable* aContext,
        RED4ext::CStackFrame* aFrame,
        OutT* aOut,
        int64_t /*unused*/);
```

Parameters read sequentially with `RED4ext::GetParameter(aFrame, &arg)`, then `aFrame->code++;`. Return value assigned via `if (aOut) *aOut = …;`.

| Native | Params | Returns | Purpose |
| --- | --- | --- | --- |
| `GetVersion` | — | `String` | Plugin version string. Hardcoded `"0.1.0"`. |
| `IsPluginReady` | — | `Bool` | `true` iff device is acquired AND at least one vehicle hook is attached. |
| `GetDeviceInfo` | — | `String` | Human-readable summary: model name, PID, FFB flag, axis/button count. Empty string if no device. |
| `HasFFB` | — | `Bool` | Cached `DIDC_FORCEFEEDBACK` check result. Drives Settings UI disabled-state. |
| `ReadConfig` | — | `String` | Returns current config as a JSON string (same schema as `config.json` on disk). |
| `ApplyConfig` | `blob: String` | `Bool` | Parses JSON, validates each field, atomically swaps the live snapshot, writes to disk. Returns `true` on success. |

Redscript side:

```swift
public native class GWheelNative extends IScriptable {
    public native func GetVersion() -> String;
    public native func IsPluginReady() -> Bool;
    public native func GetDeviceInfo() -> String;
    public native func HasFFB() -> Bool;
    public native func ReadConfig() -> String;
    public native func ApplyConfig(blob: String) -> Bool;
}
```

## Config JSON schema

Path: `<CP2077>/red4ext/plugins/gwheel/config.json`. Loaded at plugin `Load`, saved on every `ApplyConfig`, also saved on `Unload`.

```json
{
  "version": 1,

  "input": {
    "enabled": true,
    "steerDeadzonePct": 2,
    "throttleDeadzonePct": 2,
    "brakeDeadzonePct": 2,
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
    "rangeDeg": 900,
    "centeringSpringPct": 50
  },

  "perVehicle": {
    "car":        { "steeringMultiplier": 1.0, "responseDelayMs": 20 },
    "motorcycle": { "steeringMultiplier": 1.2, "responseDelayMs": 10 },
    "truck":      { "steeringMultiplier": 0.8, "responseDelayMs": 40 },
    "van":        { "steeringMultiplier": 0.9, "responseDelayMs": 30 }
  }
}
```

**Defaults** (constants in `config.cpp`):

| Field | Default | Range | Notes |
| --- | --- | --- | --- |
| `input.enabled` | `true` | — | Master toggle for wheel input |
| `input.steerDeadzonePct` | `2` | 0 – 20 | Applied after G HUB's curve |
| `input.throttleDeadzonePct` | `2` | 0 – 20 | |
| `input.brakeDeadzonePct` | `2` | 0 – 20 | |
| `input.responseCurve` | `"default"` | `"default"` \| `"subdued"` \| `"sharp"` | Shapes axis response pre-game |
| `ffb.enabled` | `true` | — | Plugin-generated effects (collision/texture) only |
| `ffb.strengthPct` | `80` | 0 – 100 | Scales plugin effect magnitudes; does not affect G HUB spring |
| `ffb.debugLogging` | `false` | — | Verbose log of every effect start/stop |
| `override.enabled` | `false` | — | Gates every field below |
| `override.sensitivity` | `1.0` | 0.25 – 2.00 | Only when `override.enabled` |
| `override.rangeDeg` | `900` | 200 – 900 | Only when `override.enabled`; applied via `SetProperty(DIPROP_RANGE)` |
| `override.centeringSpringPct` | `50` | 0 – 100 | Only when `override.enabled`; plugin-driven centering spring |
| `perVehicle.*` | as shown | positive floats / ints | Per-vehicle tuning |

Validation rules: reject with `false` from `ApplyConfig` if any field is out of range, the JSON doesn't parse, or `version` is unknown. Log each rejection.

## FFB effect model

Plugin-owned effects (game-driven): `GUID_ConstantForce`, `GUID_Damper`, `GUID_Sine`, `GUID_RampForce`. Created up front with `IDirectInputDevice8::CreateEffect`, reparameterized per event via `IDirectInputEffect::SetParameters`, started with `Start(1, 0)`, stopped with `Stop()`. Released on plugin unload.

Plugin does **not** create a spring (`GUID_Spring`) unless `override.enabled && override.centeringSpringPct > 0`. Otherwise centering stays with G HUB.

Every effect magnitude is multiplied by `ffb.strengthPct / 100.0` before `SetParameters`.

If `HasFFB()` is false, every FFB API call is a no-op that returns success. Callers do not need to branch; the plugin centralizes the check.

## G HUB coexistence

| Knob | Default owner | Mod owner when `override.enabled = true` |
| --- | --- | --- |
| Rotation range (°) | G HUB | Mod (via `SetProperty(DIPROP_RANGE)`) |
| Sensitivity curve | G HUB | Mod (pre-game shaping) |
| Centering spring | G HUB | Mod (plugin-created `GUID_Spring`) |
| Collision FFB | Mod | Mod |
| Surface-texture FFB | Mod | Mod |
| In-game deadzones | Mod | Mod |
| Per-vehicle response | Mod | Mod |

## Build / deploy

- Build: `cmake --preset default && cmake --build --preset default` → `gwheel.dll`.
- Deploy: FOMOD installs `gwheel.dll` to `<CP2077>/red4ext/plugins/gwheel/` and `gwheel_settings.reds` to `<CP2077>/r6/scripts/gwheel/`.
- Runtime deps: [RED4ext](https://github.com/WopsS/RED4ext), [redscript](https://github.com/jac3km4/redscript), [Mod Settings](https://github.com/jackhumbert/mod_settings), [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl).

## Sources

- [RED4ext documentation](https://docs.red4ext.com)
- [Creating a plugin with RedLib](https://docs.red4ext.com/mod-developers/creating-a-plugin-with-redlib)
- [Adding a Native Function](https://docs.red4ext.com/mod-developers/adding-a-native-function)
- [RED4ext & RED4ext.SDK](https://docs.red4ext.com/mod-developers/red4ext-and-red4ext.sdk)
- [RED4ext on GitHub](https://github.com/WopsS/RED4ext)
- [RED4ext.SDK on GitHub](https://github.com/WopsS/RED4ext.SDK)
- [RED4.RTTIDumper](https://github.com/WopsS/RED4.RTTIDumper)
- [Mod Settings](https://github.com/jackhumbert/mod_settings) / [Nexus listing](https://www.nexusmods.com/cyberpunk2077/mods/4885)
- [redscript](https://github.com/jac3km4/redscript) / [redscript wiki](https://wiki.redmodding.org/redscript)
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl)
- [IDirectInputDevice8](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417816(v=vs.85))
- [IDirectInput8](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417799(v=vs.85))
- [IDirectInputEffect](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417936(v=vs.85))
- [the-sz USB ID database — Logitech 046D](https://the-sz.com/products/usbid/index.php?v=0x046D)
- [G923 Linux kernel driver project](https://github.com/ZRtmWrJqXcjbqBLIMBYMCeUw/Logitech-G923-Linux-Kernel-Driver)
- [FOMOD ModuleConfig schema](https://fomod-docs.readthedocs.io/en/latest/_static/ModuleConfig.html)
