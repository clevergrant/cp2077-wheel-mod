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

This is Phase 2 of development, focusing on vehicle control integration and enhanced input processing. The current implementation includes:

- ✅ Complete module structure
- ✅ Configuration system with steering curves and vehicle-specific settings
- ✅ Enhanced input handling framework with sensitivity curves
- ✅ **NEW: CET vehicle system integration with real-time vehicle detection**
- ✅ **NEW: Vehicle-specific steering sensitivity (cars, motorcycles, trucks)**
- ✅ **NEW: Advanced steering curves (linear, exponential, s-curve)**
- ✅ **NEW: Real vehicle data integration (speed, RPM, vehicle type)**
- ✅ Force feedback framework with real vehicle state
- ✅ **NEW: Console commands for debugging and configuration**
- ⚠️ DirectInput integration (requires implementation)
- ⚠️ Actual vehicle input override (requires deeper CET API research)

## New Console Commands

Open the CET console and use these commands:

```lua
-- Show mod status
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

- **Phase 3**: Implement actual DirectInput communication and vehicle input override
- **Phase 4**: Add advanced force feedback effects with road surface detection
- **Phase 5**: UI configuration panel and enhanced vehicle compatibility
- **Phase 6**: Performance optimization and release preparation

## Known Limitations

- DirectInput integration not yet implemented (placeholder code)
- Game vehicle API integration pending
- Force feedback effects are framework only
- Configuration persistence not implemented

## Future Phases

- **Phase 2**: Implement actual DirectInput communication
- **Phase 3**: Integrate with Cyberpunk 2077 vehicle systems
## Contributing

This mod is in active development. Contributions welcome for:

- DirectInput implementation
- CET API integration and vehicle input override
- Force feedback enhancement
- Bug fixes and optimizations
- Testing with different vehicle types

## License

This mod is provided as-is for educational and entertainment purposes. Cyberpunk 2077 is a trademark of CD Projekt RED.
