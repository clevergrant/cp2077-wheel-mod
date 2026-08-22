// Declarations for native functions registered by gwheel.dll.
// Keep this file in sync with gwheel/src/rtti.cpp::PostRegisterTypes.

public static native func GWheel_GetVersion() -> String;
public static native func GWheel_IsPluginReady() -> Bool;
public static native func GWheel_GetDeviceInfo() -> String;
public static native func GWheel_HasFFB() -> Bool;
public static native func GWheel_ReadConfig() -> String;

// Controller-model auto-discovery.
public static native func GWheel_DetectedHasRightCluster() -> Bool;
public static native func GWheel_DetectedHasFfbHardware() -> Bool;
public static native func GWheel_DetectedHasRevLeds() -> Bool;
public static native func GWheel_DetectedHasYokeHardware() -> Bool;
public static native func GWheel_DetectedHasRudderPedals() -> Bool;
public static native func GWheel_DetectedModelName() -> String;

public static native func GWheel_SetInputEnabled(v: Bool) -> Bool;
public static native func GWheel_SetClutchAsBrake(v: Bool) -> Bool;
public static native func GWheel_SetResponseCurve(v: String) -> Bool;

public static native func GWheel_SetFfbEnabled(v: Bool) -> Bool;
public static native func GWheel_SetFfbDebugLogging(v: Bool) -> Bool;
public static native func GWheel_SetFfbTorquePct(pct: Int32) -> Bool;

// Flight Yoke & Rudder Pedals
public static native func GWheel_SetPitchSpringStrength(v: Float) -> Bool;
public static native func GWheel_SetPitchDamperStrength(v: Float) -> Bool;
public static native func GWheel_SetPitchAction(v: Int32) -> Bool;
public static native func GWheel_SetLeftToeBrakeInvert(v: Bool) -> Bool;
public static native func GWheel_SetRightToeBrakeInvert(v: Bool) -> Bool;

// Startup handshake (LED sweep + 4 triplets + centering breath) on wheel bind.
public static native func GWheel_SetHandshakePlayOnStart(v: Bool) -> Bool;

// Phase-1 physics FFB: speed-gated self-centering spring with yaw-rate bonus.
public static native func GWheel_SetStationaryThresholdMps(mps: Float) -> Bool;
public static native func GWheel_SetYawFeedbackPct(pct: Int32) -> Bool;
public static native func GWheel_SetActiveTorqueStrengthPct(pct: Int32) -> Bool;

// Rev-strip LED bar on top of the wheel.
public static native func GWheel_SetLedEnabled(v: Bool) -> Bool;
public static native func GWheel_SetLedVisualizerWhileMusic(v: Bool) -> Bool;
public static native func GWheel_SetMusicProcessName(v: String) -> Bool;

// Per-physical-input action binding.
public static native func GWheel_SetInputBinding(inputId: Int32, action: Int32) -> Bool;

// Tracks the player's currently-mounted vehicle.
public static native func GWheel_SetPlayerVehicle(v: ref<VehicleObject>) -> Bool;
public static native func GWheel_ClearPlayerVehicle() -> Bool;

// Vehicle telemetry pushed from Blackboard listeners.
public static native func GWheel_SetEngineRpmNormalized(v: Float) -> Bool;
public static native func GWheel_SetRadioActive(v: Bool) -> Bool;

// Collision / bump feedback.
public static native func GWheel_OnVehicleBump(v: ref<VehicleObject>, lateralKick: Float) -> Bool;
public static native func GWheel_OnVehicleHit(v: ref<VehicleObject>, lateralKick: Float) -> Bool;

// Per-wheel road material report.
public static native func GWheel_OnWheelMaterial(wheelIdx: Int32, material: CName) -> Bool;
