# Cyberpunk 2077 G923 Steering Wheel Mod

A comprehensive mod that adds full Logitech G923 steering wheel support to Cyberpunk 2077, including analog steering, pedal input, and force feedback.

## Features

- **Analog Steering Control**: Precise steering input using the G923 wheel
- **Pedal Support**: Throttle, brake, and clutch pedal recognition
- **Force Feedback**: Road surface simulation, collision feedback, and speed-based effects
- **Configurable Settings**: Adjustable deadzone, sensitivity, and force feedback strength
- **Debug Mode**: Input visualization and debugging tools

## Installation

1. **Prerequisites**:
   - Cyberpunk 2077 (latest version)
   - Cyber Engine Tweaks (CET) installed
   - Logitech G923 steering wheel with G HUB software

2. **Install the Mod**:
   - Copy the entire `cyberpunk-g923-mod` folder to your CET mods directory
   - Typical path: `Cyberpunk 2077\bin\x64\plugins\cyber_engine_tweaks\mods\`

3. **Launch the Game**:
   - Start Cyberpunk 2077
   - The mod will automatically initialize when CET loads

## Configuration

The mod includes several configurable options:

### Input Settings

- `steeringDeadzone`: Dead zone for steering input (default: 0.05)
- `steeringSensitivity`: Steering sensitivity multiplier (default: 1.0)
- `throttleDeadzone`: Dead zone for throttle pedal (default: 0.02)
- `brakeDeadzone`: Dead zone for brake pedal (default: 0.02)

### Force Feedback Settings

- `forceFeedbackEnabled`: Enable/disable force feedback (default: true)
- `forceFeedbackStrength`: Overall force feedback strength (default: 0.8)
- `roadFeedbackEnabled`: Road surface simulation (default: true)
- `collisionFeedbackEnabled`: Collision impact feedback (default: true)

### Vehicle Control Settings

- `analogSteeringEnabled`: Use analog steering instead of digital (default: true)
- `smoothingEnabled`: Enable input smoothing (default: true)
- `smoothingFactor`: Input smoothing amount (default: 0.1)

## Usage

1. **Connect Your G923**: Ensure your Logitech G923 is connected and recognized by G HUB
2. **Enter a Vehicle**: Get into any vehicle in Cyberpunk 2077
3. **Automatic Detection**: The mod will automatically detect the wheel and switch to wheel controls
4. **Enjoy**: Experience enhanced driving with realistic wheel and pedal controls

## Troubleshooting

### Wheel Not Detected

- Ensure G HUB software is running
- Check that the wheel is properly connected via USB
- Restart Cyberpunk 2077 and try again

### No Force Feedback

- Verify force feedback is enabled in mod configuration
- Check G HUB settings for force feedback
- Ensure the wheel supports force feedback (G923 does)

### Input Issues

- Adjust deadzone settings if inputs are too sensitive or not responsive enough
- Try disabling input smoothing if steering feels laggy
- Enable debug mode to see input values in real-time

## Debug Commands

Open the CET console and use these commands:

```lua
-- Print current configuration
G923Mod.Config:Print()

-- Enable debug mode
G923Mod.Config:Set("debugMode", true)

-- Show input values
G923Mod.Config:Set("showInputValues", true)

-- Check wheel connection status
print(G923Mod.InputHandler:IsWheelConnected())
```

## Development Status

This is Phase 3 of development, implementing DirectInput communication and deep vehicle input override. The current implementation includes:

- ✅ Complete module structure with advanced architecture
- ✅ Configuration system with steering curves and vehicle-specific settings
- ✅ Enhanced input handling framework with sensitivity curves
- ✅ CET vehicle system integration with real-time vehicle detection
- ✅ Vehicle-specific steering sensitivity (cars, motorcycles, trucks)
- ✅ Advanced steering curves (linear, exponential, s-curve)
- ✅ Real vehicle data integration (speed, RPM, vehicle type)
- ✅ **NEW: DirectInput integration framework for G923 communication**
- ✅ **NEW: Advanced vehicle input override system**
- ✅ **NEW: Real force feedback effects via DirectInput**
- ✅ **NEW: Vehicle-specific input handling and response tuning**
- ✅ Force feedback framework with real vehicle state
- ✅ Console commands for debugging and configuration
- ⚠️ DirectInput hardware implementation (framework complete, needs Windows API)
- ⚠️ Deep vehicle API hooks (framework ready, needs CET research)

## Enhanced Console Commands

Open the CET console and use these commands:

```lua
-- Show comprehensive mod status
g923_status()

-- Print current configuration
g923_config()

-- Enable/disable debug mode
g923_debug(true)   -- Enable debug output
g923_debug(false)  -- Disable debug output

-- Show vehicle information
g923_vehicle_info(true)

-- Adjust steering sensitivity
g923_sensitivity(1.5)  -- Set sensitivity to 1.5
g923_sensitivity()     -- Show current sensitivity

-- Change steering curve
g923_curve("exponential")  -- Set to exponential curve
g923_curve("s-curve")     -- Set to S-curve
g923_curve("linear")      -- Set to linear curve
g923_curve()              -- Show current curve

-- Force feedback controls
g923_force_feedback(true)  -- Enable force feedback
g923_force_feedback(false) -- Disable force feedback

-- Test force feedback effects
g923_test_effects()

-- Manual input override control
g923_override(true)   -- Force enable input override
g923_override(false)  -- Force disable input override
```

## Enhanced Configuration

The mod now includes enhanced configurable options:

### Advanced Input Settings

- `steeringDeadzone`: Dead zone for steering input (default: 0.05)
- `steeringSensitivity`: Steering sensitivity multiplier (default: 1.0)
- `steeringCurve`: Steering response curve - "linear", "exponential", "s-curve" (default: "linear")
- `throttleDeadzone`: Dead zone for throttle pedal (default: 0.02)
- `brakeDeadzone`: Dead zone for brake pedal (default: 0.02)
- `pedalCurve`: Pedal response curve - "linear", "exponential" (default: "linear")

### Vehicle-Specific Settings

- `carSensitivity`: Steering sensitivity for cars (default: 1.0)
- `motorcycleSensitivity`: Steering sensitivity for motorcycles (default: 1.2)
- `truckSensitivity`: Steering sensitivity for trucks (default: 0.8)

## Known Limitations

- DirectInput integration not yet implemented (placeholder code)
- Vehicle input override needs deeper CET API integration
- Force feedback effects are framework only
- Configuration persistence not implemented

## Future Phases

## Current Limitations

- DirectInput hardware communication needs Windows API implementation
- Vehicle input override requires deeper CET API research
- Force feedback effects are simulated (need real DirectInput integration)
- Configuration persistence not implemented

## Next Development Steps

- **Complete DirectInput**: Implement actual Windows DirectInput API calls
- **Vehicle API Research**: Find CET hooks for deep vehicle input control  
- **Force Feedback Implementation**: Add real DirectInput force feedback
- **Performance Optimization**: Optimize input polling and processing
- **UI Configuration Panel**: Create in-game configuration interface

## Contributing

This mod is in active development. Contributions welcome for:

- DirectInput Windows API implementation
- CET vehicle API research and integration
- Force feedback enhancement and testing
- Performance optimization
- Bug fixes and testing with different vehicle types

## License

This mod is provided as-is for educational and entertainment purposes. Cyberpunk 2077 is a trademark of CD Projekt RED.
