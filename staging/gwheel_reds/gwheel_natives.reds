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

// Wheel-button -> in-game action bindings. `action` is a free-form string
// (e.g. "handbrake", "horn") whose meaning is interpreted by the native
// action-dispatch detour once gwheel/src/sigs.h is populated.
public static native func GWheel_SetButtonBinding(button: Int32, action: String) -> Bool;
public static native func GWheel_ClearButtonBinding(button: Int32) -> Bool;
public static native func GWheel_GetButtonBinding(button: Int32) -> String;
public static native func GWheel_IsButtonPressed(button: Int32) -> Bool;
public static native func GWheel_GetLastPressedButton() -> Int32;
public static native func GWheel_GetButtonBindingsJson() -> String;
