-- Advanced Vehicle Input Override module for G923 Steering Wheel Mod
-- Handles deep integration with Cyberpunk 2077's vehicle input system

local Config = require("modules/config")
local InputHandler = require("modules/input_handler")

local VehicleInputOverride = {
    initialized = false,
    active = false,
    originalHooks = {},
    inputOverrides = {},
    currentVehicle = nil,

    -- Supported vehicle types and their specific handling
    vehicleTypes = {
        car = { steeringMultiplier = 1.0, responseDelay = 0.02 },
        motorcycle = { steeringMultiplier = 1.2, responseDelay = 0.01 },
        truck = { steeringMultiplier = 0.8, responseDelay = 0.04 },
        van = { steeringMultiplier = 0.9, responseDelay = 0.03 }
    }
}

-- Initialize the vehicle input override system
function VehicleInputOverride:Initialize()
    print("[G923Mod] Initializing vehicle input override system...")

    -- Set up hooks for vehicle input interception
    self:SetupInputHooks()

    -- Register for vehicle events
    self:RegisterVehicleEvents()

    self.initialized = true
    print("[G923Mod] Vehicle input override system initialized")
end

-- Set up input system hooks
function VehicleInputOverride:SetupInputHooks()
    print("[G923Mod] Setting up vehicle input hooks...")

    -- Hook into CET vehicle input system using ObserveAfter and Override
    -- These hooks intercept the game's vehicle input processing

    -- Hook vehicle steering input
    ObserveAfter("VehicleComponent", "GetSteeringInput", function(this)
        if self.active and InputHandler:IsWheelConnected() then
            local wheelSteering = InputHandler:GetSteering()
            if math.abs(wheelSteering) > 0.01 then
                -- Override the steering input with wheel data
                return wheelSteering
            end
        end
    end)

    -- Hook vehicle throttle input
    ObserveAfter("VehicleComponent", "GetThrottleInput", function(this)
        if self.active and InputHandler:IsWheelConnected() then
            local wheelThrottle = InputHandler:GetThrottle()
            if wheelThrottle > 0.01 then
                return wheelThrottle
            end
        end
    end)

    -- Hook vehicle brake input
    ObserveAfter("VehicleComponent", "GetBrakeInput", function(this)
        if self.active and InputHandler:IsWheelConnected() then
            local wheelBrake = InputHandler:GetBrake()
            if wheelBrake > 0.01 then
                return wheelBrake
            end
        end
    end)

    -- Hook into vehicle input processing for more direct control
    ObserveAfter("VehicleObject", "OnUpdate", function(this, deltaTime)
        if self.active and InputHandler:IsWheelConnected() then
            self:ApplyDirectVehicleInput(this, deltaTime)
        end
    end)

    -- Hook vehicle physics for enhanced control
    ObserveAfter("VehicleController", "Update", function(this, deltaTime)
        if self.active and InputHandler:IsWheelConnected() then
            self:ApplyPhysicsInput(this, deltaTime)
        end
    end)

    print("[G923Mod] Vehicle input hooks configured")
end

-- Register for vehicle-related events
function VehicleInputOverride:RegisterVehicleEvents()
    -- Register for vehicle enter/exit to activate/deactivate input override
    registerForEvent("onEnterVehicle", function(vehicle)
        self:OnVehicleEntered(vehicle)
    end)

    registerForEvent("onExitVehicle", function()
        self:OnVehicleExited()
    end)

    -- Register for input processing events
    registerForEvent("onUpdate", function(deltaTime)
        if self.active then
            self:ProcessVehicleInput(deltaTime)
        end
    end)
end

-- Handle vehicle entered event
function VehicleInputOverride:OnVehicleEntered(vehicle)
    if not self.initialized or not InputHandler:IsWheelConnected() then
        return
    end

    print("[G923Mod] Activating input override for vehicle")

    self.currentVehicle = vehicle
    self.active = true

    -- Get vehicle-specific settings
    local vehicleType = self:GetVehicleType(vehicle)
    print(string.format("[G923Mod] Vehicle type detected: %s", vehicleType))

    -- Apply vehicle-specific configuration
    self:ApplyVehicleSettings(vehicleType)
end

-- Handle vehicle exited event
function VehicleInputOverride:OnVehicleExited()
    if not self.active then
        return
    end

    print("[G923Mod] Deactivating input override")

    self.active = false
    self.currentVehicle = nil

    -- Restore any overridden settings
    self:RestoreOriginalSettings()
end

-- Process vehicle input each frame
function VehicleInputOverride:ProcessVehicleInput(deltaTime)
    if not self.active or not self.currentVehicle then
        return
    end

    -- Get current wheel inputs
    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    -- Apply vehicle-specific modifications
    local vehicleType = self:GetVehicleType(self.currentVehicle)
    steering = self:ApplyVehicleSteeringModification(steering, vehicleType)

    -- Apply the inputs to the vehicle
    self:ApplyInputsToVehicle(steering, throttle, brake)
end

-- Get vehicle type from vehicle object
function VehicleInputOverride:GetVehicleType(vehicle)
    if not vehicle then
        return "car" -- Default fallback
    end

    -- Try to determine vehicle type from game data
    local vehicleType = "car" -- Default

    pcall(function()
        local record = vehicle:GetRecord()
        if record then
            local vehicleTypeName = tostring(record.type):lower()

            if string.find(vehicleTypeName, "motorcycle") or string.find(vehicleTypeName, "bike") then
                vehicleType = "motorcycle"
            elseif string.find(vehicleTypeName, "truck") then
                vehicleType = "truck"
            elseif string.find(vehicleTypeName, "van") then
                vehicleType = "van"
            else
                vehicleType = "car"
            end
        end
    end)

    return vehicleType
end

-- Apply vehicle-specific settings
function VehicleInputOverride:ApplyVehicleSettings(vehicleType)
    local settings = self.vehicleTypes[vehicleType] or self.vehicleTypes.car

    -- Store original settings for restoration
    self.originalHooks.steeringMultiplier = Config:Get("steeringSensitivity")

    -- Apply vehicle-specific sensitivity
    local baseSensitivity = Config:Get("steeringSensitivity")
    local vehicleSensitivity = baseSensitivity * settings.steeringMultiplier

    -- Temporarily override sensitivity for this vehicle session
    self.inputOverrides.steeringSensitivity = vehicleSensitivity

    print(string.format("[G923Mod] Applied %s settings: sensitivity=%.2f",
          vehicleType, vehicleSensitivity))
end

-- Apply vehicle-specific steering modifications
function VehicleInputOverride:ApplyVehicleSteeringModification(steering, vehicleType)
    local settings = self.vehicleTypes[vehicleType] or self.vehicleTypes.car

    -- Apply vehicle-specific multiplier if we have an override
    if self.inputOverrides.steeringSensitivity then
        local baseSensitivity = Config:Get("steeringSensitivity")
        local multiplier = self.inputOverrides.steeringSensitivity / baseSensitivity
        steering = steering * multiplier
    end

    -- Clamp to valid range
    return math.max(-1.0, math.min(1.0, steering))
end

-- Apply processed inputs to the vehicle
function VehicleInputOverride:ApplyInputsToVehicle(steering, throttle, brake)
    if not self.currentVehicle then
        return
    end

    -- Apply inputs using multiple CET vehicle API approaches
    pcall(function()
        -- Method 1: Try direct vehicle input setting
        if self.currentVehicle.SetInputData then
            self.currentVehicle:SetInputData("steering", steering)
            self.currentVehicle:SetInputData("throttle", throttle)
            self.currentVehicle:SetInputData("brake", brake)
        end

        -- Method 2: Try blackboard input writing
        local blackboard = self.currentVehicle:GetBlackboard()
        if blackboard then
            blackboard:SetFloat("inputSteering", steering)
            blackboard:SetFloat("inputThrottle", throttle)
            blackboard:SetFloat("inputBrake", brake)

            -- Also try alternative blackboard keys
            blackboard:SetFloat("vehicleSteeringInput", steering)
            blackboard:SetFloat("vehicleThrottleInput", throttle)
            blackboard:SetFloat("vehicleBrakeInput", brake)
        end

        -- Method 3: Try vehicle physics control
        if self.currentVehicle.GetVehicleComponent then
            local vehicleComponent = self.currentVehicle:GetVehicleComponent()
            if vehicleComponent then
                vehicleComponent:SetSteeringInput(steering)
                vehicleComponent:SetThrottleInput(throttle)
                vehicleComponent:SetBrakeInput(brake)
            end
        end
    end)

    -- Debug logging
    if Config:Get("debugMode") and (math.abs(steering) > 0.01 or throttle > 0.01 or brake > 0.01) then
        print(string.format("[G923Mod] Applied inputs: S=%.3f, T=%.3f, B=%.3f",
                           steering, throttle, brake))
    end
end

-- Apply direct vehicle input (called from vehicle update hook)
function VehicleInputOverride:ApplyDirectVehicleInput(vehicleObject, deltaTime)
    if not self.active or not InputHandler:IsWheelConnected() then
        return
    end

    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    -- Try direct manipulation of vehicle object
    pcall(function()
        -- Attempt direct vehicle object manipulation
        if vehicleObject.inputBuffer then
            vehicleObject.inputBuffer.steering = steering
            vehicleObject.inputBuffer.throttle = throttle
            vehicleObject.inputBuffer.brake = brake
        end

        -- Try physics body manipulation
        if vehicleObject.GetPhysicsBody then
            local physicsBody = vehicleObject:GetPhysicsBody()
            if physicsBody then
                -- Apply forces based on inputs
                local forwardForce = (throttle - brake) * 5000 -- Adjust force as needed
                physicsBody:ApplyForce({x = 0, y = forwardForce, z = 0})

                -- Apply steering torque
                local steeringTorque = steering * 1000
                physicsBody:ApplyTorque({x = 0, y = 0, z = steeringTorque})
            end
        end
    end)
end

-- Apply physics-level input (called from vehicle controller hook)
function VehicleInputOverride:ApplyPhysicsInput(vehicleController, deltaTime)
    if not self.active or not InputHandler:IsWheelConnected() then
        return
    end

    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    -- Try to override controller inputs
    pcall(function()
        if vehicleController.SetInputs then
            vehicleController:SetInputs(steering, throttle, brake)
        end

        -- Try to access vehicle physics directly through controller
        if vehicleController.vehicle and vehicleController.vehicle.physicsComponent then
            local physics = vehicleController.vehicle.physicsComponent

            -- Apply wheel-based control to physics
            physics.steeringInput = steering
            physics.throttleInput = throttle
            physics.brakeInput = brake
        end
    end)
end
    end)

    -- Debug output
    if Config:Get("debugMode") and (math.abs(steering) > 0.01 or throttle > 0.01 or brake > 0.01) then
        print(string.format("[G923Mod] Override: Steering=%.2f, Throttle=%.2f, Brake=%.2f",
              steering, throttle, brake))
    end
end

-- Restore original vehicle settings
function VehicleInputOverride:RestoreOriginalSettings()
    -- Restore any temporarily overridden settings
    self.inputOverrides = {}

    print("[G923Mod] Original vehicle settings restored")
end

-- Override specific vehicle input functions (advanced)
function VehicleInputOverride:OverrideVehicleInputFunction(functionName, vehicle)
    -- TODO: This would override specific vehicle input functions
    -- This is advanced functionality that would require deep CET API knowledge

    -- Example approach:
    -- Override(vehicle.class, functionName, function(this, ...)
    --     if self.active then
    --         return self:HandleOverriddenInput(functionName, ...)
    --     end
    --     -- Call original function
    -- end)
end

-- Handle overridden input function calls
function VehicleInputOverride:HandleOverriddenInput(functionName, ...)
    local args = {...}

    -- Route different input functions to wheel inputs
    if functionName == "GetSteeringInput" then
        return InputHandler:GetSteering()
    elseif functionName == "GetThrottleInput" then
        return InputHandler:GetThrottle()
    elseif functionName == "GetBrakeInput" then
        return InputHandler:GetBrake()
    end

    -- Default: return original behavior
    return nil
end

-- Get current override status
function VehicleInputOverride:IsActive()
    return self.active
end

-- Get current vehicle info
function VehicleInputOverride:GetCurrentVehicle()
    return self.currentVehicle
end

-- Manual activate/deactivate for testing
function VehicleInputOverride:SetActive(active)
    self.active = active
    print("[G923Mod] Input override manually " .. (active and "activated" or "deactivated"))
end

-- Shutdown the override system
function VehicleInputOverride:Shutdown()
    if not self.initialized then
        return
    end

    print("[G923Mod] Shutting down vehicle input override system...")

    -- Deactivate if currently active
    if self.active then
        self:OnVehicleExited()
    end

    -- Remove any hooks or overrides
    self:RestoreOriginalSettings()

    -- TODO: Unhook any CET function overrides

    self.initialized = false
    print("[G923Mod] Vehicle input override system shutdown complete")
end

return VehicleInputOverride
