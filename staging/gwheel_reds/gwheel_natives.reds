// Declarations for native functions registered by gwheel.dll.
// Keep this file in sync with gwheel/src/rtti.cpp::PostRegisterTypes.

public static native func GWheel_GetVersion() -> String;
public static native func GWheel_IsPluginReady() -> Bool;
public static native func GWheel_GetDeviceInfo() -> String;
public static native func GWheel_HasFFB() -> Bool;
public static native func GWheel_ReadConfig() -> String;

public static native func GWheel_SetInputEnabled(v: Bool) -> Bool;
public static native func GWheel_SetSteerDeadzonePct(pct: Int32) -> Bool;
public static native func GWheel_SetThrottleDeadzonePct(pct: Int32) -> Bool;
public static native func GWheel_SetBrakeDeadzonePct(pct: Int32) -> Bool;

public static native func GWheel_SetFfbEnabled(v: Bool) -> Bool;
public static native func GWheel_SetFfbStrengthPct(pct: Int32) -> Bool;
public static native func GWheel_SetFfbDebugLogging(v: Bool) -> Bool;

public static native func GWheel_SetOverrideEnabled(v: Bool) -> Bool;
public static native func GWheel_SetOverrideSensitivity(v: Float) -> Bool;
public static native func GWheel_SetOverrideRangeDeg(deg: Int32) -> Bool;
public static native func GWheel_SetOverrideCenteringSpringPct(pct: Int32) -> Bool;

// Per-physical-input action binding. inputId is one of the stable IDs in
// gwheel/src/input_bindings.h (0 = PaddleLeft, 1 = PaddleRight, etc.).
// action is a GWheelAction enum value cast to Int32; the plugin dispatches
// it as a Windows SendInput event on rising/falling edges.
public static native func GWheel_SetInputBinding(inputId: Int32, action: Int32) -> Bool;

// Tracks the player's currently-mounted vehicle. The plugin's vehicle-
// input detour fires for every visible vehicle each tick; without this
// filter, our steering/throttle/brake writes would propagate to all of
// them (remote-driving parked cars, etc.). Call SetPlayerVehicle on
// mount and ClearPlayerVehicle on dismount from VehicleComponent event
// wrappers.
public static native func GWheel_SetPlayerVehicle(v: ref<VehicleObject>) -> Bool;
public static native func GWheel_ClearPlayerVehicle() -> Bool;
