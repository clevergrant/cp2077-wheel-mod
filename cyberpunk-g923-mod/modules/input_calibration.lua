-- Advanced Input Calibration System for G923 Steering Wheel Mod
-- Provides automatic and manual calibration for optimal wheel setup

local Config = require("modules/config")

local InputCalibration = {
    initialized = false,
    calibrationActive = false,

    -- Calibration state
    calibrationData = {
        steering = {
            centerPoint = 0,
            leftRange = -32768,
            rightRange = 32767,
            deadzone = 0.05,
            linearity = 1.0
        },

        throttle = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        },

        brake = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        },

        clutch = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        }
    },

    -- Calibration process state
    process = {
        step = 0,
        totalSteps = 7,
        currentAxis = "",
        instruction = "",
        timeout = 0,
        startTime = 0,

        -- Data collection
        samples = {},
        sampleCount = 0,
        requiredSamples = 60, -- 1 second at 60Hz

        -- Results
        completed = false,
        results = {}
    },

    -- Auto-calibration settings
    autoCalibration = {
        enabled = true,
        confidence = 0.0,
        samplesCollected = 0,
        requiredSamples = 1800, -- 30 seconds of data

        -- Running statistics
        stats = {
            steering = { min = 0, max = 0, center = 0, samples = {} },
            throttle = { min = 32767, max = 0, samples = {} },
            brake = { min = 32767, max = 0, samples = {} },
            clutch = { min = 32767, max = 0, samples = {} }
        }
    }
}

-- Initialize calibration system
function InputCalibration:Initialize()
    print("[G923Mod] Initializing input calibration system...")

    -- Load existing calibration data
    self:LoadCalibrationData()

    -- Enable auto-calibration if configured
    self.autoCalibration.enabled = Config:Get("autoCalibrationEnabled")

    self.initialized = true
    print("[G923Mod] Input calibration system initialized")
end

-- Load calibration data from configuration
function InputCalibration:LoadCalibrationData()
    -- Load calibration values from config or use defaults
    local calibData = Config:Get("calibrationData")

    if calibData then
        -- Apply loaded calibration data
        for axis, data in pairs(calibData) do
            if self.calibrationData[axis] then
                for key, value in pairs(data) do
                    self.calibrationData[axis][key] = value
                end
            end
        end
        print("[G923Mod] Loaded existing calibration data")
    else
        print("[G923Mod] Using default calibration data")
    end
end

-- Save calibration data to configuration
function InputCalibration:SaveCalibrationData()
    Config:Set("calibrationData", self.calibrationData)
    Config:Save()
    print("[G923Mod] Calibration data saved")
end

-- Start manual calibration process
function InputCalibration:StartCalibration()
    if self.calibrationActive then
        print("[G923Mod] Calibration already in progress")
        return false
    end

    print("[G923Mod] Starting manual calibration process...")

    self.calibrationActive = true
    self.process.step = 1
    self.process.startTime = os.clock()
    self.process.completed = false

    -- Reset calibration data to defaults
    self:ResetCalibrationData()

    -- Start with steering calibration
    self:StartSteeringCalibration()

    return true
end

-- Reset calibration data to factory defaults
function InputCalibration:ResetCalibrationData()
    self.calibrationData = {
        steering = {
            centerPoint = 0,
            leftRange = -32768,
            rightRange = 32767,
            deadzone = 0.05,
            linearity = 1.0
        },

        throttle = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        },

        brake = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        },

        clutch = {
            minValue = 0,
            maxValue = 32767,
            deadzone = 0.02,
            curve = "linear"
        }
    }
end

-- Update calibration process
function InputCalibration:Update(inputData)
    if not self.initialized then
        return
    end

    -- Update auto-calibration
    if self.autoCalibration.enabled and not self.calibrationActive then
        self:UpdateAutoCalibration(inputData)
    end

    -- Update manual calibration
    if self.calibrationActive then
        self:UpdateManualCalibration(inputData)
    end
end

-- Update automatic calibration
function InputCalibration:UpdateAutoCalibration(inputData)
    if not inputData then
        return
    end

    local stats = self.autoCalibration.stats

    -- Collect steering data
    if inputData.steering then
        table.insert(stats.steering.samples, inputData.steering)
        stats.steering.min = math.min(stats.steering.min, inputData.steering)
        stats.steering.max = math.max(stats.steering.max, inputData.steering)

        -- Keep only recent samples for center calculation
        if #stats.steering.samples > 300 then
            table.remove(stats.steering.samples, 1)
        end
    end

    -- Collect throttle data
    if inputData.throttle then
        table.insert(stats.throttle.samples, inputData.throttle)
        stats.throttle.min = math.min(stats.throttle.min, inputData.throttle)
        stats.throttle.max = math.max(stats.throttle.max, inputData.throttle)

        if #stats.throttle.samples > 300 then
            table.remove(stats.throttle.samples, 1)
        end
    end

    -- Collect brake data
    if inputData.brake then
        table.insert(stats.brake.samples, inputData.brake)
        stats.brake.min = math.min(stats.brake.min, inputData.brake)
        stats.brake.max = math.max(stats.brake.max, inputData.brake)

        if #stats.brake.samples > 300 then
            table.remove(stats.brake.samples, 1)
        end
    end

    -- Update sample count and confidence
    self.autoCalibration.samplesCollected = self.autoCalibration.samplesCollected + 1
    self.autoCalibration.confidence = math.min(1.0,
        self.autoCalibration.samplesCollected / self.autoCalibration.requiredSamples)

    -- Apply auto-calibration when we have enough confidence
    if self.autoCalibration.confidence > 0.8 and
       self.autoCalibration.samplesCollected % 300 == 0 then -- Update every 5 seconds
        self:ApplyAutoCalibration()
    end
end

-- Apply automatic calibration adjustments
function InputCalibration:ApplyAutoCalibration()
    local stats = self.autoCalibration.stats

    -- Calculate steering center point from recent samples
    if #stats.steering.samples > 100 then
        local sum = 0
        for _, sample in ipairs(stats.steering.samples) do
            sum = sum + sample
        end
        local avgCenter = sum / #stats.steering.samples

        -- Only adjust center if it's significantly different
        if math.abs(avgCenter - self.calibrationData.steering.centerPoint) > 1000 then
            self.calibrationData.steering.centerPoint = avgCenter

            if Config:Get("debugMode") then
                print(string.format("[G923Mod] Auto-calibration: Steering center adjusted to %d", avgCenter))
            end
        end
    end

    -- Adjust steering range
    if stats.steering.min < self.calibrationData.steering.leftRange then
        self.calibrationData.steering.leftRange = stats.steering.min
    end
    if stats.steering.max > self.calibrationData.steering.rightRange then
        self.calibrationData.steering.rightRange = stats.steering.max
    end

    -- Adjust pedal ranges
    if stats.throttle.max > self.calibrationData.throttle.maxValue then
        self.calibrationData.throttle.maxValue = stats.throttle.max
    end
    if stats.brake.max > self.calibrationData.brake.maxValue then
        self.calibrationData.brake.maxValue = stats.brake.max
    end

    -- Save updated calibration
    self:SaveCalibrationData()
end

-- Update manual calibration process
function InputCalibration:UpdateManualCalibration(inputData)
    if not inputData then
        return
    end

    -- Check timeout
    local elapsed = os.clock() - self.process.startTime
    if elapsed > self.process.timeout then
        self:AdvanceCalibrationStep()
        return
    end

    -- Collect samples for current step
    if self.process.currentAxis and inputData[self.process.currentAxis] then
        table.insert(self.process.samples, inputData[self.process.currentAxis])
        self.process.sampleCount = #self.process.samples
    end

    -- Check if we have enough samples
    if self.process.sampleCount >= self.process.requiredSamples then
        self:ProcessCalibrationStep()
    end
end

-- Start steering calibration
function InputCalibration:StartSteeringCalibration()
    self.process.step = 1
    self.process.currentAxis = "steering"
    self.process.instruction = "Center the steering wheel and hold steady"
    self.process.timeout = 10.0
    self.process.samples = {}
    self.process.sampleCount = 0
    self.process.startTime = os.clock()

    print("[G923Mod] Calibration Step 1: " .. self.process.instruction)
end

-- Process current calibration step
function InputCalibration:ProcessCalibrationStep()
    if #self.process.samples == 0 then
        print("[G923Mod] No samples collected for calibration step")
        self:AdvanceCalibrationStep()
        return
    end

    -- Process samples based on current step
    if self.process.step == 1 then
        -- Steering center
        local sum = 0
        for _, sample in ipairs(self.process.samples) do
            sum = sum + sample
        end
        self.calibrationData.steering.centerPoint = sum / #self.process.samples
        print(string.format("[G923Mod] Steering center calibrated: %d", self.calibrationData.steering.centerPoint))

    elseif self.process.step == 2 then
        -- Steering left range
        local minValue = math.min(table.unpack(self.process.samples))
        self.calibrationData.steering.leftRange = minValue
        print(string.format("[G923Mod] Steering left range calibrated: %d", minValue))

    elseif self.process.step == 3 then
        -- Steering right range
        local maxValue = math.max(table.unpack(self.process.samples))
        self.calibrationData.steering.rightRange = maxValue
        print(string.format("[G923Mod] Steering right range calibrated: %d", maxValue))

    elseif self.process.step == 4 then
        -- Throttle range
        local minValue = math.min(table.unpack(self.process.samples))
        local maxValue = math.max(table.unpack(self.process.samples))
        self.calibrationData.throttle.minValue = minValue
        self.calibrationData.throttle.maxValue = maxValue
        print(string.format("[G923Mod] Throttle range calibrated: %d - %d", minValue, maxValue))

    elseif self.process.step == 5 then
        -- Brake range
        local minValue = math.min(table.unpack(self.process.samples))
        local maxValue = math.max(table.unpack(self.process.samples))
        self.calibrationData.brake.minValue = minValue
        self.calibrationData.brake.maxValue = maxValue
        print(string.format("[G923Mod] Brake range calibrated: %d - %d", minValue, maxValue))

    elseif self.process.step == 6 then
        -- Clutch range (if available)
        local minValue = math.min(table.unpack(self.process.samples))
        local maxValue = math.max(table.unpack(self.process.samples))
        self.calibrationData.clutch.minValue = minValue
        self.calibrationData.clutch.maxValue = maxValue
        print(string.format("[G923Mod] Clutch range calibrated: %d - %d", minValue, maxValue))
    end

    self:AdvanceCalibrationStep()
end

-- Advance to next calibration step
function InputCalibration:AdvanceCalibrationStep()
    self.process.step = self.process.step + 1

    if self.process.step > self.process.totalSteps then
        self:CompleteCalibration()
        return
    end

    -- Set up next step
    self.process.samples = {}
    self.process.sampleCount = 0
    self.process.startTime = os.clock()

    if self.process.step == 2 then
        self.process.currentAxis = "steering"
        self.process.instruction = "Turn steering wheel fully to the LEFT and hold"
        self.process.timeout = 10.0

    elseif self.process.step == 3 then
        self.process.currentAxis = "steering"
        self.process.instruction = "Turn steering wheel fully to the RIGHT and hold"
        self.process.timeout = 10.0

    elseif self.process.step == 4 then
        self.process.currentAxis = "throttle"
        self.process.instruction = "Press throttle pedal fully and release completely several times"
        self.process.timeout = 15.0

    elseif self.process.step == 5 then
        self.process.currentAxis = "brake"
        self.process.instruction = "Press brake pedal fully and release completely several times"
        self.process.timeout = 15.0

    elseif self.process.step == 6 then
        self.process.currentAxis = "clutch"
        self.process.instruction = "Press clutch pedal fully and release completely several times (skip if no clutch)"
        self.process.timeout = 15.0

    elseif self.process.step == 7 then
        self.process.instruction = "Calibration complete - testing all inputs"
        self.process.timeout = 5.0
    end

    print("[G923Mod] Calibration Step " .. self.process.step .. ": " .. self.process.instruction)
end

-- Complete calibration process
function InputCalibration:CompleteCalibration()
    print("[G923Mod] Manual calibration completed successfully!")

    self.calibrationActive = false
    self.process.completed = true

    -- Validate calibration data
    self:ValidateCalibrationData()

    -- Save calibration
    self:SaveCalibrationData()

    -- Apply calibration to current configuration
    self:ApplyCalibrationToConfig()

    print("[G923Mod] Calibration data applied and saved")
end

-- Validate calibration data for sanity
function InputCalibration:ValidateCalibrationData()
    local steering = self.calibrationData.steering

    -- Validate steering ranges
    if steering.leftRange >= steering.centerPoint or steering.rightRange <= steering.centerPoint then
        print("[G923Mod] Warning: Invalid steering calibration detected, using defaults")
        steering.leftRange = -32768
        steering.rightRange = 32767
        steering.centerPoint = 0
    end

    -- Validate pedal ranges
    for _, axis in ipairs({"throttle", "brake", "clutch"}) do
        local data = self.calibrationData[axis]
        if data.maxValue <= data.minValue then
            print(string.format("[G923Mod] Warning: Invalid %s calibration, using defaults", axis))
            data.minValue = 0
            data.maxValue = 32767
        end
    end
end

-- Apply calibration to current configuration
function InputCalibration:ApplyCalibrationToConfig()
    -- Update configuration with calibration values
    Config:Set("steeringDeadzone", self.calibrationData.steering.deadzone)
    Config:Set("throttleDeadzone", self.calibrationData.throttle.deadzone)
    Config:Set("brakeDeadzone", self.calibrationData.brake.deadzone)

    -- Save configuration
    Config:Save()
end

-- Apply calibration to raw input values
function InputCalibration:ApplyCalibration(rawInputs)
    if not self.initialized then
        return rawInputs
    end

    local calibrated = {}

    -- Apply steering calibration
    local steering = rawInputs.steering or 0
    local steeringCal = self.calibrationData.steering

    -- Center the steering
    steering = steering - steeringCal.centerPoint

    -- Normalize to range
    if steering < 0 then
        local range = steeringCal.centerPoint - steeringCal.leftRange
        calibrated.steering = range > 0 and (steering / range) or 0
    else
        local range = steeringCal.rightRange - steeringCal.centerPoint
        calibrated.steering = range > 0 and (steering / range) or 0
    end

    -- Clamp steering to valid range
    calibrated.steering = math.max(-1.0, math.min(1.0, calibrated.steering))

    -- Apply pedal calibrations
    for _, axis in ipairs({"throttle", "brake", "clutch"}) do
        local rawValue = rawInputs[axis] or 0
        local axisCal = self.calibrationData[axis]

        -- Normalize to 0-1 range
        local range = axisCal.maxValue - axisCal.minValue
        if range > 0 then
            calibrated[axis] = math.max(0.0, math.min(1.0,
                (rawValue - axisCal.minValue) / range))
        else
            calibrated[axis] = 0.0
        end
    end

    -- Copy button states
    calibrated.buttons = rawInputs.buttons or {}

    return calibrated
end

-- Get calibration status
function InputCalibration:GetStatus()
    return {
        initialized = self.initialized,
        calibrationActive = self.calibrationActive,
        autoCalibrationEnabled = self.autoCalibration.enabled,
        autoCalibrationConfidence = self.autoCalibration.confidence,

        process = self.calibrationActive and {
            step = self.process.step,
            totalSteps = self.process.totalSteps,
            instruction = self.process.instruction,
            samplesCollected = self.process.sampleCount,
            requiredSamples = self.process.requiredSamples,
            timeRemaining = math.max(0, self.process.timeout - (os.clock() - self.process.startTime))
        } or nil,

        calibrationData = self.calibrationData
    }
end

-- Cancel ongoing calibration
function InputCalibration:CancelCalibration()
    if not self.calibrationActive then
        return false
    end

    print("[G923Mod] Calibration cancelled by user")
    self.calibrationActive = false
    self.process.completed = false

    return true
end

-- Shutdown calibration system
function InputCalibration:Shutdown()
    if not self.initialized then
        return
    end

    if self.calibrationActive then
        self:CancelCalibration()
    end

    print("[G923Mod] Input calibration system shutdown")
    self.initialized = false
end

return InputCalibration
