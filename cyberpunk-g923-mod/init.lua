-- Cyberpunk 2077 G923 Steering Wheel Mod
-- Main initialization script for Cyber Engine Tweaks

local G923Mod = {
    version = "1.0.0",
    name = "G923 Steering Wheel Support",
    initialized = false
}

-- Import modules
local InputHandler = require("modules/input_handler")
local VehicleControl = require("modules/vehicle_control")
local ForceFeedback = require("modules/force_feedback")
local Config = require("modules/config")

-- Initialize the mod
function G923Mod:Initialize()
    if self.initialized then
        return
    end
    
    print("[G923Mod] Initializing G923 Steering Wheel Mod v" .. self.version)
    
    -- Load configuration
    Config:Load()
    
    -- Initialize input handler
    InputHandler:Initialize()
    
    -- Initialize vehicle control system
    VehicleControl:Initialize()
    
    -- Initialize force feedback
    ForceFeedback:Initialize()
    
    self.initialized = true
    print("[G923Mod] Initialization complete")
end

-- Update function called every frame
function G923Mod:Update(deltaTime)
    if not self.initialized then
        return
    end
    
    -- Update input handling
    InputHandler:Update(deltaTime)
    
    -- Update vehicle controls
    VehicleControl:Update(deltaTime)
    
    -- Update force feedback
    ForceFeedback:Update(deltaTime)
end

-- Shutdown function
function G923Mod:Shutdown()
    if not self.initialized then
        return
    end
    
    print("[G923Mod] Shutting down G923 Steering Wheel Mod")
    
    ForceFeedback:Shutdown()
    VehicleControl:Shutdown()
    InputHandler:Shutdown()
    
    self.initialized = false
end

-- Register CET events
registerForEvent("onInit", function()
    G923Mod:Initialize()
end)

registerForEvent("onUpdate", function(deltaTime)
    G923Mod:Update(deltaTime)
end)

registerForEvent("onShutdown", function()
    G923Mod:Shutdown()
end)

-- Export for console access
return G923Mod
