#pragma once

#include "ffb_backend.h"
#include "dinput_backend.h"
#include <memory>
#include <mutex>

namespace gwheel
{
    class MozaBackend : public FFBBackend
    {
    public:
        MozaBackend();
        ~MozaBackend() override;

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
        bool TryLoadMozaNativeSdk();

        mutable std::recursive_mutex m_mutex;
        BackendCaps m_caps = {};
        bool m_ready = false;
        bool m_hasNativeSdk = false;
        float m_globalStrength = 1.0f;

        // Fallback / DirectInput pipeline
        std::unique_ptr<DInputBackend> m_dinputBackend;
        HMODULE m_mozaSdkModule = nullptr;
    };
}
