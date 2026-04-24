// Declarations for native functions registered by gwheel.dll.
// Keep this file in sync with gwheel/src/rtti.cpp::PostRegisterTypes.

public static native func GWheel_GetVersion() -> String;
public static native func GWheel_IsPluginReady() -> Bool;
public static native func GWheel_GetDeviceInfo() -> String;
public static native func GWheel_HasFFB() -> Bool;
public static native func GWheel_ReadConfig() -> String;

public static native func GWheel_SetInputEnabled(v: Bool) -> Bool;
public static native func GWheel_SetClutchAsBrake(v: Bool) -> Bool;

public static native func GWheel_SetFfbEnabled(v: Bool) -> Bool;
public static native func GWheel_SetFfbDebugLogging(v: Bool) -> Bool;
public static native func GWheel_SetFfbTorquePct(pct: Int32) -> Bool;

// Phase-1 physics FFB: speed-gated self-centering spring with yaw-rate bonus.
public static native func GWheel_SetStationaryThresholdMps(mps: Float) -> Bool;
public static native func GWheel_SetYawFeedbackPct(pct: Int32) -> Bool;
public static native func GWheel_SetActiveTorqueStrengthPct(pct: Int32) -> Bool;

// Rev-strip LED bar on top of the wheel (G29/G920/G923). VisualizerWhileMusic
// swaps the speed-driven rev bar for a WASAPI-loopback audio visualizer
// whenever system audio is playing; falls back to rev-strip on silence.
public static native func GWheel_SetLedEnabled(v: Bool) -> Bool;
public static native func GWheel_SetLedVisualizerWhileMusic(v: Bool) -> Bool;

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

// Vehicle telemetry pushed from Blackboard listeners (gwheel_vehicle_signals.reds).
// Normalized engine RPM in [0..1] = RPMValue / VehEngineData.MaxRPM().
// Radio state mirrors VehicleDef.VehRadioState.
public static native func GWheel_SetEngineRpmNormalized(v: Float) -> Bool;
public static native func GWheel_SetRadioActive(v: Bool) -> Bool;

// Collision / bump feedback. lateralKick is the world-space hit direction
// dotted with the vehicle's right vector, signed in [-1..+1] (negative =
// struck on left). The plugin filters out events from non-player vehicles
// using the handle; we still forward every event for simplicity.
public static native func GWheel_OnVehicleBump(v: ref<VehicleObject>, lateralKick: Float) -> Bool;
public static native func GWheel_OnVehicleHit(v: ref<VehicleObject>, lateralKick: Float) -> Bool;

// Per-wheel road material report. Called from the redscript raycast
// poller (see gwheel_surface.reds) when a wheel's material CName
// changes. wheelIdx is 0..3, material is the physics material CName
// (e.g. `asphalt`, `dirt`, `metal`). C++ logs the transition and
// eventually drives surface-aware FFB off the category.
public static native func GWheel_OnWheelMaterial(wheelIdx: Int32, material: CName) -> Bool;
