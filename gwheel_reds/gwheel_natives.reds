// Declarations for native functions registered by gwheel.dll (RED4ext plugin).
// The plugin registers these in PostRegisterTypes() via CGlobalFunction::Create.
// Keep this file in sync with gwheel/src/rtti.cpp.

public static native func GWheel_GetVersion() -> String;
public static native func GWheel_IsPluginReady() -> Bool;
public static native func GWheel_GetDeviceInfo() -> String;
public static native func GWheel_HasFFB() -> Bool;
public static native func GWheel_ReadConfig() -> String;

public static native func GWheel_MaybeOverrideFloat(inputName: CName, original: Float) -> Float;

public static native func GWheel_SetInputEnabled(v: Bool) -> Bool;
public static native func GWheel_SetSteerDeadzonePct(pct: Int32) -> Bool;
public static native func GWheel_SetThrottleDeadzonePct(pct: Int32) -> Bool;
public static native func GWheel_SetBrakeDeadzonePct(pct: Int32) -> Bool;
public static native func GWheel_SetResponseCurve(curve: String) -> Bool;

public static native func GWheel_SetFfbEnabled(v: Bool) -> Bool;
public static native func GWheel_SetFfbStrengthPct(pct: Int32) -> Bool;
public static native func GWheel_SetFfbDebugLogging(v: Bool) -> Bool;

public static native func GWheel_SetOverrideEnabled(v: Bool) -> Bool;
public static native func GWheel_SetOverrideSensitivity(v: Float) -> Bool;
public static native func GWheel_SetOverrideRangeDeg(deg: Int32) -> Bool;
public static native func GWheel_SetOverrideCenteringSpringPct(pct: Int32) -> Bool;
