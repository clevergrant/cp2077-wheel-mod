#pragma once

#include "ffb_backend.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <mutex>

namespace gwheel
{
    class DInputBackend : public FFBBackend
    {
    public:
        DInputBackend();
        ~DInputBackend() override;

        bool Init(HWND hwnd) override;
        void Shutdown() override;
        bool IsReady() const override;
        const BackendCaps& GetCaps() const override;

        void PlayConstant(float magnitude) override;
        void StopConstant() override;
        void PlayDamper(float coefficient) override;
        void StopDamper() override;
        void PlaySpring(float coefficient) override;
        void StopSpring() override;

        void PlayPitchCentering(float springCoef, float damperCoef) override;
        void StopPitchCentering() override;

        void PlaySurface(float magnitude, int periodMs) override;
        void StopSurface() override;
        void TriggerJolt(float lateralKick, int durationMs) override;
        void SetGlobalStrength(float mul) override;
        void StopAll() override;

    private:
        bool CreateEffects(LPDIRECTINPUTDEVICE8 dev);
        void ReleaseEffects();

        mutable std::recursive_mutex m_mutex;
        BackendCaps m_caps = {};
        bool m_ready = false;
        float m_globalStrength = 1.0f;

        LPDIRECTINPUTEFFECT m_constantEffect = nullptr;
        LPDIRECTINPUTEFFECT m_damperEffect = nullptr;
        LPDIRECTINPUTEFFECT m_springEffect = nullptr;
        LPDIRECTINPUTEFFECT m_pitchSpringEffect = nullptr;
        LPDIRECTINPUTEFFECT m_sineEffect = nullptr;
        LPDIRECTINPUTEFFECT m_joltEffect = nullptr;
    };
}
