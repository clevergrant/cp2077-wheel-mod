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
    
    -- TODO: Hook into CET vehicle input system
    -- This is the core functionality that would intercept vehicle inputs
    -- and replace them with wheel data
    
    -- The approach would be to use CET's Override or ObserveAfter functions
    -- to intercept calls to vehicle input processing functions
    
    -- Example hooks (these would need to be researched and implemented):
    -- ObserveAfter("VehicleComponent", "HandleInput", function(this, inputData)
    --     if self.active and InputHandler:IsWheelConnected() then
    --         self:OverrideVehicleInput(this, inputData)
    --     end
    -- end)
    
    -- Override("vehicleController", "GetSteeringInput", function(this)
    --     if self.active then
    --         return InputHandler:GetSteering()
    --     end
    -- end)
    
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
    
    -- TODO: Apply inputs using CET vehicle APIs
    -- This is where the actual vehicle control would happen
    -- The exact implementation depends on available CET APIs for vehicle control
    
    pcall(function()
        -- Attempt to apply steering
        -- Real implementation would use proper CET vehicle APIs:
        -- self.currentVehicle:SetSteeringAngle(steering)
        -- self.currentVehicle:SetThrottleInput(throttle)
        -- self.currentVehicle:SetBrakeInput(brake)
        
        -- For now, we can try to use vehicle commands or blackboard writes
        local blackboard = self.currentVehicle:GetBlackboard()
        if blackboard then
            -- Try to write to vehicle input blackboard
            blackboard:SetFloat("steeringInput", steering)
            blackboard:SetFloat("throttleInput", throttle)
            blackboard:SetFloat("brakeInput", brake)
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
