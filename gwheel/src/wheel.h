#pragma once

#include <cstdint>

namespace gwheel::wheel
{
    // Controller-axis / button / POV snapshot, published each pump tick.
    struct Snapshot
    {
        float    steer    = 0.f; // -1..+1
        float    throttle = 0.f; //  0..1
        float    brake    = 0.f; //  0..1
        float    clutch   = 0.f; //  0..1
        uint32_t buttons  = 0;   // bit per button, low 32
        uint16_t pov      = 0xFFFF;
        bool     connected = false;
    };

    struct Caps
    {
        uint16_t vid = 0;
        uint16_t pid = 0;
        char     productName[256] = {};
        bool     hasFFB = false;
        int      operatingRangeDeg = 0;
        int      sdkMajor = 0;
        int      sdkMinor = 0;
        int      sdkBuild = 0;
    };

    bool Init();            // verify SDK version + schedule deferred LogiSteeringInitialize
    void Shutdown();
    bool IsReady();
    void Pump();            // LogiUpdate + publish snapshot; driven by plugin pump thread
    Snapshot CurrentSnapshot();
    const Caps& GetCaps();

    // FFB. Constant takes -1..+1 (sign is direction); others are 0..1.
    // Percentages -100..+100 are derived from the float inputs.
    void PlayConstant(float magnitude);
    void StopConstant();
    void PlayDamper(float coefficient);
    void StopDamper();
    void PlaySpring(float coefficient);
    void StopSpring();
    void SetGlobalStrength(float mul);   // 0..1 multiplier applied to all effects
    void StopAll();
}
