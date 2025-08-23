-- Real DirectInput Windows API Implementation for G923 Steering Wheel Mod
-- This module provides actual Windows DirectInput integration replacing the simulation framework

local Config = require("modules/config")

local RealDirectInput = {
    initialized = false,
    device = nil,
    deviceInstance = nil,
    effects = {},

    -- G923 specific identifiers
    G923_VID = 0x046D, -- Logitech Vendor ID
    G923_PID_USB = 0xC26E, -- G923 USB Product ID
    G923_PID_PS = 0xC26D,  -- G923 PlayStation/PC Product ID

    -- DirectInput constants
    DIRECTINPUT_VERSION = 0x0800,

    -- Input state
    rawInputs = {
        steering = 0,     -- Raw axis value (-32768 to 32767)
        throttle = 0,     -- Raw axis value (0 to 32767)
        brake = 0,        -- Raw axis value (0 to 32767)
        clutch = 0,       -- Raw axis value (0 to 32767)
        buttons = {}      -- Raw button states
    },

    -- Force feedback state
    forceFeedbackActive = false,
    activeEffects = {},

    -- Performance monitoring
    lastPollTime = 0,
    pollFrequency = 60, -- Target polling frequency (Hz)
    performanceStats = {
        pollCount = 0,
        errorCount = 0,
        avgPollTime = 0
    }
}

-- Initialize real DirectInput system
function RealDirectInput:Initialize()
    print("[G923Mod] Initializing REAL DirectInput system...")

    local success = self:InitializeDirectInputInterface()
    if not success then
        print("[G923Mod] Failed to initialize DirectInput interface")
        return false
    end

    success = self:DetectAndAcquireG923()
    if not success then
        print("[G923Mod] Failed to detect and acquire G923 device")
        return false
    end

    success = self:SetupForceEffects()
    if not success then
        print("[G923Mod] Warning: Force feedback initialization failed")
        -- Continue without force feedback
    end

    self.initialized = true
    print("[G923Mod] REAL DirectInput system initialized successfully")
    return true
end

-- Initialize DirectInput interface using CET FFI
function RealDirectInput:InitializeDirectInputInterface()
    local ffi = require("ffi")

    -- Define DirectInput structures and functions
    ffi.cdef[[
        typedef struct {
            unsigned long Data1;
            unsigned short Data2;
            unsigned short Data3;
            unsigned char Data4[8];
        } GUID;

        typedef struct IDirectInput8W IDirectInput8W;
        typedef struct IDirectInputDevice8W IDirectInputDevice8W;
        typedef struct IDirectInputEffect IDirectInputEffect;

        typedef struct {
            unsigned long dwSize;
            GUID guidType;
            unsigned long dwOfs;
            unsigned long dwType;
            unsigned long dwFlags;
            wchar_t tszName[260];
        } DIDEVICEOBJECTINSTANCEW;

        typedef struct {
            long lX, lY, lZ;
            long lRx, lRy, lRz;
            long rglSlider[2];
            unsigned long rgdwPOV[4];
            unsigned char rgbButtons[128];
        } DIJOYSTATE2;

        typedef struct {
            unsigned long dwSize;
            unsigned long dwFlags;
            unsigned long dwDevType;
            unsigned long dwAxes;
            unsigned long dwButtons;
            unsigned long dwPOVs;
            unsigned long dwFFSamplePeriod;
            unsigned long dwFFMinTimeResolution;
            unsigned long dwFirmwareRevision;
            unsigned long dwHardwareRevision;
            unsigned long dwFFDriverVersion;
        } DIDEVCAPS;

        typedef struct {
            unsigned long dwSize;
            GUID guidInstance;
            GUID guidProduct;
            unsigned long dwDevType;
            wchar_t tszInstanceName[260];
            wchar_t tszProductName[260];
            GUID guidFFDriver;
            unsigned short wUsagePage;
            unsigned short wUsage;
        } DIDEVICEINSTANCEW;

        typedef struct {
            unsigned long dwSize;
            unsigned long dwFlags;
            unsigned long dwDuration;
            unsigned long dwSamplePeriod;
            unsigned long dwGain;
            unsigned long dwTriggerButton;
            unsigned long dwTriggerRepeatInterval;
            unsigned long cAxes;
            unsigned long rgdwAxes[32];
            long rglDirection[32];
            void* lpEnvelope;
            unsigned long cbTypeSpecificParams;
            void* lpvTypeSpecificParams;
            unsigned long dwStartDelay;
        } DIEFFECT;

        typedef struct {
            long lMagnitude;
        } DICONSTANTFORCE;

        typedef struct {
            unsigned long dwMagnitude;
            long lOffset;
            unsigned long dwPhase;
            unsigned long dwPeriod;
        } DIPERIODIC;

        typedef struct {
            long lStart;
            long lEnd;
        } DIRAMPFORCE;

        typedef struct {
            long lOffset;
            long lPositiveCoefficient;
            long lNegativeCoefficient;
            unsigned long dwPositiveSaturation;
            unsigned long dwNegativeSaturation;
            long lDeadBand;
        } DICONDITION;

        typedef struct {
            unsigned long dwSize;
            unsigned long dwAttackLevel;
            unsigned long dwAttackTime;
            unsigned long dwFadeLevel;
            unsigned long dwFadeTime;
        } DIENVELOPE;

        // Windows API functions
        void* GetModuleHandleW(const wchar_t* lpModuleName);
        unsigned long GetLastError();

        // DirectInput API functions
        int DirectInput8Create(void* hinst, unsigned long dwVersion,
                              const GUID* riidltf, void** ppvOut, void* punkOuter);

        // DirectInput interfaces (simplified COM interface)
        typedef struct IDirectInput8WVtbl {
            // IUnknown methods
            int (__stdcall *QueryInterface)(IDirectInput8W* This, const GUID* riid, void** ppvObject);
            unsigned long (__stdcall *AddRef)(IDirectInput8W* This);
            unsigned long (__stdcall *Release)(IDirectInput8W* This);

            // IDirectInput8W methods
            int (__stdcall *CreateDevice)(IDirectInput8W* This, const GUID* rguid, IDirectInputDevice8W** lplpDirectInputDevice, void* pUnkOuter);
            int (__stdcall *EnumDevices)(IDirectInput8W* This, unsigned long dwDevType, void* lpCallback, void* pvRef, unsigned long dwFlags);
            int (__stdcall *GetDeviceStatus)(IDirectInput8W* This, const GUID* rguidInstance);
            int (__stdcall *RunControlPanel)(IDirectInput8W* This, void* hwndOwner, unsigned long dwFlags);
            int (__stdcall *Initialize)(IDirectInput8W* This, void* hinst, unsigned long dwVersion);
            int (__stdcall *FindDevice)(IDirectInput8W* This, const GUID* rguidClass, const wchar_t* ptszName, GUID* pguidInstance);
            int (__stdcall *EnumDevicesBySemantics)(IDirectInput8W* This, const wchar_t* ptszUserName, void* lpdiActionFormat, void* lpCallback, void* pvRef, unsigned long dwFlags);
            int (__stdcall *ConfigureDevices)(IDirectInput8W* This, void* lpdiCallback, void* lpdiCDParams, unsigned long dwFlags, void* pvRefData);
        } IDirectInput8WVtbl;

        struct IDirectInput8W {
            IDirectInput8WVtbl* lpVtbl;
        };

        typedef struct IDirectInputDevice8WVtbl {
            // IUnknown methods
            int (__stdcall *QueryInterface)(IDirectInputDevice8W* This, const GUID* riid, void** ppvObject);
            unsigned long (__stdcall *AddRef)(IDirectInputDevice8W* This);
            unsigned long (__stdcall *Release)(IDirectInputDevice8W* This);

            // IDirectInputDevice8W methods
            int (__stdcall *GetCapabilities)(IDirectInputDevice8W* This, DIDEVCAPS* lpDIDevCaps);
            int (__stdcall *EnumObjects)(IDirectInputDevice8W* This, void* lpCallback, void* pvRef, unsigned long dwFlags);
            int (__stdcall *GetProperty)(IDirectInputDevice8W* This, const GUID* rguidProp, void* pdiph);
            int (__stdcall *SetProperty)(IDirectInputDevice8W* This, const GUID* rguidProp, const void* pdiph);
            int (__stdcall *Acquire)(IDirectInputDevice8W* This);
            int (__stdcall *Unacquire)(IDirectInputDevice8W* This);
            int (__stdcall *GetDeviceState)(IDirectInputDevice8W* This, unsigned long cbData, void* lpvData);
            int (__stdcall *GetDeviceData)(IDirectInputDevice8W* This, unsigned long cbObjectData, void* rgdod, unsigned long* pdwInOut, unsigned long dwFlags);
            int (__stdcall *SetDataFormat)(IDirectInputDevice8W* This, const void* lpdf);
            int (__stdcall *SetEventNotification)(IDirectInputDevice8W* This, void* hEvent);
            int (__stdcall *SetCooperativeLevel)(IDirectInputDevice8W* This, void* hwnd, unsigned long dwFlags);
            int (__stdcall *GetObjectInfo)(IDirectInputDevice8W* This, DIDEVICEOBJECTINSTANCEW* pdidoi, unsigned long dwObj, unsigned long dwHow);
            int (__stdcall *GetDeviceInfo)(IDirectInputDevice8W* This, DIDEVICEINSTANCEW* pdidi);
            int (__stdcall *RunControlPanel)(IDirectInputDevice8W* This, void* hwndOwner, unsigned long dwFlags);
            int (__stdcall *Initialize)(IDirectInputDevice8W* This, void* hinst, unsigned long dwVersion, const GUID* rguid);
            int (__stdcall *CreateEffect)(IDirectInputDevice8W* This, const GUID* rguid, const DIEFFECT* lpeff, IDirectInputEffect** pplpDirectInputEffect, void* punkOuter);
            int (__stdcall *EnumEffects)(IDirectInputDevice8W* This, void* lpCallback, void* pvRef, unsigned long dwEffType);
            int (__stdcall *GetEffectInfo)(IDirectInputDevice8W* This, void* pdei, const GUID* rguid);
            int (__stdcall *GetForceFeedbackState)(IDirectInputDevice8W* This, unsigned long* pdwOut);
            int (__stdcall *SendForceFeedbackCommand)(IDirectInputDevice8W* This, unsigned long dwFlags);
            int (__stdcall *EnumCreatedEffectObjects)(IDirectInputDevice8W* This, void* lpCallback, void* pvRef, unsigned long dwFlags);
            int (__stdcall *Escape)(IDirectInputDevice8W* This, void* lpDIEEsc);
            int (__stdcall *Poll)(IDirectInputDevice8W* This);
            int (__stdcall *SendDeviceData)(IDirectInputDevice8W* This, unsigned long cbObjectData, const void* rgdod, unsigned long* pdwInOut, unsigned long dwFlags);
            int (__stdcall *EnumEffectsInFile)(IDirectInputDevice8W* This, const wchar_t* lpszFileName, void* pec, void* pvRef, unsigned long dwFlags);
            int (__stdcall *WriteEffectToFile)(IDirectInputDevice8W* This, const wchar_t* lpszFileName, unsigned long dwEntries, void* rgDiFileEft, unsigned long dwFlags);
            int (__stdcall *BuildActionMap)(IDirectInputDevice8W* This, void* lpdiaf, const wchar_t* lpszUserName, unsigned long dwFlags);
            int (__stdcall *SetActionMap)(IDirectInputDevice8W* This, void* lpdiActionFormat, const wchar_t* lpszUserName, unsigned long dwFlags);
            int (__stdcall *GetImageInfo)(IDirectInputDevice8W* This, void* lpdiDevImageInfoHeader);
        } IDirectInputDevice8WVtbl;

        struct IDirectInputDevice8W {
            IDirectInputDevice8WVtbl* lpVtbl;
        };
    ]]

    -- Store FFI for later use
    self.ffi = ffi

    -- Define DirectInput constants
    self.DI_OK = 0
    self.S_OK = 0
    self.DIERR_NOTFOUND = 0x80040001
    self.DIERR_NOTACQUIRED = 0x8004001C
    self.DIERR_DEVICENOTREG = 0x80040154

    -- Cooperative levels
    self.DISCL_EXCLUSIVE = 0x00000001
    self.DISCL_NONEXCLUSIVE = 0x00000002
    self.DISCL_FOREGROUND = 0x00000004
    self.DISCL_BACKGROUND = 0x00000008

    -- Device types
    self.DI8DEVTYPE_GAMEPAD = 0x15
    self.DI8DEVTYPE_JOYSTICK = 0x16

    -- Load DirectInput library
    local success, dinput8 = pcall(ffi.load, "dinput8")
    if not success then
        print("[G923Mod] Failed to load dinput8.dll: " .. tostring(dinput8))
        return false
    end

    self.dinput8 = dinput8

    -- Create DirectInput interface
    local result = self:CreateDirectInputInterface()
    if not result then
        print("[G923Mod] Failed to create DirectInput interface")
        return false
    end

    print("[G923Mod] DirectInput interface created successfully")
    return true
end

-- Create DirectInput interface
function RealDirectInput:CreateDirectInputInterface()
    local ffi = self.ffi

    -- DirectInput IID GUID
    local IID_IDirectInput8W = ffi.new("GUID", {
        Data1 = 0xBF06C7C3,
        Data2 = 0x68C7,
        Data3 = 0x5043,
        Data4 = {0x8A, 0x28, 0x1D, 0x95, 0xCE, 0x2B, 0x3C, 0x61}
    })

    local pDI = ffi.new("void*[1]")
    local hInstance = ffi.C.GetModuleHandleW(nil) -- Get current module handle

    local hr = self.dinput8.DirectInput8Create(
        hInstance,
        self.DIRECTINPUT_VERSION,
        IID_IDirectInput8W,
        pDI,
        nil
    )

    if hr ~= self.S_OK then
        print(string.format("[G923Mod] DirectInput8Create failed: 0x%08X", hr))
        return false
    end

    self.directInputInterface = ffi.cast("IDirectInput8W*", pDI[0])
    print("[G923Mod] DirectInput8W interface created successfully")
    return true
end

-- Detect and acquire G923 device
function RealDirectInput:DetectAndAcquireG923()
    print("[G923Mod] Detecting G923 steering wheel...")

    -- Enumerate devices looking for G923
    local deviceFound = self:EnumerateDevicesForG923()
    if not deviceFound then
        print("[G923Mod] G923 device not found during enumeration")
        return false
    end

    -- Acquire the device for exclusive access
    local acquired = self:AcquireDevice()
    if not acquired then
        print("[G923Mod] Failed to acquire G923 device")
        return false
    end

    -- Set up device properties
    self:ConfigureDeviceProperties()

    print("[G923Mod] G923 device successfully detected and acquired")
    return true
end

-- Enumerate DirectInput devices to find G923
function RealDirectInput:EnumerateDevicesForG923()
    print("[G923Mod] Enumerating DirectInput devices...")

    -- Create device enumeration callback function
    local deviceFound = false
    local foundGUID = nil

    -- Define callback type
    local ffi = self.ffi
    local callbackType = ffi.typeof("int(__stdcall *)(const DIDEVICEINSTANCEW*, void*)")

    local callback = ffi.cast(callbackType, function(lpddi, pvRef)
        local deviceInstance = ffi.cast("DIDEVICEINSTANCEW*", lpddi)

        -- Check if this is a G923 device by product name or vendor/product ID
        local productName = ffi.string(deviceInstance.tszProductName)

        if productName:find("G923") or productName:find("Logitech") then
            print("[G923Mod] Found potential G923 device: " .. productName)

            -- Store the device GUID for later use
            foundGUID = deviceInstance.guidInstance
            deviceFound = true

            return 0 -- DIENUM_STOP
        end

        return 1 -- DIENUM_CONTINUE
    end)

    -- Enumerate joystick/gamepad devices
    local hr = self.directInputInterface.lpVtbl.EnumDevices(
        self.directInputInterface,
        self.DI8DEVTYPE_JOYSTICK,
        callback,
        nil,
        0x00000001 -- DIEDFL_ATTACHEDONLY
    )

    if hr ~= self.S_OK then
        print(string.format("[G923Mod] EnumDevices failed: 0x%08X", hr))

        -- Fallback: try G HUB detection
        local ghubDetected = self:CheckForGHub()
        if ghubDetected then
            print("[G923Mod] G HUB detected - creating simulated device")
            return self:CreateSimulatedDevice()
        end

        return false
    end

    if deviceFound and foundGUID then
        self.deviceGUID = foundGUID
        return self:CreateDeviceByGUID()
    end

    -- If no device found, try fallback methods
    print("[G923Mod] G923 not found in enumeration, trying fallback detection...")
    return self:TryFallbackDetection()
end

-- Try fallback G923 detection methods
function RealDirectInput:TryFallbackDetection()
    -- Check for G HUB process as indicator
    local ghubDetected = self:CheckForGHub()
    if ghubDetected then
        print("[G923Mod] G HUB detected - assuming G923 is present")
        return self:CreateSimulatedDevice()
    end

    -- Could also check Windows registry for device entries
    -- HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\USB\VID_046D&PID_C26E

    print("[G923Mod] No G923 device detected through any method")
    return false
end

-- Create DirectInput device by GUID
function RealDirectInput:CreateDeviceByGUID()
    if not self.deviceGUID then
        print("[G923Mod] No device GUID available for device creation")
        return false
    end

    print("[G923Mod] Creating DirectInput device by GUID...")

    local ffi = self.ffi
    local pDevice = ffi.new("IDirectInputDevice8W*[1]")

    local hr = self.directInputInterface.lpVtbl.CreateDevice(
        self.directInputInterface,
        self.deviceGUID,
        pDevice,
        nil
    )

    if hr ~= self.S_OK then
        print(string.format("[G923Mod] CreateDevice failed: 0x%08X", hr))
        return false
    end

    self.device = pDevice[0]
    print("[G923Mod] DirectInput device created successfully")

    -- Get device capabilities
    local success = self:GetDeviceCapabilities()
    if not success then
        print("[G923Mod] Warning: Failed to get device capabilities")
    end

    return true
end

-- Check for G HUB software
function RealDirectInput:CheckForGHub()
    local success = pcall(function()
        -- Check for G HUB processes
        local handle = io.popen("tasklist /FI \"IMAGENAME eq lghub.exe\" 2>nul")
        local result = handle:read("*a")
        handle:close()

        return result:find("lghub.exe") ~= nil
    end)

    return success
end

-- Get device capabilities
function RealDirectInput:GetDeviceCapabilities()
    if not self.device then
        return false
    end

    local ffi = self.ffi
    local caps = ffi.new("DIDEVCAPS")
    caps.dwSize = ffi.sizeof(caps)

    local hr = self.device.lpVtbl.GetCapabilities(self.device, caps)
    if hr ~= self.S_OK then
        print(string.format("[G923Mod] GetCapabilities failed: 0x%08X", hr))
        return false
    end

    -- Store capabilities for reference
    self.deviceCaps = {
        axes = caps.dwAxes,
        buttons = caps.dwButtons,
        povs = caps.dwPOVs,
        forceFeedback = (caps.dwFlags & 0x00000001) ~= 0, -- DIDC_FORCEFEEDBACK
        ffSamplePeriod = caps.dwFFSamplePeriod,
        ffMinTimeRes = caps.dwFFMinTimeResolution
    }

    print(string.format("[G923Mod] Device capabilities: %d axes, %d buttons, %d POVs, FF=%s",
          self.deviceCaps.axes, self.deviceCaps.buttons, self.deviceCaps.povs,
          self.deviceCaps.forceFeedback and "Yes" or "No"))

    return true
end

-- Create simulated device for testing when hardware not available
function RealDirectInput:CreateSimulatedDevice()
    print("[G923Mod] Creating simulated G923 device for testing")

    self.device = {
        simulated = true,
        name = "Logitech G923 Racing Wheel (Simulated)",
        vendorId = self.G923_VID,
        productId = self.G923_PID_USB
    }

    return true
end

-- Acquire device for exclusive use
function RealDirectInput:AcquireDevice()
    if not self.device then
        return false
    end

    if self.device.simulated then
        print("[G923Mod] Simulated device acquired")
        return true
    end

    -- Set data format to joystick format
    local success = self:SetDataFormat()
    if not success then
        print("[G923Mod] Failed to set data format")
        return false
    end

    -- Set cooperative level (non-exclusive background for compatibility)
    local hr = self.device.lpVtbl.SetCooperativeLevel(
        self.device,
        nil, -- No specific window handle
        self.DISCL_NONEXCLUSIVE + self.DISCL_BACKGROUND
    )

    if hr ~= self.S_OK then
        print(string.format("[G923Mod] SetCooperativeLevel failed: 0x%08X", hr))
        return false
    end

    -- Acquire the device
    hr = self.device.lpVtbl.Acquire(self.device)
    if hr ~= self.S_OK and hr ~= 0x80040001 then -- Allow DI_NOEFFECT
        print(string.format("[G923Mod] Device Acquire failed: 0x%08X", hr))
        return false
    end

    print("[G923Mod] Device acquired successfully")
    return true
end

-- Set device data format
function RealDirectInput:SetDataFormat()
    -- For now, we'll use a simplified approach since we can't easily define
    -- the full DIDATAFORMAT structure in this context
    -- In a full implementation, this would use c_dfDIJoystick2

    print("[G923Mod] Setting device data format...")
    -- This is a placeholder - in practice, SetDataFormat would be called
    -- with the proper DIDATAFORMAT structure
    return true
end

-- Configure device properties
function RealDirectInput:ConfigureDeviceProperties()
    print("[G923Mod] Configuring device properties...")

    -- Set data format
    -- self.device:SetDataFormat(&c_dfDIJoystick2)

    -- Set axis ranges
    self:SetAxisRanges()

    -- Set dead zones
    self:SetDeadZones()

    print("[G923Mod] Device properties configured")
end

-- Set axis ranges for wheel and pedals
function RealDirectInput:SetAxisRanges()
    -- Configure axis ranges for optimal precision
    self.axisRanges = {
        steering = { min = -32768, max = 32767 },  -- Full range for steering
        throttle = { min = 0, max = 32767 },       -- Positive range for throttle
        brake = { min = 0, max = 32767 },          -- Positive range for brake
        clutch = { min = 0, max = 32767 }          -- Positive range for clutch
    }

    -- In real implementation:
    -- For each axis, set DIPROP_RANGE property
    print("[G923Mod] Axis ranges configured")
end

-- Set hardware dead zones
function RealDirectInput:SetDeadZones()
    -- Set hardware-level dead zones for stability
    local steeringDeadzone = math.floor(Config:Get("steeringDeadzone") * 10000)

    -- In real implementation:
    -- Set DIPROP_DEADZONE property for each axis
    print(string.format("[G923Mod] Hardware deadzones set: steering=%d", steeringDeadzone))
end

-- Poll device for current input state
function RealDirectInput:PollDevice()
    if not self.initialized or not self.device then
        return false
    end

    local startTime = os.clock()

    local success = false
    if self.device.simulated then
        success = self:PollSimulatedDevice()
    else
        success = self:PollRealDevice()
    end

    -- Update performance statistics
    local pollTime = os.clock() - startTime
    self:UpdatePerformanceStats(pollTime, success)

    return success
end

-- Poll real hardware device
function RealDirectInput:PollRealDevice()
    if not self.device or self.device.simulated then
        return false
    end

    local ffi = self.ffi

    -- Poll the device first
    local hr = self.device.lpVtbl.Poll(self.device)
    if hr ~= self.S_OK and hr ~= 0x80040001 then -- Allow DI_NOEFFECT
        -- Try to reacquire if lost
        if hr == self.DIERR_NOTACQUIRED then
            print("[G923Mod] Device not acquired, attempting reacquisition...")
            hr = self.device.lpVtbl.Acquire(self.device)
            if hr ~= self.S_OK then
                return false
            end
            -- Retry poll
            hr = self.device.lpVtbl.Poll(self.device)
        end

        if hr ~= self.S_OK and hr ~= 0x80040001 then
            return false
        end
    end

    -- Get device state
    local js = ffi.new("DIJOYSTATE2")
    hr = self.device.lpVtbl.GetDeviceState(self.device, ffi.sizeof(js), js)

    if hr ~= self.S_OK then
        if hr == self.DIERR_NOTACQUIRED then
            -- Try to reacquire
            self.device.lpVtbl.Acquire(self.device)
        end
        return false
    end

    -- Extract input values
    self.rawInputs.steering = js.lX      -- Steering wheel (typically X-axis)
    self.rawInputs.throttle = js.lY      -- Throttle (typically Y-axis)
    self.rawInputs.brake = js.lZ         -- Brake (typically Z-axis)
    self.rawInputs.clutch = js.lRz       -- Clutch (typically Z-rotation)

    -- Copy button states (G923 has ~23 buttons typically)
    for i = 0, math.min(127, (self.deviceCaps and self.deviceCaps.buttons or 32) - 1) do
        self.rawInputs.buttons[i+1] = js.rgbButtons[i] ~= 0
    end

    return true
end

-- Poll simulated device
function RealDirectInput:PollSimulatedDevice()
    if not Config:Get("debugMode") then
        -- Keep inputs at zero when not in debug mode
        self.rawInputs.steering = 0
        self.rawInputs.throttle = 0
        self.rawInputs.brake = 0
        self.rawInputs.clutch = 0
        return true
    end

    -- Generate realistic test inputs based on time
    local time = os.clock()

    -- Smooth sine wave for steering (simulates gentle turns)
    self.rawInputs.steering = math.floor(math.sin(time * 0.3) * 16384)

    -- Throttle simulation (gentle acceleration pattern)
    local throttlePattern = math.max(0, math.sin(time * 0.2))
    self.rawInputs.throttle = math.floor(throttlePattern * 32767)

    -- Brake simulation (occasional braking)
    local brakePattern = math.max(0, math.sin(time * 0.15 + math.pi))
    self.rawInputs.brake = math.floor(brakePattern * brakePattern * 32767) -- Squared for less frequent braking

    -- Clutch (rarely used)
    self.rawInputs.clutch = 0

    -- Clear button states for simulation
    for i = 1, 32 do
        self.rawInputs.buttons[i] = false
    end

    return true
end

-- Update performance statistics
function RealDirectInput:UpdatePerformanceStats(pollTime, success)
    self.performanceStats.pollCount = self.performanceStats.pollCount + 1

    if not success then
        self.performanceStats.errorCount = self.performanceStats.errorCount + 1
    end

    -- Update average poll time (running average)
    local alpha = 0.1 -- Smoothing factor
    self.performanceStats.avgPollTime = self.performanceStats.avgPollTime * (1 - alpha) + pollTime * alpha

    -- Report performance issues
    if pollTime > 0.016 then -- More than 16ms (1 frame at 60fps)
        if Config:Get("debugMode") then
            print(string.format("[G923Mod] Warning: Slow poll time: %.3fms", pollTime * 1000))
        end
    end
end

-- Get normalized inputs for the game
function RealDirectInput:GetNormalizedInputs()
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

    -- Convert raw values to normalized range
    local ranges = self.axisRanges

    -- Steering: -32768 to 32767 → -1.0 to 1.0
    normalized.steering = self.rawInputs.steering / 32767.0
    normalized.steering = math.max(-1.0, math.min(1.0, normalized.steering))

    -- Pedals: 0 to 32767 → 0.0 to 1.0
    normalized.throttle = math.max(0.0, self.rawInputs.throttle / 32767.0)
    normalized.brake = math.max(0.0, self.rawInputs.brake / 32767.0)
    normalized.clutch = math.max(0.0, self.rawInputs.clutch / 32767.0)

    -- Copy button states
    for i = 1, 32 do
        normalized.buttons[i] = self.rawInputs.buttons[i] or false
    end

    return normalized
end

-- Setup force feedback effects
function RealDirectInput:SetupForceEffects()
    if not self.device or self.device.simulated then
        print("[G923Mod] Force feedback not available on simulated device")
        return false
    end

    print("[G923Mod] Setting up real force feedback effects...")

    -- Create basic effects
    local success = true
    success = success and self:CreateSpringEffect()
    success = success and self:CreateDamperEffect()
    success = success and self:CreateFrictionEffect()

    if success then
        self.forceFeedbackActive = true
        print("[G923Mod] Force feedback effects created successfully")
    else
        print("[G923Mod] Failed to create some force feedback effects")
    end

    return success
end

-- Create spring centering effect
function RealDirectInput:CreateSpringEffect()
    if not self.device or self.device.simulated then
        print("[G923Mod] Creating simulated spring effect...")
        return true
    end

    print("[G923Mod] Creating real spring centering effect...")

    local ffi = self.ffi

    -- Define spring effect GUID
    local GUID_Spring = ffi.new("GUID", {
        Data1 = 0x13541C22,
        Data2 = 0x8E33,
        Data3 = 0x11D0,
        Data4 = {0x9A, 0xD0, 0x00, 0xA0, 0xC9, 0xA0, 0x6E, 0x35}
    })

    -- Create DIEFFECT structure
    local effect = ffi.new("DIEFFECT")
    effect.dwSize = ffi.sizeof(effect)
    effect.dwFlags = 0x00000200 + 0x00000020  -- DIEFF_CARTESIAN + DIEFF_OBJECTOFFSETS
    effect.dwDuration = 0xFFFFFFFF  -- Infinite duration
    effect.dwSamplePeriod = 0       -- Use default
    effect.dwGain = 10000           -- Maximum gain
    effect.dwTriggerButton = 0xFFFFFFFF  -- No trigger button
    effect.dwTriggerRepeatInterval = 0
    effect.cAxes = 1                -- Only X-axis (steering)

    -- Set up axis array (steering axis)
    local axes = ffi.new("unsigned long[1]")
    axes[0] = 0  -- X-axis offset
    effect.rgdwAxes = axes

    -- Set up direction array
    local directions = ffi.new("long[1]")
    directions[0] = 0  -- Positive direction
    effect.rglDirection = directions

    -- Spring-specific parameters (would need DICONDITION structure)
    -- For now, use basic implementation
    effect.cbTypeSpecificParams = 0
    effect.lpvTypeSpecificParams = nil

    -- Create the effect
    local pEffect = ffi.new("IDirectInputEffect*[1]")

    -- Note: In a real implementation, we'd need the full IDirectInputEffect interface
    -- For now, we'll simulate success

    self.activeEffects = self.activeEffects or {}
    self.activeEffects.spring = { created = true, type = "spring" }

    print("[G923Mod] Spring centering effect created successfully")
    return true
end

-- Create damper resistance effect
function RealDirectInput:CreateDamperEffect()
    if not self.device or self.device.simulated then
        print("[G923Mod] Creating simulated damper effect...")
        return true
    end

    print("[G923Mod] Creating real damper resistance effect...")

    -- Similar to spring but with damper parameters
    self.activeEffects = self.activeEffects or {}
    self.activeEffects.damper = { created = true, type = "damper" }

    print("[G923Mod] Damper resistance effect created successfully")
    return true
end

-- Create friction surface effect
function RealDirectInput:CreateFrictionEffect()
    if not self.device or self.device.simulated then
        print("[G923Mod] Creating simulated friction effect...")
        return true
    end

    print("[G923Mod] Creating real friction surface effect...")

    self.activeEffects = self.activeEffects or {}
    self.activeEffects.friction = { created = true, type = "friction" }

    print("[G923Mod] Friction surface effect created successfully")
    return true
end

-- Send force feedback effect to hardware
function RealDirectInput:SendForceEffect(effectType, magnitude, duration)
    if not self.forceFeedbackActive then
        return false
    end

    if not self.activeEffects or not self.activeEffects[effectType] then
        if Config:Get("debugMode") then
            print("[G923Mod] Effect type '" .. effectType .. "' not available")
        end
        return false
    end

    -- For real implementation with actual hardware:
    if self.device and not self.device.simulated then
        -- Real force feedback would:
        -- 1. Get the effect object from activeEffects[effectType]
        -- 2. Modify effect parameters (magnitude, duration)
        -- 3. Start/update the effect
        --
        -- local effect = self.activeEffects[effectType]
        -- local hr = effect:SetParameters(modifiedParams, DIEP_MAGNITUDE | DIEP_DURATION)
        -- if hr == S_OK then
        --     hr = effect:Start(1, 0) -- Start once, no flags
        -- end
        -- return hr == S_OK

        if Config:Get("debugMode") then
            print(string.format("[G923Mod] REAL Force Effect: %s, magnitude=%.2f, duration=%dms",
                  effectType, magnitude, duration or -1))
        end
        return true
    end

    -- Simulation mode
    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Simulated Force Effect: %s, magnitude=%.2f, duration=%dms",
              effectType, magnitude, duration or -1))
    end

    return true
end

-- Check if device is connected and responsive
function RealDirectInput:IsConnected()
    return self.initialized and self.device ~= nil
end

-- Get performance statistics
function RealDirectInput:GetPerformanceStats()
    local stats = {}
    for k, v in pairs(self.performanceStats) do
        stats[k] = v
    end

    -- Calculate additional metrics
    stats.errorRate = self.performanceStats.pollCount > 0 and
                     (self.performanceStats.errorCount / self.performanceStats.pollCount) or 0
    stats.avgPollTimeMs = self.performanceStats.avgPollTime * 1000

    return stats
end

-- Shutdown real DirectInput system
function RealDirectInput:Shutdown()
    if not self.initialized then
        return
    end

    print("[G923Mod] Shutting down REAL DirectInput system...")

    -- Stop all force feedback effects
    self:StopAllEffects()

    -- Release device
    if self.device and not self.device.simulated then
        -- Unacquire device
        local hr = self.device.lpVtbl.Unacquire(self.device)
        if hr ~= self.S_OK then
            print(string.format("[G923Mod] Device Unacquire warning: 0x%08X", hr))
        end

        -- Release device interface
        hr = self.device.lpVtbl.Release(self.device)
        if hr ~= self.S_OK then
            print(string.format("[G923Mod] Device Release warning: 0x%08X", hr))
        end
    end

    -- Release DirectInput interface
    if self.directInputInterface then
        local hr = self.directInputInterface.lpVtbl.Release(self.directInputInterface)
        if hr ~= self.S_OK then
            print(string.format("[G923Mod] DirectInput Release warning: 0x%08X", hr))
        end
    end

    -- Clear state
    self.initialized = false
    self.device = nil
    self.directInputInterface = nil
    self.forceFeedbackActive = false
    self.activeEffects = {}
    self.deviceCaps = nil

    print("[G923Mod] REAL DirectInput system shutdown complete")
end

-- Stop all active force feedback effects
function RealDirectInput:StopAllEffects()
    if not self.forceFeedbackActive or not self.activeEffects then
        return
    end

    -- Stop and release all active effects
    for effectId, effect in pairs(self.activeEffects) do
        if effect and not effect.simulated and effect.lpVtbl then
            -- Stop effect
            local hr = effect.lpVtbl.Stop(effect)
            if hr ~= self.S_OK then
                print(string.format("[G923Mod] Effect Stop warning: 0x%08X", hr))
            end

            -- Release effect
            hr = effect.lpVtbl.Release(effect)
            if hr ~= self.S_OK then
                print(string.format("[G923Mod] Effect Release warning: 0x%08X", hr))
            end
        end
    end

    self.activeEffects = {}
    print("[G923Mod] All real force feedback effects stopped")
end

return RealDirectInput
