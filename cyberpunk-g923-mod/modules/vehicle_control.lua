-- Vehicle Control module for G923 Steering Wheel Mod
-- Handles overriding game vehicle controls with wheel inputs

local Config = require("modules/config")
local InputHandler = require("modules/input_handler")

local VehicleControl = {
    initialized = false,
    inVehicle = false,
    currentVehicle = nil,
    originalInputs = {}
}

-- Initialize vehicle control system
function VehicleControl:Initialize()
    print("[G923Mod] Initializing vehicle control system...")

    -- Register for vehicle enter/exit events using CET
    registerForEvent("onEnterVehicle", function(vehicle)
        self:OnVehicleEntered(vehicle)
    end)

    registerForEvent("onExitVehicle", function()
        self:OnVehicleExited()
    end)

    -- Hook into vehicle update events
    registerForEvent("onUpdate", function(deltaTime)
        self:VehicleUpdateHook(deltaTime)
    end)

    self.initialized = true
    print("[G923Mod] Vehicle control system initialized")
end

-- Update vehicle controls
function VehicleControl:Update(deltaTime)
    if not self.initialized or not InputHandler:IsWheelConnected() then
        return
    end

    -- Check if player is in a vehicle
    self:CheckVehicleState()

    if self.inVehicle and Config:Get("analogSteeringEnabled") then
        self:ApplyWheelControls()
    end
end

-- Check current vehicle state
function VehicleControl:CheckVehicleState()
    -- Use CET APIs to check vehicle state
    local player = Game.GetPlayer()
    if player then
        local vehicle = player:GetMountedVehicle()
        local wasInVehicle = self.inVehicle

        self.inVehicle = vehicle ~= nil
        self.currentVehicle = vehicle

        -- Handle state changes
        if self.inVehicle and not wasInVehicle then
            self:OnVehicleEntered(vehicle)
        elseif not self.inVehicle and wasInVehicle then
            self:OnVehicleExited()
        end
    end
end

-- Vehicle update hook for continuous monitoring
function VehicleControl:VehicleUpdateHook(deltaTime)
    if not self.initialized then
        return
    end

    -- Continuously check vehicle state
    self:CheckVehicleState()

    -- Apply wheel controls if in vehicle
    if self.inVehicle and Config:Get("analogSteeringEnabled") and InputHandler:IsWheelConnected() then
        self:ApplyWheelControls()
    end
end

-- Apply wheel controls to vehicle
function VehicleControl:ApplyWheelControls()
    if not self.inVehicle or not self.currentVehicle then
        return
    end

    -- Get wheel inputs
    local steering = InputHandler:GetSteering()
    local throttle = InputHandler:GetThrottle()
    local brake = InputHandler:GetBrake()

    -- Apply inputs to vehicle using CET vehicle APIs
    pcall(function()
        -- Try to apply steering input
        if math.abs(steering) > 0.01 then
            self.currentVehicle:QueueCommand("VehicleDriveToPointStraightCommand", {
                steeringInput = steering
            })
        end

        -- Apply throttle/brake inputs
        if throttle > 0.01 then
            self.currentVehicle:QueueCommand("VehicleAccelerateCommand", {
                throttleInput = throttle
            })
        end

        if brake > 0.01 then
            self.currentVehicle:QueueCommand("VehicleBrakeCommand", {
                brakeInput = brake
            })
        end
    end)

    -- Debug output if enabled
    if Config:Get("debugMode") then
        self:DebugLogVehicleInputs(steering, throttle, brake)
    end
end

-- Override game input system
function VehicleControl:OverrideGameInputs()
    -- Hook into the game's input system to replace default vehicle controls
    print("[G923Mod] Overriding game input system for vehicle controls")

    -- Store original input handlers for restoration
    self.originalInputs.steering = true -- Placeholder for actual input hooks

    -- In a real implementation, this would involve:
    -- 1. Hooking into the vehicle component's input processing
    -- 2. Intercepting calls to vehicle steering/throttle/brake functions
    -- 3. Replacing input values with wheel data before they reach vehicle system

    -- For CET, we might use:
    -- ObserveAfter("VehicleComponent", "GetInputToProcess", function(this)
    --     if self.inVehicle then
    --         this:SetSteeringInput(InputHandler:GetSteering())
    --         this:SetThrottleInput(InputHandler:GetThrottle())
    --         this:SetBrakeInput(InputHandler:GetBrake())
    --     end
    -- end)
end

-- Restore original game inputs
function VehicleControl:RestoreGameInputs()
    -- Restore the original input handling system
    print("[G923Mod] Restoring original game input system")

    -- Remove any input hooks or overrides
    self.originalInputs = {}

    -- In a real implementation, this would:
    -- 1. Unhook any input processing functions
    -- 2. Restore original vehicle input handlers
    -- 3. Ensure smooth transition back to keyboard/gamepad controls
end

-- Handle vehicle entered event
function VehicleControl:OnVehicleEntered(vehicle)
    print("[G923Mod] Player entered vehicle")
    self.inVehicle = true
    self.currentVehicle = vehicle

    if InputHandler:IsWheelConnected() then
        print("[G923Mod] Switching to wheel controls")
        self:OverrideGameInputs()
    end
end

-- Handle vehicle exited event
function VehicleControl:OnVehicleExited()
    print("[G923Mod] Player exited vehicle")
    self.inVehicle = false
    self.currentVehicle = nil

    self:RestoreGameInputs()
end

-- Get current vehicle info
function VehicleControl:GetVehicleInfo()
    if not self.inVehicle or not self.currentVehicle then
        return nil
    end

    -- Extract vehicle information using CET APIs
    local vehicleInfo = {
        type = "unknown",
        speed = 0,
        rpm = 0,
        gear = 1,
        name = "Unknown Vehicle"
    }

    pcall(function()
        -- Get vehicle type and properties
        local vehicleRecord = self.currentVehicle:GetRecord()
        if vehicleRecord then
            vehicleInfo.name = tostring(vehicleRecord.displayName)
            vehicleInfo.type = tostring(vehicleRecord.type)
        end

        -- Get vehicle physics data
        local blackboard = self.currentVehicle:GetBlackboard()
        if blackboard then
            vehicleInfo.speed = blackboard:GetFloat("speed") or 0
            vehicleInfo.rpm = blackboard:GetFloat("rpm") or 0
            vehicleInfo.gear = blackboard:GetInt("gear") or 1
        end
    end)

    return vehicleInfo
end

-- Debug log vehicle inputs
function VehicleControl:DebugLogVehicleInputs(steering, throttle, brake)
    if math.abs(steering) > 0.01 or throttle > 0.01 or brake > 0.01 then
        print(string.format("[G923Mod] Vehicle inputs: Steering=%.2f, Throttle=%.2f, Brake=%.2f",
              steering, throttle, brake))
    end
end

-- Shutdown vehicle control system
function VehicleControl:Shutdown()
    print("[G923Mod] Shutting down vehicle control system...")

    if self.inVehicle then
        self:RestoreGameInputs()
    end

    self.initialized = false
    self.inVehicle = false
    self.currentVehicle = nil

    print("[G923Mod] Vehicle control system shutdown complete")
end

return VehicleControl
