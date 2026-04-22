#include "wheel.h"
#include "logging.h"
#include "device_table.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <LogitechSteeringWheelLib.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace gwheel::wheel
{
    namespace
    {
        // Normalize a DIJOYSTATE2 LONG axis. The Logi SDK for the G923
        // returns bipolar signed 16-bit values in [-32768, +32767]
        // (verified empirically 2026-04-21 via diagnostic logging).
        //
        // Steer: center=0, full-left=-32768, full-right=+32767.
        // Pedals: idle=+32767, fully-pressed=-32768.
        float NormalizePedal(LONG v)
        {
            constexpr float kMax = 32767.f;
            constexpr float kRange = 65535.f; // kMax - kMin = 32767 - (-32768)
            const float t = (kMax - static_cast<float>(v)) / kRange;
            return std::clamp(t, 0.f, 1.f);
        }

        float NormalizeSteer(LONG v)
        {
            constexpr float kMax = 32767.f;
            return std::clamp(static_cast<float>(v) / kMax, -1.f, 1.f);
        }

        struct State
        {
            std::atomic<bool>  sdkInitialized{false};
            std::atomic<bool>  ready{false};
            std::atomic<int>   index{-1};
            std::atomic<bool>  hasFFB{false};
            std::atomic<float> globalStrength{1.0f};

            std::mutex         snapMtx;
            Snapshot           snap{};

            std::mutex         capsMtx;
            Caps               caps{};

            std::atomic<uint32_t> initAttempts{0};
            std::atomic<bool>     helloFired{false};
        };

        State& S() { static State s; return s; }

        // Pick the first LOGI_DEVICE_TYPE_WHEEL index that reports connected.
        int FindWheelIndex()
        {
            for (int i = 0; i < LOGI_MAX_CONTROLLERS; ++i)
            {
                if (!LogiIsConnected(i)) continue;
                if (LogiIsDeviceConnected(i, LOGI_DEVICE_TYPE_WHEEL)) return i;
            }
            // Fall back: accept any connected Logitech-manufactured controller.
            for (int i = 0; i < LOGI_MAX_CONTROLLERS; ++i)
            {
                if (!LogiIsConnected(i)) continue;
                if (LogiIsManufacturerConnected(i, LOGI_MANUFACTURER_LOGITECH)) return i;
            }
            return -1;
        }

        // Only accept a top-level window belonging to the game's own process.
        // DirectInput (used by the SDK under the hood) rejects foreign HWNDs.
        HWND FindOwnGameWindow()
        {
            const DWORD myPid = GetCurrentProcessId();
            auto ownedByMe = [&](HWND h) -> bool {
                if (!h || !IsWindow(h)) return false;
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                return pid == myPid;
            };
            HWND h = FindWindowW(L"RED4Engine", nullptr);
            if (ownedByMe(h)) return h;
            h = FindWindowW(L"Cyberpunk 2077", nullptr);
            if (ownedByMe(h)) return h;

            struct Ctx { DWORD pid; HWND hit; } ctx{ myPid, nullptr };
            EnumWindows([](HWND w, LPARAM p) -> BOOL {
                auto* c = reinterpret_cast<Ctx*>(p);
                DWORD pid = 0;
                GetWindowThreadProcessId(w, &pid);
                if (pid != c->pid) return TRUE;
                if (!IsWindowVisible(w)) return TRUE;
                RECT r{};
                if (!GetClientRect(w, &r) || (r.right - r.left) < 400 || (r.bottom - r.top) < 300)
                    return TRUE;
                c->hit = w;
                return FALSE;
            }, reinterpret_cast<LPARAM>(&ctx));
            return ctx.hit;
        }

        void LogSdkVersion()
        {
            auto& st = S();
            int major = 0, minor = 0, build = 0;
            if (LogiSteeringGetSdkVersion(&major, &minor, &build))
            {
                std::lock_guard lk(st.capsMtx);
                st.caps.sdkMajor = major;
                st.caps.sdkMinor = minor;
                st.caps.sdkBuild = build;
                log::InfoF("[gwheel] Logitech Steering Wheel SDK v%d.%d build %d", major, minor, build);
            }
            else
            {
                log::Warn("[gwheel] LogiSteeringGetSdkVersion failed; continuing anyway");
            }
        }

        void CaptureCaps(int idx)
        {
            auto& st = S();
            Caps c{};
            {
                std::lock_guard lk(st.capsMtx);
                c = st.caps;
            }

            wchar_t friendly[256] = {};
            if (LogiGetFriendlyProductName(idx, friendly, 256))
            {
                WideCharToMultiByte(CP_UTF8, 0, friendly, -1,
                                    c.productName, sizeof(c.productName),
                                    nullptr, nullptr);
            }
            else
            {
                std::snprintf(c.productName, sizeof(c.productName),
                              "Logitech wheel (slot %d)", idx);
            }

            c.hasFFB = LogiHasForceFeedback(idx);

            int range = 0;
            if (LogiGetOperatingRange(idx, range)) c.operatingRangeDeg = range;

            // Best-effort model probe against the constants this SDK version
            // actually ships (header tops out at G920; newer wheels report via
            // the friendlier name path).
            const int probes[] = {
                LOGI_MODEL_G920,
                LOGI_MODEL_G29,
                LOGI_MODEL_G27,
                LOGI_MODEL_G25,
                LOGI_MODEL_DRIVING_FORCE_GT,
                LOGI_MODEL_DRIVING_FORCE_PRO,
                LOGI_MODEL_DRIVING_FORCE,
                LOGI_MODEL_MOMO_RACING,
                LOGI_MODEL_MOMO_FORCE,
                LOGI_MODEL_FORMULA_FORCE,
                LOGI_MODEL_FORMULA_FORCE_GP,
            };
            for (int model : probes)
            {
                if (LogiIsModelConnected(idx, model))
                {
                    log::InfoF("[gwheel] SDK model match: LOGI_MODEL id=%d", model);
                    break;
                }
            }

            {
                std::lock_guard lk(st.capsMtx);
                st.caps = c;
            }
        }

        void FireHelloPulse(int idx)
        {
            auto& st = S();
            if (st.helloFired.exchange(true)) return;
            std::thread([idx] {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(400ms);
                log::Info("[gwheel] firing hello pulse (2 triplets + center)");

                // Two triplets: R L R, L R L. Final step centers.
                // 150 BPM: quarter note = 400ms. Rhythm per measure is
                // quarter quarter quarter rest. Each beat fires a short
                // kick of force then releases for the remainder of the beat
                // so the pulse feels like a tap rather than a held push.
                constexpr int kTriplets[2][3] = {
                    { +1, -1, +1 },   // R L R
                    { -1, +1, -1 },   // L R L
                };
                constexpr auto kPulseMs   = 80ms;               // active kick
                constexpr auto kBeatMs    = 400ms;              // quarter @ 150 BPM
                constexpr auto kGapMs     = kBeatMs - kPulseMs; // silence within beat
                constexpr auto kRestMs    = 400ms;              // quarter rest between triplets
                constexpr int  kMagnitude = 45;                 // percent

                for (int t = 0; t < 2; ++t)
                {
                    for (int b = 0; b < 3; ++b)
                    {
                        LogiPlayConstantForce(idx, kTriplets[t][b] * kMagnitude);
                        std::this_thread::sleep_for(kPulseMs);
                        LogiStopConstantForce(idx);
                        std::this_thread::sleep_for(kGapMs);
                    }
                    std::this_thread::sleep_for(kRestMs);
                }

                // Final step: center the wheel with a spring + damper, hold
                // for 3s, then release all forces so the wheel is free to
                // move under the user's hand (or under game-driven FFB
                // later). The damper is what actually kills the momentum of
                // the wheel snapping back to center - spring alone lets it
                // oscillate around 0 for a while.
                LogiStopConstantForce(idx);
                const bool springOk = LogiPlaySpringForce(idx, 0, 100, 80);
                const bool damperOk = LogiPlayDamperForce(idx, 60);
                log::InfoF("[gwheel] hello centering begin (spring=%d damper=%d) - holding 3s",
                           springOk ? 1 : 0, damperOk ? 1 : 0);
                std::this_thread::sleep_for(3000ms);
                log::Info("[gwheel] hello centering end - releasing forces");
                LogiStopSpringForce(idx);
                LogiStopConstantForce(idx);
                LogiStopDamperForce(idx);
            }).detach();
        }

        bool TryInitSdk()
        {
            auto& st = S();
            if (st.sdkInitialized.load(std::memory_order_acquire)) return true;

            HWND hwnd = FindOwnGameWindow();
            const uint32_t n = st.initAttempts.fetch_add(1, std::memory_order_relaxed) + 1;
            // Log the per-attempt result only on the first try and every ~5s
            // afterwards (pump thread runs at 250 Hz so ~1250 attempts == 5s).
            const bool verbose = (n == 1) || (n % 1250 == 0);

            bool ok = false;
            if (hwnd)
            {
                ok = LogiSteeringInitializeWithWindow(false, hwnd);
                if (verbose || ok)
                    log::InfoF("[gwheel] LogiSteeringInitializeWithWindow(hwnd=0x%p) -> %s",
                               static_cast<void*>(hwnd), ok ? "true" : "false");
            }
            else
            {
                ok = LogiSteeringInitialize(false);
                if (verbose)
                    log::InfoF("[gwheel] LogiSteeringInitialize(ignoreXInput=false) -> %s (no game window yet)",
                               ok ? "true" : "false");
            }

            if (!ok)
            {
                if (verbose)
                {
                    log::WarnF("[gwheel] SDK init did not succeed (attempt %u). "
                               "Will retry each pump tick. Common causes: G HUB not running, "
                               "wheel unplugged, or SDK not permitted while another exclusive client holds it.",
                               n);
                }
                return false;
            }

            st.sdkInitialized.store(true, std::memory_order_release);
            LogSdkVersion();
            return true;
        }

        bool TryBindController()
        {
            auto& st = S();
            if (st.ready.load(std::memory_order_acquire)) return true;

            const int idx = FindWheelIndex();
            if (idx < 0)
            {
                const uint32_t n = st.initAttempts.load(std::memory_order_relaxed);
                if (n == 1 || (n % 250) == 0)
                    log::Warn("[gwheel] no Logitech wheel detected by the SDK yet - waiting");
                return false;
            }

            st.index.store(idx, std::memory_order_release);
            CaptureCaps(idx);

            const Caps snap = GetCaps();
            log::InfoF("[gwheel] wheel bound at SDK slot %d: \"%s\" (FFB=%s range=%d deg)",
                       idx,
                       snap.productName,
                       snap.hasFFB ? "yes" : "no",
                       snap.operatingRangeDeg);

            st.hasFFB.store(snap.hasFFB, std::memory_order_release);
            st.ready.store(true, std::memory_order_release);

            if (snap.hasFFB) FireHelloPulse(idx);

            return true;
        }
    }

    bool Init()
    {
        log::Info("[gwheel] wheel::Init (Logitech SDK) - SDK init is deferred to the pump thread");
        // We do NOT call LogiSteeringInitialize here. At plugin-load time the
        // game window does not exist yet and init can fail. TryInitSdk() runs
        // from the pump thread and retries until it succeeds.
        return true;
    }

    void Shutdown()
    {
        auto& st = S();
        st.ready.store(false, std::memory_order_release);
        if (st.sdkInitialized.exchange(false))
        {
            const int idx = st.index.load(std::memory_order_acquire);
            if (idx >= 0)
            {
                LogiStopSpringForce(idx);
                LogiStopDamperForce(idx);
                LogiStopConstantForce(idx);
            }
            LogiSteeringShutdown();
            log::Info("[gwheel] LogiSteeringShutdown ok");
        }
        st.index.store(-1, std::memory_order_release);
        log::Info("[gwheel] wheel::Shutdown complete");
    }

    bool IsReady() { return S().ready.load(std::memory_order_acquire); }

    const Caps& GetCaps()
    {
        auto& st = S();
        std::lock_guard lk(st.capsMtx);
        static thread_local Caps tl;
        tl = st.caps;
        return tl;
    }

    void Pump()
    {
        auto& st = S();

        if (!st.sdkInitialized.load(std::memory_order_acquire))
        {
            if (!TryInitSdk()) return;
        }

        LogiUpdate();

        if (!st.ready.load(std::memory_order_acquire))
        {
            if (!TryBindController()) return;
        }

        const int idx = st.index.load(std::memory_order_acquire);
        if (idx < 0) return;

        if (!LogiIsConnected(idx))
        {
            if (st.ready.exchange(false))
                log::Warn("[gwheel] wheel disconnected; will re-bind when it returns");
            return;
        }

        const DIJOYSTATE2* raw = LogiGetState(idx);
        if (!raw) return;

        Snapshot s;
        s.connected = true;
        s.steer     = NormalizeSteer(raw->lX);
        // Classic Logitech pedal mapping via SDK: Y=throttle, Rz=brake, Slider0=clutch.
        s.throttle  = NormalizePedal(raw->lY);
        s.brake     = NormalizePedal(raw->lRz);
        s.clutch    = NormalizePedal(raw->rglSlider[0]);
        s.pov       = static_cast<uint16_t>(raw->rgdwPOV[0] & 0xFFFF);

        uint32_t bits = 0;
        for (int i = 0; i < 32; ++i)
            if (raw->rgbButtons[i] & 0x80) bits |= (1u << i);
        s.buttons = bits;

        {
            std::lock_guard lk(st.snapMtx);
            st.snap = s;
        }
    }

    Snapshot CurrentSnapshot()
    {
        auto& st = S();
        std::lock_guard lk(st.snapMtx);
        return st.snap;
    }

    namespace
    {
        int Scale100(float v)
        {
            const float mul = std::clamp(S().globalStrength.load(std::memory_order_relaxed), 0.f, 1.f);
            const float scaled = std::clamp(v * mul, -1.f, 1.f);
            const int pct = static_cast<int>(std::lround(scaled * 100.f));
            return std::clamp(pct, -100, 100);
        }

        int ScaleUnsigned100(float v)
        {
            const float mul = std::clamp(S().globalStrength.load(std::memory_order_relaxed), 0.f, 1.f);
            const float scaled = std::clamp(v * mul, 0.f, 1.f);
            return std::clamp(static_cast<int>(std::lround(scaled * 100.f)), 0, 100);
        }
    }

    void PlayConstant(float magnitude)
    {
        auto& st = S();
        if (!st.ready.load() || !st.hasFFB.load()) return;
        const int idx = st.index.load(std::memory_order_acquire);
        const int pct = Scale100(magnitude);
        if (!LogiPlayConstantForce(idx, pct))
            log::DebugF("[gwheel] LogiPlayConstantForce(%d) returned false", pct);
        else if (log::DebugEnabled())
            log::DebugF("[gwheel] constant=%d%%", pct);
    }

    void StopConstant()
    {
        auto& st = S();
        const int idx = st.index.load(std::memory_order_acquire);
        if (idx >= 0) LogiStopConstantForce(idx);
    }

    void PlayDamper(float coefficient)
    {
        auto& st = S();
        if (!st.ready.load() || !st.hasFFB.load()) return;
        const int idx = st.index.load(std::memory_order_acquire);
        const int pct = ScaleUnsigned100(coefficient);
        if (!LogiPlayDamperForce(idx, pct))
            log::DebugF("[gwheel] LogiPlayDamperForce(%d) returned false", pct);
        else if (log::DebugEnabled())
            log::DebugF("[gwheel] damper=%d%%", pct);
    }

    void StopDamper()
    {
        auto& st = S();
        const int idx = st.index.load(std::memory_order_acquire);
        if (idx >= 0) LogiStopDamperForce(idx);
    }

    void PlaySpring(float coefficient)
    {
        auto& st = S();
        if (!st.ready.load() || !st.hasFFB.load()) return;
        const int idx = st.index.load(std::memory_order_acquire);
        const int pct = ScaleUnsigned100(coefficient);
        // offset=0 (centered), saturation=100, coefficient=pct.
        if (!LogiPlaySpringForce(idx, 0, 100, pct))
            log::DebugF("[gwheel] LogiPlaySpringForce(coef=%d) returned false", pct);
        else if (log::DebugEnabled())
            log::DebugF("[gwheel] spring=%d%%", pct);
    }

    void StopSpring()
    {
        auto& st = S();
        const int idx = st.index.load(std::memory_order_acquire);
        if (idx >= 0) LogiStopSpringForce(idx);
    }

    void SetGlobalStrength(float mul)
    {
        S().globalStrength.store(std::clamp(mul, 0.f, 1.f), std::memory_order_relaxed);
    }

    void StopAll()
    {
        auto& st = S();
        const int idx = st.index.load(std::memory_order_acquire);
        if (idx < 0) return;
        LogiStopConstantForce(idx);
        LogiStopDamperForce(idx);
        LogiStopSpringForce(idx);
    }
}
