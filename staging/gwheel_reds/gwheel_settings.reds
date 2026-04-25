// In-game settings page for the gwheel mod.
//
// The settings UI is built dynamically from the wheel detected at game
// attach. Each settings group lives in its own class; the coordinator
// (GWheelCoordinator) instantiates and registers only the classes that
// match the bound wheel's capabilities. On a no-FFB wheel the FFB module
// is never registered, so the FFB section literally doesn't exist in the
// settings page — no toggles, no sliders, no greyed-out clutter.
//
// Re-registration on wheel-swap: when the user opens the Mod Settings
// menu we re-evaluate detection and rebuild the registered set if the
// bound wheel's capabilities have changed since OnGameAttached. This
// covers the "plugged the wheel in after game launch" case without
// requiring a restart.
//
// Mod Settings persists per-class (one [ClassName] section per module in
// red4ext/plugins/mod_settings/user.ini). Modules that come in and out
// of registration as wheels change keep their persisted values across
// the gap.

// ============================================================================
// Module 1: Input  (always registered)
// ============================================================================

public class GWheelSettings_Input extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable wheel input")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Treat clutch as brake")
  @runtimeProperty("ModSettings.description", "The clutch acts just like the brake pedal. Useful for stiff brake pedals, since there's no gear system in the game.")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let clutchAsBrake: Bool = true;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetInputEnabled(this.inputEnabled);
    GWheel_SetClutchAsBrake(this.clutchAsBrake);
  }
}

// ============================================================================
// Module 2: Force Feedback  (registered only if wheel has an FFB motor)
// ============================================================================

public class GWheelSettings_FFB extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let ffbTorquePct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Cornering feedback (%)")
  @runtimeProperty("ModSettings.description", "Adds spring stiffness while the car is rotating.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let yawFeedbackPct: Int32 = 50;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Active torque (%)")
  @runtimeProperty("ModSettings.description", "How hard the wheel pushes toward center, scaled by speed and steering angle.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let activeTorqueStrengthPct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Stationary threshold (m/s)")
  @runtimeProperty("ModSettings.description", "Below this speed the wheel has no centering force.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "5.0")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let stationaryThresholdMps: Float = 0.5;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetFfbEnabled(this.ffbEnabled);
    GWheel_SetFfbTorquePct(this.ffbTorquePct);
    GWheel_SetYawFeedbackPct(this.yawFeedbackPct);
    GWheel_SetActiveTorqueStrengthPct(this.activeTorqueStrengthPct);
    GWheel_SetStationaryThresholdMps(this.stationaryThresholdMps);
  }
}

// ============================================================================
// Module 3: Greeting  (always registered)
// ============================================================================

public class GWheelSettings_Greeting extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Pon pon shi greeting")
  @runtimeProperty("ModSettings.description", "Make the wheel dance when the game starts.")
  let handshakePlayOnStart: Bool = false;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetHandshakePlayOnStart(this.handshakePlayOnStart);
  }
}

// ============================================================================
// Module 4: Rev-strip LEDs  (registered only if wheel has the LED bar)
// ============================================================================

public class GWheelSettings_LEDs extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Enable rev-strip LEDs")
  let ledEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Rev strip as visualizer while music is playing")
  @runtimeProperty("ModSettings.dependency", "ledEnabled")
  let ledVisualizerWhileMusic: Bool = true;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetLedEnabled(this.ledEnabled);
    GWheel_SetLedVisualizerWhileMusic(this.ledVisualizerWhileMusic);
  }
}

// ============================================================================
// Module 5: Debug  (always registered)
// ============================================================================

public class GWheelSettings_Debug extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  @runtimeProperty("ModSettings.description", "Logs to red4ext/logs/gwheel-*.log.")
  let ffbDebugLogging: Bool = false;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetFfbDebugLogging(this.ffbDebugLogging);
  }
}

// ============================================================================
// Module 6: Common bindings  (always registered)
//
// The 15 bindings below are present on every G-series wheel from the G29
// onward (and their Logitech-branded ancestors via remapped indices).
// ============================================================================
//
// Every binding here is user-controlled, with no hidden overrides. The
// D-pad + A defaults are Menu-nav (Up/Down/Left/Right arrow keys and
// Enter) so the wheel navigates pause/map/inventory menus like a
// controller out of the box. If you'd rather those wheel controls stay
// inert while driving, set them to None — CP2077's arrow keys are
// secondary vehicle controls (Up/Down = accelerate/decelerate, Left/Right
// = steer), so binding the D-pad to Menu-nav and then pressing the D-pad
// while driving will nudge the car.
//
// IMPORTANT: clear the D-pad and A/B/X/Y keyboard bindings in G HUB's
// Cyberpunk profile. Otherwise G HUB + plugin both fire keyboard
// events and you'll get doubled keypresses.

public class GWheelSettings_Bindings_Common extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Left paddle shifter")
  let bindPaddleLeft: GWheelAction = GWheelAction.ShootPrimary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Right paddle shifter")
  let bindPaddleRight: GWheelAction = GWheelAction.ShootPrimary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Up")
  let bindDpadUp: GWheelAction = GWheelAction.MenuUp;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Down")
  let bindDpadDown: GWheelAction = GWheelAction.MenuDown;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Left")
  let bindDpadLeft: GWheelAction = GWheelAction.MenuLeft;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "D-pad Right")
  let bindDpadRight: GWheelAction = GWheelAction.MenuRight;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "A button")
  let bindButtonA: GWheelAction = GWheelAction.MenuConfirm;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "B button (in-vehicle)")
  let bindButtonB: GWheelAction = GWheelAction.Handbrake;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "X button (in-vehicle)")
  let bindButtonX: GWheelAction = GWheelAction.Horn;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Y button (in-vehicle)")
  let bindButtonY: GWheelAction = GWheelAction.Autodrive;

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
  let bindLSB: GWheelAction = GWheelAction.OpenPhone;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "RSB (right stick click)")
  let bindRSB: GWheelAction = GWheelAction.RadioMenu;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Xbox / Guide button")
  let bindXbox: GWheelAction = GWheelAction.ExitVehicle;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
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
    GWheel_SetInputBinding(19, EnumInt(this.bindXbox));
  }
}

// ============================================================================
// Module 7: Right-cluster bindings  (registered only if the wheel has
// Plus / Minus / Scroll on the right grip — G29 / G923 / G PRO; not G920).
// ============================================================================

public class GWheelSettings_Bindings_RightCluster extends IScriptable {

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Plus (+) button")
  let bindPlus: GWheelAction = GWheelAction.CameraCycleForward;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Minus (-) button")
  let bindMinus: GWheelAction = GWheelAction.Headlights;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll click (Return)")
  let bindScrollClick: GWheelAction = GWheelAction.HolsterWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll clockwise")
  let bindScrollCW: GWheelAction = GWheelAction.NextWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button Bindings")
  @runtimeProperty("ModSettings.displayName", "Scroll counter-clockwise")
  let bindScrollCCW: GWheelAction = GWheelAction.PrevWeapon;

  public func OnModSettingsChange() -> Void { this.Push(); }

  public func Push() -> Void {
    GWheel_SetInputBinding(14, EnumInt(this.bindPlus));
    GWheel_SetInputBinding(15, EnumInt(this.bindMinus));
    GWheel_SetInputBinding(16, EnumInt(this.bindScrollClick));
    GWheel_SetInputBinding(17, EnumInt(this.bindScrollCW));
    GWheel_SetInputBinding(18, EnumInt(this.bindScrollCCW));
  }

  // Push None for every right-cluster binding. Called from the coordinator
  // when this module is being unregistered (wheel swapped to one without
  // the cluster) so the C++ side stops firing actions for dead inputs.
  public func PushNone() -> Void {
    GWheel_SetInputBinding(14, EnumInt(GWheelAction.None));
    GWheel_SetInputBinding(15, EnumInt(GWheelAction.None));
    GWheel_SetInputBinding(16, EnumInt(GWheelAction.None));
    GWheel_SetInputBinding(17, EnumInt(GWheelAction.None));
    GWheel_SetInputBinding(18, EnumInt(GWheelAction.None));
  }
}

// ============================================================================
// Coordinator: owns module instances + manages register/unregister cycles.
// ============================================================================
//
// Module instances are kept across registration cycles so persisted Mod
// Settings values stay in memory; only the listener registration with
// Mod Settings is added/removed.
//
// `lastDetected*` flags record what the wheel detection said the last
// time we (re)built the registered set. ReregisterIfNeeded() compares
// against current detection and only does work if something has changed.

public class GWheelCoordinator extends IScriptable {

  public let input:          ref<GWheelSettings_Input>;
  public let ffb:            ref<GWheelSettings_FFB>;
  public let leds:           ref<GWheelSettings_LEDs>;
  public let greeting:       ref<GWheelSettings_Greeting>;
  public let bindingsCommon: ref<GWheelSettings_Bindings_Common>;
  public let bindingsRight:  ref<GWheelSettings_Bindings_RightCluster>;
  public let debug:          ref<GWheelSettings_Debug>;

  private let ffbRegistered:    Bool = false;
  private let ledsRegistered:   Bool = false;
  private let rightRegistered:  Bool = false;

  private let lastDetectedFfb:   Bool = false;
  private let lastDetectedLeds:  Bool = false;
  private let lastDetectedRight: Bool = false;

  // First-time setup. Always-on modules go on now; capability-gated
  // modules go on if detection says so.
  public func InitialRegister() -> Void {
    this.input          = new GWheelSettings_Input();
    this.greeting       = new GWheelSettings_Greeting();
    this.bindingsCommon = new GWheelSettings_Bindings_Common();
    this.debug          = new GWheelSettings_Debug();
    this.ffb            = new GWheelSettings_FFB();
    this.leds           = new GWheelSettings_LEDs();
    this.bindingsRight  = new GWheelSettings_Bindings_RightCluster();

    ModSettings.RegisterListenerToClass(this.input);
    ModSettings.RegisterListenerToClass(this.greeting);
    ModSettings.RegisterListenerToClass(this.bindingsCommon);
    ModSettings.RegisterListenerToClass(this.debug);

    this.lastDetectedFfb   = GWheel_DetectedHasFfbHardware();
    this.lastDetectedLeds  = GWheel_DetectedHasRevLeds();
    this.lastDetectedRight = GWheel_DetectedHasRightCluster();

    if this.lastDetectedFfb {
      ModSettings.RegisterListenerToClass(this.ffb);
      this.ffbRegistered = true;
    }
    if this.lastDetectedLeds {
      ModSettings.RegisterListenerToClass(this.leds);
      this.ledsRegistered = true;
    }
    if this.lastDetectedRight {
      ModSettings.RegisterListenerToClass(this.bindingsRight);
      this.rightRegistered = true;
    }

    this.PushAll();
  }

  // Called when the user opens the Mod Settings menu. If wheel detection
  // has changed since the last register cycle (typical case: user plugged
  // the wheel in after game launch, so OnGameAttached's snapshot is
  // permissive but reality has narrowed), unregister the now-incorrect
  // modules and register the now-correct ones.
  public func ReregisterIfNeeded() -> Void {
    // Idempotent sync of registered modules with current detection. We
    // compare each module's registered flag against the detection result
    // directly (no Bool!=Bool — redscript's NotEqual operator has no Bool
    // overload). Each branch is a no-op when registered state already
    // matches detection.
    let nowFfb:   Bool = GWheel_DetectedHasFfbHardware();
    let nowLeds:  Bool = GWheel_DetectedHasRevLeds();
    let nowRight: Bool = GWheel_DetectedHasRightCluster();

    if nowFfb && !this.ffbRegistered {
      ModSettings.RegisterListenerToClass(this.ffb);
      this.ffbRegistered = true;
      this.ffb.Push();
    }
    if !nowFfb && this.ffbRegistered {
      ModSettings.UnregisterListenerToClass(this.ffb);
      this.ffbRegistered = false;
      // Force C++ FFB off so we stop driving forces the wheel can't render.
      GWheel_SetFfbEnabled(false);
    }
    this.lastDetectedFfb = nowFfb;

    if nowLeds && !this.ledsRegistered {
      ModSettings.RegisterListenerToClass(this.leds);
      this.ledsRegistered = true;
      this.leds.Push();
    }
    if !nowLeds && this.ledsRegistered {
      ModSettings.UnregisterListenerToClass(this.leds);
      this.ledsRegistered = false;
      GWheel_SetLedEnabled(false);
    }
    this.lastDetectedLeds = nowLeds;

    if nowRight && !this.rightRegistered {
      ModSettings.RegisterListenerToClass(this.bindingsRight);
      this.rightRegistered = true;
      this.bindingsRight.Push();
    }
    if !nowRight && this.rightRegistered {
      ModSettings.UnregisterListenerToClass(this.bindingsRight);
      this.rightRegistered = false;
      this.bindingsRight.PushNone();
    }
    this.lastDetectedRight = nowRight;
  }

  public func PushAll() -> Void {
    this.input.Push();
    this.greeting.Push();
    this.bindingsCommon.Push();
    this.debug.Push();
    if this.ffbRegistered     { this.ffb.Push();           } else { GWheel_SetFfbEnabled(false); }
    if this.ledsRegistered    { this.leds.Push();          } else { GWheel_SetLedEnabled(false); }
    if this.rightRegistered   { this.bindingsRight.Push(); } else { this.bindingsRight.PushNone(); }
  }
}

// ============================================================================
// Action enum (shared by all bindings classes)
// ============================================================================
//
// Actions the plugin knows how to dispatch. Indices must match the Action
// enum in gwheel/src/input_bindings.h — same values, same order.
// Grouped by category (driving / camera / combat / weapons / radio /
// gameplay / menus / menu-nav) so Mod Settings' scroll-list shows similar
// actions adjacent. MUST stay in lockstep with the C++ `Action` enum in
// gwheel/src/input_bindings.h — same names, same integer values, same
// order. Mod Settings persists each binding by integer value, so any
// reorder shifts users' saved bindings (one-time forced re-bind).
//
// `RearViewCamera` is the redscript-side spelling of CameraCycleBackward;
// the dropdown label inherits the redscript identifier so we use the
// gameplay-meaningful name here even though the C++ side keeps the older
// "Backward" naming for historical reasons.

enum GWheelAction {
  None = 0,

  // Driving
  Horn = 1,
  Headlights = 2,
  Handbrake = 3,
  Autodrive = 4,
  ExitVehicle = 5,
  CallVehicle = 6,

  // Camera
  CameraCycleForward = 7,
  RearViewCamera = 8,
  CameraReset = 9,

  // Combat
  ShootPrimary = 10,
  ShootSecondary = 11,
  ShootTertiary = 12,

  // Weapons
  NextWeapon = 13,
  PrevWeapon = 14,
  WeaponSlot1 = 15,
  WeaponSlot2 = 16,
  SwitchWeapons = 17,
  HolsterWeapon = 18,

  // Radio
  RadioMenu = 19,
  RadioNext = 20,

  // Gameplay misc
  UseConsumable = 21,
  IconicCyberware = 22,
  QuickSave = 23,

  // Menus
  OpenMap = 24,
  OpenJournal = 25,
  OpenInventory = 26,
  OpenPhone = 27,
  OpenPerks = 28,
  OpenCrafting = 29,
  Pause = 30,

  // Menu navigation
  MenuConfirm = 31,
  MenuCancel = 32,
  MenuUp = 33,
  MenuDown = 34,
  MenuLeft = 35,
  MenuRight = 36,
}

// ============================================================================
// PlayerPuppet attachment + Mod Settings menu hook.
// ============================================================================

// Module-level holder for the coordinator. Plain global so the menu
// hook below can find it without needing a GameInstance — MenuScenarios
// don't expose one. Set by PlayerPuppet.OnGameAttached on first attach,
// cleared by OnDetach so a save reload doesn't leak the prior session's
// coordinator.

public class GWheelHolder {
  public static let coordinator: ref<GWheelCoordinator>;
}

@addField(PlayerPuppet)
public let m_gwheelCoord: ref<GWheelCoordinator>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if !IsDefined(this.m_gwheelCoord) {
    this.m_gwheelCoord = new GWheelCoordinator();
    this.m_gwheelCoord.InitialRegister();
    GWheelHolder.coordinator = this.m_gwheelCoord;
  }
  return result;
}

// Re-evaluate wheel detection every time the user opens the Mod Settings
// menu. If the bound wheel's capabilities have changed since the last
// register cycle (typical: wheel binding finished AFTER OnGameAttached
// snapshotted the permissive defaults), rebuild the registered set BEFORE
// the UI builds so it reflects actual hardware on first render. Runs only
// on menu open — zero per-frame cost during gameplay.

@wrapMethod(MenuScenario_ModSettings)
protected cb func OnEnterScenario(prevScenario: CName, userData: ref<IScriptable>) -> Bool {
  if IsDefined(GWheelHolder.coordinator) {
    GWheelHolder.coordinator.ReregisterIfNeeded();
  }
  return wrappedMethod(prevScenario, userData);
}
