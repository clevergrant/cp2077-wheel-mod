-- Input Handler module for G923 Steering Wheel Mod
-- Handles detection and processing of G923 wheel inputs

local Config = require("modules/config")

local InputHandler = {
    wheelConnected = false,
    currentInputs = {
        steering = 0.0,      -- -1.0 to 1.0
        throttle = 0.0,      -- 0.0 to 1.0
        brake = 0.0,         -- 0.0 to 1.0
        clutch = 0.0,        -- 0.0 to 1.0 (if available)
        buttons = {}         -- Table of button states
    },
    lastInputs = {},
    inputDevice = nil
}

-- Initialize input handler
function InputHandler:Initialize()
    print("[G923Mod] Initializing input handler...")

    -- Copy current inputs to last inputs
    self.lastInputs = {}
    for key, value in pairs(self.currentInputs) do
        if type(value) == "table" then
            self.lastInputs[key] = {}
        else
            self.lastInputs[key] = value
        end
    end

    -- Attempt to detect G923 wheel
    self:DetectWheel()

    print("[G923Mod] Input handler initialized")
end

-- Detect G923 steering wheel
function InputHandler:DetectWheel()
    print("[G923Mod] Attempting to detect G923 steering wheel...")

    -- TODO: Implement DirectInput or SDL2 integration
    -- For now, simulate wheel detection
    -- In a real implementation, this would:
    -- 1. Enumerate DirectInput devices
    -- 2. Look for Logitech G923 VID/PID
    -- 3. Initialize device communication

    -- Placeholder detection
    self.wheelConnected = true
    print("[G923Mod] G923 steering wheel detected (placeholder)")

    if not self.wheelConnected then
        print("[G923Mod] Warning: G923 steering wheel not found")
    end
end

-- Update input readings
function InputHandler:Update(deltaTime)
    if not self.wheelConnected then
        return
    end

    -- Store previous inputs
    self.lastInputs.steering = self.currentInputs.steering
    self.lastInputs.throttle = self.currentInputs.throttle
    self.lastInputs.brake = self.currentInputs.brake
    self.lastInputs.clutch = self.currentInputs.clutch

    -- TODO: Read actual input from wheel
    -- This would involve DirectInput calls to get:
    -- - Steering axis (typically X-axis)
    -- - Throttle pedal (typically Y-axis or Z-axis)
    -- - Brake pedal (typically RZ-axis or separate axis)
    -- - Button states

    -- For now, use placeholder values
    self:ReadWheelInputs()

    -- Apply deadzones and sensitivity
    self:ProcessInputs()

    -- Debug output if enabled
    if Config:Get("debugMode") and Config:Get("showInputValues") then
        self:DebugPrintInputs()
    end
end

-- Read raw inputs from wheel (placeholder implementation)
function InputHandler:ReadWheelInputs()
    -- TODO: Replace with actual DirectInput/SDL2 calls
    -- This is a placeholder that would be replaced with:
    -- local device = GetDirectInputDevice(G923_VID, G923_PID)
    -- local state = device:GetState()
    -- self.currentInputs.steering = NormalizeAxis(state.lX, -32768, 32767)
    -- etc.

    -- Placeholder: simulate some input variation for testing
    -- In real implementation, these would come from the actual wheel

    -- For now, we can use CET's input system as a fallback for testing
    -- This allows testing the vehicle control logic without actual wheel hardware
    if Config:Get("debugMode") then
        -- Use keyboard input for testing (A/D for steering, W/S for throttle/brake)
        local steering = 0.0
        if Game.IsActionPressed("MoveLeft") then
            steering = steering - 1.0
        end
        if Game.IsActionPressed("MoveRight") then
            steering = steering + 1.0
        end

        local throttle = 0.0
        if Game.IsActionPressed("MoveForward") then
            throttle = 1.0
        end

        local brake = 0.0
        if Game.IsActionPressed("MoveBack") then
            brake = 1.0
        end

        -- Apply inputs gradually for smoother testing
        self.currentInputs.steering = steering * 0.8
        self.currentInputs.throttle = throttle
        self.currentInputs.brake = brake
    end
end

-- Process inputs (apply deadzones, sensitivity, smoothing)
function InputHandler:ProcessInputs()
    local deadzone = Config:Get("steeringDeadzone")
    local sensitivity = Config:Get("steeringSensitivity")

    -- Apply steering deadzone
    if math.abs(self.currentInputs.steering) < deadzone then
        self.currentInputs.steering = 0.0
    else
        -- Apply sensitivity
        local sign = self.currentInputs.steering >= 0 and 1 or -1
        local value = math.abs(self.currentInputs.steering)
        value = (value - deadzone) / (1.0 - deadzone)

        -- Apply steering curve
        value = self:ApplySteeringCurve(value)

        self.currentInputs.steering = sign * value * sensitivity
    end

    -- Apply throttle deadzone and curve
    local throttleDeadzone = Config:Get("throttleDeadzone")
    if self.currentInputs.throttle < throttleDeadzone then
        self.currentInputs.throttle = 0.0
    else
        local value = (self.currentInputs.throttle - throttleDeadzone) / (1.0 - throttleDeadzone)
        self.currentInputs.throttle = self:ApplyPedalCurve(value)
    end

    -- Apply brake deadzone and curve
    local brakeDeadzone = Config:Get("brakeDeadzone")
    if self.currentInputs.brake < brakeDeadzone then
        self.currentInputs.brake = 0.0
    else
        local value = (self.currentInputs.brake - brakeDeadzone) / (1.0 - brakeDeadzone)
        self.currentInputs.brake = self:ApplyPedalCurve(value)
    end

    -- Apply smoothing if enabled
    if Config:Get("smoothingEnabled") then
        local factor = Config:Get("smoothingFactor")
        self.currentInputs.steering = self:SmoothInput(self.lastInputs.steering, self.currentInputs.steering, factor)
        self.currentInputs.throttle = self:SmoothInput(self.lastInputs.throttle, self.currentInputs.throttle, factor)
        self.currentInputs.brake = self:SmoothInput(self.lastInputs.brake, self.currentInputs.brake, factor)
    end
end

-- Apply steering curve based on configuration
function InputHandler:ApplySteeringCurve(value)
    local curveType = Config:Get("steeringCurve")

    if curveType == "exponential" then
        -- Exponential curve: more precision at center, more aggressive at extremes
        local sign = value >= 0 and 1 or -1
        return sign * (value * value)
    elseif curveType == "s-curve" then
        -- S-curve: smooth transition with precision at center and extremes
        local x = value * 2 - 1 -- Convert to -1 to 1 range
        local result = x * x * x -- Cubic function
        return (result + 1) / 2 -- Convert back to 0 to 1 range
    else
        -- Linear curve (default)
        return value
    end
end

-- Apply pedal curve based on configuration
function InputHandler:ApplyPedalCurve(value)
    local curveType = Config:Get("pedalCurve")

    if curveType == "exponential" then
        -- Exponential curve for more gradual initial response
        return value * value
    else
        -- Linear curve (default)
        return value
    end
end

-- Smooth input transition
function InputHandler:SmoothInput(lastValue, currentValue, factor)
    return lastValue + (currentValue - lastValue) * factor
end

-- Get current steering input with vehicle-specific adjustments
function InputHandler:GetSteering()
    local steering = self.currentInputs.steering

    -- Apply vehicle-specific sensitivity
    local VehicleControl = require("modules/vehicle_control")
    local vehicleInfo = VehicleControl:GetVehicleInfo()

    if vehicleInfo then
        local vehicleType = vehicleInfo.type or "unknown"
        local sensitivityMultiplier = 1.0

        if string.find(vehicleType:lower(), "motorcycle") or string.find(vehicleType:lower(), "bike") then
            sensitivityMultiplier = Config:Get("motorcycleSensitivity")
        elseif string.find(vehicleType:lower(), "truck") or string.find(vehicleType:lower(), "van") then
            sensitivityMultiplier = Config:Get("truckSensitivity")
        else
            sensitivityMultiplier = Config:Get("carSensitivity")
        end

        steering = steering * sensitivityMultiplier
    end

    -- Clamp to valid range
    return math.max(-1.0, math.min(1.0, steering))
end

-- Get current throttle input
function InputHandler:GetThrottle()
    return self.currentInputs.throttle
end

-- Get current brake input
function InputHandler:GetBrake()
    return self.currentInputs.brake
end

-- Get current clutch input
function InputHandler:GetClutch()
    return self.currentInputs.clutch
end

-- Check if wheel is connected
function InputHandler:IsWheelConnected()
    return self.wheelConnected
end

-- Debug print current inputs
function InputHandler:DebugPrintInputs()
    local steering = self:GetSteering() -- Use the processed steering value
    print(string.format("[G923Mod] Inputs: Steering=%.3f, Throttle=%.3f, Brake=%.3f",
          steering, self.currentInputs.throttle, self.currentInputs.brake))

    -- Show vehicle info if enabled
    if Config:Get("showVehicleInfo") then
        local VehicleControl = require("modules/vehicle_control")
        local vehicleInfo = VehicleControl:GetVehicleInfo()
        if vehicleInfo then
            print(string.format("[G923Mod] Vehicle: %s (Type: %s, Speed: %.1f, RPM: %.0f)",
                  vehicleInfo.name, vehicleInfo.type, vehicleInfo.speed, vehicleInfo.rpm))
        end
    end
end

-- Shutdown input handler
function InputHandler:Shutdown()
    print("[G923Mod] Shutting down input handler...")

    -- TODO: Clean up DirectInput resources

    self.wheelConnected = false
    print("[G923Mod] Input handler shutdown complete")
end

return InputHandler
