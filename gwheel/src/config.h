#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace gwheel::config
{
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
        // CP2077's steering tops out at ~90 degrees of virtual wheel rotation,
        // so 90 is a better match than a sim-racing 900. Raise via the slider
        // if you want more travel.
        int32_t rangeDeg = 90;
        int32_t centeringSpringPct = 50;
    };

    struct PerVehicle
    {
        float steeringMultiplier = 1.0f;
        int32_t responseDelayMs = 20;
    };

    struct Hello
    {
        // Play the FFB handshake (4 triplets + centering) on wheel connect.
        // Installer-level choice; persisted to config.json so it survives
        // restarts until changed.
        bool playOnStart = true;
    };

    struct Config
    {
        int32_t     version = 2;
        Input       input;
        Ffb         ffb;
        Override    override_;
        Hello       hello;
        PerVehicle  car        = { 1.0f, 20 };
        PerVehicle  motorcycle = { 1.2f, 10 };
        PerVehicle  truck      = { 0.8f, 40 };
        PerVehicle  van        = { 0.9f, 30 };

        // Per-physical-input action binding. Indexed by input_bindings::
        // PhysicalInput; value is input_bindings::Action as int32_t. Array
        // size is hardcoded at 20 to match PhysicalInput::kCount. Keep in
        // sync if we add wheel controls.
        static constexpr size_t kBindingCount = 20;
        std::array<int32_t, kBindingCount> bindings{};
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

    void SetHelloPlayOnStart(bool v);

    // Single-input binding: inputId in [0, kBindingCount), action as the
    // Action int from input_bindings.h.
    void SetInputBinding(int32_t inputId, int32_t action);
}
