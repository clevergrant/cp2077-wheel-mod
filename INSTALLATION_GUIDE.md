# Installation Guide - Cyberpunk 2077 G923 Steering Wheel Mod v3.0.0

## 📋 Prerequisites

### Required Software
- **Cyberpunk 2077** (Version 2.0+)
- **Cyber Engine Tweaks (CET)** - Latest version
- **Logitech G HUB** - For G923 device drivers
- **Windows 10/11** - For DirectInput API support

### Hardware Requirements
- **Logitech G923 Steering Wheel** (Primary support)
- **USB 3.0 Port** (Recommended for optimal performance)
- **Stable PC Performance** (60+ FPS recommended)

---

## 🚀 Installation Steps

### Step 1: Install Cyber Engine Tweaks
1. Download CET from the official repository
2. Extract to your Cyberpunk 2077 game directory
3. Launch the game once to initialize CET
4. Verify CET is working (press `~` key for console)

### Step 2: Install G923 Mod
1. Download the `cyberpunk-g923-mod` folder
2. Copy the entire folder to: `Cyberpunk 2077/bin/x64/plugins/cyber_engine_tweaks/mods/`
3. Your structure should look like:
   ```
   Cyberpunk 2077/
   └── bin/x64/plugins/cyber_engine_tweaks/mods/
       └── cyberpunk-g923-mod/
           ├── init.lua
           ├── modules/
           └── assets/
   ```

### Step 3: Connect Your G923
1. Connect your Logitech G923 via USB
2. Install/update Logitech G HUB
3. Ensure the wheel is detected in G HUB
4. Set the wheel to "Cyberpunk 2077" game mode (if available)

### Step 4: Launch and Configure
1. Start Cyberpunk 2077
2. Open the CET console (press `~`)
3. Type `g923_status()` to verify mod is loaded
4. Type `g923_help()` to see all available commands

---

## ⚙️ First-Time Setup

### Quick Setup (Recommended)
```lua
-- In CET console (~)
g923_status()              -- Check mod is working
g923_calibrate("auto")     -- Start automatic calibration
g923_performance("enable") -- Enable performance monitoring
```

### Manual Setup (Advanced Users)
```lua
-- Sensitivity adjustment
g923_sensitivity(1.5)      -- Set global sensitivity
g923_deadzone(0.05, 0.02, 0.02)  -- Set deadzones (steering, throttle, brake)

-- Vehicle-specific sensitivity
g923_vehicle_sensitivity(1.2, 0.8, 1.8)  -- Car, motorcycle, truck

-- Force feedback
g923_force_feedback(true)  -- Enable force feedback
g923_test_effects()        -- Test force feedback

-- Save configuration
g923_save_config()         -- Save settings permanently
```

---

## 🎮 Basic Usage

### Getting Started
1. **Enter a Vehicle**: Get into any car, motorcycle, or truck
2. **Automatic Activation**: The mod automatically detects vehicles
3. **Steering Control**: Your G923 wheel now controls vehicle steering
4. **Force Feedback**: Feel road texture, crashes, and vehicle dynamics

### Essential Commands
- `g923_status()` - Show current mod status
- `g923_calibrate("auto")` - Auto-calibrate your wheel
- `g923_sensitivity(value)` - Adjust steering sensitivity
- `g923_help()` - Show all available commands
   - Ensure all module files are present in the `modules/` folder

4. **Configure G HUB**:
   - Ensure G923 is connected and recognized
   - Set wheel to "PC Mode" (not PlayStation/Xbox mode)
   - Configure basic wheel settings in G HUB

## Testing & Configuration

### Initial Testing

1. **Start Cyberpunk 2077**
2. **Open CET Console** (tilde key ~)
3. **Check mod status**:

   ```lua
   g923_status()
   ```

Expected output:

```
[G923Mod] === G923 Mod Status ===
  Version: 2.0.0
  Initialized: true
  Wheel Connected: true
  In Vehicle: false
  Input Override Active: false
  Debug Mode: false
  DirectInput Active: true
```

### Enable Debug Mode

```lua
g923_debug(true)
g923_simulate(true)  -- Enable simulation for testing
```

### Test Without Hardware

The mod includes a simulation mode for testing:

```lua
g923_simulate(true)
g923_debug(true)
```

This will generate time-based test inputs you can see in the console.

### Vehicle Testing

1. **Enter any vehicle** in the game
2. **Check status again**:

   ```lua
   g923_status()
   ```

3. **Enable input display**:

   ```lua
   g923_vehicle_info(true)
   ```

4. **Monitor inputs** in console while driving

### Force Feedback Testing

```lua
g923_force_feedback(true)
g923_test_effects()
```

## Configuration Commands

### Basic Configuration

```lua
-- View current configuration
g923_config()

-- Set steering sensitivity
g923_sensitivity(1.5)  -- 1.5x sensitivity

-- Set steering curve
g923_curve("exponential")  -- or "linear", "s-curve"

-- Set deadzones
g923_deadzone(0.05, 0.02, 0.02)  -- steering, throttle, brake
```

### Vehicle-Specific Settings

```lua
-- Set sensitivity per vehicle type
g923_vehicle_sensitivity(1.0, 1.2, 0.8)  -- car, motorcycle, truck
```

### Advanced Settings

```lua
-- Save configuration
g923_save_config()

-- Reset to defaults
g923_reset()

-- Reload from file
g923_reload_config()
```

### Help

```lua
g923_help()  -- Show all available commands
```

## Configuration File

The mod saves settings to: `mods/cyberpunk-g923-mod/config.json`

Example configuration:

```json
{
  "steeringDeadzone": 0.05,
  "steeringSensitivity": 1.0,
  "steeringCurve": "linear",
  "throttleDeadzone": 0.02,
  "brakeDeadzone": 0.02,
  "forceFeedbackEnabled": true,
  "forceFeedbackStrength": 0.8,
  "carSensitivity": 1.0,
  "motorcycleSensitivity": 1.2,
  "truckSensitivity": 0.8,
  "debugMode": false
}
```

## Troubleshooting

### Wheel Not Detected

1. **Check G HUB**:

   ```lua
   g923_status()
   ```

   - Should show "Wheel Connected: true"

2. **Enable simulation mode** for testing:

   ```lua
   g923_simulate(true)
   ```

3. **Check DirectInput**:
   - Look for "DirectInput Active: true" in status

### No Vehicle Response

1. **Check override status**:

   ```lua
   g923_status()
   ```

   - Look for "Input Override Active: true" when in vehicle

2. **Enable debug output**:

   ```lua
   g923_debug(true)
   ```

   - Monitor console for input values

3. **Try manual override**:

   ```lua
   g923_override(true)
   ```

### Performance Issues

1. **Reduce debug output**:

   ```lua
   g923_debug(false)
   g923_vehicle_info(false)
   ```

2. **Check sensitivity settings**:

   ```lua
   g923_sensitivity(0.8)  -- Reduce if too sensitive
   ```

## Known Limitations

### DirectInput Implementation

- **Framework Complete**: Full DirectInput integration structure ready
- **Needs**: Actual Windows API implementation for hardware communication
- **Workaround**: Simulation mode provides testing capability

### CET Vehicle API

- **Multiple Approaches**: Several CET API methods implemented
- **Needs**: Game-specific testing to determine which APIs work
- **Workaround**: Debug mode shows which methods are being attempted

### Force Feedback

- **Structure Ready**: Complete force feedback framework
- **Needs**: Real DirectInput effects implementation
- **Workaround**: Effect simulation with debug output

## Next Development Steps

### For Developers

1. **Complete DirectInput**:
   - Implement actual Windows DirectInput API calls
   - Add real device polling and force feedback

2. **Test CET APIs**:
   - Research which vehicle APIs work in practice
   - Optimize the most effective input override method

3. **Performance Optimization**:
   - Profile input polling performance
   - Optimize update loops and memory usage

### For Testers

1. **Test Vehicle Compatibility**:
   - Try different vehicle types (cars, motorcycles, trucks)
   - Report which vehicles respond to wheel input

2. **Configuration Testing**:
   - Test different sensitivity curves
   - Find optimal deadzone settings

3. **Performance Testing**:
   - Monitor FPS impact during wheel use
   - Test in different game areas and situations

## Contributing

The mod is structured for easy contribution:

- **DirectInput**: `modules/directinput.lua` - Hardware communication
- **Vehicle Control**: `modules/vehicle_input_override.lua` - Game integration
- **Force Feedback**: `modules/force_feedback.lua` - Haptic effects
- **Configuration**: `modules/config.lua` - Settings management

### Testing Contributions

Even without DirectInput implementation, you can:

- Test CET API approaches
- Optimize configuration settings
- Improve simulation mode
- Test vehicle compatibility

## Support

For issues and development discussion:

- Check console output with `g923_debug(true)`
- Use `g923_status()` for system information
- Save logs when reporting issues
- Include vehicle type and game situation in reports

## License

This mod is provided for educational and entertainment purposes. Cyberpunk 2077 is a trademark of CD Projekt RED.
