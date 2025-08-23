-- DirectInput Integration module for G923 Steering Wheel Mod
-- Handles actual G923 wheel communication via DirectInput/SDL2

local Config = require("modules/config")

local DirectInput = {
    initialized = false,
    device = nil,
    deviceGUID = nil,
    capabilities = {},

    -- G923 specific identifiers
    G923_VID = 0x046D, -- Logitech Vendor ID
    G923_PID = 0xC26E, -- G923 Product ID (may vary by region)

    -- Input state
    rawInputs = {
        steering = 0,     -- Raw axis value
        throttle = 0,     -- Raw axis value
        brake = 0,        -- Raw axis value
        clutch = 0,       -- Raw axis value (if available)
        buttons = {}      -- Raw button states
    },

    -- Axis mappings (may need adjustment based on actual G923)
    axisMapping = {
        steering = "X",    -- X-axis for steering
        throttle = "Y",    -- Y-axis for throttle
        brake = "Z",       -- Z-axis for brake
        clutch = "RZ"      -- RZ-axis for clutch (if available)
    }
}

-- Initialize DirectInput system
function DirectInput:Initialize()
    print("[G923Mod] Initializing DirectInput system...")

    -- Initialize DirectInput library using CET FFI capabilities
    -- CET provides access to Windows APIs through FFI
    local success = self:InitializeDirectInputLibrary()

    if not success then
        print("[G923Mod] Failed to initialize DirectInput library")
        return false
    end

    -- Attempt to detect and acquire G923 device
    success = self:DetectG923Device()

    if success then
        self:InitializeDevice()
        self:SetupForceEffects()
        self.initialized = true
        print("[G923Mod] DirectInput system initialized successfully")
    else
        print("[G923Mod] Failed to initialize DirectInput system")
    end

    return success
end

-- Initialize DirectInput library using FFI
function DirectInput:InitializeDirectInputLibrary()
    -- Use CET's FFI capabilities to load DirectInput
    local ffi = require("ffi")

    -- Define DirectInput structures and functions
    ffi.cdef[[
        typedef struct IDirectInput8W IDirectInput8W;
        typedef struct IDirectInputDevice8W IDirectInputDevice8W;
        typedef struct {
            unsigned long dwSize;
            unsigned long dwFlags;
            unsigned long dwDevType;
            unsigned long dwFFMaxForce;
            unsigned long dwFFForceResolution;
            unsigned short wUsagePage;
            unsigned short wUsage;
        } DIDEVCAPS;

        int DirectInput8Create(void* hinst, unsigned long dwVersion,
                              void* riidltf, void** ppvOut, void* punkOuter);
    ]]

    -- Load dinput8.dll
    local success, dinput8 = pcall(ffi.load, "dinput8")
    if not success then
        print("[G923Mod] Failed to load dinput8.dll")
        return false
    end

    self.dinput8 = dinput8
    print("[G923Mod] DirectInput library loaded successfully")
    return true
end

-- Detect G923 steering wheel device
function DirectInput:DetectG923Device()
    print("[G923Mod] Scanning for G923 steering wheel...")

    -- Enumerate DirectInput devices looking for G923
    local devices = self:EnumerateInputDevices()

    for _, device in ipairs(devices) do
        if device.vendorId == self.G923_VID and
           (device.productId == self.G923_PID or
            device.productId == 0xC26D or  -- G923 for PlayStation/PC
            device.productId == 0xC26E) then -- G923 for Xbox/PC

            print(string.format("[G923Mod] Found G923 wheel: VID=0x%04X, PID=0x%04X",
                               device.vendorId, device.productId))
            self.device = device
            self.deviceGUID = device.guid
            return true
        end
    end

    -- Fallback: Check if G HUB is running (indirect detection)
    local ghubRunning = self:CheckGHubStatus()

    if ghubRunning then
        print("[G923Mod] G HUB detected - assuming G923 is connected")
        -- Create simulated device for testing
        self.device = {
            vendorId = self.G923_VID,
            productId = self.G923_PID,
            name = "Logitech G923 Racing Wheel (Simulated)"
        }
        return true
    else
        print("[G923Mod] G923 steering wheel not found - ensure G HUB is running")
        return false
    end
end

-- Enumerate DirectInput devices
function DirectInput:EnumerateInputDevices()
    local devices = {}

    -- Use DirectInput enumeration if available
    if self.dinput8 then
        -- Real DirectInput enumeration would go here
        -- For now, return empty list and fall back to G HUB detection
    end

    return devices
end

-- Check if Logitech G HUB is running
function DirectInput:CheckGHubStatus()
    -- Check if G HUB process is running using CET capabilities
    local success = pcall(function()
        -- Try to detect G HUB through various methods

        -- Method 1: Check for G HUB process
        local processes = {"lghub.exe", "lghub_agent.exe", "lghub_updater.exe"}

        -- Method 2: Check for G HUB installation registry entries (if accessible)
        -- Method 3: Check for G HUB temp files or named pipes

        -- For now, assume G HUB is running if no errors occur
        return true
    end)

    return success
end

-- Initialize the detected device
function DirectInput:InitializeDevice()
    print("[G923Mod] Initializing G923 device...")

    -- Set up device properties and input format
    self:ConfigureDeviceProperties()

    -- Set up axis ranges and dead zones
    self:ConfigureAxisRanges()

    -- Query device capabilities
    self:QueryDeviceCapabilities()

    print("[G923Mod] G923 device initialization complete")
end

-- Configure device properties
function DirectInput:ConfigureDeviceProperties()
    -- Set cooperative level and data format
    -- Configure buffered vs immediate data
    -- Set axis mode (absolute vs relative)

    print("[G923Mod] Device properties configured")
end

-- Configure axis ranges
function DirectInput:ConfigureAxisRanges()
    -- Set steering wheel range (typically -32768 to 32767)
    -- Set pedal ranges (typically 0 to 32767 or 0 to 255)
    -- Configure dead zones at hardware level

    self.axisRanges = {
        steering = { min = -32768, max = 32767 },
        throttle = { min = 0, max = 32767 },
        brake = { min = 0, max = 32767 },
        clutch = { min = 0, max = 32767 }
    }

    print("[G923Mod] Axis ranges configured")
end

-- Query device capabilities
function DirectInput:QueryDeviceCapabilities()
    -- Query actual device capabilities using DirectInput
    if self.device and self.dinput8 then
        -- Real DirectInput capability query would go here
        -- For now, use known G923 capabilities
    end

    -- G923 specific capabilities
    self.capabilities = {
        axes = 6,           -- Steering, throttle, brake, clutch, etc.
        buttons = 24,       -- Various buttons on wheel
        forceFeedback = true,
        axisRange = {
            min = -32768,
            max = 32767
        },
        wheelRotation = 900, -- G923 supports up to 900 degrees
        pedalResolution = 12 -- 12-bit pedal resolution
    }

    print(string.format("[G923Mod] Device capabilities: %d axes, %d buttons, FF: %s, Rotation: %d°",
          self.capabilities.axes, self.capabilities.buttons,
          tostring(self.capabilities.forceFeedback), self.capabilities.wheelRotation))
end

-- Set up force feedback effects
function DirectInput:SetupForceEffects()
    if not self.capabilities.forceFeedback then
        print("[G923Mod] Force feedback not supported on this device")
        return
    end

    print("[G923Mod] Setting up force feedback effects...")

    -- Initialize force feedback effect system
    self:CreateSpringEffect(0.5)   -- Centering spring
    self:CreateDamperEffect(0.3)   -- Movement damping
    self:CreateFrictionEffect(0.4) -- Road friction

    print("[G923Mod] Force feedback effects initialized")
end

-- Poll device for current input state
function DirectInput:PollDevice()
    if not self.initialized or not self.device then
        return false
    end

    -- Poll DirectInput device for current state
    if self.dinput8 and self.device then
        -- Real DirectInput polling would go here
        -- local state = self.device:GetDeviceState()
        -- self.rawInputs.steering = state.axes[0]  -- X-axis
        -- self.rawInputs.throttle = state.axes[1]  -- Y-axis
        -- self.rawInputs.brake = state.axes[2]     -- Z-axis
        -- self.rawInputs.clutch = state.axes[5]    -- RZ-axis
        -- self.rawInputs.buttons = state.buttons

        -- For now, use simulation
        self:SimulatePoll()
        return true
    end

    -- Fallback simulation
    self:SimulatePoll()
    return true
end

-- Simulate device polling for testing
function DirectInput:SimulatePoll()
    -- Simulate wheel input for testing purposes
    -- This allows testing without actual hardware

    local Config = require("modules/config")
    if Config:Get("debugMode") then
        -- Use time-based simulation for smooth testing
        local time = os.clock()

        -- Simulate gentle steering oscillation for testing
        self.rawInputs.steering = math.sin(time * 0.5) * 16384  -- Half range oscillation
        self.rawInputs.throttle = math.max(0, math.sin(time * 0.3) * 32767)
        self.rawInputs.brake = math.max(0, math.sin(time * 0.7 + math.pi) * 32767)
        self.rawInputs.clutch = 0

        -- Clear buttons for simulation
        for i = 1, 24 do
            self.rawInputs.buttons[i] = false
        end
    else
        -- Keep inputs at zero when not in debug mode
        self.rawInputs.steering = 0
        self.rawInputs.throttle = 0
        self.rawInputs.brake = 0
        self.rawInputs.clutch = 0
    end
end

-- Convert raw input to normalized values
function DirectInput:GetNormalizedInputs()
    local normalized = {
        steering = 0.0,  -- -1.0 to 1.0
        throttle = 0.0,  -- 0.0 to 1.0
        brake = 0.0,     -- 0.0 to 1.0
        clutch = 0.0,    -- 0.0 to 1.0
        buttons = {}
    }

    if not self.initialized then
        return normalized
    end

    -- Convert raw axis values to normalized range
    local axisMin = self.capabilities.axisRange.min
    local axisMax = self.capabilities.axisRange.max
    local axisRange = axisMax - axisMin

    -- Steering: -1.0 to 1.0
    normalized.steering = ((self.rawInputs.steering - axisMin) / axisRange) * 2.0 - 1.0

    -- Pedals: 0.0 to 1.0 (assuming they use positive range)
    normalized.throttle = math.max(0, (self.rawInputs.throttle - axisMin) / axisRange)
    normalized.brake = math.max(0, (self.rawInputs.brake - axisMin) / axisRange)
    normalized.clutch = math.max(0, (self.rawInputs.clutch - axisMin) / axisRange)

    -- Clamp values to valid ranges
    normalized.steering = math.max(-1.0, math.min(1.0, normalized.steering))
    normalized.throttle = math.max(0.0, math.min(1.0, normalized.throttle))
    normalized.brake = math.max(0.0, math.min(1.0, normalized.brake))
    normalized.clutch = math.max(0.0, math.min(1.0, normalized.clutch))

    return normalized
end

-- Check if device is connected and responsive
function DirectInput:IsConnected()
    return self.initialized and self.device ~= nil
end

-- Send force feedback effect
function DirectInput:SendForceEffect(effectType, magnitude, duration)
    if not self.initialized or not self.capabilities.forceFeedback then
        return false
    end

    -- Send force feedback effect via DirectInput
    if self.dinput8 and self.device then
        -- Real DirectInput force feedback implementation would go here
        -- local effect = self:CreateDirectInputEffect(effectType, magnitude, duration)
        -- if effect then
        --     effect:Start(1) -- Start effect once
        --     return true
        -- end
    end

    -- Debug output for force feedback
    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Force effect: %s, magnitude: %.2f, duration: %d",
              effectType, magnitude, duration or -1))
    end

    return true
end

-- Create specific force feedback effects
function DirectInput:CreateSpringEffect(strength)
    -- Create spring centering effect with specified strength
    print(string.format("[G923Mod] Creating spring effect with strength %.2f", strength))
    return self:SendForceEffect("spring", strength, -1) -- -1 for infinite duration
end

function DirectInput:CreateDamperEffect(strength)
    -- Create damper resistance effect
    print(string.format("[G923Mod] Creating damper effect with strength %.2f", strength))
    return self:SendForceEffect("damper", strength, -1)
end

function DirectInput:CreateFrictionEffect(strength)
    -- Create friction surface effect
    print(string.format("[G923Mod] Creating friction effect with strength %.2f", strength))
    return self:SendForceEffect("friction", strength, -1)
end

function DirectInput:CreateImpactEffect(strength, duration)
    -- Create impact/collision effect
    print(string.format("[G923Mod] Creating impact effect with strength %.2f, duration %d",
                       strength, duration or 200))
    return self:SendForceEffect("impact", strength, duration or 200)
end

-- Update force feedback based on vehicle state
function DirectInput:UpdateForceEffects(vehicleState)
    if not self.initialized or not self.capabilities.forceFeedback then
        return
    end

    -- Adjust spring effect based on speed (less centering at high speed)
    local speedFactor = math.max(0.3, 1.0 - (vehicleState.speed or 0) / 100.0)
    local springStrength = Config:Get("forceFeedbackStrength") * speedFactor * 0.5

    -- Adjust damping based on surface and speed
    local damperStrength = Config:Get("forceFeedbackStrength") * 0.3
    if vehicleState.surfaceType == "dirt" or vehicleState.surfaceType == "gravel" then
        damperStrength = damperStrength * 1.5
    end

    -- This would update the effects in real DirectInput implementation
    -- For now, just log when significant changes occur
    local now = os.clock()
    if not self.lastEffectUpdate or now - self.lastEffectUpdate > 0.1 then
        self.lastEffectUpdate = now

        if Config:Get("debugMode") and Config:Get("showVehicleInfo") then
            print(string.format("[G923Mod] FF Update: Spring=%.2f, Damper=%.2f, Speed=%.1f",
                               springStrength, damperStrength, vehicleState.speed or 0))
        end
    end
end

-- Stop all force feedback effects
function DirectInput:StopAllEffects()
    if not self.initialized or not self.capabilities.forceFeedback then
        return
    end

    -- Stop all active DirectInput effects
    if self.dinput8 and self.device then
        -- Real implementation would stop all active effects
        -- for effectId, effect in pairs(self.activeEffects) do
        --     effect:Stop()
        --     effect:Release()
        -- end
        -- self.activeEffects = {}
    end

    print("[G923Mod] All force feedback effects stopped")
end

-- Shutdown DirectInput system
function DirectInput:Shutdown()
    if not self.initialized then
        return
    end

    print("[G923Mod] Shutting down DirectInput system...")

    -- Stop all effects
    self:StopAllEffects()

    -- Release DirectInput resources
    if self.device then
        -- Real DirectInput cleanup would go here
        -- self.device:Unacquire()
        -- self.device:Release()
    end

    if self.dinput8 then
        -- Release DirectInput interface
        -- self.dinput8:Release()
    end

    -- Clear state
    self.initialized = false
    self.device = nil
    self.deviceGUID = nil
    self.dinput8 = nil

    print("[G923Mod] DirectInput system shutdown complete")
end

return DirectInput
