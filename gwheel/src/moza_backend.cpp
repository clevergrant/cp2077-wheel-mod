#include "moza_backend.h"
#include "dinput_manager.h"
#include "logging.h"
#include "config.h"
#include <algorithm>
#include <cstdio>

namespace gwheel
{
    MozaBackend::MozaBackend()
    {
        m_dinputBackend = std::make_unique<DInputBackend>();
    }

    MozaBackend::~MozaBackend()
    {
        Shutdown();
    }

    bool MozaBackend::TryLoadMozaNativeSdk()
    {
        m_mozaSdkModule = LoadLibraryW(L"MozaSDK.dll");
        if (!m_mozaSdkModule)
        {
            m_mozaSdkModule = LoadLibraryW(L"MozaRacingSDK.dll");
        }
        if (!m_mozaSdkModule)
        {
            m_mozaSdkModule = LoadLibraryW(L"red4ext/plugins/gwheel/MozaSDK.dll");
        }

        if (m_mozaSdkModule)
        {
            log::Info("[gwheel] MozaBackend: Native MozaSDK.dll successfully loaded.");
            m_hasNativeSdk = true;
            return true;
        }

        log::Info("[gwheel] MozaBackend: Native MozaSDK.dll not present; utilizing DirectInput FFB pipeline.");
        m_hasNativeSdk = false;
        return false;
    }

    bool MozaBackend::Init(HWND hwnd)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_ready) return true;

        TryLoadMozaNativeSdk();

        if (!m_dinputBackend->Init(hwnd))
        {
            log::Warn("[gwheel] MozaBackend: DirectInput init failed.");
            return false;
        }

        m_caps.vid = 0x346E; // MOZA VID
        m_caps.pid = 0x0020; // MOZA AY90/AY210 default
        m_caps.hasFFB = true;
        m_caps.isFlightYoke = true;
        m_caps.operatingRangeDeg = 180;
        std::snprintf(m_caps.productName, sizeof(m_caps.productName), "MOZA AY Force Feedback Yoke");

        m_ready = true;
        log::Info("[gwheel] MozaBackend initialized successfully for MOZA AY Flight Yoke.");

        return true;
    }

    void MozaBackend::Shutdown()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        StopAll();
        if (m_dinputBackend)
        {
            m_dinputBackend->Shutdown();
        }
        if (m_mozaSdkModule)
        {
            FreeLibrary(m_mozaSdkModule);
            m_mozaSdkModule = nullptr;
        }
        m_ready = false;
    }

    bool MozaBackend::IsReady() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_ready;
    }

    const BackendCaps& MozaBackend::GetCaps() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_caps;
    }

    void MozaBackend::PlayConstant(float magnitude)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->PlayConstant(magnitude * m_globalStrength);
        }
    }

    void MozaBackend::StopConstant()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend) m_dinputBackend->StopConstant();
    }

    void MozaBackend::PlayDamper(float coefficient)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->PlayDamper(coefficient * m_globalStrength);
        }
    }

    void MozaBackend::StopDamper()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend) m_dinputBackend->StopDamper();
    }

    void MozaBackend::PlaySpring(float coefficient)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->PlaySpring(coefficient * m_globalStrength);
        }
    }

    void MozaBackend::StopSpring()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend) m_dinputBackend->StopSpring();
    }

    void MozaBackend::PlayPitchCentering(float springCoef, float damperCoef)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->PlayPitchCentering(springCoef, damperCoef);
        }
    }

    void MozaBackend::StopPitchCentering()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend) m_dinputBackend->StopPitchCentering();
    }

    void MozaBackend::PlaySurface(float magnitude, int periodMs)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->PlaySurface(magnitude * m_globalStrength, periodMs);
        }
    }

    void MozaBackend::StopSurface()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend) m_dinputBackend->StopSurface();
    }

    void MozaBackend::TriggerJolt(float lateralKick, int durationMs)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->TriggerJolt(lateralKick, durationMs);
        }
    }

    void MozaBackend::SetGlobalStrength(float mul)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_globalStrength = std::clamp(mul, 0.0f, 1.0f);
        if (m_dinputBackend)
        {
            m_dinputBackend->SetGlobalStrength(m_globalStrength);
        }
    }

    void MozaBackend::StopAll()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_dinputBackend)
        {
            m_dinputBackend->StopAll();
        }
    }
}
