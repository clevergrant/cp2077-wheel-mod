-- Configuration module for G923 Steering Wheel Mod
-- Handles loading and saving of mod settings

local Config = {
    settings = {},
    defaultSettings = {
        -- Input settings
        steeringDeadzone = 0.05,
        steeringSensitivity = 1.0,
        steeringCurve = "linear", -- "linear", "exponential", "s-curve"
        throttleDeadzone = 0.02,
        brakeDeadzone = 0.02,
        pedalCurve = "linear", -- "linear", "exponential"

        -- Force feedback settings
        forceFeedbackEnabled = true,
        forceFeedbackStrength = 0.8,
        roadFeedbackEnabled = true,
        collisionFeedbackEnabled = true,
        speedFeedbackEnabled = true,

        -- Vehicle control settings
        analogSteeringEnabled = true,
        smoothingEnabled = true,
        smoothingFactor = 0.1,

        -- Vehicle-specific settings
        carSensitivity = 1.0,
        motorcycleSensitivity = 1.2,
        truckSensitivity = 0.8,

        -- Debug settings
        debugMode = false,
        showInputValues = false,
        showVehicleInfo = false
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

    -- Try to load from JSON file
    local success = self:LoadFromFile()
    if not success then
        print("[G923Mod] Using default configuration")
    end

    print("[G923Mod] Configuration loaded")
end

-- Load configuration from JSON file
function Config:LoadFromFile()
    local configPath = "mods/cyberpunk-g923-mod/config.json"

    -- Try to read config file using CET file system access
    local success, content = pcall(function()
        -- CET provides file I/O through io operations
        local file = io.open(configPath, "r")
        if not file then
            return nil
        end

        local content = file:read("*all")
        file:close()
        return content
    end)

    if not success or not content then
        print("[G923Mod] No configuration file found at " .. configPath)
        return false
    end

    -- Parse JSON content
    local configData = self:ParseJSON(content)
    if not configData then
        print("[G923Mod] Failed to parse configuration file")
        return false
    end

    -- Apply loaded settings
    for key, value in pairs(configData) do
        if self.defaultSettings[key] ~= nil then
            self.settings[key] = value
        end
    end

    print("[G923Mod] Configuration loaded from " .. configPath)
    return true
end

-- Save configuration to file
function Config:Save()
    print("[G923Mod] Saving configuration...")

    local success = self:SaveToFile()
    if success then
        print("[G923Mod] Configuration saved")
    else
        print("[G923Mod] Failed to save configuration")
    end
end

-- Save configuration to JSON file
function Config:SaveToFile()
    local configPath = "mods/cyberpunk-g923-mod/config.json"

    -- Create JSON content
    local jsonContent = self:ToJSON(self.settings)
    if not jsonContent then
        print("[G923Mod] Failed to serialize configuration")
        return false
    end

    -- Write to file
    local success = pcall(function()
        -- Ensure directory exists
        local dir = "mods/cyberpunk-g923-mod"
        os.execute("mkdir \"" .. dir .. "\" 2>nul") -- Windows mkdir

        local file = io.open(configPath, "w")
        if not file then
            error("Could not open file for writing")
        end

        file:write(jsonContent)
        file:close()
    end)

    if success then
        print("[G923Mod] Configuration saved to " .. configPath)
        return true
    else
        print("[G923Mod] Failed to write configuration file")
        return false
    end
end

-- Simple JSON parser for configuration
function Config:ParseJSON(jsonString)
    -- Simple JSON parsing for our configuration needs
    -- This is a basic implementation for CET compatibility

    local success, result = pcall(function()
        -- Remove whitespace and comments
        local cleaned = jsonString:gsub("//.-\n", ""):gsub("%s+", " ")

        -- Simple key-value parsing for our flat configuration
        local config = {}

        -- Parse simple key: value pairs
        for key, value in cleaned:gmatch('"([^"]+)"%s*:%s*([^,}]+)') do
            -- Parse different value types
            if value == "true" then
                config[key] = true
            elseif value == "false" then
                config[key] = false
            elseif value:match('^".*"$') then
                config[key] = value:sub(2, -2) -- Remove quotes
            elseif value:match('^%-?%d+%.%d+$') then
                config[key] = tonumber(value)
            elseif value:match('^%-?%d+$') then
                config[key] = tonumber(value)
            else
                config[key] = value
            end
        end

        return config
    end)

    if success then
        return result
    else
        return nil
    end
end

-- Simple JSON serializer for configuration
function Config:ToJSON(data)
    local success, result = pcall(function()
        local lines = {"{\n"}

        for key, value in pairs(data) do
            local valueStr
            if type(value) == "string" then
                valueStr = '"' .. value .. '"'
            elseif type(value) == "boolean" then
                valueStr = tostring(value)
            elseif type(value) == "number" then
                valueStr = tostring(value)
            else
                valueStr = '"' .. tostring(value) .. '"'
            end

            table.insert(lines, '  "' .. key .. '": ' .. valueStr .. ',\n')
        end

        -- Remove last comma and close
        if #lines > 1 then
            lines[#lines] = lines[#lines]:sub(1, -3) .. '\n' -- Remove last comma
        end
        table.insert(lines, "}")

        return table.concat(lines)
    end)

    if success then
        return result
    else
        return nil
    end
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
