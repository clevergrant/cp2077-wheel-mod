-- Cyberpunk 2077 G923 Steering Wheel Mod
-- Main initialization script for Cyber Engine Tweaks

local G923Mod = {
    version = "2.0.0", -- Phase 3 with DirectInput and deep vehicle integration
    name = "G923 Steering Wheel Support",
    initialized = false
}

-- Import modules
local InputHandler = require("modules/input_handler")
local VehicleControl = require("modules/vehicle_control")
local ForceFeedback = require("modules/force_feedback")
local VehicleInputOverride = require("modules/vehicle_input_override")
local Config = require("modules/config")

-- Initialize the mod
function G923Mod:Initialize()
    if self.initialized then
        return
    end

    print("[G923Mod] Initializing G923 Steering Wheel Mod v" .. self.version)

    -- Load configuration
    Config:Load()

    -- Initialize input handler (includes DirectInput)
    InputHandler:Initialize()

    -- Initialize vehicle control system
    VehicleControl:Initialize()

    -- Initialize advanced vehicle input override
    VehicleInputOverride:Initialize()

    -- Initialize force feedback
    ForceFeedback:Initialize()

    self.initialized = true
    print("[G923Mod] Phase 3 initialization complete - DirectInput and vehicle override active")
end

-- Update function called every frame
function G923Mod:Update(deltaTime)
    if not self.initialized then
        return
    end

    -- Update input handling
    InputHandler:Update(deltaTime)

    -- Update force feedback (vehicle control handles its own updates now)
    ForceFeedback:Update(deltaTime)
end

-- Shutdown function
function G923Mod:Shutdown()
    if not self.initialized then
        return
    end

    print("[G923Mod] Shutting down G923 Steering Wheel Mod")

    VehicleInputOverride:Shutdown()
    ForceFeedback:Shutdown()
    VehicleControl:Shutdown()
    InputHandler:Shutdown()

    self.initialized = false
end

-- Register CET events
registerForEvent("onInit", function()
    G923Mod:Initialize()

    -- Register console commands for debugging
    G923Mod:RegisterConsoleCommands()
end)

registerForEvent("onUpdate", function(deltaTime)
    G923Mod:Update(deltaTime)
end)

registerForEvent("onShutdown", function()
    G923Mod:Shutdown()
end)

-- Register console commands for debugging and configuration
function G923Mod:RegisterConsoleCommands()
    -- Command to show current configuration
    registerForEvent("onConsoleOpen", function()
        _G.g923_config = function()
            Config:Print()
        end

        _G.g923_debug = function(enabled)
            if enabled == nil then enabled = true end
            Config:Set("debugMode", enabled)
            Config:Set("showInputValues", enabled)
            print("[G923Mod] Debug mode " .. (enabled and "enabled" or "disabled"))
        end

        _G.g923_vehicle_info = function(enabled)
            if enabled == nil then enabled = true end
            Config:Set("showVehicleInfo", enabled)
            print("[G923Mod] Vehicle info display " .. (enabled and "enabled" or "disabled"))
        end

        _G.g923_sensitivity = function(value)
            if value then
                Config:Set("steeringSensitivity", tonumber(value))
                print("[G923Mod] Steering sensitivity set to " .. value)
            else
                print("[G923Mod] Current steering sensitivity: " .. Config:Get("steeringSensitivity"))
            end
        end

        _G.g923_status = function()
            print("[G923Mod] === G923 Mod Status ===")
            print("  Version: " .. G923Mod.version)
            print("  Initialized: " .. tostring(G923Mod.initialized))
            print("  Wheel Connected: " .. tostring(InputHandler:IsWheelConnected()))
            print("  In Vehicle: " .. tostring(VehicleControl.inVehicle))
            print("  Input Override Active: " .. tostring(VehicleInputOverride:IsActive()))
            print("  Debug Mode: " .. tostring(Config:Get("debugMode")))
        end
        
        _G.g923_force_feedback = function(enabled)
            if enabled == nil then enabled = true end
            ForceFeedback:SetEnabled(enabled)
        end
        
        _G.g923_override = function(enabled)
            if enabled == nil then enabled = true end
            VehicleInputOverride:SetActive(enabled)
        end
        
        _G.g923_curve = function(curveType)
            if curveType then
                if curveType == "linear" or curveType == "exponential" or curveType == "s-curve" then
                    Config:Set("steeringCurve", curveType)
                    print("[G923Mod] Steering curve set to " .. curveType)
                else
                    print("[G923Mod] Invalid curve type. Use: linear, exponential, s-curve")
                end
            else
                print("[G923Mod] Current steering curve: " .. Config:Get("steeringCurve"))
            end
        end
        
        _G.g923_test_effects = function()
            print("[G923Mod] Testing force feedback effects...")
            ForceFeedback:TriggerCollisionEffect()
        end
    end)
end

-- Export for console access
return G923Mod
