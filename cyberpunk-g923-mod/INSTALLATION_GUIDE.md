# 🎮 Cyberpunk 2077 G923 Steering Wheel Mod - Installation Guide

## 📋 Requirements (CRITICAL)

**⚠️ INSTALL THESE FIRST:**

1. **Cyberpunk 2077** v2.0 or later
2. **[Cyber Engine Tweaks (CET)](https://www.nexusmods.com/cyberpunk2077/mods/107)** v1.32.0+ (**REQUIRED**)
3. **Logitech G923 Steering Wheel** with G HUB software
4. **Windows 10/11** with DirectInput support

## 🚀 Installation Methods

### Method 1: Vortex Mod Manager (Recommended)

1. **Install [Vortex Mod Manager](https://www.nexusmods.com/site/mods/1)**
2. **Add Cyberpunk 2077** as a managed game in Vortex
3. **Install Cyber Engine Tweaks** via Vortex first
4. **Download and install this mod** via Vortex
5. **Deploy mods** in Vortex
6. **Launch Cyberpunk 2077**

### Method 2: Manual Installation

1. **Install Cyber Engine Tweaks** first
2. **Extract this mod** to: `Cyberpunk 2077/bin/x64/plugins/cyber_engine_tweaks/mods/cyberpunk-g923-mod/`
3. **Verify file structure**:
   ```
   Cyberpunk 2077/
   └── bin/x64/plugins/cyber_engine_tweaks/mods/
       └── cyberpunk-g923-mod/
           ├── init.lua
           ├── modules/
           ├── assets/
           └── fomod/
   ```

## ✅ First-Time Setup

### 1. Hardware Setup
- Connect your **Logitech G923** via USB
- Launch **Logitech G HUB** and ensure wheel is detected
- Set wheel to **"PC Mode"** (not PlayStation/Xbox mode)

### 2. Verify Installation
1. **Launch Cyberpunk 2077**
2. **Open CET Console** (press `~` key)
3. **Type**: `g923_status()`
4. **Expected output**:
   ```
   [G923Mod] === G923 Mod Status ===
     Version: 3.0.0
     Initialized: true
     Installation: OK
   ```

### 3. Auto-Calibration (Recommended)
1. **Type**: `g923_calibrate("auto")`
2. **Follow on-screen instructions**
3. **Get in any vehicle** to test

### 4. Manual Configuration (Advanced)
```lua
-- Sensitivity settings
g923_sensitivity(1.5)              -- Global sensitivity
g923_vehicle_sensitivity(1.2, 0.8, 1.8)  -- Car, motorcycle, truck

-- Deadzones
g923_deadzone(0.05, 0.02, 0.02)   -- Steering, throttle, brake

-- Force feedback
g923_force_feedback(true)          -- Enable force feedback
g923_test_effects()                -- Test force feedback

-- Save configuration
g923_save_config()                 -- Save settings permanently
```

## 🔧 Essential Console Commands

### Status & Information
- `g923_status()` - Check mod status and hardware detection
- `g923_help()` - Show all available commands
- `g923_config()` - Display current configuration

### Configuration
- `g923_sensitivity(1.5)` - Set steering sensitivity (0.1-5.0)
- `g923_curve("exponential")` - Set response curve (linear/exponential/s-curve)
- `g923_deadzone(5, 10, 5)` - Set deadzones (steering, throttle, brake)
- `g923_force_feedback(true)` - Enable/disable force feedback

### Testing & Debug
- `g923_debug(true)` - Enable debug mode with live input display
- `g923_simulate(true)` - Test without hardware (simulation mode)
- `g923_test_effects()` - Test force feedback effects

## 🚗 Vehicle-Specific Recommendations

### Cars (Default Settings)
- **Sensitivity**: 1.2-1.8 (recommended: 1.5)
- **Curve**: Exponential for realistic progressive steering
- **Deadzone**: 3-8° depending on preference

### Motorcycles
- **Sensitivity**: 0.8-1.2 (more responsive)
- **Curve**: Linear for direct response
- **Deadzone**: 2-5° for precision

### Trucks & Heavy Vehicles
- **Sensitivity**: 2.0-3.0 (slower, heavier feel)
- **Curve**: S-curve for gradual response
- **Deadzone**: 5-10° for stability

## 🛠️ Troubleshooting

### ❌ "Mod not found" or Console Commands Don't Work
**Cause**: CET not installed or mod in wrong location
**Solution**:
1. Install [Cyber Engine Tweaks](https://www.nexusmods.com/cyberpunk2077/mods/107) first
2. Verify mod is in: `cyber_engine_tweaks/mods/cyberpunk-g923-mod/`
3. Check CET console works (~ key)

### ❌ Wheel Not Detected
**Troubleshooting**:
1. Check G HUB is running: `g923_status()`
2. Enable simulation mode: `g923_simulate(true)`
3. Check console for error messages: `g923_debug(true)`

### ❌ No Vehicle Response
**Troubleshooting**:
1. Enable debug mode: `g923_debug(true)`
2. Check for input values in console while driving
3. Try manual override: `g923_override(true)`
4. Verify in-vehicle: `g923_status()` should show "In Vehicle: true"

### ❌ Force Feedback Not Working
**Troubleshooting**:
1. Check G HUB force feedback is enabled
2. Test effects: `g923_test_effects()`
3. Enable force feedback: `g923_force_feedback(true)`
4. Check wheel supports force feedback (G923 does)

### ❌ Performance Issues
**Solutions**:
1. Disable debug output: `g923_debug(false)`
2. Reduce sensitivity if too responsive: `g923_sensitivity(0.8)`
3. Check performance status: `g923_performance("status")`

## 📖 Documentation Files

- **README.md** - Technical documentation and features
- **README_NEXUS.md** - Nexus Mods description (BBCode formatted)
- **CHANGELOG.md** - Version history and features
- **mod_info.json** - Mod metadata and configuration

## 🎯 Getting Help

1. **Enable debug mode**: `g923_debug(true)`
2. **Check status**: `g923_status()`
3. **Monitor console output** while testing
4. **Report issues** with full console output
5. **Include system specs** and wheel model

## 📝 Configuration File Location

Settings are saved to: `mods/cyberpunk-g923-mod/config.json`

Example configuration:
```json
{
  "steeringDeadzone": 0.05,
  "steeringSensitivity": 1.5,
  "steeringCurve": "exponential",
  "forceFeedbackEnabled": true,
  "carSensitivity": 1.2,
  "motorcycleSensitivity": 0.8,
  "truckSensitivity": 1.8
}
```

---

**🌃 Ready to experience Night City with realistic steering control! 🚗💨**
