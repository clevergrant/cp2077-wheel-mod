# Cyberpunk 2077 G923 Steering Wheel Mod - Development Instructions

## Project Overview

Create a mod for Cyberpunk 2077 that enables full support for the Logitech G923 steering wheel, including analog steering, pedal input, and force feedback.

## Development Environment Setup

### Required Tools

1. **Cyber Engine Tweaks (CET)** - Primary modding framework
2. **REDmod** - Official CD Projekt RED modding tools
3. **WolvenKit** - Asset extraction and modification tool
4. **Logitech G HUB** - Ensure wheel drivers are properly installed

### Project Structure

```
cyberpunk-g923-mod/
├── init.lua                 # Main CET initialization script
├── modules/
│   ├── input_handler.lua    # Wheel input detection and processing
│   ├── vehicle_control.lua  # Vehicle control override system
│   ├── force_feedback.lua   # Force feedback implementation
│   └── config.lua          # Configuration and settings
├── assets/
│   └── ui/                 # Any UI elements for configuration
└── README.md
```

## Implementation Phases

### Phase 1: Basic Input Detection

**Objective**: Detect G923 wheel and read basic inputs

**Tasks**:

1. Create main `init.lua` file that registers with CET
2. Implement DirectInput or SDL2 integration for wheel detection
3. Read steering wheel axis (-1 to 1 range)
4. Read pedal inputs (throttle, brake, clutch if applicable)
5. Map wheel buttons to game functions

**Key Code Areas**:

- Use CET's `registerForEvent` for game update loops
- Implement polling mechanism for wheel input
- Create input mapping configuration system

### Phase 2: Vehicle Control Integration

**Objective**: Override game's vehicle input system with wheel data

**Tasks**:

1. Hook into Cyberpunk's vehicle control systems
2. Replace keyboard/gamepad steering with wheel input
3. Implement analog throttle/brake control
4. Add steering sensitivity curves and dead zones
5. Handle different vehicle types (cars, motorcycles, etc.)

**Key Systems to Modify**:

- `vehicleComponent` - Main vehicle control
- `inputHandler` - Input processing
- `playerPuppet` - Player character vehicle interaction

### Phase 3: Advanced Features

**Objective**: Add force feedback and enhanced controls

**Tasks**:

1. Implement force feedback based on:
   - Road surface texture
   - Vehicle speed and handling
   - Collision forces
   - Tire grip/slip
2. Add gear shifting support (if wheel has shifter)
3. Implement handbrake mapping
4. Add vibration for engine effects

### Phase 4: Configuration & Polish

**Objective**: User-friendly configuration and optimization

**Tasks**:

1. Create in-game configuration menu
2. Add sensitivity adjustment sliders
3. Implement multiple control profiles
4. Add wheel calibration system
5. Optimize performance and reduce input lag

## Technical Implementation Details

### Input Handling (input_handler.lua)

```lua
-- Core structure for wheel input processing
local InputHandler = {}

function InputHandler.Initialize()
    -- Initialize DirectInput or SDL2
    -- Detect G923 wheel
    -- Set up input polling
end

function InputHandler.Update()
    -- Poll wheel for current state
    -- Process steering input (-1 to 1)
    -- Process pedal inputs (0 to 1)
    -- Handle button presses
    -- Apply dead zones and curves
end

function InputHandler.GetSteeringInput()
    -- Return processed steering value
end

function InputHandler.GetThrottleInput()
    -- Return processed throttle value
end

function InputHandler.GetBrakeInput()
    -- Return processed brake value
end
```

### Vehicle Control Override (vehicle_control.lua)

```lua
-- Override game's vehicle input system
local VehicleControl = {}

function VehicleControl.Initialize()
    -- Hook into vehicle control events
    -- Register input override functions
end

function VehicleControl.OverrideVehicleInput(vehicle)
    -- Replace default input with wheel input
    -- Apply steering to vehicle
    -- Control throttle and brake
    -- Handle special cases for different vehicle types
end
```

### Force Feedback System (force_feedback.lua)

```lua
-- Implement force feedback effects
local ForceFeedback = {}

function ForceFeedback.Initialize()
    -- Set up force feedback device communication
    -- Define effect types (constant, spring, damper, etc.)
end

function ForceFeedback.UpdateEffects(vehicle_state)
    -- Calculate forces based on vehicle physics
    -- Apply road texture effects
    -- Handle collision feedback
    -- Send effects to wheel
end
```

## CET Integration Points

### Main Registration (init.lua)

```lua
registerForEvent("onInit", function()
    -- Initialize all modules
    -- Set up event listeners
    -- Register console commands for debugging
end)

registerForEvent("onUpdate", function(deltaTime)
    -- Update input polling
    -- Process vehicle control
    -- Update force feedback
end)

registerForEvent("onOverlayOpen", function()
    -- Show configuration UI if needed
end)
```

### Game Event Hooks

- `onVehicleEnter` - Initialize wheel control when entering vehicle
- `onVehicleExit` - Clean up and restore default controls
- `onVehicleCollision` - Trigger force feedback effects
- `onPlayerTeleport` - Handle state transitions

## Configuration System

### Settings to Include

- Steering sensitivity (0.1 - 3.0)
- Steering dead zone (0 - 20%)
- Force feedback strength (0 - 100%)
- Pedal sensitivity curves
- Button mappings
- Vehicle-specific profiles

### Configuration File Format

```json
{
    "general": {
        "enabled": true,
        "debug_mode": false
    },
    "steering": {
        "sensitivity": 1.5,
        "dead_zone": 0.05,
        "curve_type": "linear"
    },
    "pedals": {
        "throttle_curve": "exponential",
        "brake_curve": "linear",
        "combined_pedals": false
    },
    "force_feedback": {
        "enabled": true,
        "strength": 75,
        "road_effects": true,
        "collision_effects": true
    }
}
```

## Testing Strategy

### Testing Phases

1. **Input Detection Test**: Verify wheel is detected and inputs are read correctly
2. **Basic Control Test**: Test steering and pedal input in simple driving scenarios
3. **Vehicle Compatibility Test**: Test with different vehicle types (cars, bikes, trucks)
4. **Performance Test**: Ensure no significant FPS impact
5. **Edge Case Testing**: Test vehicle entry/exit, fast travel, mission sequences

### Debug Features to Implement

- Console commands to display current input values
- On-screen overlay showing wheel state
- Logging system for troubleshooting
- Input recording and playback for testing

## Performance Considerations

### Optimization Guidelines

- Limit input polling to 60Hz or game framerate
- Use efficient data structures for input processing
- Minimize allocations in update loops
- Cache frequently accessed game objects
- Implement proper cleanup on mod disable

### Memory Management

- Properly release DirectInput/SDL2 resources
- Clean up event registrations on shutdown
- Avoid memory leaks in continuous polling loops

## Troubleshooting Common Issues

### Wheel Not Detected

- Verify G HUB is running
- Check Windows game controller settings
- Ensure wheel is in correct mode (PC mode, not console)

### Input Lag

- Reduce polling frequency if needed
- Check for conflicts with other input mods
- Verify game's vsync settings

### Force Feedback Issues

- Confirm wheel supports DirectInput force feedback
- Check Windows force feedback test functionality
- Ensure proper cleanup between effect updates

## Resources and References

### Official Documentation

- [Cyber Engine Tweaks Wiki](https://wiki.cybermods.net/cyber-engine-tweaks/)
- [REDmod Documentation](https://www.cyberpunk.net/en/modding-support)
- [Logitech Gaming Software SDK](https://www.logitechg.com/en-us/innovation/developer-lab.html)

### Community Resources

- [Cyberpunk 2077 Modding Discord](https://discord.gg/cp77modding)
- [WolvenKit Documentation](https://wiki.cybermods.net/wolvenkit/)
- [DirectInput Programming Guide](https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ee418273(v=vs.85))

### Example Mods for Reference

- Look at existing vehicle control mods
- Study input handling in other CET mods
- Examine UI mods for configuration interface patterns

## Development Workflow

1. **Setup**: Install tools, create project structure
2. **Prototype**: Basic input detection and console output
3. **Iterate**: Gradually add vehicle control features
4. **Test**: Regular testing with different scenarios
5. **Polish**: Add configuration, optimize performance
6. **Document**: Create user installation guide
7. **Release**: Package for distribution on NexusMods

## Success Criteria

The mod should achieve:

- ✅ Reliable G923 wheel detection
- ✅ Smooth analog steering control
- ✅ Proper throttle and brake pedal response
- ✅ Force feedback effects for immersion
- ✅ Configurable sensitivity and dead zones
- ✅ Compatibility with all vehicle types
- ✅ No performance impact during gameplay
- ✅ Easy installation and configuration for end users
