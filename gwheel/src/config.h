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
        bool        clutchAsBrake = false;
        std::string responseCurve = "default"; // default | subdued | sharp
    };

    struct Ffb
    {
        bool    enabled = true;
        bool    debugLogging = false;
        int32_t torquePct = 100;
        float   stationaryThresholdMps  = 0.5f;
        int32_t yawFeedbackPct          = 50;
        int32_t activeTorqueStrengthPct = 100;
    };

    struct FlightYoke
    {
        bool    enabled = true;
        float   pitchSpringStrength = 0.8f;
        float   pitchDamperStrength = 0.5f;
        int32_t pitchAction = 0; // 0 = Centered Only, 1 = Throttle/Brake, 2 = Custom
    };

    struct RudderPedals
    {
        bool enabled = true;
        bool leftToeBrakeInvert = false;
        bool rightToeBrakeInvert = false;
    };

    struct PerVehicle
    {
        float steeringMultiplier = 1.0f;
        int32_t responseDelayMs = 20;
    };

    struct Handshake
    {
        bool playOnStart = true;
    };

    struct Led
    {
        bool enabled = true;
        bool visualizerWhileMusic = true;
    };

    struct Music
    {
        std::string processName;
    };

    struct Config
    {
        int32_t      version = 5;
        Input        input;
        Ffb          ffb;
        FlightYoke   yoke;
        RudderPedals rudder;
        Handshake    handshake;
        Led          led;
        Music        music;
        PerVehicle   car        = { 1.0f, 20 };
        PerVehicle   motorcycle = { 1.2f, 10 };
        PerVehicle   truck      = { 0.8f, 40 };
        PerVehicle   van        = { 0.9f, 30 };

        static constexpr size_t kBindingCount = 20;
        std::array<int32_t, kBindingCount> bindings{};
    };

    Config Current();
    void Load();
    std::string ReadAsJson();

    void SetInputEnabled(bool v);
    void SetClutchAsBrake(bool v);
    void SetResponseCurve(std::string_view v);

    void SetFfbEnabled(bool v);
    void SetFfbDebugLogging(bool v);
    void SetFfbTorquePct(int32_t v);

    void SetStationaryThresholdMps(float v);
    void SetYawFeedbackPct(int32_t v);
    void SetActiveTorqueStrengthPct(int32_t v);

    // Flight Yoke & Rudder Pedals
    void SetPitchSpringStrength(float v);
    void SetPitchDamperStrength(float v);
    void SetPitchAction(int32_t v);
    void SetLeftToeBrakeInvert(bool v);
    void SetRightToeBrakeInvert(bool v);

    void SetHandshakePlayOnStart(bool v);
    void SetLedEnabled(bool v);
    void SetLedVisualizerWhileMusic(bool v);
    void SetMusicProcessName(std::string_view v);
    void SetInputBinding(int32_t inputId, int32_t action);
}
