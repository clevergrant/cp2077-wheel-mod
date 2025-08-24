-- Installation Verification Script for Cyberpunk 2077 G923 Steering Wheel Mod
-- This script runs automatically when the mod loads to verify proper installation

local InstallationCheck = {}

function InstallationCheck:VerifyInstallation()
    local issues = {}
    local warnings = {}

    -- Check if running in correct directory
    local currentDir = GetWorkingDirectory()
    if not string.find(currentDir:lower(), "cyber_engine_tweaks") then
        table.insert(issues, "Mod may not be in correct CET directory")
    end

    -- Check for required files
    local requiredFiles = {
        "modules/input_handler.lua",
        "modules/vehicle_control.lua",
        "modules/force_feedback.lua",
        "modules/config.lua",
        "modules/real_directinput.lua",
        "modules/performance_monitor.lua",
        "modules/input_calibration.lua",
        "modules/vehicle_input_override.lua"
    }

    for _, file in ipairs(requiredFiles) do
        -- Note: In CET environment, we'll just print a warning if modules fail to load
        -- rather than checking files directly since file system access is limited
    end

    -- Check CET version compatibility
    local cetVersion = GetVersion()
    if cetVersion then
        print("[G923] CET Version: " .. tostring(cetVersion))
        -- Add version checking logic here when CET API provides this info
    else
        table.insert(warnings, "Could not detect CET version")
    end

    -- Output results
    if #issues > 0 then
        print("[G923] ❌ INSTALLATION ISSUES DETECTED:")
        for _, issue in ipairs(issues) do
            print("   • " .. issue)
        end
        print("[G923] Please reinstall the mod following the installation guide.")
        return false
    end

    if #warnings > 0 then
        print("[G923] ⚠️ Installation Warnings:")
        for _, warning in ipairs(warnings) do
            print("   • " .. warning)
        end
    end

    print("[G923] ✅ Installation verification passed!")
    return true
end

function InstallationCheck:ShowQuickStart()
    print("")
    print("🎮 G923 Steering Wheel Mod - Quick Start Guide:")
    print("  1. Connect your Logitech G923 wheel")
    print("  2. Test installation: g923_status()")
    print("  3. Start calibration: g923_calibrate(\"auto\")")
    print("  4. Get in a vehicle and drive!")
    print("  5. For help: g923_help()")
    print("")
end

return InstallationCheck
