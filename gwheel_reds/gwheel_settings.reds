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
  @runtimeProperty("ModSettings.min", "200")
  @runtimeProperty("ModSettings.max", "900")
  @runtimeProperty("ModSettings.step", "10")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideRangeDeg: Int32 = 900;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.displayName", "Centering spring strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideCenteringSpringPct: Int32 = 50;

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
  }
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
