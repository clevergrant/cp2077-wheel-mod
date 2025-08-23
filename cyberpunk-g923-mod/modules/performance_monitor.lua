-- Performance Monitor for G923 Steering Wheel Mod
-- Tracks performance impact and optimizes mod behavior

local Config = require("modules/config")

local PerformanceMonitor = {
    enabled = false,
    startTime = 0,

    -- Performance metrics
    metrics = {
        frameTime = {},        -- Frame time samples
        inputLatency = {},     -- Input processing latency
        memoryUsage = {},      -- Memory usage samples
        cpuUsage = {},         -- CPU usage estimates

        -- Counters
        totalFrames = 0,
        totalInputPolls = 0,
        totalErrors = 0,

        -- Averages (rolling)
        avgFrameTime = 0,
        avgInputLatency = 0,
        avgMemoryUsage = 0,
        avgCpuUsage = 0
    },

    -- Performance targets
    targets = {
        maxFrameTime = 16.67,     -- 60 FPS target (milliseconds)
        maxInputLatency = 8.0,    -- Maximum input latency (milliseconds)
        maxMemoryUsage = 50.0,    -- Maximum memory usage (MB)
        maxCpuUsage = 2.0,        -- Maximum CPU usage (%)

        -- Warning thresholds (80% of max)
        warnFrameTime = 13.33,
        warnInputLatency = 6.4,
        warnMemoryUsage = 40.0,
        warnCpuUsage = 1.6
    },

    -- Optimization state
    optimization = {
        adaptivePolling = true,    -- Adjust polling rate based on performance
        dynamicQuality = true,     -- Reduce force feedback quality if needed
        emergencyMode = false,     -- Disable features if performance is critical

        -- Current optimization level (0 = full features, 5 = minimal)
        level = 0
    },

    -- Sample collection
    sampleSize = 300,             -- Number of samples to keep (5 seconds at 60fps)
    sampleInterval = 0.1,         -- Sample collection interval (seconds)
    lastSampleTime = 0
}

-- Initialize performance monitoring
function PerformanceMonitor:Initialize()
    print("[G923Mod] Initializing performance monitor...")

    self.enabled = Config:Get("performanceMonitoringEnabled") or Config:Get("debugMode")
    self.startTime = os.clock()

    -- Initialize metric arrays
    self:ResetMetrics()

    -- Set up monitoring based on configuration
    self.optimization.adaptivePolling = Config:Get("adaptivePollingEnabled")
    self.optimization.dynamicQuality = Config:Get("dynamicQualityEnabled")

    if self.enabled then
        print("[G923Mod] Performance monitoring enabled")
    else
        print("[G923Mod] Performance monitoring disabled")
    end
end

-- Reset all metrics
function PerformanceMonitor:ResetMetrics()
    for key, _ in pairs(self.metrics) do
        if type(self.metrics[key]) == "table" then
            self.metrics[key] = {}
        else
            self.metrics[key] = 0
        end
    end
end

-- Update performance metrics
function PerformanceMonitor:Update(deltaTime)
    if not self.enabled then
        return
    end

    local currentTime = os.clock()

    -- Only sample at specified intervals
    if currentTime - self.lastSampleTime < self.sampleInterval then
        return
    end

    self.lastSampleTime = currentTime

    -- Collect frame time metric
    self:CollectFrameTime(deltaTime)

    -- Collect memory usage
    self:CollectMemoryUsage()

    -- Collect CPU usage estimate
    self:CollectCpuUsage()

    -- Update counters
    self.metrics.totalFrames = self.metrics.totalFrames + 1

    -- Calculate rolling averages
    self:UpdateAverages()

    -- Check performance and adjust optimization if needed
    self:CheckPerformanceAndOptimize()

    -- Report performance issues if debug mode is enabled
    if Config:Get("debugMode") and self.metrics.totalFrames % 300 == 0 then
        self:ReportPerformanceStatus()
    end
end

-- Collect frame time data
function PerformanceMonitor:CollectFrameTime(deltaTime)
    local frameTimeMs = deltaTime * 1000

    -- Add to sample array
    table.insert(self.metrics.frameTime, frameTimeMs)

    -- Keep only recent samples
    if #self.metrics.frameTime > self.sampleSize then
        table.remove(self.metrics.frameTime, 1)
    end
end

-- Collect memory usage
function PerformanceMonitor:CollectMemoryUsage()
    -- Estimate memory usage (collectgarbage returns KB, convert to MB)
    local memoryKB = collectgarbage("count")
    local memoryMB = memoryKB / 1024

    table.insert(self.metrics.memoryUsage, memoryMB)

    if #self.metrics.memoryUsage > self.sampleSize then
        table.remove(self.metrics.memoryUsage, 1)
    end
end

-- Collect CPU usage estimate
function PerformanceMonitor:CollectCpuUsage()
    -- Estimate CPU usage based on processing time
    local processingTime = os.clock()

    -- Simulate some processing to measure
    local startTime = os.clock()

    -- Quick CPU measurement (this is a simple estimate)
    local iterations = 1000
    for i = 1, iterations do
        math.sin(i * 0.1)
    end

    local processingTimeMs = (os.clock() - startTime) * 1000
    local estimatedCpuPercent = processingTimeMs * 0.1 -- Rough estimate

    table.insert(self.metrics.cpuUsage, estimatedCpuPercent)

    if #self.metrics.cpuUsage > self.sampleSize then
        table.remove(self.metrics.cpuUsage, 1)
    end
end

-- Record input latency
function PerformanceMonitor:RecordInputLatency(latencyMs)
    if not self.enabled then
        return
    end

    table.insert(self.metrics.inputLatency, latencyMs)

    if #self.metrics.inputLatency > self.sampleSize then
        table.remove(self.metrics.inputLatency, 1)
    end

    self.metrics.totalInputPolls = self.metrics.totalInputPolls + 1
end

-- Record error occurrence
function PerformanceMonitor:RecordError(errorType, details)
    if not self.enabled then
        return
    end

    self.metrics.totalErrors = self.metrics.totalErrors + 1

    if Config:Get("debugMode") then
        print(string.format("[G923Mod] Performance error recorded: %s - %s",
                           errorType, details or "No details"))
    end
end

-- Update rolling averages
function PerformanceMonitor:UpdateAverages()
    -- Frame time average
    if #self.metrics.frameTime > 0 then
        local sum = 0
        for _, frameTime in ipairs(self.metrics.frameTime) do
            sum = sum + frameTime
        end
        self.metrics.avgFrameTime = sum / #self.metrics.frameTime
    end

    -- Input latency average
    if #self.metrics.inputLatency > 0 then
        local sum = 0
        for _, latency in ipairs(self.metrics.inputLatency) do
            sum = sum + latency
        end
        self.metrics.avgInputLatency = sum / #self.metrics.inputLatency
    end

    -- Memory usage average
    if #self.metrics.memoryUsage > 0 then
        local sum = 0
        for _, memory in ipairs(self.metrics.memoryUsage) do
            sum = sum + memory
        end
        self.metrics.avgMemoryUsage = sum / #self.metrics.memoryUsage
    end

    -- CPU usage average
    if #self.metrics.cpuUsage > 0 then
        local sum = 0
        for _, cpu in ipairs(self.metrics.cpuUsage) do
            sum = sum + cpu
        end
        self.metrics.avgCpuUsage = sum / #self.metrics.cpuUsage
    end
end

-- Check performance and apply optimizations
function PerformanceMonitor:CheckPerformanceAndOptimize()
    if not self.optimization.adaptivePolling and not self.optimization.dynamicQuality then
        return
    end

    local performanceIssues = self:DetectPerformanceIssues()

    if performanceIssues.critical then
        self:ApplyEmergencyOptimizations()
    elseif performanceIssues.warning then
        self:ApplyStandardOptimizations()
    else
        self:RelaxOptimizations()
    end
end

-- Detect performance issues
function PerformanceMonitor:DetectPerformanceIssues()
    local issues = {
        warning = false,
        critical = false,
        details = {}
    }

    -- Check frame time
    if self.metrics.avgFrameTime > self.targets.maxFrameTime then
        issues.critical = true
        table.insert(issues.details, "Critical frame time exceeded")
    elseif self.metrics.avgFrameTime > self.targets.warnFrameTime then
        issues.warning = true
        table.insert(issues.details, "Frame time warning threshold exceeded")
    end

    -- Check input latency
    if self.metrics.avgInputLatency > self.targets.maxInputLatency then
        issues.critical = true
        table.insert(issues.details, "Critical input latency exceeded")
    elseif self.metrics.avgInputLatency > self.targets.warnInputLatency then
        issues.warning = true
        table.insert(issues.details, "Input latency warning threshold exceeded")
    end

    -- Check memory usage
    if self.metrics.avgMemoryUsage > self.targets.maxMemoryUsage then
        issues.critical = true
        table.insert(issues.details, "Critical memory usage exceeded")
    elseif self.metrics.avgMemoryUsage > self.targets.warnMemoryUsage then
        issues.warning = true
        table.insert(issues.details, "Memory usage warning threshold exceeded")
    end

    -- Check CPU usage
    if self.metrics.avgCpuUsage > self.targets.maxCpuUsage then
        issues.critical = true
        table.insert(issues.details, "Critical CPU usage exceeded")
    elseif self.metrics.avgCpuUsage > self.targets.warnCpuUsage then
        issues.warning = true
        table.insert(issues.details, "CPU usage warning threshold exceeded")
    end

    return issues
end

-- Apply emergency optimizations
function PerformanceMonitor:ApplyEmergencyOptimizations()
    if self.optimization.emergencyMode then
        return -- Already in emergency mode
    end

    print("[G923Mod] PERFORMANCE CRITICAL - Applying emergency optimizations")

    self.optimization.emergencyMode = true
    self.optimization.level = 5

    -- Reduce polling frequency dramatically
    Config:Set("inputPollingRate", 15) -- Reduce to 15Hz

    -- Disable force feedback temporarily
    Config:Set("forceFeedbackEnabled", false)

    -- Disable debug output
    Config:Set("showInputValues", false)
    Config:Set("showVehicleInfo", false)

    -- Force garbage collection
    collectgarbage("collect")

    print("[G923Mod] Emergency optimizations applied - functionality reduced")
end

-- Apply standard optimizations
function PerformanceMonitor:ApplyStandardOptimizations()
    if self.optimization.level >= 3 then
        return -- Already optimized
    end

    print("[G923Mod] Performance warning - Applying standard optimizations")

    self.optimization.level = 3

    -- Reduce polling frequency moderately
    local currentRate = Config:Get("inputPollingRate") or 60
    local newRate = math.max(30, currentRate * 0.8)
    Config:Set("inputPollingRate", newRate)

    -- Reduce force feedback quality
    local currentStrength = Config:Get("forceFeedbackStrength") or 0.8
    Config:Set("forceFeedbackStrength", currentStrength * 0.8)

    -- Reduce debug output frequency
    if Config:Get("debugMode") then
        print("[G923Mod] Reducing debug output frequency due to performance")
    end

    print(string.format("[G923Mod] Standard optimizations applied - polling rate: %.0fHz", newRate))
end

-- Relax optimizations when performance is good
function PerformanceMonitor:RelaxOptimizations()
    if self.optimization.level <= 1 then
        return -- Already at normal performance level
    end

    -- Gradually restore full functionality
    if self.optimization.emergencyMode then
        print("[G923Mod] Performance improved - Exiting emergency mode")
        self.optimization.emergencyMode = false

        -- Restore force feedback if it was enabled originally
        Config:Set("forceFeedbackEnabled", true)
    end

    self.optimization.level = math.max(0, self.optimization.level - 1)

    -- Gradually increase polling rate back to normal
    local targetRate = 60 -- Default target
    local currentRate = Config:Get("inputPollingRate") or 30
    local newRate = math.min(targetRate, currentRate * 1.1)
    Config:Set("inputPollingRate", newRate)

    -- Restore force feedback strength
    local targetStrength = 0.8 -- Default target
    local currentStrength = Config:Get("forceFeedbackStrength") or 0.6
    local newStrength = math.min(targetStrength, currentStrength * 1.05)
    Config:Set("forceFeedbackStrength", newStrength)
end

-- Get current performance status
function PerformanceMonitor:GetStatus()
    if not self.enabled then
        return {
            enabled = false,
            message = "Performance monitoring disabled"
        }
    end

    local uptime = os.clock() - self.startTime

    return {
        enabled = true,
        uptime = uptime,

        frameTime = {
            current = self.metrics.avgFrameTime,
            target = self.targets.maxFrameTime,
            status = self.metrics.avgFrameTime <= self.targets.warnFrameTime and "Good" or
                    self.metrics.avgFrameTime <= self.targets.maxFrameTime and "Warning" or "Critical"
        },

        inputLatency = {
            current = self.metrics.avgInputLatency,
            target = self.targets.maxInputLatency,
            status = self.metrics.avgInputLatency <= self.targets.warnInputLatency and "Good" or
                    self.metrics.avgInputLatency <= self.targets.maxInputLatency and "Warning" or "Critical"
        },

        memoryUsage = {
            current = self.metrics.avgMemoryUsage,
            target = self.targets.maxMemoryUsage,
            status = self.metrics.avgMemoryUsage <= self.targets.warnMemoryUsage and "Good" or
                    self.metrics.avgMemoryUsage <= self.targets.maxMemoryUsage and "Warning" or "Critical"
        },

        optimization = {
            level = self.optimization.level,
            emergencyMode = self.optimization.emergencyMode,
            adaptivePolling = self.optimization.adaptivePolling,
            dynamicQuality = self.optimization.dynamicQuality
        },

        counters = {
            totalFrames = self.metrics.totalFrames,
            totalInputPolls = self.metrics.totalInputPolls,
            totalErrors = self.metrics.totalErrors,
            errorRate = self.metrics.totalInputPolls > 0 and
                       (self.metrics.totalErrors / self.metrics.totalInputPolls) or 0
        }
    }
end

-- Report performance status to console
function PerformanceMonitor:ReportPerformanceStatus()
    local status = self:GetStatus()

    print("[G923Mod] === Performance Status ===")
    print(string.format("  Uptime: %.1f seconds", status.uptime))
    print(string.format("  Frame Time: %.2fms (%s)", status.frameTime.current, status.frameTime.status))
    print(string.format("  Input Latency: %.2fms (%s)", status.inputLatency.current, status.inputLatency.status))
    print(string.format("  Memory Usage: %.1fMB (%s)", status.memoryUsage.current, status.memoryUsage.status))
    print(string.format("  Optimization Level: %d", status.optimization.level))

    if status.optimization.emergencyMode then
        print("  ⚠️  EMERGENCY MODE ACTIVE")
    end

    print(string.format("  Total Frames: %d, Input Polls: %d, Errors: %d",
          status.counters.totalFrames, status.counters.totalInputPolls, status.counters.totalErrors))
end

-- Shutdown performance monitor
function PerformanceMonitor:Shutdown()
    if not self.enabled then
        return
    end

    print("[G923Mod] Shutting down performance monitor...")

    -- Final performance report
    local status = self:GetStatus()
    print(string.format("[G923Mod] Final stats - Uptime: %.1fs, Frames: %d, Errors: %d",
          status.uptime, status.counters.totalFrames, status.counters.totalErrors))

    self.enabled = false
end

return PerformanceMonitor
