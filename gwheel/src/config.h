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
        bool    debugLogging = true;

        // Wheel torque, pushed to G HUB via LogiSetPreferredControllerProperties
        // (overallGain). When "Apply Settings from Game" is checked in G HUB
        // (the default), G HUB greys out its own Torque slider and expects
        // the game to drive this value. When unchecked, G HUB ignores our
        // Set and uses its slider value. 100 = full, 0 = off.
        int32_t torquePct = 100;

        // Physics-model self-centering. Wheel is free at rest; spring engages
        // and active torque builds with speed². Shape + cruise speed + spring
        // baseline are derived per-car from WheeledPhysics. The knobs below
        // are the user-facing taste scalars layered on top:
        //
        // stationaryThresholdMps: below this, all centering forces are off
        //   so the wheel rests wherever the driver leaves it.
        // yawFeedbackPct: additive spring-stiffness bonus during rotation,
        //   normalised against the car's own turnRate. Pure preference.
        // activeTorqueStrengthPct: 0..100 gain on the directional push-back
        //   constant force. Peak is shaped (humped sqrt curve over deflection,
        //   lateral-accel proxy, grip-factor lightening past the yaw limit).
        float   stationaryThresholdMps  = 0.5f;
        int32_t yawFeedbackPct          = 50;
        int32_t activeTorqueStrengthPct = 100;
    };

    struct Wheel
    {
        // Linear multiplier on raw wheel position before it hits the game's
        // steer input. 1.0 = identity. Operating range is owned by G HUB
        // per-profile; the mod reads it at runtime and auto-scales FFB to
        // match, so there's no mod-side range knob.
        float steeringSensitivity = 1.0f;
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
        Wheel       wheel;
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
    void SetFfbDebugLogging(bool v);
    void SetFfbTorquePct(int32_t v);

    void SetStationaryThresholdMps(float v);
    void SetYawFeedbackPct(int32_t v);
    void SetActiveTorqueStrengthPct(int32_t v);

    void SetSteeringSensitivity(float v);

    void SetHelloPlayOnStart(bool v);

    // Single-input binding: inputId in [0, kBindingCount), action as the
    // Action int from input_bindings.h.
    void SetInputBinding(int32_t inputId, int32_t action);
}
