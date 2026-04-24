// Push real vehicle telemetry (engine RPM, radio on/off) from the game's
// own Blackboard into the gwheel plugin so the LED rev strip can show
// the actual simulated RPM instead of a throttle-derived bluff, and so
// the music-visualizer mode fires from the game's authoritative radio
// state rather than audio-amplitude heuristics.
//
// Blackboard fields used (VehicleDef, scripts/core/blackboard/blackboardDefinitions.script):
//   RPMValue       : Float   — current engine RPM, driven by the audio/
//                              gameplay system (same value car_hud.script
//                              feeds its RPM gauge widget).
//   VehRadioState  : Bool    — true while the in-car radio receiver is on.
//
// RPM is normalised against VehicleRecord.VehEngineData().MaxRPM() —
// per-vehicle, read once per mount.
//
// Lifecycles that must all converge to an attached listener set:
//   - Fresh mount (VehicleFinishedMountingEvent) — happens on entering
//     a vehicle normally
//   - Save load while inside a vehicle (PlayerPuppet.OnGameAttached) —
//     the mount event doesn't replay post-load, so we re-attach here
//   - Unmount (UnmountingEvent) — tears down listeners and clears state

@addField(VehicleComponent)
public let m_gwheelRpmMax: Float;

@addField(VehicleComponent)
public let m_gwheelRpmBbId: ref<CallbackHandle>;

@addField(VehicleComponent)
public let m_gwheelRadioBbId: ref<CallbackHandle>;

// Resolve max-RPM, register Blackboard listeners, seed initial state.
// Idempotent: if the listeners are already attached (m_gwheelRpmBbId
// is set) this short-circuits. Safe to call from any lifecycle event
// that might be the "first" one to observe the mounted vehicle.
@addMethod(VehicleComponent)
public func GWheelAttach() -> Void {
  if IsDefined(this.m_gwheelRpmBbId) {
    return;  // already attached for this vehicle instance
  }

  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if !IsDefined(vehicle) {
    return;
  }

  // Resolve this vehicle's max RPM once. VehEngineData is a static
  // record attached to every VehicleObject; falling back to 8000 is
  // defensive for tanks / oddball vehicles where the record lookup
  // fails (an arbitrary "reasonable redline" that prevents div-by-0).
  let record: wref<Vehicle_Record> = vehicle.GetRecord();
  let maxRpm: Float = 8000.0;
  if IsDefined(record) {
    let engineData: wref<VehicleEngineData_Record> = record.VehEngineData();
    if IsDefined(engineData) {
      let recordedMax: Float = engineData.MaxRPM();
      if recordedMax > 0.0 {
        maxRpm = recordedMax;
      }
    }
  }
  this.m_gwheelRpmMax = maxRpm;

  let bb: ref<IBlackboard> = vehicle.GetBlackboard();
  if !IsDefined(bb) {
    return;
  }

  // Seed initial values. Radio state comes from the native
  // IsRadioReceiverActive() rather than the Blackboard field because
  // the Blackboard can lag a frame or two after mount / save-load;
  // the native method queries the live radio component.
  let initialRpm: Float = bb.GetFloat(GetAllBlackboardDefs().Vehicle.RPMValue);
  GWheel_SetEngineRpmNormalized(initialRpm / this.m_gwheelRpmMax);
  GWheel_SetRadioActive(vehicle.IsRadioReceiverActive());

  // Subscribe to change events.
  this.m_gwheelRpmBbId = bb.RegisterListenerFloat(
    GetAllBlackboardDefs().Vehicle.RPMValue, this, n"OnGwheelRpmChanged");
  this.m_gwheelRadioBbId = bb.RegisterListenerBool(
    GetAllBlackboardDefs().Vehicle.VehRadioState, this, n"OnGwheelRadioChanged");

  // The C++ plugin tracks in-vehicle context via
  // GWheel_Set/ClearPlayerVehicle, normally set by the mount event
  // wrappers in gwheel_mount.reds. Save-load-in-vehicle skips those,
  // so explicitly assert the cached pointer here as a safety net.
  GWheel_SetPlayerVehicle(vehicle);
}

// Tear down listeners + clear plugin state. Idempotent.
@addMethod(VehicleComponent)
public func GWheelDetach() -> Void {
  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if IsDefined(vehicle) {
    let bb: ref<IBlackboard> = vehicle.GetBlackboard();
    if IsDefined(bb) {
      if IsDefined(this.m_gwheelRpmBbId) {
        bb.UnregisterListenerFloat(GetAllBlackboardDefs().Vehicle.RPMValue, this.m_gwheelRpmBbId);
      }
      if IsDefined(this.m_gwheelRadioBbId) {
        bb.UnregisterListenerBool(GetAllBlackboardDefs().Vehicle.VehRadioState, this.m_gwheelRadioBbId);
      }
    }
  }
  this.m_gwheelRpmBbId = null;
  this.m_gwheelRadioBbId = null;
  this.m_gwheelRpmMax = 0.0;

  GWheel_SetEngineRpmNormalized(0.0);
  GWheel_SetRadioActive(false);
}

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let character: ref<GameObject> = evt.character as GameObject;
  if IsDefined(character) && character.IsPlayer() && evt.isMounting {
    this.GWheelAttach();
  }
  return result;
}

@wrapMethod(VehicleComponent)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let game: GameInstance = this.GetVehicle().GetGame();
  let childId: EntityID = evt.request.lowLevelMountingInfo.childId;
  let mountChild: ref<GameObject> = GameInstance.FindEntityByID(game, childId) as GameObject;
  if IsDefined(mountChild) && mountChild.IsPlayer() {
    this.GWheelDetach();
  }
  return result;
}

// Callbacks must live on VehicleComponent so `this` binds correctly
// when RegisterListenerFloat / RegisterListenerBool dispatch them.
// Top-level `cb func` without @addMethod is treated as static and
// fails to compile with UNEXPECTED_THIS.
@addMethod(VehicleComponent)
protected cb func OnGwheelRpmChanged(value: Float) -> Bool {
  if this.m_gwheelRpmMax > 0.0 {
    GWheel_SetEngineRpmNormalized(value / this.m_gwheelRpmMax);
  }
  return true;
}

@addMethod(VehicleComponent)
protected cb func OnGwheelRadioChanged(value: Bool) -> Bool {
  GWheel_SetRadioActive(value);
  return true;
}

// Belt-and-suspenders: the Blackboard listener should catch every radio
// on/off transition, but if the game ever routes a toggle through a
// code path that doesn't update the Blackboard field, this wrap on the
// explicit event guarantees the plugin sees it. Re-read via the native
// method (state may settle async inside wrappedMethod's handler).
@wrapMethod(VehicleComponent)
protected cb func OnRadioToggleEvent(evt: ref<RadioToggleEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if IsDefined(vehicle) {
    GWheel_SetRadioActive(vehicle.IsRadioReceiverActive());
  }
  return result;
}

// Save-load-in-vehicle recovery. PlayerPuppet.OnGameAttached fires after
// every save load; VehicleFinishedMountingEvent does NOT replay for a
// vehicle that was already mounted when the save was written. If V is
// currently mounted at attach time, run the same attach path we'd have
// run on a fresh mount.
@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  let vehicle: wref<VehicleObject> = GetMountedVehicle(this);
  if IsDefined(vehicle) {
    let vc: ref<VehicleComponent> = vehicle.GetVehicleComponent();
    if IsDefined(vc) {
      vc.GWheelAttach();
    }
  }
  return result;
}
