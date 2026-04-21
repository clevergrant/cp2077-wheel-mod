// Intercept VehicleComponent's float input queries and route Steer /
// Accelerate / Brake through the gwheel plugin when a wheel is connected.
// Everything else falls through via wrappedMethod().
//
// If CDPR ever renames VehicleComponent or changes the method signature, this
// file is the one that needs updating. The native side
// (GWheel_MaybeOverrideFloat) is signature-stable.

@wrapMethod(VehicleComponent)
public final func GetInputValueFloat(inputName: CName) -> Float {
  let original: Float = wrappedMethod(inputName);
  if GWheel_IsPluginReady() {
    return GWheel_MaybeOverrideFloat(inputName, original);
  }
  return original;
}
