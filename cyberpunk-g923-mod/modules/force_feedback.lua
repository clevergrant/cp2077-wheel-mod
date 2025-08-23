-- Force Feedback module for G923 Steering Wheel Mod
-- Handles force feedback effects based on game events

local Config = require("modules/config")
local InputHandler = require("modules/input_handler")

local ForceFeedback = {
    initialized = false,
    effectsEnabled = false,
    directInput = nil,
    currentEffects = {},
    lastVehicleState = {
        speed = 0,
        rpm = 0,
        onRoad = true,
        collision = false
    }
}

-- Initialize force feedback system
function ForceFeedback:Initialize()
    print("[G923Mod] Initializing force feedback system...")

    if not InputHandler:IsWheelConnected() then
        print("[G923Mod] No wheel connected, force feedback disabled")
        return
    end

    -- Get DirectInput interface from InputHandler
    self.directInput = InputHandler.directInput

    if not self.directInput or not self.directInput.capabilities.forceFeedback then
        print("[G923Mod] Force feedback not supported on this device")
        return
    end

    self.effectsEnabled = Config:Get("forceFeedbackEnabled")
    self.initialized = true

    if self.effectsEnabled then
        self:CreateBaseEffects()
        print("[G923Mod] Force feedback system initialized")
    else
        print("[G923Mod] Force feedback disabled in configuration")
    end
end

-- Create base force feedback effects
function ForceFeedback:CreateBaseEffects()
    -- Create DirectInput force feedback effects using the DirectInput interface

    -- 1. Spring effect (centering force)
    self:CreateSpringEffect()

    -- 2. Damper effect (resistance to movement)
    self:CreateDamperEffect()

    -- 3. Friction effect (road surface simulation)
    self:CreateFrictionEffect()

    -- 4. Impact effects (for collisions)
    self:CreateImpactEffects()
end

-- Create spring centering effect
function ForceFeedback:CreateSpringEffect()
    if self.directInput then
        local strength = Config:Get("forceFeedbackStrength") * 0.5 -- Base centering strength
        self.directInput:CreateSpringEffect(strength)
        print("[G923Mod] Created spring centering effect")
    end
end

-- Create damper resistance effect
function ForceFeedback:CreateDamperEffect()
    if self.directInput then
        local strength = Config:Get("forceFeedbackStrength") * 0.3 -- Base damping strength
        self.directInput:CreateDamperEffect(strength)
        print("[G923Mod] Created damper resistance effect")
    end
end

-- Create friction road surface effect
function ForceFeedback:CreateFrictionEffect()
    if self.directInput then
        local strength = Config:Get("forceFeedbackStrength") * 0.4 -- Base friction strength
        self.directInput:CreateFrictionEffect(strength)
        print("[G923Mod] Created friction road surface effect")
    end
end

-- Create impact collision effects
function ForceFeedback:CreateImpactEffects()
    -- Impact effects are created on-demand during collisions
    -- This just prepares the system for impact feedback
    print("[G923Mod] Impact collision effects ready")
end
    -- - Light collision
    -- - Heavy collision
    -- - Explosion nearby

    print("[G923Mod] Created impact collision effects")
end

-- Update force feedback based on game state
function ForceFeedback:Update(deltaTime)
    if not self.initialized or not self.effectsEnabled then
        return
    end

    -- Get current vehicle state
    local vehicleState = self:GetVehicleState()

    if vehicleState then
        -- Update road surface feedback
        if Config:Get("roadFeedbackEnabled") then
            self:UpdateRoadFeedback(vehicleState)
        end

        -- Update speed-based effects
        self:UpdateSpeedEffects(vehicleState)

        -- Check for collision feedback
        if Config:Get("collisionFeedbackEnabled") then
            self:UpdateCollisionFeedback(vehicleState)
        end

        -- Store state for next frame
        self.lastVehicleState = vehicleState
    end
end

-- Get current vehicle state for force feedback
function ForceFeedback:GetVehicleState()
    -- Get vehicle state from the vehicle control module
    local VehicleControl = require("modules/vehicle_control")
    local vehicleInfo = VehicleControl:GetVehicleInfo()

    if not vehicleInfo then
        return nil
    end

    -- Get more detailed vehicle state from game
    local vehicleState = {
        speed = vehicleInfo.speed or 0,
        rpm = vehicleInfo.rpm or 0,
        onRoad = true,
        collision = false,
        surfaceType = "asphalt",
        vehicleType = vehicleInfo.type or "unknown",
        wheelSlip = 0,
        braking = false,
        accelerating = false
    }

    -- Try to get additional vehicle data from blackboard
    pcall(function()
        local VehicleControl = require("modules/vehicle_control")
        if VehicleControl.currentVehicle then
            local blackboard = VehicleControl.currentVehicle:GetBlackboard()
            if blackboard then
                -- Get surface information
                vehicleState.onRoad = blackboard:GetBool("onRoad") or true
                vehicleState.surfaceType = blackboard:GetString("surfaceType") or "asphalt"

                -- Get tire/physics data
                vehicleState.wheelSlip = blackboard:GetFloat("wheelSlip") or 0
                vehicleState.braking = blackboard:GetBool("isBraking") or false
                vehicleState.accelerating = blackboard:GetBool("isAccelerating") or false

                -- Get steering forces
                vehicleState.steeringForce = blackboard:GetFloat("steeringForce") or 0
            end
        end
    end)

    -- Detect collision based on sudden speed changes or vehicle events
    if self.lastVehicleState then
        local speedDelta = math.abs(vehicleState.speed - self.lastVehicleState.speed)
        if speedDelta > 20 then -- Sudden speed change threshold
            vehicleState.collision = true
        end
    end

    -- Get input handler state for additional feedback
    local InputHandler = require("modules/input_handler")
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    vehicleState.accelerating = throttle > 0.1
    vehicleState.braking = brake > 0.1

    return vehicleState
end

-- Update road surface feedback
function ForceFeedback:UpdateRoadFeedback(vehicleState)
    if not Config:Get("roadFeedbackEnabled") then
        return
    end

    -- Adjust friction and damping based on road surface
    local surfaceMultiplier = self:GetSurfaceMultiplier(vehicleState.surfaceType)

    -- Add road texture effects
    if vehicleState.speed > 5 then -- Only apply when moving
        local textureIntensity = surfaceMultiplier * (vehicleState.speed / 50.0)
        textureIntensity = math.min(textureIntensity, 1.0)

        -- Apply surface-specific texture
        self:ApplyRoadTexture(vehicleState.surfaceType, textureIntensity)
    end

    -- Update base friction level
    self:SetFrictionLevel(surfaceMultiplier)
end

-- Apply road texture effects
function ForceFeedback:ApplyRoadTexture(surfaceType, intensity)
    if not self.directInput then
        return
    end

    -- Generate road texture vibration patterns
    local patterns = {
        asphalt = { frequency = 5, amplitude = 0.1 },
        dirt = { frequency = 15, amplitude = 0.3 },
        gravel = { frequency = 25, amplitude = 0.5 },
        grass = { frequency = 8, amplitude = 0.2 },
        concrete = { frequency = 3, amplitude = 0.05 }
    }

    local pattern = patterns[surfaceType] or patterns.asphalt

    -- Create periodic force effect for texture
    local textureForce = math.sin(os.clock() * pattern.frequency) * pattern.amplitude * intensity
    self.directInput:SendForceEffect("texture", textureForce, 100)
end

-- Get surface friction multiplier
function ForceFeedback:GetSurfaceMultiplier(surfaceType)
    local multipliers = {
        asphalt = 1.0,
        concrete = 0.9,
        dirt = 1.5,
        gravel = 2.0,
        grass = 0.7,
        sand = 1.8,
        ice = 0.3,
        water = 0.5
    }

    return multipliers[surfaceType] or 1.0
end

-- Update speed-based effects
function ForceFeedback:UpdateSpeedEffects(vehicleState)
    -- Increase damping at higher speeds for stability
    local speedDamping = math.min(vehicleState.speed / 100.0, 1.0) * 0.5
    self:SetDampingLevel(speedDamping)

    -- Add slight vibration based on engine RPM
    if vehicleState.rpm > 0 then
        local rpmVibration = (vehicleState.rpm / 8000.0) * 0.3
        self:SetVibrationLevel(rpmVibration)
    end
end

-- Update collision feedback
function ForceFeedback:UpdateCollisionFeedback(vehicleState)
    if vehicleState.collision and not self.lastVehicleState.collision then
        -- Trigger collision impact effect
        self:TriggerCollisionEffect()
    end
end

-- Set friction level
function ForceFeedback:SetFrictionLevel(level)
    if self.directInput then
        local adjustedLevel = level * Config:Get("forceFeedbackStrength")
        self.directInput:CreateFrictionEffect(adjustedLevel)
    end

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting friction level: %.2f", level))
    end
end

-- Set damping level
function ForceFeedback:SetDampingLevel(level)
    if self.directInput then
        local adjustedLevel = level * Config:Get("forceFeedbackStrength")
        self.directInput:CreateDamperEffect(adjustedLevel)
    end

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting damping level: %.2f", level))
    end
end

-- Set vibration level
function ForceFeedback:SetVibrationLevel(level)
    if self.directInput then
        local adjustedLevel = level * Config:Get("forceFeedbackStrength")
        -- Use a short duration impact effect for vibration simulation
        self.directInput:CreateImpactEffect(adjustedLevel, 100) -- 100ms vibration
    end

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting vibration level: %.2f", level))
    end
end

-- Trigger collision impact effect
function ForceFeedback:TriggerCollisionEffect()
    if self.directInput then
        local impactStrength = Config:Get("forceFeedbackStrength") * 0.8
        self.directInput:CreateImpactEffect(impactStrength, 300) -- 300ms impact
    end
    -- This would be a short, strong force pulse

    print("[G923Mod] Collision impact feedback triggered")
end

-- Enable/disable force feedback
function ForceFeedback:SetEnabled(enabled)
    self.effectsEnabled = enabled and self.initialized
    Config:Set("forceFeedbackEnabled", enabled)

    if not self.effectsEnabled then
        self:StopAllEffects()
    end

    print("[G923Mod] Force feedback " .. (enabled and "enabled" or "disabled"))
end

-- Stop all active effects
function ForceFeedback:StopAllEffects()
    if self.directInput then
        self.directInput:StopAllEffects()
    end

    print("[G923Mod] All force feedback effects stopped")
end

-- Shutdown force feedback system
function ForceFeedback:Shutdown()
    print("[G923Mod] Shutting down force feedback system...")

    if self.initialized then
        self:StopAllEffects()

        -- TODO: Release DirectInput force feedback resources
    end

    self.initialized = false
    self.effectsEnabled = false

    print("[G923Mod] Force feedback system shutdown complete")
end

return ForceFeedback
