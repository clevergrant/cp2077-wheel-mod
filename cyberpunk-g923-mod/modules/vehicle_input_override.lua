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

    -- Store original hooks for cleanup
    self.originalHooks = {}

    -- Method 1: Hook into vehicle input methods directly
    self:SetupDirectInputHooks()

    -- Method 2: Hook into player input system
    self:SetupPlayerInputHooks()

    -- Method 3: Hook into vehicle blackboard system
    self:SetupBlackboardHooks()

    -- Method 4: Hook into vehicle controller updates
    self:SetupControllerHooks()

    print("[G923Mod] Vehicle input hooks configured with multiple approaches")
end

-- Set up direct vehicle input method hooks
function VehicleInputOverride:SetupDirectInputHooks()
    -- Hook VehicleComponent methods for steering, throttle, brake
    local steeringHook = Override("VehicleComponent", "GetInputValueFloat", function(this, inputName, wrappedMethod)
        if self.active and InputHandler:IsWheelConnected() then
            if inputName == CName("Steer") or inputName == CName("steering") then
                local wheelSteering = InputHandler:GetSteering()
                if math.abs(wheelSteering) > Config:Get("steeringDeadzone") then
                    local modified = self:ApplyVehicleSteeringModification(wheelSteering, self:GetVehicleType(self.currentVehicle))
                    if Config:Get("debugMode") then
                        print(string.format("[G923Mod] Steering override: %.3f -> %.3f", wheelSteering, modified))
                    end
                    return modified
                end
            elseif inputName == CName("Accelerate") or inputName == CName("throttle") then
                local wheelThrottle = InputHandler:GetThrottle()
                if wheelThrottle > Config:Get("throttleDeadzone") then
                    return wheelThrottle
                end
            elseif inputName == CName("Brake") or inputName == CName("brake") then
                local wheelBrake = InputHandler:GetBrake()
                if wheelBrake > Config:Get("brakeDeadzone") then
                    return wheelBrake
                end
            end
        end
        return wrappedMethod(inputName)
    end)

    self.originalHooks.vehicleInputFloat = steeringHook

    -- Hook vehicle input vector methods for more complex input
    local vectorHook = Override("VehicleComponent", "GetInputValueVector", function(this, inputName, wrappedMethod)
        if self.active and InputHandler:IsWheelConnected() then
            if inputName == CName("VehicleMovement") then
                local steering = InputHandler:GetSteering()
                local throttle = InputHandler:GetThrottle()
                local brake = InputHandler:GetBrake()

                -- Create movement vector (X = steering, Y = throttle/brake)
                local moveY = throttle - brake  -- Forward/backward
                return Vector2.new(steering, moveY)
            end
        end
        return wrappedMethod(inputName)
    end)

    self.originalHooks.vehicleInputVector = vectorHook
end

-- Set up player input system hooks
function VehicleInputOverride:SetupPlayerInputHooks()
    -- Hook the main player input processing
    local playerInputHook = Override("PlayerPuppet", "GetInputValueFloat", function(this, inputName, wrappedMethod)
        if self.active and InputHandler:IsWheelConnected() then
            -- Check if we're in a vehicle
            local vehicle = Game.GetMountedVehicle(this)
            if vehicle then
                if inputName == CName("VehicleSteer") then
                    local wheelSteering = InputHandler:GetSteering()
                    if math.abs(wheelSteering) > Config:Get("steeringDeadzone") then
                        return self:ApplyVehicleSteeringModification(wheelSteering, self:GetVehicleType(vehicle))
                    end
                elseif inputName == CName("VehicleAccelerate") then
                    local wheelThrottle = InputHandler:GetThrottle()
                    if wheelThrottle > Config:Get("throttleDeadzone") then
                        return wheelThrottle
                    end
                elseif inputName == CName("VehicleBrake") then
                    local wheelBrake = InputHandler:GetBrake()
                    if wheelBrake > Config:Get("brakeDeadzone") then
                        return wheelBrake
                    end
                end
            end
        end
        return wrappedMethod(inputName)
    end)

    self.originalHooks.playerInput = playerInputHook
end

-- Set up blackboard input hooks
function VehicleInputOverride:SetupBlackboardHooks()
    -- Hook blackboard value setting to intercept input writes
    local blackboardHook = ObserveAfter("VehicleObject", "OnUpdate", function(vehicle, deltaTime)
        if self.active and InputHandler:IsWheelConnected() and vehicle == self.currentVehicle then
            self:UpdateVehicleBlackboard(vehicle)
        end
    end)

    self.originalHooks.blackboard = blackboardHook
end

-- Set up vehicle controller hooks
function VehicleInputOverride:SetupControllerHooks()
    -- Hook into different vehicle controller types
    local carControllerHook = ObserveAfter("CarController", "Update", function(this, deltaTime)
        if self.active and InputHandler:IsWheelConnected() then
            self:ApplyCarControllerInput(this, deltaTime)
        end
    end)

    local bikeControllerHook = ObserveAfter("BikeController", "Update", function(this, deltaTime)
        if self.active and InputHandler:IsWheelConnected() then
            self:ApplyBikeControllerInput(this, deltaTime)
        end
    end)

    self.originalHooks.carController = carControllerHook
    self.originalHooks.bikeController = bikeControllerHook
end

-- Register for vehicle-related events
function VehicleInputOverride:RegisterVehicleEvents()
    -- Enhanced vehicle enter/exit detection with multiple methods

    -- Method 1: Standard CET events
    registerForEvent("onEnterVehicle", function(vehicle)
        self:OnVehicleEntered(vehicle)
    end)

    registerForEvent("onExitVehicle", function()
        self:OnVehicleExited()
    end)

    -- Method 2: Player state monitoring
    registerForEvent("onUpdate", function(deltaTime)
        if self.initialized then
            self:MonitorVehicleState(deltaTime)
        end
    end)

    -- Method 3: Direct vehicle state observation
    ObserveAfter("PlayerPuppet", "OnMountingEvent", function(this, evt)
        if evt:IsMount() then
            local vehicle = Game.GetMountedVehicle(this)
            if vehicle then
                self:OnVehicleEntered(vehicle)
            end
        else
            self:OnVehicleExited()
        end
    end)
end

-- Monitor vehicle state for changes
function VehicleInputOverride:MonitorVehicleState(deltaTime)
    local player = Game.GetPlayer()
    if not player then return end

    local currentVehicle = Game.GetMountedVehicle(player)

    -- Check if vehicle state changed
    if currentVehicle and not self.active then
        -- Player entered vehicle
        self:OnVehicleEntered(currentVehicle)
    elseif not currentVehicle and self.active then
        -- Player exited vehicle
        self:OnVehicleExited()
    elseif currentVehicle and self.active and currentVehicle ~= self.currentVehicle then
        -- Player switched vehicles
        self:OnVehicleExited()
        self:OnVehicleEntered(currentVehicle)
    end

    -- Process input if active
    if self.active and currentVehicle then
        self:ProcessVehicleInput(deltaTime)
    end
end

-- Update vehicle blackboard with wheel inputs
function VehicleInputOverride:UpdateVehicleBlackboard(vehicle)
    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    -- Apply deadzone filtering
    if math.abs(steering) < Config:Get("steeringDeadzone") then steering = 0 end
    if throttle < Config:Get("throttleDeadzone") then throttle = 0 end
    if brake < Config:Get("brakeDeadzone") then brake = 0 end

    -- Skip if no wheel input
    if steering == 0 and throttle == 0 and brake == 0 then
        return
    end

    pcall(function()
        local blackboard = vehicle:GetBlackboard()
        if blackboard then
            -- Try multiple blackboard key variations
            local steeringKeys = {
                "steering_input", "vehicle_steering", "input_steering",
                "steer", "turn", "horizontal_movement"
            }
            local throttleKeys = {
                "throttle_input", "vehicle_throttle", "input_throttle",
                "accelerate", "forward", "gas"
            }
            local brakeKeys = {
                "brake_input", "vehicle_brake", "input_brake",
                "brake", "stop", "reverse"
            }

            -- Apply steering to all possible keys
            for _, key in ipairs(steeringKeys) do
                blackboard:SetFloat(GetAllBlackboardDefs().Vehicle[key] or CName(key), steering)
            end

            -- Apply throttle
            for _, key in ipairs(throttleKeys) do
                blackboard:SetFloat(GetAllBlackboardDefs().Vehicle[key] or CName(key), throttle)
            end

            -- Apply brake
            for _, key in ipairs(brakeKeys) do
                blackboard:SetFloat(GetAllBlackboardDefs().Vehicle[key] or CName(key), brake)
            end

            if Config:Get("debugMode") then
                print(string.format("[G923Mod] Blackboard updated: S=%.2f T=%.2f B=%.2f", steering, throttle, brake))
            end
        end
    end)
end

-- Apply input to car controller specifically
function VehicleInputOverride:ApplyCarControllerInput(controller, deltaTime)
    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    pcall(function()
        -- Try direct controller input methods
        if controller.SetSteeringInput then
            controller:SetSteeringInput(steering)
        end
        if controller.SetThrottleInput then
            controller:SetThrottleInput(throttle)
        end
        if controller.SetBrakeInput then
            controller:SetBrakeInput(brake)
        end

        -- Try accessing internal input state
        if controller.m_inputContext then
            controller.m_inputContext.steering = steering
            controller.m_inputContext.throttle = throttle
            controller.m_inputContext.brake = brake
        end

        -- Try physics-level application
        if controller.GetVehicle then
            local vehicle = controller:GetVehicle()
            if vehicle and vehicle.GetPhysicsComponent then
                local physics = vehicle:GetPhysicsComponent()
                if physics then
                    physics:SetSteeringInput(steering)
                    physics:SetThrottleInput(throttle)
                    physics:SetBrakeInput(brake)
                end
            end
        end
    end)
end

-- Apply input to bike controller specifically
function VehicleInputOverride:ApplyBikeControllerInput(controller, deltaTime)
    -- Motorcycles need different handling - higher sensitivity, quicker response
    local steering = InputHandler:GetSteering() * 1.2  -- Increased sensitivity for bikes
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    pcall(function()
        -- Similar to car controller but with bike-specific parameters
        if controller.SetSteeringInput then
            controller:SetSteeringInput(steering)
        end
        if controller.SetThrottleInput then
            controller:SetThrottleInput(throttle)
        end
        if controller.SetBrakeInput then
            controller:SetBrakeInput(brake)
        end

        -- Bikes might need lean/balance input
        if controller.SetLeanInput then
            controller:SetLeanInput(steering * 0.5)  -- Subtle lean based on steering
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

    local vehicleType = "car" -- Default

    pcall(function()
        -- Method 1: Check vehicle record type
        if vehicle.GetRecord then
            local record = vehicle:GetRecord()
            if record and record.Type then
                local typeName = tostring(record.Type():GetClassName()):lower()
                if typeName:find("bike") or typeName:find("motorcycle") then
                    vehicleType = "motorcycle"
                elseif typeName:find("truck") or typeName:find("heavy") then
                    vehicleType = "truck"
                elseif typeName:find("van") then
                    vehicleType = "van"
                end
            end
        end

        -- Method 2: Check entity template path
        if vehicleType == "car" and vehicle.GetEntityTemplate then
            local template = vehicle:GetEntityTemplate()
            if template then
                local templatePath = tostring(template:GetPath()):lower()
                if templatePath:find("bike") or templatePath:find("motorcycle") then
                    vehicleType = "motorcycle"
                elseif templatePath:find("truck") or templatePath:find("heavy") then
                    vehicleType = "truck"
                elseif templatePath:find("van") then
                    vehicleType = "van"
                end
            end
        end

        -- Method 3: Check display name as fallback
        if vehicleType == "car" and vehicle.GetDisplayName then
            local displayName = tostring(vehicle:GetDisplayName()):lower()
            if displayName:find("bike") or displayName:find("motorcycle") then
                vehicleType = "motorcycle"
            elseif displayName:find("truck") then
                vehicleType = "truck"
            elseif displayName:find("van") then
                vehicleType = "van"
            end
        end

        -- Method 4: Fallback to wheel count or physics properties
        if vehicleType == "car" and vehicle.GetPhysicsComponent then
            local physics = vehicle:GetPhysicsComponent()
            if physics then
                -- Check wheel count or mass for classification
                if physics.GetWheelCount and physics:GetWheelCount() == 2 then
                    vehicleType = "motorcycle"
                elseif physics.GetMass then
                    local mass = physics:GetMass()
                    if mass > 3000 then
                        vehicleType = "truck"
                    elseif mass < 500 then
                        vehicleType = "motorcycle"
                    elseif mass > 2000 then
                        vehicleType = "van"
                    end
                end
            end
        end
    end)

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Vehicle type detected: %s", vehicleType))
    end

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

-- Test vehicle input override functionality
function VehicleInputOverride:TestVehicleInputMethods()
    if not self.currentVehicle then
        print("[G923Mod] No vehicle available for testing")
        return false
    end

    print("[G923Mod] Testing vehicle input override methods...")

    local testResults = {
        blackboard = false,
        component = false,
        physics = false,
        controller = false
    }

    -- Test values
    local testSteering = 0.5
    local testThrottle = 0.3
    local testBrake = 0.2

    -- Test blackboard method
    testResults.blackboard = self:ApplyInputsViaBlackboard(testSteering, testThrottle, testBrake)

    -- Test component method
    testResults.component = self:ApplyInputsViaComponent(testSteering, testThrottle, testBrake)

    -- Test physics method
    testResults.physics = self:ApplyInputsViaPhysics(testSteering, testThrottle, testBrake)

    -- Test controller method
    testResults.controller = self:ApplyInputsViaController(testSteering, testThrottle, testBrake)

    -- Report results
    print("[G923Mod] Vehicle Input Test Results:")
    for method, success in pairs(testResults) do
        local status = success and "✅ PASS" or "❌ FAIL"
        print(string.format("  %s: %s", method, status))
    end

    local totalPassed = 0
    for _, success in pairs(testResults) do
        if success then totalPassed = totalPassed + 1 end
    end

    print(string.format("[G923Mod] Test Summary: %d/4 methods working", totalPassed))
    return totalPassed > 0
end

-- Validate current vehicle input override status
function VehicleInputOverride:ValidateInputOverride()
    if not self.active then
        return { status = "inactive", message = "Input override not active" }
    end

    if not self.currentVehicle then
        return { status = "error", message = "No current vehicle found" }
    end

    if not InputHandler:IsWheelConnected() then
        return { status = "error", message = "Wheel not connected" }
    end

    -- Test if wheel inputs are being detected
    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    local hasInput = math.abs(steering) > 0.01 or throttle > 0.01 or brake > 0.01

    if not hasInput then
        return {
            status = "warning",
            message = "No wheel input detected - try moving wheel or pressing pedals"
        }
    end

    -- Test if input methods are working
    local methodsWorking = self:TestVehicleInputMethods()

    if not methodsWorking then
        return {
            status = "error",
            message = "No vehicle input methods are working - CET API compatibility issue"
        }
    end

    return {
        status = "success",
        message = "Vehicle input override is working correctly",
        vehicleType = self:GetVehicleType(self.currentVehicle),
        inputs = { steering = steering, throttle = throttle, brake = brake }
    }
end

-- Get detailed status report for debugging
function VehicleInputOverride:GetDetailedStatus()
    local status = {
        initialized = self.initialized,
        active = self.active,
        wheelConnected = InputHandler:IsWheelConnected(),
        currentVehicle = self.currentVehicle ~= nil,
        vehicleType = self.currentVehicle and self:GetVehicleType(self.currentVehicle) or "none",
        hooksActive = #self.originalHooks > 0,
        inputOverrides = self.inputOverrides
    }

    if self.currentVehicle then
        status.vehicleDetails = {
            hasBlackboard = self.currentVehicle.GetBlackboard ~= nil,
            hasVehicleComponent = self.currentVehicle.GetVehicleComponent ~= nil,
            hasPhysicsComponent = self.currentVehicle.GetPhysicsComponent ~= nil,
            hasRecord = self.currentVehicle.GetRecord ~= nil
        }
    end

    return status
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

    -- Clean up CET hooks/overrides
    self:CleanupHooks()

    self.initialized = false
    print("[G923Mod] Vehicle input override system shutdown complete")
end

-- Clean up all registered hooks and overrides
function VehicleInputOverride:CleanupHooks()
    -- Note: CET doesn't provide a direct way to remove Override/ObserveAfter hooks
    -- The hooks will be cleaned up when the mod is reloaded
    -- In a production version, we might need to track and manage hook lifecycle differently

    if self.originalHooks then
        -- Clear hook references
        for hookName, hookRef in pairs(self.originalHooks) do
            -- Individual hook cleanup would go here if CET supported it
            print(string.format("[G923Mod] Cleaned up hook: %s", hookName))
        end
        self.originalHooks = {}
    end

    print("[G923Mod] All hooks cleaned up (will be fully removed on mod reload)")
end

return VehicleInputOverride
