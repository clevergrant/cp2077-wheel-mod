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

public class GWheelSettings extends ScriptableService {

  // ---- Input --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Input")
  @runtimeProperty("ModSettings.displayName", "Enable wheel input")
  @runtimeProperty("ModSettings.description", "Master toggle. When off, the mod stops injecting wheel values into vehicle input.")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Input")
  @runtimeProperty("ModSettings.displayName", "Steering deadzone (%)")
  @runtimeProperty("ModSettings.description", "Small centre deadzone applied after G HUB's own curve.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let steerDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Input")
  @runtimeProperty("ModSettings.displayName", "Throttle deadzone (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let throttleDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Input")
  @runtimeProperty("ModSettings.displayName", "Brake deadzone (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "20")
  @runtimeProperty("ModSettings.step", "1")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let brakeDeadzonePct: Int32 = 2;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Input")
  @runtimeProperty("ModSettings.displayName", "Response curve")
  @runtimeProperty("ModSettings.displayValues", "\"Default\", \"Subdued\", \"Sharp\"")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let responseCurve: GWheelResponseCurve = GWheelResponseCurve.Default;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-FFB")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.description", "Plugin-driven effects only (collision, surface texture). Centering spring stays with G HUB.")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-FFB")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.description", "Scales plugin-generated effects. Does not affect G HUB's centering spring.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "ffbEnabled")
  let ffbStrengthPct: Int32 = 80;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-FFB")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  let ffbDebugLogging: Bool = false;

  // ---- Advanced (override G HUB) -----------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Advanced")
  @runtimeProperty("ModSettings.displayName", "Override G HUB settings")
  @runtimeProperty("ModSettings.description", "When ON, this mod takes control of sensitivity, rotation range, and centering spring. When OFF (default), those remain managed by Logitech G HUB.")
  let overrideGHub: Bool = false;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Advanced")
  @runtimeProperty("ModSettings.displayName", "Steering sensitivity")
  @runtimeProperty("ModSettings.min", "0.25")
  @runtimeProperty("ModSettings.max", "2.0")
  @runtimeProperty("ModSettings.step", "0.05")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideSensitivity: Float = 1.0;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Advanced")
  @runtimeProperty("ModSettings.displayName", "Operating range (degrees)")
  @runtimeProperty("ModSettings.min", "200")
  @runtimeProperty("ModSettings.max", "900")
  @runtimeProperty("ModSettings.step", "10")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideRangeDeg: Int32 = 900;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel-Advanced")
  @runtimeProperty("ModSettings.displayName", "Centering spring strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "overrideGHub")
  let overrideCenteringSpringPct: Int32 = 50;

  // ---- Lifecycle ---------------------------------------------------------

  private cb func OnLoad() {
    ModSettings.RegisterListenerToClass(this);
    ModSettings.RegisterListenerToModifications(this);
    this.Push();
  }

  public cb func OnModSettingsChange() -> Void {
    this.Push();
  }

  private final func Push() -> Void {
    GWheel_SetInputEnabled(this.inputEnabled);
    GWheel_SetSteerDeadzonePct(this.steerDeadzonePct);
    GWheel_SetThrottleDeadzonePct(this.throttleDeadzonePct);
    GWheel_SetBrakeDeadzonePct(this.brakeDeadzonePct);
    GWheel_SetResponseCurve(GWheelResponseCurve_ToString(this.responseCurve));

    GWheel_SetFfbEnabled(this.ffbEnabled);
    GWheel_SetFfbStrengthPct(this.ffbStrengthPct);
    GWheel_SetFfbDebugLogging(this.ffbDebugLogging);

    GWheel_SetOverrideEnabled(this.overrideGHub);
    GWheel_SetOverrideSensitivity(this.overrideSensitivity);
    GWheel_SetOverrideRangeDeg(this.overrideRangeDeg);
    GWheel_SetOverrideCenteringSpringPct(this.overrideCenteringSpringPct);
  }
}

enum GWheelResponseCurve {
  Default = 0,
  Subdued = 1,
  Sharp = 2,
}

public static func GWheelResponseCurve_ToString(c: GWheelResponseCurve) -> String {
  switch c {
    case GWheelResponseCurve.Subdued: return "subdued";
    case GWheelResponseCurve.Sharp: return "sharp";
    default: return "default";
  }
}
