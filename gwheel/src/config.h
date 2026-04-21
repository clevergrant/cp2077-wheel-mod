#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gwheel::config
{
    struct ButtonBinding
    {
        int32_t     button = -1;
        std::string action;
    };

    struct Input
    {
        bool        enabled = true;
        int32_t     steerDeadzonePct = 2;
        int32_t     throttleDeadzonePct = 2;
        int32_t     brakeDeadzonePct = 2;
        std::string responseCurve = "default"; // default | subdued | sharp
    };

    struct Ffb
    {
        bool    enabled = true;
        int32_t strengthPct = 80;
        bool    debugLogging = false;
    };

    struct Override
    {
        bool  enabled = false;
        float sensitivity = 1.0f;
        int32_t rangeDeg = 900;
        int32_t centeringSpringPct = 50;
    };

    struct PerVehicle
    {
        float steeringMultiplier = 1.0f;
        int32_t responseDelayMs = 20;
    };

    struct Config
    {
        int32_t     version = 2;
        Input       input;
        Ffb         ffb;
        Override    override_;
        PerVehicle  car        = { 1.0f, 20 };
        PerVehicle  motorcycle = { 1.2f, 10 };
        PerVehicle  truck      = { 0.8f, 40 };
        PerVehicle  van        = { 0.9f, 30 };
        std::vector<ButtonBinding> buttons;
    };

    // Read the published snapshot. Non-blocking; safe from any thread.
    Config Current();

    // Load config.json from the plugin's install dir. Falls back to defaults
    // if the file doesn't exist or fails to parse (a warning is logged in
    // that case). Idempotent; safe to call multiple times.
    void Load();

    // Serialize the current snapshot as a JSON string. Used by redscript at
    // Settings-page init time to hydrate the page with the persisted values.
    std::string ReadAsJson();

    // Per-field setters. Each one swaps the snapshot atomically and writes
    // the updated config back to disk.
    void SetInputEnabled(bool v);
    void SetSteerDeadzonePct(int32_t v);
    void SetThrottleDeadzonePct(int32_t v);
    void SetBrakeDeadzonePct(int32_t v);
    void SetResponseCurve(std::string_view v);

    void SetFfbEnabled(bool v);
    void SetFfbStrengthPct(int32_t v);
    void SetFfbDebugLogging(bool v);

    void SetOverrideEnabled(bool v);
    void SetOverrideSensitivity(float v);
    void SetOverrideRangeDeg(int32_t v);
    void SetOverrideCenteringSpringPct(int32_t v);

    // Button bindings: button index (0..31) -> action name. Empty action clears.
    void SetButtonBinding(int32_t button, std::string_view action);
    void ClearButtonBinding(int32_t button);
}
