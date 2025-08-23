-- Cyberpunk 2077 G923 Steering Wheel Mod
-- Main initialization script for Cyber Engine Tweaks

local G923Mod = {
    version = "3.0.0", -- Phase 4 - Final Implementation & Production Release
    name = "G923 Steering Wheel Support",
    initialized = false
}

-- Import modules
local InputHandler = require("modules/input_handler")
local VehicleControl = require("modules/vehicle_control")
local ForceFeedback = require("modules/force_feedback")
local VehicleInputOverride = require("modules/vehicle_input_override")
local Config = require("modules/config")

-- Phase 4 modules
local RealDirectInput = require("modules/real_directinput")
local PerformanceMonitor = require("modules/performance_monitor")
local InputCalibration = require("modules/input_calibration")

-- Initialize the mod
function G923Mod:Initialize()
    if self.initialized then
        return
    end

    print("[G923Mod] Initializing G923 Steering Wheel Mod v" .. self.version)

    -- Load configuration
    Config:Load()

    -- Initialize performance monitoring first
    PerformanceMonitor:Initialize()

    -- Initialize input calibration system
    InputCalibration:Initialize()

    -- Try real DirectInput first, fallback to simulation
    local realDirectInputSuccess = RealDirectInput:Initialize()
    if realDirectInputSuccess then
        print("[G923Mod] Using REAL DirectInput hardware communication")
        InputHandler.directInput = RealDirectInput
    else
        print("[G923Mod] Falling back to DirectInput simulation framework")
        -- Keep existing DirectInput simulation from input_handler
    end

    -- Initialize input handler (includes DirectInput)
    InputHandler:Initialize()

    -- Initialize vehicle control system
    VehicleControl:Initialize()

    -- Initialize advanced vehicle input override
    VehicleInputOverride:Initialize()

    -- Initialize force feedback
    ForceFeedback:Initialize()

    self.initialized = true
    print("[G923Mod] Phase 4 initialization complete - Production-ready implementation active")
end

-- Update function called every frame
function G923Mod:Update(deltaTime)
    if not self.initialized then
        return
    end

    -- Update performance monitoring
    PerformanceMonitor:Update(deltaTime)

    -- Get current inputs for calibration
    local rawInputs = nil
    if InputHandler.directInput and InputHandler.directInput.GetNormalizedInputs then
        rawInputs = InputHandler.directInput:GetNormalizedInputs()
    end

    -- Update input calibration
    InputCalibration:Update(rawInputs)

    -- Update input handling with performance tracking
    local inputStartTime = os.clock()
    InputHandler:Update(deltaTime)
    local inputLatency = (os.clock() - inputStartTime) * 1000
    PerformanceMonitor:RecordInputLatency(inputLatency)

    -- Update force feedback (vehicle control handles its own updates now)
    ForceFeedback:Update(deltaTime)
end

-- Shutdown function
function G923Mod:Shutdown()
    if not self.initialized then
        return
    end

    print("[G923Mod] Shutting down G923 Steering Wheel Mod")

    -- Shutdown in reverse order
    InputCalibration:Shutdown()
    PerformanceMonitor:Shutdown()
    VehicleInputOverride:Shutdown()
    ForceFeedback:Shutdown()
    VehicleControl:Shutdown()
    InputHandler:Shutdown()

    -- Shutdown real DirectInput if it was used
    if RealDirectInput and RealDirectInput.initialized then
        RealDirectInput:Shutdown()
    end

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
                Config:Save() -- Auto-save when changed
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

            -- Phase 4 status additions
            if RealDirectInput and RealDirectInput.initialized then
                print("  Real DirectInput: ACTIVE")
                local perfStats = RealDirectInput:GetPerformanceStats()
                print(string.format("    Poll Rate: %.1fHz, Errors: %d, Latency: %.2fms",
                      60, perfStats.errorCount, perfStats.avgPollTimeMs))
            else
                print("  Real DirectInput: Simulation Mode")
            end

            local perfStatus = PerformanceMonitor:GetStatus()
            if perfStatus.enabled then
                print(string.format("  Performance: Frame=%.2fms (%s), Memory=%.1fMB",
                      perfStatus.frameTime.current, perfStatus.frameTime.status, perfStatus.memoryUsage.current))
                if perfStatus.optimization.emergencyMode then
                    print("  ⚠️  EMERGENCY OPTIMIZATION ACTIVE")
                end
            end

            local calStatus = InputCalibration:GetStatus()
            print(string.format("  Calibration: Auto=%.0f%% confidence, Manual=%s",
                  calStatus.autoCalibrationConfidence * 100,
                  calStatus.calibrationActive and "IN PROGRESS" or "Ready"))
        end

        _G.g923_force_feedback = function(enabled)
            if enabled == nil then enabled = true end
            ForceFeedback:SetEnabled(enabled)
            Config:Set("forceFeedbackEnabled", enabled)
            Config:Save()
        end

        _G.g923_override = function(enabled)
            if enabled == nil then enabled = true end
            VehicleInputOverride:SetActive(enabled)
        end

        _G.g923_curve = function(curveType)
            if curveType then
                if curveType == "linear" or curveType == "exponential" or curveType == "s-curve" then
                    Config:Set("steeringCurve", curveType)
                    Config:Save()
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

        -- New advanced commands
        _G.g923_save_config = function()
            Config:Save()
            print("[G923Mod] Configuration saved to file")
        end

        _G.g923_reload_config = function()
            Config:Load()
            print("[G923Mod] Configuration reloaded from file")
        end

        _G.g923_deadzone = function(steering, throttle, brake)
            if steering then
                Config:Set("steeringDeadzone", tonumber(steering))
                print("[G923Mod] Steering deadzone set to " .. steering)
            end
            if throttle then
                Config:Set("throttleDeadzone", tonumber(throttle))
                print("[G923Mod] Throttle deadzone set to " .. throttle)
            end
            if brake then
                Config:Set("brakeDeadzone", tonumber(brake))
                print("[G923Mod] Brake deadzone set to " .. brake)
            end
            if steering or throttle or brake then
                Config:Save()
            else
                print(string.format("[G923Mod] Current deadzones - Steering: %.3f, Throttle: %.3f, Brake: %.3f",
                      Config:Get("steeringDeadzone"), Config:Get("throttleDeadzone"), Config:Get("brakeDeadzone")))
            end
        end

        _G.g923_reset = function()
            Config:Reset()
            Config:Save()
            print("[G923Mod] Configuration reset to defaults")
        end

        _G.g923_simulate = function(enabled)
            if enabled == nil then enabled = true end
            if enabled then
                Config:Set("debugMode", true)
                print("[G923Mod] Simulation mode enabled - using time-based test inputs")
            else
                Config:Set("debugMode", false)
                print("[G923Mod] Simulation mode disabled")
            end
        end

        _G.g923_vehicle_sensitivity = function(car, motorcycle, truck)
            if car then
                Config:Set("carSensitivity", tonumber(car))
                print("[G923Mod] Car sensitivity set to " .. car)
            end
            if motorcycle then
                Config:Set("motorcycleSensitivity", tonumber(motorcycle))
                print("[G923Mod] Motorcycle sensitivity set to " .. motorcycle)
            end
            if truck then
                Config:Set("truckSensitivity", tonumber(truck))
                print("[G923Mod] Truck sensitivity set to " .. truck)
            end
            if car or motorcycle or truck then
                Config:Save()
            else
                print(string.format("[G923Mod] Vehicle sensitivities - Car: %.2f, Motorcycle: %.2f, Truck: %.2f",
                      Config:Get("carSensitivity"), Config:Get("motorcycleSensitivity"), Config:Get("truckSensitivity")))
            end
        end

        -- Phase 4 Advanced Commands
        _G.g923_calibrate = function(mode)
            local mode = mode or "auto"
            if mode == "auto" then
                print("[G923Mod] Starting automatic calibration...")
                if InputCalibration:StartAutoCalibration() then
                    print("  Auto-calibration started. Drive normally for optimal results.")
                else
                    print("  Auto-calibration failed to start. Check wheel connection.")
                end
            elseif mode == "manual" then
                print("[G923Mod] Starting manual calibration wizard...")
                InputCalibration:StartManualCalibration()
                print("  Follow the on-screen instructions to calibrate your wheel.")
            elseif mode == "status" then
                local status = InputCalibration:GetStatus()
                print("[G923Mod] Calibration Status:")
                print(string.format("  Auto: %.0f%% confidence", status.autoCalibrationConfidence * 100))
                print("  Manual: " .. (status.calibrationActive and "IN PROGRESS" or "Ready"))
                print("  Dead zone: " .. tostring(status.deadZone))
                print("  Sensitivity: " .. tostring(status.sensitivity))
            else
                print("[G923Mod] Usage: g923_calibrate(mode)")
                print("  Modes: 'auto', 'manual', 'status'")
            end
        end

        _G.g923_performance = function(action)
            local action = action or "status"
            if action == "enable" then
                PerformanceMonitor:Enable()
                print("[G923Mod] Performance monitoring enabled")
            elseif action == "disable" then
                PerformanceMonitor:Disable()
                print("[G923Mod] Performance monitoring disabled")
            elseif action == "reset" then
                PerformanceMonitor:ResetStats()
                print("[G923Mod] Performance statistics reset")
            elseif action == "optimization" then
                local status = PerformanceMonitor:GetStatus()
                print("[G923Mod] Optimization Status:")
                print("  Mode: " .. tostring(status.optimization.mode))
                print("  Emergency Active: " .. tostring(status.optimization.emergencyMode))
                print("  Quality Level: " .. tostring(status.optimization.qualityLevel))
                print("  Force Feedback Enabled: " .. tostring(status.optimization.forceFeedbackEnabled))
            else
                local status = PerformanceMonitor:GetStatus()
                print("[G923Mod] Performance Status:")
                print("  Enabled: " .. tostring(status.enabled))
                print(string.format("  Frame Time: %.2fms (%s)", status.frameTime.current, status.frameTime.status))
                print(string.format("  Input Latency: %.2fms (%s)", status.inputLatency.current, status.inputLatency.status))
                print(string.format("  Memory Usage: %.1fMB (%s)", status.memoryUsage.current, status.memoryUsage.status))
                print("  Commands: enable, disable, reset, optimization")
            end
        end

        _G.g923_hardware = function(action)
            local action = action or "status"
            if action == "switch" then
                if RealDirectInput and RealDirectInput.initialized then
                    RealDirectInput:Cleanup()
                    print("[G923Mod] Switched to simulation mode")
                else
                    if RealDirectInput:Initialize() then
                        print("[G923Mod] Switched to real hardware mode")
                    else
                        print("[G923Mod] Failed to initialize real hardware")
                    end
                end
            elseif action == "reset" then
                if RealDirectInput and RealDirectInput.initialized then
                    RealDirectInput:ResetDevice()
                    print("[G923Mod] Hardware device reset")
                else
                    print("[G923Mod] No hardware to reset (simulation mode)")
                end
            elseif action == "test" then
                if RealDirectInput and RealDirectInput.initialized then
                    RealDirectInput:TestForceFeedback()
                    print("[G923Mod] Force feedback test sent")
                else
                    print("[G923Mod] Hardware test not available in simulation mode")
                end
            else
                print("[G923Mod] Hardware Status:")
                if RealDirectInput and RealDirectInput.initialized then
                    local perfStats = RealDirectInput:GetPerformanceStats()
                    print("  Mode: Real DirectInput Hardware")
                    print(string.format("  Poll Rate: %.1fHz", 60))
                    print(string.format("  Error Count: %d", perfStats.errorCount))
                    print(string.format("  Avg Poll Time: %.2fms", perfStats.avgPollTimeMs))
                    print("  Commands: switch, reset, test")
                else
                    print("  Mode: Simulation (no hardware)")
                    print("  Commands: switch")
                end
            end
        end

        _G.g923_help = function()
            print("[G923Mod] === Available Commands ===")
            print("  Basic Commands:")
            print("    g923_status() - Show mod status")
            print("    g923_config() - Show current configuration")
            print("    g923_debug(true/false) - Toggle debug mode")
            print("    g923_help() - Show this help")
            print("")
            print("  Input Configuration:")
            print("    g923_sensitivity(value) - Set steering sensitivity")
            print("    g923_curve(type) - Set steering curve (linear/exponential/s-curve)")
            print("    g923_deadzone(steering, throttle, brake) - Set deadzones")
            print("    g923_vehicle_sensitivity(car, motorcycle, truck) - Set vehicle-specific sensitivity")
            print("")
            print("  Force Feedback:")
            print("    g923_force_feedback(true/false) - Toggle force feedback")
            print("    g923_test_effects() - Test force feedback effects")
            print("")
            print("  Advanced (Phase 4):")
            print("    g923_calibrate(mode) - Auto/manual calibration (auto/manual/status)")
            print("    g923_performance(action) - Performance monitoring (enable/disable/reset/optimization)")
            print("    g923_hardware(action) - Hardware control (switch/reset/test)")
            print("")
            print("  Configuration Management:")
            print("    g923_save_config() - Save configuration to file")
            print("    g923_reload_config() - Reload configuration from file")
            print("    g923_reset() - Reset to default configuration")
            print("")
            print("  Testing:")
            print("    g923_simulate(true/false) - Toggle simulation mode")
        end
    end)
end

-- Export for console access
return G923Mod
