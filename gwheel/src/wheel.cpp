#include "wheel.h"
#include "ffb_backend.h"
#include "moza_backend.h"
#include "dinput_backend.h"
#include "dinput_manager.h"
#include "logging.h"
#include "device_table.h"
#include "config.h"
#include "sources.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

namespace gwheel::wheel
{
    namespace
    {
        struct State
        {
            std::atomic<bool>  ready{false};
            std::atomic<float> globalStrength{1.0f};

            std::mutex         snapMtx;
            Snapshot           snap{};

            std::mutex         capsMtx;
            Caps               caps{};

            std::atomic<bool>  handshakeFired{false};
            std::atomic<bool>  handshakeActive{false};

            std::unique_ptr<FFBBackend> backend;
        };

        State& S() { static State s; return s; }

        HWND FindOwnGameWindow()
        {
            const DWORD myPid = GetCurrentProcessId();
            struct EnumCtx
            {
                DWORD targetPid;
                HWND  found;
            } ctx = { myPid, nullptr };

            EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
                auto* c = reinterpret_cast<EnumCtx*>(lp);
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid != c->targetPid) return TRUE;
                if (!IsWindowVisible(hwnd)) return TRUE;
                if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
                c->found = hwnd;
                return FALSE;
            }, reinterpret_cast<LPARAM>(&ctx));

            return ctx.found;
        }

        std::atomic<float> s_lastConstant{0.f};
        std::atomic<float> s_lastDamper{0.f};
        std::atomic<float> s_lastSpring{0.f};
        std::atomic<float> s_surfaceBaselineMag{0.f};
    }

    bool Init()
    {
        auto& st = S();
        HWND hwnd = FindOwnGameWindow();

        DInputManager::Instance().Init(hwnd);

        bool isMoza = false;
        const auto& devices = DInputManager::Instance().GetConnectedDevices();
        for (const auto& d : devices)
        {
            if (d.vid == 0x346E || d.vid == 0x0483 || d.isFlightYoke)
            {
                isMoza = true;
                break;
            }
        }

        if (isMoza || DInputManager::Instance().HasFlightYoke())
        {
            log::Info("[gwheel] wheel::Init: Initializing MozaBackend for flight yoke / hardware.");
            st.backend = std::make_unique<MozaBackend>();
        }
        else
        {
            log::Info("[gwheel] wheel::Init: Initializing DInputBackend for generic / wheel hardware.");
            st.backend = std::make_unique<DInputBackend>();
        }

        if (st.backend && st.backend->Init(hwnd))
        {
            st.ready.store(true, std::memory_order_release);
            const auto& bCaps = st.backend->GetCaps();
            std::lock_guard<std::mutex> lock(st.capsMtx);
            st.caps.vid = bCaps.vid;
            st.caps.pid = bCaps.pid;
            st.caps.hasFFB = bCaps.hasFFB;
            st.caps.operatingRangeDeg = bCaps.operatingRangeDeg;
            strncpy(st.caps.productName, bCaps.productName, sizeof(st.caps.productName) - 1);
            log::InfoF("[gwheel] wheel::Init: Bound backend '%s'", bCaps.productName);
            return true;
        }

        st.ready.store(false, std::memory_order_release);
        return false;
    }

    void Shutdown()
    {
        auto& st = S();
        st.ready.store(false, std::memory_order_release);
        if (st.backend)
        {
            st.backend->Shutdown();
            st.backend.reset();
        }
        DInputManager::Instance().Shutdown();
    }

    bool IsReady()
    {
        return S().ready.load(std::memory_order_acquire);
    }

    void Pump()
    {
        auto& st = S();
        DInputManager::Instance().Poll();
        Snapshot snap = DInputManager::Instance().GetSnapshot();

        {
            std::lock_guard<std::mutex> lock(st.snapMtx);
            st.snap = snap;
        }

        // Publish to hardware-agnostic sources seam
        sources::Frame frame{};
        frame.axes.steer    = snap.steer;
        frame.axes.throttle = snap.throttle;
        frame.axes.brake    = snap.brake;
        frame.axes.clutch   = snap.clutch;
        frame.digital.buttons = snap.buttons;
        frame.digital.pov     = snap.pov;
        frame.connected       = snap.connected;
        sources::Publish(frame);

    }

    Snapshot CurrentSnapshot()
    {
        auto& st = S();
        std::lock_guard<std::mutex> lock(st.snapMtx);
        return st.snap;
    }

    const Caps& GetCaps()
    {
        auto& st = S();
        std::lock_guard<std::mutex> lock(st.capsMtx);
        return st.caps;
    }

    void PlayConstant(float magnitude)
    {
        auto& st = S();
        s_lastConstant.store(magnitude, std::memory_order_relaxed);
        if (st.backend) st.backend->PlayConstant(magnitude);
    }

    void StopConstant()
    {
        auto& st = S();
        s_lastConstant.store(0.f, std::memory_order_relaxed);
        if (st.backend) st.backend->StopConstant();
    }

    void PlayDamper(float coefficient)
    {
        auto& st = S();
        s_lastDamper.store(coefficient, std::memory_order_relaxed);
        if (st.backend) st.backend->PlayDamper(coefficient);
    }

    void StopDamper()
    {
        auto& st = S();
        s_lastDamper.store(0.f, std::memory_order_relaxed);
        if (st.backend) st.backend->StopDamper();
    }

    void PlaySpring(float coefficient)
    {
        auto& st = S();
        s_lastSpring.store(coefficient, std::memory_order_relaxed);
        if (st.backend) st.backend->PlaySpring(coefficient);
    }

    void StopSpring()
    {
        auto& st = S();
        s_lastSpring.store(0.f, std::memory_order_relaxed);
        if (st.backend) st.backend->StopSpring();
    }

    void PlayRoadSurface(float magnitude, int periodMs)
    {
        auto& st = S();
        if (st.backend) st.backend->PlaySurface(magnitude, periodMs);
    }

    void StopRoadSurface()
    {
        auto& st = S();
        if (st.backend) st.backend->StopSurface();
    }

    void PlayCarAirborne()
    {
        StopConstant();
        StopSpring();
        PlayDamper(0.05f);
    }

    void StopCarAirborne()
    {
    }

    void SetGlobalStrength(float mul)
    {
        auto& st = S();
        st.globalStrength.store(mul, std::memory_order_release);
        if (st.backend) st.backend->SetGlobalStrength(mul);
    }

    void StopAll()
    {
        auto& st = S();
        if (st.backend) st.backend->StopAll();
    }

    void PlayLeds(float level)
    {
        auto& st = S();
        if (st.backend) st.backend->PlayLeds(level);
    }

    void ClearLeds()
    {
        auto& st = S();
        if (st.backend) st.backend->ClearLeds();
    }

    bool IsHandshakeActive()
    {
        return S().handshakeActive.load(std::memory_order_acquire);
    }

    void TriggerJolt(float lateralKick, int durationMs)
    {
        auto& st = S();
        if (st.backend) st.backend->TriggerJolt(lateralKick, durationMs);
    }

    void SetSurfaceBaselineMag(float mag)
    {
        s_surfaceBaselineMag.store(std::clamp(mag, 0.f, 1.f), std::memory_order_relaxed);
    }

    void UpdateCenteringSpring(float absSpeedMps,
                                float angVelMagRad,
                                float suspensionActivity,
                                float lateralVelocityMps,
                                float steer,
                                float throttle,
                                float brake,
                                bool  isReversing,
                                bool  isOnGround,
                                bool  enabled,
                                float stationaryMps,
                                float cruiseMps,
                                float centeringBaseline,
                                int   yawFeedbackPct,
                                float yawRef,
                                int   activeTorqueStrengthPct,
                                bool  debugLog)
    {
        (void)debugLog;
        auto& st = S();
        if (!st.ready.load(std::memory_order_acquire) || !enabled)
        {
            StopAll();
            return;
        }

        if (!isOnGround)
        {
            PlayCarAirborne();
            return;
        }

        if (absSpeedMps < stationaryMps)
        {
            StopConstant();
            StopSpring();
            PlayDamper(0.05f);
            return;
        }

        const float speedRatio = cruiseMps > 0.001f ? (absSpeedMps / cruiseMps) : 0.f;
        const float speedSq    = std::clamp(speedRatio * speedRatio, 0.f, 1.2f);
        const float yawRatio   = yawRef > 0.001f ? (angVelMagRad / yawRef) : 0.f;
        const float yawRamp    = std::clamp(yawRatio, 0.f, 1.f);
        const float gripFactor = (yawRatio < 1.f) ? 1.f : std::exp(-2.f * (yawRatio - 1.f));
        const float loadFactor = std::clamp((angVelMagRad * absSpeedMps) / (std::max(yawRef * cruiseMps, 0.001f)), 0.f, 1.0f);
        const float weight     = 1.f + 0.2f * brake - 0.05f * throttle;

        const float absSteer   = std::abs(steer);
        const float steer4     = absSteer * absSteer * absSteer * absSteer;
        const float steerShape = std::sqrt(absSteer) * (1.f - steer4);
        const float reverseMul = isReversing ? 0.4f : 1.f;

        // Centering spring: scaled moderately so high speed remains agile and easy to steer
        const float springCoef = std::clamp(
            (centeringBaseline * speedSq * 0.45f * gripFactor + yawRamp * (static_cast<float>(yawFeedbackPct) / 100.f) * 0.3f),
            0.f, 0.50f) * reverseMul;

        const float signSteer = (steer > 0.001f) ? 1.f : ((steer < -0.001f) ? -1.f : 0.f);
        // Active restoring counter-steer torque
        float activeForce = signSteer * steerShape * loadFactor * gripFactor
                            * weight * (static_cast<float>(activeTorqueStrengthPct) / 100.f) * 0.6f * reverseMul;

        if (gripFactor < 0.8f && std::abs(lateralVelocityMps) > 0.5f)
        {
            const float counterSign = (lateralVelocityMps > 0.f) ? -1.f : 1.f;
            activeForce += counterSign * (1.f - gripFactor) * 0.2f;
        }

        const float damperCoef = std::clamp(speedSq * 0.15f, 0.f, 0.20f);

        PlayConstant(activeForce);
        PlaySpring(springCoef);
        PlayDamper(damperCoef);

        const float surfaceMag = std::max(suspensionActivity, s_surfaceBaselineMag.load(std::memory_order_relaxed));
        if (surfaceMag > 0.02f)
        {
            PlayRoadSurface(surfaceMag, 40);
        }
        else
        {
            StopRoadSurface();
        }
    }
}
