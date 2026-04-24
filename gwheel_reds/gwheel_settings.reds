// In-game settings page for the gwheel mod.
//
// Settings live in the game's own Settings menu under "Mod Settings → G-series
// Wheel", powered by jackhumbert/mod_settings. Mod Settings persists field
// values across runs; OnModSettingsChange fires whenever the user accepts a
// change, at which point we push the new values to the red4ext plugin via
// native functions registered in gwheel.dll.
//
// Design intent: respect Logitech G HUB by default. The "Advanced" section
// contains every knob that duplicates a G HUB setting; all of those fields
// are gated on overrideGHub.

public class GWheelSettings extends IScriptable {

  // ---- Input --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable wheel input")
  @runtimeProperty("ModSettings.description", "Master toggle. When off, the mod stops injecting wheel values into vehicle input.")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Treat clutch as brake")
  @runtimeProperty("ModSettings.description", "Treat the clutch pedal as a second brake. The clutch's softer spring makes it easier to modulate than the stiff brake pedal. Both pedals brake when enabled; whichever is pressed deeper wins.")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let clutchAsBrake: Bool = false;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.description", "Master toggle for plugin-driven FFB: speed-gated self-centering, collision, surface texture. Note: if you toggle this off and then back on mid-session with G HUB's 'Centering Spring in Non Force Feedback Games' checkbox enabled, G HUB's canned spring may persist alongside the mod's physics spring. Workaround: cycle that G HUB checkbox off and on to force G HUB to re-evaluate. This is a G HUB limitation — its Properties API for signaling 'game is now producing FFB again' is broken.")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  @runtimeProperty("ModSettings.description", "Enables verbose plugin logging. Find logs at red4ext/logs/gwheel-*.log.")
  let ffbDebugLogging: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.description", "Mod-side multiplier on every force this mod generates (spring, active torque, damper, road surface). Composes with G HUB's TRUEFORCE Torque. The Logi Properties API is broken on recent G HUB builds, so we apply the scaling before the effect reaches the SDK rather than via overallGain.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let ffbTorquePct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Stationary threshold (m/s)")
  @runtimeProperty("ModSettings.description", "Below this speed the wheel is free (no centering forces). ~0.5 m/s is a slow walking pace.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "5.0")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let stationaryThresholdMps: Float = 0.5;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Cornering feedback (%)")
  @runtimeProperty("ModSettings.description", "Extra spring stiffness added while the car is rotating hard (cornering, sliding). Approximates dynamic tire alignment torque — hard corners feel heavier than straight-line driving.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let yawFeedbackPct: Int32 = 50;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Active torque (%)")
  @runtimeProperty("ModSettings.description", "Active push-back toward center, proportional to speed × steering angle. Models tire alignment torque — the faster you go and the further off-center the wheel is, the harder the car pushes it back. Peak at max deflection + cruise speed. 0 = disabled (passive spring only).")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let activeTorqueStrengthPct: Int32 = 100;

  // ---- Wheel hardware ---------------------------------------------------
  //
  // Operating range and sensitivity are owned by Logitech G HUB (per-
  // profile). The mod reads G HUB's current value and auto-scales FFB to
  // match, so a 900-degree rotation isn't weighed down by SAT meant for a
  // 180. Change the operating range / sensitivity in G HUB's Cyberpunk
  // 2077 profile.

  // ---- Button bindings (all user-assignable) -----------------------------
  //
  // D-pad + A/B/X/Y bindings apply WHILE DRIVING. When any fullscreen
  // menu is open (pause, map, inventory, etc.), the plugin overrides
  // these 8 with hardcoded gamepad-nav (D-pad = arrow keys, A = Enter,
  // B = Escape, X/Y = nothing) so the wheel navigates menus like a
  // controller.
  //
  // IMPORTANT: clear the D-pad and A/B/X/Y keyboard bindings in G HUB's
  // Cyberpunk profile. Otherwise G HUB + plugin both fire keyboard
  // events and you'll get doubled keypresses. Other controls (paddles,
  // +/-, scroll click, etc.) can still be bound in G HUB if you want,
  // but the plugin's in-vehicle bindings below are the recommended
  // source of truth.

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Left paddle shifter")
  let bindPaddleLeft: GWheelAction = GWheelAction.Handbrake;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Right paddle shifter")
  let bindPaddleRight: GWheelAction = GWheelAction.Handbrake;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Up (in-vehicle)")
  let bindDpadUp: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Down (in-vehicle)")
  let bindDpadDown: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Left (in-vehicle)")
  let bindDpadLeft: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Right (in-vehicle)")
  let bindDpadRight: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "A button (in-vehicle)")
  let bindButtonA: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "B button (in-vehicle)")
  let bindButtonB: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "X button (in-vehicle)")
  let bindButtonX: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Y button (in-vehicle)")
  let bindButtonY: GWheelAction = GWheelAction.None;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Start button")
  let bindStart: GWheelAction = GWheelAction.Pause;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Select / View button")
  let bindSelect: GWheelAction = GWheelAction.OpenMap;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "LSB (left stick click)")
  let bindLSB: GWheelAction = GWheelAction.Autodrive;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "RSB (right stick click)")
  let bindRSB: GWheelAction = GWheelAction.ExitVehicle;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Plus (+) button")
  let bindPlus: GWheelAction = GWheelAction.CameraCycleForward;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Minus (-) button")
  let bindMinus: GWheelAction = GWheelAction.RearViewCamera;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll click (Return)")
  let bindScrollClick: GWheelAction = GWheelAction.Horn;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll clockwise")
  let bindScrollCW: GWheelAction = GWheelAction.NextWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll counter-clockwise")
  let bindScrollCCW: GWheelAction = GWheelAction.PrevWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Xbox / Guide button")
  let bindXbox: GWheelAction = GWheelAction.None;

  // ---- Listener callbacks (invoked by Mod Settings, NOT cb funcs) --------

  public func OnModSettingsChange() -> Void {
    this.Push();
  }

  public func Push() -> Void {
    GWheel_SetInputEnabled(this.inputEnabled);
    GWheel_SetClutchAsBrake(this.clutchAsBrake);

    GWheel_SetFfbEnabled(this.ffbEnabled);
    GWheel_SetFfbDebugLogging(this.ffbDebugLogging);
    GWheel_SetFfbTorquePct(this.ffbTorquePct);

    GWheel_SetStationaryThresholdMps(this.stationaryThresholdMps);
    GWheel_SetYawFeedbackPct(this.yawFeedbackPct);
    GWheel_SetActiveTorqueStrengthPct(this.activeTorqueStrengthPct);

    // Input IDs match the PhysicalInput enum in
    // gwheel/src/input_bindings.h. D-pad + A/B/X/Y (ids 2-9) are
    // in-vehicle bindings here; the plugin overrides them with
    // gamepad-nav whenever a menu is open.
    GWheel_SetInputBinding(0,  EnumInt(this.bindPaddleLeft));
    GWheel_SetInputBinding(1,  EnumInt(this.bindPaddleRight));
    GWheel_SetInputBinding(2,  EnumInt(this.bindDpadUp));
    GWheel_SetInputBinding(3,  EnumInt(this.bindDpadDown));
    GWheel_SetInputBinding(4,  EnumInt(this.bindDpadLeft));
    GWheel_SetInputBinding(5,  EnumInt(this.bindDpadRight));
    GWheel_SetInputBinding(6,  EnumInt(this.bindButtonA));
    GWheel_SetInputBinding(7,  EnumInt(this.bindButtonB));
    GWheel_SetInputBinding(8,  EnumInt(this.bindButtonX));
    GWheel_SetInputBinding(9,  EnumInt(this.bindButtonY));
    GWheel_SetInputBinding(10, EnumInt(this.bindStart));
    GWheel_SetInputBinding(11, EnumInt(this.bindSelect));
    GWheel_SetInputBinding(12, EnumInt(this.bindLSB));
    GWheel_SetInputBinding(13, EnumInt(this.bindRSB));
    GWheel_SetInputBinding(14, EnumInt(this.bindPlus));
    GWheel_SetInputBinding(15, EnumInt(this.bindMinus));
    GWheel_SetInputBinding(16, EnumInt(this.bindScrollClick));
    GWheel_SetInputBinding(17, EnumInt(this.bindScrollCW));
    GWheel_SetInputBinding(18, EnumInt(this.bindScrollCCW));
    GWheel_SetInputBinding(19, EnumInt(this.bindXbox));
  }
}

// Actions the plugin knows how to dispatch. Indices must match the Action
// enum in gwheel/src/input_bindings.h — same values, same order.
enum GWheelAction {
  None = 0,
  Horn = 1,
  Headlights = 2,
  Handbrake = 3,
  Autodrive = 4,
  ExitVehicle = 5,
  CameraCycleForward = 6,
  RearViewCamera = 7,
  CameraReset = 8,
  ZoomIn = 9,
  ZoomOut = 10,
  ShootPrimary = 11,
  ShootSecondary = 12,
  ShootTertiary = 13,
  NextWeapon = 14,
  PrevWeapon = 15,
  WeaponSlot1 = 16,
  WeaponSlot2 = 17,
  SwitchWeapons = 18,
  HolsterWeapon = 19,
  OpenMap = 20,
  OpenJournal = 21,
  OpenInventory = 22,
  OpenPhone = 23,
  OpenPerks = 24,
  OpenCrafting = 25,
  QuickSave = 26,
  RadioMenu = 27,
  UseConsumable = 28,
  IconicCyberware = 29,
  Pause = 30,
  Tag = 31,
  CallVehicle = 32,
  MenuConfirm = 33,
  MenuCancel = 34,
  MenuUp = 35,
  MenuDown = 36,
  MenuLeft = 37,
  MenuRight = 38,
}

// Attach our settings instance to the player puppet so it lives for the
// session. On attach, register with Mod Settings and push current values to
// the native plugin.

@addField(PlayerPuppet)
public let m_gwheelSettings: ref<GWheelSettings>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if !IsDefined(this.m_gwheelSettings) {
    this.m_gwheelSettings = new GWheelSettings();
    ModSettings.RegisterListenerToClass(this.m_gwheelSettings);
    ModSettings.RegisterListenerToModifications(this.m_gwheelSettings);
    this.m_gwheelSettings.Push();
  }
  return result;
}
