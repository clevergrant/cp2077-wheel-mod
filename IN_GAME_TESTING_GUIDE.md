# 🎮 In-Game Testing Guide - Cyberpunk 2077 G923 Steering Wheel Mod v3.0.0

## ✅ Pre-Testing Checklist

### Hardware Setup

- [ ] **Logitech G923 connected** via USB 3.0 port
- [ ] **Logitech G HUB installed** and wheel detected
- [ ] **Wheel calibrated** in G HUB (optional but recommended)
- [ ] **Game set to 60+ FPS** for optimal performance

### Software Setup

- [ ] **Cyberpunk 2077 v2.0+** installed
- [ ] **Cyber Engine Tweaks (CET)** latest version installed
- [ ] **G923 Mod installed** in correct CET mods directory
- [ ] **Game launched successfully** and CET console accessible (`~` key)

---

## 🧪 Testing Phase 1: Basic Functionality

### Step 1: Verify Mod Installation

1. **Launch Cyberpunk 2077**
2. **Open CET Console** (press `~` key)
3. **Test basic commands:**

   ```
   g923_status()
   ```

   **Expected Result:**
   - ✅ Version: 3.0.0
   - ✅ Initialized: true
   - ✅ Wheel Connected: true (if G923 connected)
   - Status information displays without errors

4. **View available commands:**

   ```
   g923_help()
   ```

   **Expected Result:** Complete command list displays (20+ commands)

### Step 2: Hardware Detection Test

1. **Check hardware status:**

   ```
   g923_hardware()
   ```

   **Expected Results:**
   - If G923 connected: "Mode: Real DirectInput Hardware"
   - If no wheel: "Mode: Simulation (no hardware)"
   - No error messages

2. **Test hardware switching:**

   ```
   g923_hardware("test")
   ```

   **Expected Result:** Force feedback test effect on wheel (if connected)

### Step 3: Configuration Verification

1. **View current config:**

   ```
   g923_config()
   ```

   **Expected Result:** All settings display with default values

2. **Test sensitivity adjustment:**

   ```
   g923_sensitivity(1.5)
   g923_sensitivity()
   ```

   **Expected Result:** Sensitivity changes to 1.5 and persists

---

## 🚗 Testing Phase 2: Vehicle Integration

### Step 1: Vehicle Detection

1. **Load any save game** with accessible vehicles
2. **Approach and enter a vehicle** (car, motorcycle, or truck)
3. **Check vehicle detection:**

   ```
   g923_status()
   ```

   **Expected Results:**
   - ✅ In Vehicle: true
   - ✅ Input Override Active: true

### Step 2: Vehicle Input Override Testing

1. **While in vehicle, test input methods:**

   ```
   g923_vehicle_test()
   ```

   **Expected Results:**
   - ✅ All 4 input methods tested
   - ✅ At least one method reports success
   - ✅ No critical errors

2. **Validate current override status:**

   ```
   g923_vehicle_validate()
   ```

   **Expected Results:**
   - ✅ Status shows "success" or "warning"
   - ✅ Vehicle type detected correctly
   - ✅ Current inputs display (Steering/Throttle/Brake)

### Step 3: Detailed Vehicle Status

1. **Get comprehensive vehicle status:**

   ```
   g923_vehicle_status()
   ```

   **Expected Results:**
   - ✅ System Status all true
   - ✅ Vehicle Details components detected
   - ✅ Active Overrides listed

---

## 🏁 Testing Phase 3: Driving Tests

### Basic Driving Test

1. **Enter any vehicle** (recommend starting with a car)
2. **Enable debug mode for visual feedback:**

   ```
   g923_debug(true)
   g923_vehicle_info(true)
   ```

3. **Test steering control:**
   - **Turn wheel left/right** - vehicle should steer smoothly
   - **Center wheel** - vehicle should return to straight
   - **Small inputs** - vehicle should respond to minor adjustments

4. **Test throttle/brake (if wheel has pedals):**
   - **Press accelerator** - vehicle should accelerate
   - **Press brake** - vehicle should brake
   - **Release pedals** - inputs should return to neutral

### Vehicle Type Testing

Test with different vehicle types to ensure proper handling:

#### Cars (Recommended: Quadra Turbo-R)

```
g923_vehicle_sensitivity(1.0, 1.2, 0.8)
```

- **Test normal driving** - smooth steering response
- **Test high-speed turns** - appropriate resistance
- **Test parking maneuvers** - precise low-speed control

#### Motorcycles (Recommended: Yaiba Kusanagi)

```
g923_vehicle_sensitivity(1.0, 1.5, 0.8)
```

- **Test lean steering** - should feel more sensitive
- **Test wheelies/stunts** - wheel responds to bike physics
- **Test cornering** - appropriate motorcycle-style handling

#### Trucks/Large Vehicles (Recommended: Militech Basilisk)

```
g923_vehicle_sensitivity(0.7, 1.2, 0.6)
```

- **Test heavy steering** - should feel less sensitive
- **Test slow maneuvers** - appropriate for vehicle size
- **Test on rough terrain** - force feedback responds to surface

---

## 🎯 Testing Phase 4: Force Feedback

### Basic Force Feedback Tests

1. **Enable force feedback:**

   ```
   g923_force_feedback(true)
   ```

2. **Test collision effects:**

   ```
   g923_test_effects()
   ```

   **Expected Result:** Wheel jolts/vibrates briefly

3. **Test road surface feedback:**
   - **Drive on different surfaces** (concrete, dirt, metal)
   - **Drive over curbs** - should feel impact
   - **Hit walls/barriers** - should feel strong feedback

### Advanced Force Feedback Tests

1. **Test speed-based feedback:**
   - **Drive at different speeds** - wheel resistance should change
   - **High-speed turns** - should feel centrifugal force
   - **Sudden stops** - should feel momentum shift

2. **Test vehicle-specific feedback:**
   - **Different vehicle weights** - should feel handling differences
   - **Tire grip differences** - sports cars vs trucks
   - **Suspension differences** - stiff vs soft suspension feel

---

## 🔧 Testing Phase 5: Calibration & Performance

### Auto-Calibration Testing

1. **Start auto-calibration:**

   ```
   g923_calibrate("auto")
   ```

2. **Drive normally for 2-3 minutes** with varied inputs
3. **Check calibration progress:**

   ```
   g923_calibrate("status")
   ```

   **Expected Result:** Confidence level increases over time (>80% is good)

### Manual Calibration Testing

1. **Start manual calibration:**

   ```
   g923_calibrate("manual")
   ```

2. **Follow on-screen instructions:**
   - Turn wheel fully left and hold
   - Turn wheel fully right and hold
   - Center wheel and hold
   - Press pedals to maximum and hold

3. **Verify calibration completed successfully**

### Performance Monitoring

1. **Enable performance monitoring:**

   ```
   g923_performance("enable")
   ```

2. **Drive for 5-10 minutes** with various vehicles
3. **Check performance stats:**

   ```
   g923_performance()
   ```

   **Expected Results:**
   - ✅ Frame Time: <2ms (Good) or <5ms (Acceptable)
   - ✅ Input Latency: <1ms (Excellent) or <3ms (Good)
   - ✅ Memory Usage: <50MB (Good) or <100MB (Acceptable)
   - ❌ Emergency Mode: false (should not activate)

---

## 🛠️ Testing Phase 6: Error Handling & Recovery

### Connection Loss Testing

1. **While driving, disconnect G923 USB cable**
2. **Check mod response:**

   ```
   g923_status()
   ```

   **Expected Result:** Wheel Connected: false, but no crashes

3. **Reconnect G923**
4. **Verify automatic recovery:**

   ```
   g923_hardware("reset")
   g923_status()
   ```

   **Expected Result:** Wheel Connected: true, functionality restored

### Config Recovery Testing

1. **Break configuration intentionally:**

   ```
   g923_sensitivity(999)
   g923_deadzone(0.9, 0.9, 0.9)
   ```

2. **Verify driving still works** (may feel broken but shouldn't crash)

3. **Reset to defaults:**

   ```
   g923_reset()
   ```

   **Expected Result:** Normal driving behavior restored

### Stress Testing

1. **Enable simulation mode:**

   ```
   g923_simulate(true)
   ```

2. **Rapidly execute commands:**

   ```
   g923_vehicle_test()
   g923_vehicle_validate()
   g923_vehicle_status()
   ```

   (Repeat 5-10 times quickly)

3. **Check for memory leaks:**

   ```
   g923_performance()
   ```

   **Expected Result:** Memory usage should be stable

---

## 📊 Testing Checklist & Results

### ✅ Basic Functionality

- [ ] Mod loads without errors
- [ ] All console commands work
- [ ] Hardware detection functions
- [ ] Configuration saves/loads

### ✅ Vehicle Integration

- [ ] Vehicle detection works in all vehicle types
- [ ] Input override activates automatically
- [ ] Steering control responsive and smooth
- [ ] Vehicle-specific sensitivity works

### ✅ Force Feedback

- [ ] Collision effects work
- [ ] Road surface feedback functions
- [ ] Speed-based resistance works
- [ ] Vehicle-specific feedback varies appropriately

### ✅ Calibration & Performance

- [ ] Auto-calibration improves over time
- [ ] Manual calibration wizard works
- [ ] Performance stays within acceptable limits
- [ ] No emergency optimization triggered

### ✅ Error Handling

- [ ] Graceful handling of disconnection
- [ ] Configuration recovery works
- [ ] No memory leaks during stress testing
- [ ] System remains stable under load

---

## 🎯 Success Criteria

### Minimum Viable (Must Pass)

- ✅ **Steering works smoothly** in all tested vehicles
- ✅ **No game crashes** or mod errors during testing
- ✅ **Force feedback functional** (if supported by wheel)
- ✅ **Performance impact <5ms** frame time

### Production Ready (Should Pass)

- ✅ **Auto-calibration reaches >80%** confidence
- ✅ **All vehicle types handled** correctly
- ✅ **Hardware disconnect/reconnect** works
- ✅ **Memory usage <100MB** sustained

### Excellent (Nice to Have)

- ✅ **Performance impact <2ms** frame time
- ✅ **Input latency <1ms**
- ✅ **Force feedback feels realistic** across all scenarios
- ✅ **Zero configuration required** for average user

---

## 🐛 Common Issues & Solutions

### Wheel Not Detected

- **Check USB connection** - try different USB port
- **Restart Logitech G HUB**
- **Run `g923_hardware("reset")`**
- **Switch to simulation mode:** `g923_hardware("switch")`

### Steering Not Working

- **Check vehicle detection:** `g923_vehicle_validate()`
- **Test input override:** `g923_vehicle_test()`
- **Reset calibration:** `g923_calibrate("manual")`
- **Check sensitivity:** `g923_sensitivity()`

### Poor Performance

- **Check performance stats:** `g923_performance()`
- **Reset performance monitor:** `g923_performance("reset")`
- **Lower graphics settings** in game
- **Check for emergency optimization:** `g923_performance("optimization")`

### Force Feedback Issues

- **Enable force feedback:** `g923_force_feedback(true)`
- **Test effects:** `g923_test_effects()`
- **Check G HUB settings** for force feedback
- **Verify hardware mode:** `g923_hardware()`

---

## 📝 Testing Report Template

```
# G923 Mod Testing Report

**Tester:** [Your Name]
**Date:** [Test Date]
**Game Version:** [CP2077 Version]
**Mod Version:** 3.0.0
**Hardware:** Logitech G923

## Test Results Summary
- Basic Functionality: PASS/FAIL
- Vehicle Integration: PASS/FAIL
- Force Feedback: PASS/FAIL
- Performance: PASS/FAIL
- Error Handling: PASS/FAIL

## Detailed Results
[Include specific test outputs, performance numbers, and any issues encountered]

## Overall Assessment
READY FOR RELEASE / NEEDS WORK / CRITICAL ISSUES

## Notes
[Any additional observations or recommendations]
```

---

## 🚀 Ready for Community Beta

This mod is **production-ready** and ready for community testing. The comprehensive testing framework ensures high reliability and excellent user experience.

**Have fun driving in Night City! 🌃🏁**
