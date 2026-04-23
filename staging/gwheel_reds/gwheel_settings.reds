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
  @runtimeProperty("ModSettings.displayName", "Steering deadzone (%)")
  @runtimeProperty("ModSettings.description", "Small centre deadzone applied after G HUB's own curve.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let steerDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Throttle deadzone (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let throttleDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Brake deadzone (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let brakeDeadzonePct: Int32 = 2;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.description", "Plugin-driven effects only (collision, surface texture). Centering spring stays with G HUB.")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.description", "Scales plugin-generated effects. Does not affect G HUB's centering spring.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let ffbStrengthPct: Int32 = 80;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  @runtimeProperty("ModSettings.description", "Enables verbose plugin logging. Find logs at red4ext/logs/gwheel-*.log.")
  let ffbDebugLogging: Bool = false;

  // ---- Advanced (override G HUB) -----------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Override G HUB settings")
  @runtimeProperty("ModSettings.description", "When ON, this mod takes control of sensitivity, rotation range, and centering spring. When OFF (default), those remain managed by Logitech G HUB.")
  let overrideGHub: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Steering sensitivity")
  @runtimeProperty("ModSettings.min", "0.25")
  @runtimeProperty("ModSettings.max", "2.0")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideSensitivity: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Operating range (degrees)")
  @runtimeProperty("ModSettings.description", "CP2077's steering tops out at about 90 degrees of virtual wheel rotation. Raise this for more physical travel at the cost of lower on-screen responsiveness per degree.")
  @runtimeProperty("ModSettings.min", "40")
  @runtimeProperty("ModSettings.max", "900")
  @runtimeProperty("ModSettings.step", "10")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideRangeDeg: Int32 = 90;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Centering spring strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideCenteringSpringPct: Int32 = 50;

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
    GWheel_SetSteerDeadzonePct(this.steerDeadzonePct);
    GWheel_SetThrottleDeadzonePct(this.throttleDeadzonePct);
    GWheel_SetBrakeDeadzonePct(this.brakeDeadzonePct);

    GWheel_SetFfbEnabled(this.ffbEnabled);
    GWheel_SetFfbStrengthPct(this.ffbStrengthPct);
    GWheel_SetFfbDebugLogging(this.ffbDebugLogging);

    GWheel_SetOverrideEnabled(this.overrideGHub);
    GWheel_SetOverrideSensitivity(this.overrideSensitivity);
    GWheel_SetOverrideRangeDeg(this.overrideRangeDeg);
    GWheel_SetOverrideCenteringSpringPct(this.overrideCenteringSpringPct);

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
