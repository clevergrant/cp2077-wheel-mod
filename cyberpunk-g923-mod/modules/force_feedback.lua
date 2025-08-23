-- Force Feedback module for G923 Steering Wheel Mod
-- Handles force feedback effects based on game events

local Config = require("modules/config")
local InputHandler = require("modules/input_handler")

local ForceFeedback = {
    initialized = false,
    effectsEnabled = false,
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

    -- TODO: Initialize DirectInput force feedback
    -- This would involve:
    -- 1. Getting the force feedback device interface
    -- 2. Setting up effect parameters
    -- 3. Creating base effects (spring, damper, friction)

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
    -- TODO: Create DirectInput force feedback effects
    -- Base effects would include:

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
    -- TODO: Implement DirectInput spring effect
    -- Parameters:
    -- - Coefficient: strength of centering force
    -- - Saturation: maximum force level
    -- - Dead band: area around center with no force

    print("[G923Mod] Created spring centering effect")
end

-- Create damper resistance effect
function ForceFeedback:CreateDamperEffect()
    -- TODO: Implement DirectInput damper effect
    -- Parameters:
    -- - Coefficient: resistance to movement
    -- - Saturation: maximum damping force

    print("[G923Mod] Created damper resistance effect")
end

-- Create friction road surface effect
function ForceFeedback:CreateFrictionEffect()
    -- TODO: Implement DirectInput friction effect
    -- Parameters:
    -- - Coefficient: friction strength
    -- - Saturation: maximum friction force

    print("[G923Mod] Created friction road surface effect")
end

-- Create impact collision effects
function ForceFeedback:CreateImpactEffects()
    -- TODO: Create various impact effects
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

    -- Convert vehicle info to force feedback state
    local vehicleState = {
        speed = vehicleInfo.speed or 0,
        rpm = vehicleInfo.rpm or 0,
        onRoad = true, -- TODO: Detect road surface from game
        collision = false, -- TODO: Detect collisions from game events
        surfaceType = "asphalt", -- TODO: Get actual surface type
        vehicleType = vehicleInfo.type or "unknown"
    }

    -- Detect collision based on sudden speed changes
    if self.lastVehicleState and self.lastVehicleState.speed > 0 then
        local speedDelta = math.abs(vehicleState.speed - self.lastVehicleState.speed)
        if speedDelta > 20 then -- Sudden speed change threshold
            vehicleState.collision = true
        end
    end

    return vehicleState
end

-- Update road surface feedback
function ForceFeedback:UpdateRoadFeedback(vehicleState)
    if not vehicleState.onRoad then
        return
    end

    -- TODO: Adjust friction and damping based on road surface
    -- Different surfaces would have different characteristics:
    -- - Asphalt: smooth, moderate friction
    -- - Dirt: rough, high friction
    -- - Gravel: very rough, variable friction
    -- - Grass: soft, low friction

    local surfaceMultiplier = self:GetSurfaceMultiplier(vehicleState.surfaceType)
    self:SetFrictionLevel(surfaceMultiplier)
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
    -- TODO: Update DirectInput friction effect
    -- Adjust the coefficient of the friction effect

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting friction level: %.2f", level))
    end
end

-- Set damping level
function ForceFeedback:SetDampingLevel(level)
    -- TODO: Update DirectInput damper effect
    -- Adjust the coefficient of the damper effect

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting damping level: %.2f", level))
    end
end

-- Set vibration level
function ForceFeedback:SetVibrationLevel(level)
    -- TODO: Create periodic vibration effect
    -- This could be a sine wave or random vibration

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Setting vibration level: %.2f", level))
    end
end

-- Trigger collision impact effect
function ForceFeedback:TriggerCollisionEffect()
    -- TODO: Play collision impact effect
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
    -- TODO: Stop all DirectInput effects

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
