#include "ffb.h"
#include "wheel_device.h"
#include "logging.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace gwheel::ffb
{
    namespace
    {
        struct State
        {
            IDirectInputEffect* constant = nullptr;
            IDirectInputEffect* damper   = nullptr;
            IDirectInputEffect* texture  = nullptr;

            std::atomic<bool>  ready{false};
            std::atomic<float> strength{0.8f}; // default matches config ffb.strengthPct = 80

            // One axis — steering. The game only drives forces on the X axis.
            DWORD axes[1] = { DIJOFS_X };
            LONG  directions[1] = { 0 };
        };

        State& S()
        {
            static State s;
            return s;
        }

        float Scale(float v)
        {
            const float s = std::clamp(S().strength.load(std::memory_order_relaxed), 0.f, 1.f);
            return v * s;
        }

        LONG ToDiMag(float v)
        {
            // DI magnitude range: [-10000, 10000].
            const float clamped = std::clamp(v, -1.f, 1.f);
            return static_cast<LONG>(clamped * 10000.f);
        }

        DWORD ToDiPeriod(float hz)
        {
            // DI period: microseconds per cycle.
            if (hz <= 0.1f) hz = 0.1f;
            return static_cast<DWORD>(1'000'000.0f / hz);
        }

        IDirectInputEffect* CreateConstantEffect(IDirectInputDevice8W* dev)
        {
            auto& st = S();
            DICONSTANTFORCE cf{};
            cf.lMagnitude = 0;

            DIEFFECT eff{};
            eff.dwSize = sizeof(eff);
            eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
            eff.dwDuration = INFINITE;
            eff.dwSamplePeriod = 0;
            eff.dwGain = DI_FFNOMINALMAX;
            eff.dwTriggerButton = DIEB_NOTRIGGER;
            eff.dwTriggerRepeatInterval = 0;
            eff.cAxes = 1;
            eff.rgdwAxes = st.axes;
            eff.rglDirection = st.directions;
            eff.lpEnvelope = nullptr;
            eff.cbTypeSpecificParams = sizeof(cf);
            eff.lpvTypeSpecificParams = &cf;
            eff.dwStartDelay = 0;

            IDirectInputEffect* out = nullptr;
            HRESULT hr = dev->CreateEffect(GUID_ConstantForce, &eff, &out, nullptr);
            if (FAILED(hr))
            {
                log::WarnF("[gwheel] CreateEffect(constant) failed: %s", log::HresultName(hr));
                return nullptr;
            }
            return out;
        }

        IDirectInputEffect* CreateDamperEffect(IDirectInputDevice8W* dev)
        {
            auto& st = S();
            DICONDITION cond{};
            cond.lOffset = 0;
            cond.lPositiveCoefficient = 0;
            cond.lNegativeCoefficient = 0;
            cond.dwPositiveSaturation = DI_FFNOMINALMAX;
            cond.dwNegativeSaturation = DI_FFNOMINALMAX;
            cond.lDeadBand = 0;

            DIEFFECT eff{};
            eff.dwSize = sizeof(eff);
            eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
            eff.dwDuration = INFINITE;
            eff.dwGain = DI_FFNOMINALMAX;
            eff.dwTriggerButton = DIEB_NOTRIGGER;
            eff.cAxes = 1;
            eff.rgdwAxes = st.axes;
            eff.rglDirection = st.directions;
            eff.cbTypeSpecificParams = sizeof(cond);
            eff.lpvTypeSpecificParams = &cond;

            IDirectInputEffect* out = nullptr;
            HRESULT hr = dev->CreateEffect(GUID_Damper, &eff, &out, nullptr);
            if (FAILED(hr))
            {
                log::WarnF("[gwheel] CreateEffect(damper) failed: %s", log::HresultName(hr));
                return nullptr;
            }
            return out;
        }

        IDirectInputEffect* CreateSineEffect(IDirectInputDevice8W* dev)
        {
            auto& st = S();
            DIPERIODIC periodic{};
            periodic.dwMagnitude = 0;
            periodic.lOffset = 0;
            periodic.dwPhase = 0;
            periodic.dwPeriod = ToDiPeriod(40.f); // placeholder 40 Hz

            DIEFFECT eff{};
            eff.dwSize = sizeof(eff);
            eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
            eff.dwDuration = INFINITE;
            eff.dwGain = DI_FFNOMINALMAX;
            eff.dwTriggerButton = DIEB_NOTRIGGER;
            eff.cAxes = 1;
            eff.rgdwAxes = st.axes;
            eff.rglDirection = st.directions;
            eff.cbTypeSpecificParams = sizeof(periodic);
            eff.lpvTypeSpecificParams = &periodic;

            IDirectInputEffect* out = nullptr;
            HRESULT hr = dev->CreateEffect(GUID_Sine, &eff, &out, nullptr);
            if (FAILED(hr))
            {
                log::WarnF("[gwheel] CreateEffect(sine) failed: %s", log::HresultName(hr));
                return nullptr;
            }
            return out;
        }
    }

    bool Init()
    {
        auto* raw = device::GetRawDevice();
        if (!raw)
        {
            log::Info("[gwheel] FFB: no device, disabling effects");
            return false;
        }
        const auto& caps = device::GetCaps();
        if (!caps.ffb_runtime)
        {
            log::Info("[gwheel] FFB: device has no DIDC_FORCEFEEDBACK, operating input-only");
            return false;
        }

        auto* dev = static_cast<IDirectInputDevice8W*>(raw);
        auto& st = S();
        st.constant = CreateConstantEffect(dev);
        st.damper   = CreateDamperEffect(dev);
        st.texture  = CreateSineEffect(dev);

        if (!st.constant && !st.damper && !st.texture)
        {
            log::Warn("[gwheel] FFB: no effects could be created — operating input-only");
            return false;
        }

        st.ready.store(true, std::memory_order_release);
        log::InfoF("[gwheel] FFB ready (constant=%s damper=%s sine=%s)",
                   st.constant ? "yes" : "no",
                   st.damper   ? "yes" : "no",
                   st.texture  ? "yes" : "no");
        return true;
    }

    void Shutdown()
    {
        auto& st = S();
        st.ready.store(false, std::memory_order_release);
        auto release = [](IDirectInputEffect*& e) {
            if (e) { e->Stop(); e->Release(); e = nullptr; }
        };
        release(st.constant);
        release(st.damper);
        release(st.texture);
    }

    bool IsReady() { return S().ready.load(std::memory_order_acquire); }

    void StopAll()
    {
        auto& st = S();
        if (!st.ready.load()) return;
        if (st.constant) st.constant->Stop();
        if (st.damper)   st.damper->Stop();
        if (st.texture)  st.texture->Stop();
    }

    void PlayConstant(float magnitude, uint32_t duration_ms)
    {
        auto& st = S();
        if (!st.ready.load() || !st.constant) return;

        DICONSTANTFORCE cf{};
        cf.lMagnitude = ToDiMag(Scale(magnitude));

        DIEFFECT eff{};
        eff.dwSize = sizeof(eff);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.dwDuration = static_cast<DWORD>(duration_ms) * 1000u;
        eff.cbTypeSpecificParams = sizeof(cf);
        eff.lpvTypeSpecificParams = &cf;

        HRESULT hr = st.constant->SetParameters(&eff,
            DIEP_DURATION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] constant effect SetParameters failed: %s", log::HresultName(hr));
        }
    }

    void SetDamper(float coefficient)
    {
        auto& st = S();
        if (!st.ready.load() || !st.damper) return;

        const LONG coeff = static_cast<LONG>(std::clamp(Scale(coefficient), 0.f, 1.f) * 10000.f);

        DICONDITION cond{};
        cond.lPositiveCoefficient = coeff;
        cond.lNegativeCoefficient = coeff;
        cond.dwPositiveSaturation = DI_FFNOMINALMAX;
        cond.dwNegativeSaturation = DI_FFNOMINALMAX;

        DIEFFECT eff{};
        eff.dwSize = sizeof(eff);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cbTypeSpecificParams = sizeof(cond);
        eff.lpvTypeSpecificParams = &cond;

        DWORD flags = DIEP_TYPESPECIFICPARAMS;
        flags |= coeff > 0 ? DIEP_START : DIEP_NODOWNLOAD;

        HRESULT hr = st.damper->SetParameters(&eff, flags);
        if (coeff == 0) st.damper->Stop();
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] damper SetParameters failed: %s", log::HresultName(hr));
        }
    }

    void PlayTexture(float frequency_hz, float magnitude)
    {
        auto& st = S();
        if (!st.ready.load() || !st.texture) return;

        const float scaled = Scale(magnitude);
        if (scaled <= 0.f)
        {
            st.texture->Stop();
            return;
        }

        DIPERIODIC periodic{};
        periodic.dwMagnitude = static_cast<DWORD>(std::clamp(scaled, 0.f, 1.f) * 10000.f);
        periodic.dwPeriod = ToDiPeriod(frequency_hz);

        DIEFFECT eff{};
        eff.dwSize = sizeof(eff);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cbTypeSpecificParams = sizeof(periodic);
        eff.lpvTypeSpecificParams = &periodic;

        HRESULT hr = st.texture->SetParameters(&eff,
            DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] texture SetParameters failed: %s", log::HresultName(hr));
        }
    }

    void SetGlobalStrength(float scalar)
    {
        S().strength.store(std::clamp(scalar, 0.f, 1.f), std::memory_order_relaxed);
    }
}
