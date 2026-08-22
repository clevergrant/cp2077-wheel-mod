#pragma once

#include <cstdint>
#include <string_view>
#include <windows.h>

namespace gwheel
{
    struct BackendCaps
    {
        uint16_t vid = 0;
        uint16_t pid = 0;
        char     productName[256] = {};
        bool     hasFFB = false;
        bool     isFlightYoke = false;
        int      operatingRangeDeg = 900;
        int      sdkMajor = 0;
        int      sdkMinor = 0;
        int      sdkBuild = 0;
    };

    class FFBBackend
    {
    public:
        virtual ~FFBBackend() = default;

        virtual bool Init(HWND hwnd) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsReady() const = 0;
        virtual const BackendCaps& GetCaps() const = 0;

        // Roll Axis (Steering) Force Feedback Effectors
        virtual void PlayConstant(float magnitude) = 0;
        virtual void StopConstant() = 0;
        virtual void PlayDamper(float coefficient) = 0;
        virtual void StopDamper() = 0;
        virtual void PlaySpring(float coefficient) = 0;
        virtual void StopSpring() = 0;

        // Pitch Axis (Push/Pull) Stabilization Effectors (Flight Yokes)
        virtual void PlayPitchCentering(float springCoef, float damperCoef) = 0;
        virtual void StopPitchCentering() = 0;

        // Vibration & Dynamic Overlays
        virtual void PlaySurface(float magnitude, int periodMs) = 0;
        virtual void StopSurface() = 0;
        virtual void TriggerJolt(float lateralKick, int durationMs) = 0;
        virtual void SetGlobalStrength(float mul) = 0;
        virtual void StopAll() = 0;

        // Optional Rev LEDs / visualizer
        virtual void PlayLeds(float level) { (void)level; }
        virtual void ClearLeds() {}
    };
}
