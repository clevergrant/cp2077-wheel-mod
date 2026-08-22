#include "dinput_backend.h"
#include "dinput_manager.h"
#include "logging.h"
#include <algorithm>
#include <cmath>

namespace gwheel
{
    DInputBackend::DInputBackend() = default;

    DInputBackend::~DInputBackend()
    {
        Shutdown();
    }

    bool DInputBackend::Init(HWND hwnd)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_ready) return true;

        if (!DInputManager::Instance().Init(hwnd))
        {
            return false;
        }

        LPDIRECTINPUTDEVICE8 dev = DInputManager::Instance().GetPrimaryDevice();
        if (!dev)
        {
            log::Warn("[gwheel] DInputBackend: No primary DirectInput device available for FFB.");
            return false;
        }

        m_caps.vid = 0;
        m_caps.pid = 0;
        m_caps.hasFFB = true;
        m_caps.isFlightYoke = DInputManager::Instance().HasFlightYoke();
        m_caps.operatingRangeDeg = m_caps.isFlightYoke ? 180 : 900;
        std::snprintf(m_caps.productName, sizeof(m_caps.productName), "DirectInput FFB Device");

        dev->SendForceFeedbackCommand(DISFFC_SETACTUATORSON);
        dev->SendForceFeedbackCommand(DISFFC_CONTINUE);

        CreateEffects(dev);
        m_ready = true;
        log::Info("[gwheel] DInputBackend initialized successfully with FFB actuators enabled.");
        return true;
    }

    void DInputBackend::Shutdown()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        StopAll();
        ReleaseEffects();
        m_ready = false;
    }

    bool DInputBackend::IsReady() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_ready;
    }

    const BackendCaps& DInputBackend::GetCaps() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_caps;
    }

    bool DInputBackend::CreateEffects(LPDIRECTINPUTDEVICE8 dev)
    {
        if (!dev) return false;

        DWORD axes[1] = { DIJOFS_X };
        LONG direction[1] = { 0 };

        // 1. Constant Force Effect
        DICONSTANTFORCE cf{};
        cf.lMagnitude = 0;

        DIEFFECT eff{};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.dwDuration = INFINITE;
        eff.dwSamplePeriod = 0;
        eff.dwGain = DI_FFNOMINALMAX;
        eff.dwTriggerButton = DIEB_NOTRIGGER;
        eff.dwTriggerRepeatInterval = 0;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.lpEnvelope = nullptr;
        eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        eff.lpvTypeSpecificParams = &cf;

        HRESULT hr = dev->CreateEffect(GUID_ConstantForce, &eff, &m_constantEffect, nullptr);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] DInput: Failed to create ConstantForce effect (0x%08lX)", static_cast<unsigned long>(hr));
        }

        // 2. Spring Effect (Roll)
        DICONDITION cond{};
        cond.lOffset = 0;
        cond.lPositiveCoefficient = 0;
        cond.lNegativeCoefficient = 0;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;
        cond.lDeadBand = 0;

        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;
        hr = dev->CreateEffect(GUID_Spring, &eff, &m_springEffect, nullptr);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] DInput: Failed to create Spring effect (0x%08lX)", static_cast<unsigned long>(hr));
        }

        // 3. Damper Effect (Roll)
        hr = dev->CreateEffect(GUID_Damper, &eff, &m_damperEffect, nullptr);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] DInput: Failed to create Damper effect (0x%08lX)", static_cast<unsigned long>(hr));
        }

        // 4. Periodic Sine Effect (Surface Vibration)
        DIPERIODIC per{};
        per.dwMagnitude = 0;
        per.lOffset = 0;
        per.dwPhase = 0;
        per.dwPeriod = 50 * 1000;

        eff.rgdwAxes = axes;
        eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
        eff.lpvTypeSpecificParams = &per;
        hr = dev->CreateEffect(GUID_Sine, &eff, &m_sineEffect, nullptr);
        if (FAILED(hr))
        {
            log::Debug("[gwheel] DInput: Sine effect not supported");
        }

        return true;
    }

    void DInputBackend::ReleaseEffects()
    {
        if (m_constantEffect) { m_constantEffect->Release(); m_constantEffect = nullptr; }
        if (m_damperEffect) { m_damperEffect->Release(); m_damperEffect = nullptr; }
        if (m_springEffect) { m_springEffect->Release(); m_springEffect = nullptr; }
        if (m_pitchSpringEffect) { m_pitchSpringEffect->Release(); m_pitchSpringEffect = nullptr; }
        if (m_sineEffect) { m_sineEffect->Release(); m_sineEffect = nullptr; }
        if (m_joltEffect) { m_joltEffect->Release(); m_joltEffect = nullptr; }
    }

    void DInputBackend::PlayConstant(float magnitude)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_constantEffect) return;

        float scaled = std::clamp(magnitude * m_globalStrength, -1.0f, 1.0f);
        LONG force = static_cast<LONG>(scaled * DI_FFNOMINALMAX);

        DWORD axes[1] = { DIJOFS_X };
        LONG direction[1] = { 0 };
        DICONSTANTFORCE cf{};
        cf.lMagnitude = force;

        DIEFFECT eff{};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        eff.lpvTypeSpecificParams = &cf;

        m_constantEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        m_constantEffect->Start(INFINITE, 0);
    }

    void DInputBackend::StopConstant()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_constantEffect) m_constantEffect->Stop();
    }

    void DInputBackend::PlayDamper(float coefficient)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_damperEffect) return;

        DWORD axes[1] = { DIJOFS_X };
        LONG direction[1] = { 0 };
        LONG coef = static_cast<LONG>(std::clamp(coefficient * m_globalStrength, 0.0f, 1.0f) * 10000);
        DICONDITION cond{};
        cond.lOffset = 0;
        cond.lPositiveCoefficient = coef;
        cond.lNegativeCoefficient = coef;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;

        DIEFFECT eff{};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;

        m_damperEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        m_damperEffect->Start(INFINITE, 0);
    }

    void DInputBackend::StopDamper()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_damperEffect) m_damperEffect->Stop();
    }

    void DInputBackend::PlaySpring(float coefficient)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_springEffect) return;

        DWORD axes[1] = { DIJOFS_X };
        LONG direction[1] = { 0 };
        LONG coef = static_cast<LONG>(std::clamp(coefficient * m_globalStrength, 0.0f, 1.0f) * 10000);
        DICONDITION cond{};
        cond.lOffset = 0;
        cond.lPositiveCoefficient = coef;
        cond.lNegativeCoefficient = coef;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;

        DIEFFECT eff{};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;

        m_springEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        m_springEffect->Start(INFINITE, 0);
    }

    void DInputBackend::StopSpring()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_springEffect) m_springEffect->Stop();
    }

    void DInputBackend::PlayPitchCentering(float springCoef, float damperCoef)
    {
        (void)springCoef;
        (void)damperCoef;
    }

    void DInputBackend::StopPitchCentering()
    {
    }

    void DInputBackend::PlaySurface(float magnitude, int periodMs)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_sineEffect) return;

        DWORD axes[1] = { DIJOFS_X };
        LONG direction[1] = { 0 };
        DIPERIODIC per{};
        per.dwMagnitude = static_cast<DWORD>(std::clamp(magnitude * m_globalStrength, 0.0f, 1.0f) * DI_FFNOMINALMAX);
        per.dwPeriod = static_cast<DWORD>(std::max(10, periodMs) * 1000);

        DIEFFECT eff{};
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
        eff.lpvTypeSpecificParams = &per;

        m_sineEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        m_sineEffect->Start(INFINITE, 0);
    }

    void DInputBackend::StopSurface()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_sineEffect) m_sineEffect->Stop();
    }

    void DInputBackend::TriggerJolt(float lateralKick, int durationMs)
    {
        PlayConstant(lateralKick);
        (void)durationMs;
    }

    void DInputBackend::SetGlobalStrength(float mul)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_globalStrength = std::clamp(mul, 0.0f, 1.0f);
    }

    void DInputBackend::StopAll()
    {
        StopConstant();
        StopSpring();
        StopDamper();
        StopPitchCentering();
        StopSurface();
    }
}
