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
    
    -- Placeholder: simulate some input variation
    -- In real implementation, these would come from the actual wheel
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
        self.currentInputs.steering = sign * value * sensitivity
    end
    
    -- Apply throttle deadzone
    local throttleDeadzone = Config:Get("throttleDeadzone")
    if self.currentInputs.throttle < throttleDeadzone then
        self.currentInputs.throttle = 0.0
    else
        self.currentInputs.throttle = (self.currentInputs.throttle - throttleDeadzone) / (1.0 - throttleDeadzone)
    end
    
    -- Apply brake deadzone
    local brakeDeadzone = Config:Get("brakeDeadzone")
    if self.currentInputs.brake < brakeDeadzone then
        self.currentInputs.brake = 0.0
    else
        self.currentInputs.brake = (self.currentInputs.brake - brakeDeadzone) / (1.0 - brakeDeadzone)
    end
    
    -- Apply smoothing if enabled
    if Config:Get("smoothingEnabled") then
        local factor = Config:Get("smoothingFactor")
        self.currentInputs.steering = self:SmoothInput(self.lastInputs.steering, self.currentInputs.steering, factor)
        self.currentInputs.throttle = self:SmoothInput(self.lastInputs.throttle, self.currentInputs.throttle, factor)
        self.currentInputs.brake = self:SmoothInput(self.lastInputs.brake, self.currentInputs.brake, factor)
    end
end

-- Smooth input transition
function InputHandler:SmoothInput(lastValue, currentValue, factor)
    return lastValue + (currentValue - lastValue) * factor
end

-- Get current steering input
function InputHandler:GetSteering()
    return self.currentInputs.steering
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
    print(string.format("[G923Mod] Inputs: Steering=%.3f, Throttle=%.3f, Brake=%.3f", 
          self.currentInputs.steering, self.currentInputs.throttle, self.currentInputs.brake))
end

-- Shutdown input handler
function InputHandler:Shutdown()
    print("[G923Mod] Shutting down input handler...")
    
    -- TODO: Clean up DirectInput resources
    
    self.wheelConnected = false
    print("[G923Mod] Input handler shutdown complete")
end

return InputHandler
