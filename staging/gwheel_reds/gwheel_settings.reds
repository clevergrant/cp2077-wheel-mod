// In-game settings page for the gwheel mod.
//
// Single GWheelSettings class. Categories (`ModSettings.category` +
// `ModSettings.category.order`) provide visual grouping on the scrolling
// settings panel.

public class GWheelSettings extends IScriptable {

  // ---- Hidden capability flags (auto-set in OnGameAttached) --------------
  //
  // ModSettings.hidden = "true" makes these invisible in the settings UI.
  // Requires our patched mod_settings build (vendor/mod_settings/). Other
  // fields reference these as `ModSettings.dependency` targets to hide
  // entire sections on wheels/controllers without the relevant hardware.

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasFfbHardware: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRevLeds: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRightCluster: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasYokeHardware: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRudderPedals: Bool = true;

  // ---- Wheel / Yoke input -------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel & Yoke input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Enable input")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel & Yoke input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Treat clutch as brake")
  @runtimeProperty("ModSettings.description", "The clutch acts just like the brake pedal. Useful for stiff brake pedals, since there's no gear system in the game.")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let clutchAsBrake: Bool = true;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbTorquePct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Cornering feedback (%)")
  @runtimeProperty("ModSettings.description", "Adds spring stiffness while the car is rotating.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let yawFeedbackPct: Int32 = 50;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Active torque (%)")
  @runtimeProperty("ModSettings.description", "How hard the wheel pushes toward center, scaled by speed and steering angle.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let activeTorqueStrengthPct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Stationary threshold (m/s)")
  @runtimeProperty("ModSettings.description", "Below this speed the wheel has no centering force.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "5.0")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let stationaryThresholdMps: Float = 0.5;

  // ---- Flight Yoke Pitch (AY90 / AY210) -----------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Flight Yoke (Pitch axis)")
  @runtimeProperty("ModSettings.category.order", "250")
  @runtimeProperty("ModSettings.displayName", "Yoke pitch centering stiffness")
  @runtimeProperty("ModSettings.description", "Centering spring force applied to keep the push/pull pitch column stable while driving.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "1.0")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.dependency", "hasYokeHardware")
  let pitchSpringStrength: Float = 0.8;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Flight Yoke (Pitch axis)")
  @runtimeProperty("ModSettings.category.order", "250")
  @runtimeProperty("ModSettings.displayName", "Yoke pitch damping")
  @runtimeProperty("ModSettings.description", "Viscous damping to prevent push/pull column oscillation.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "1.0")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.dependency", "hasYokeHardware")
  let pitchDamperStrength: Float = 0.5;

  // ---- Rudder Pedals & Toe Brakes -----------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rudder Pedals & Toe Brakes")
  @runtimeProperty("ModSettings.category.order", "260")
  @runtimeProperty("ModSettings.displayName", "Invert left toe brake (Brake)")
  @runtimeProperty("ModSettings.description", "Invert polarity of left toe brake pedal if it applies 100% brake at rest.")
  @runtimeProperty("ModSettings.dependency", "hasRudderPedals")
  let leftToeBrakeInvert: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rudder Pedals & Toe Brakes")
  @runtimeProperty("ModSettings.category.order", "260")
  @runtimeProperty("ModSettings.displayName", "Invert right toe brake (Throttle)")
  @runtimeProperty("ModSettings.description", "Invert polarity of right toe brake pedal if it applies 100% throttle at rest.")
  @runtimeProperty("ModSettings.dependency", "hasRudderPedals")
  let rightToeBrakeInvert: Bool = false;

  // ---- Rev-strip LEDs -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Enable rev-strip LEDs")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Audio visualizer during music")
  @runtimeProperty("ModSettings.description", "Reflects game audio envelope on the rev-strip when music or radio plays.")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledVisualizerWhileMusic: Bool = true;

  // ---- Button bindings ----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Left paddle shifter")
  let bindPaddleLeft: GWheelAction = GWheelAction.ShootSecondary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Right paddle shifter")
  let bindPaddleRight: GWheelAction = GWheelAction.ShootPrimary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Up")
  let bindDpadUp: GWheelAction = GWheelAction.MenuUp;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Down")
  let bindDpadDown: GWheelAction = GWheelAction.MenuDown;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Left")
  let bindDpadLeft: GWheelAction = GWheelAction.MenuLeft;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Right")
  let bindDpadRight: GWheelAction = GWheelAction.MenuRight;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "A button")
  let bindButtonA: GWheelAction = GWheelAction.MenuConfirm;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "B button (in-vehicle)")
  let bindButtonB: GWheelAction = GWheelAction.Handbrake;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "X button (in-vehicle)")
  let bindButtonX: GWheelAction = GWheelAction.Horn;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Y button (in-vehicle)")
  let bindButtonY: GWheelAction = GWheelAction.Autodrive;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Start button")
  let bindStart: GWheelAction = GWheelAction.Pause;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Select / View button")
  let bindSelect: GWheelAction = GWheelAction.OpenMap;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "LSB (left stick click)")
  let bindLSB: GWheelAction = GWheelAction.OpenPhone;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "RSB (right stick click)")
  let bindRSB: GWheelAction = GWheelAction.RadioMenu;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Xbox / Guide button")
  let bindXbox: GWheelAction = GWheelAction.None;

  // ---- Right-cluster controls ---------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Right-cluster controls")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Plus button")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindPlus: GWheelAction = GWheelAction.NextWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Right-cluster controls")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Minus button")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindMinus: GWheelAction = GWheelAction.PrevWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Right-cluster controls")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll wheel click")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollClick: GWheelAction = GWheelAction.SwitchWeapons;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Right-cluster controls")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll clockwise")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollCW: GWheelAction = GWheelAction.RadioNext;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Right-cluster controls")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll counter-clockwise")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollCCW: GWheelAction = GWheelAction.None;

  // ---- Startup ------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Startup")
  @runtimeProperty("ModSettings.category.order", "600")
  @runtimeProperty("ModSettings.displayName", "Startup greeting")
  let handshakePlayOnStart: Bool = false;

  // ---- Debug --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Debug")
  @runtimeProperty("ModSettings.category.order", "700")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  let ffbDebugLogging: Bool = false;

  // ---- Listener callbacks -------------------------------------------------

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

    // Flight Yoke & Rudder Pedals
    GWheel_SetPitchSpringStrength(this.pitchSpringStrength);
    GWheel_SetPitchDamperStrength(this.pitchDamperStrength);
    GWheel_SetLeftToeBrakeInvert(this.leftToeBrakeInvert);
    GWheel_SetRightToeBrakeInvert(this.rightToeBrakeInvert);

    GWheel_SetHandshakePlayOnStart(this.handshakePlayOnStart);

    GWheel_SetLedEnabled(this.ledEnabled);
    GWheel_SetLedVisualizerWhileMusic(this.ledVisualizerWhileMusic);

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

enum GWheelAction {
  None = 0,
  Horn = 1,
  Headlights = 2,
  Handbrake = 3,
  Autodrive = 4,
  ExitVehicle = 5,
  CallVehicle = 6,
  CameraCycleForward = 7,
  RearViewCamera = 8,
  CameraReset = 9,
  ShootPrimary = 10,
  ShootSecondary = 11,
  ShootTertiary = 12,
  NextWeapon = 13,
  PrevWeapon = 14,
  WeaponSlot1 = 15,
  WeaponSlot2 = 16,
  SwitchWeapons = 17,
  HolsterWeapon = 18,
  RadioMenu = 19,
  RadioNext = 20,
  UseConsumable = 21,
  IconicCyberware = 22,
  QuickSave = 23,
  OpenMap = 24,
  OpenJournal = 25,
  OpenInventory = 26,
  OpenPhone = 27,
  OpenPerks = 28,
  OpenCrafting = 29,
  Pause = 30,
  MenuConfirm = 31,
  MenuCancel = 32,
  MenuUp = 33,
  MenuDown = 34,
  MenuLeft = 35,
  MenuRight = 36,
}

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
