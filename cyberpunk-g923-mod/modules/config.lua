-- Configuration module for G923 Steering Wheel Mod
-- Handles loading and saving of mod settings

local Config = {
    settings = {},
    defaultSettings = {
        -- Input settings
        steeringDeadzone = 0.05,
        steeringSensitivity = 1.0,
        throttleDeadzone = 0.02,
        brakeDeadzone = 0.02,
        
        -- Force feedback settings
        forceFeedbackEnabled = true,
        forceFeedbackStrength = 0.8,
        roadFeedbackEnabled = true,
        collisionFeedbackEnabled = true,
        
        -- Vehicle control settings
        analogSteeringEnabled = true,
        smoothingEnabled = true,
        smoothingFactor = 0.1,
        
        -- Debug settings
        debugMode = false,
        showInputValues = false
    }
}

-- Load configuration from file
function Config:Load()
    print("[G923Mod] Loading configuration...")
    
    -- Start with default settings
    self.settings = {}
    for key, value in pairs(self.defaultSettings) do
        self.settings[key] = value
    end
    
    -- TODO: Load from JSON file when CET file I/O is available
    -- For now, use default settings
    
    print("[G923Mod] Configuration loaded")
end

-- Save configuration to file
function Config:Save()
    print("[G923Mod] Saving configuration...")
    
    -- TODO: Save to JSON file when CET file I/O is available
    
    print("[G923Mod] Configuration saved")
end

-- Get a configuration value
function Config:Get(key)
    return self.settings[key]
end

-- Set a configuration value
function Config:Set(key, value)
    if self.defaultSettings[key] ~= nil then
        self.settings[key] = value
        print("[G923Mod] Config updated: " .. key .. " = " .. tostring(value))
    else
        print("[G923Mod] Warning: Unknown config key: " .. key)
    end
end

-- Reset to default settings
function Config:Reset()
    print("[G923Mod] Resetting configuration to defaults...")
    for key, value in pairs(self.defaultSettings) do
        self.settings[key] = value
    end
end

-- Print current configuration
function Config:Print()
    print("[G923Mod] Current configuration:")
    for key, value in pairs(self.settings) do
        print("  " .. key .. " = " .. tostring(value))
    end
end

return Config
