#include "wheel.h"
#include "logging.h"
#include "device_table.h"
#include "config.h"
#include "input_bindings.h"

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

            // G HUB's operating range captured at bind time, so we can hand
            // the wheel back cleanly when the override toggle flips off.
            std::atomic<int>      originalRangeDeg{0};
            std::atomic<bool>     haveOriginalRange{false};

            // Last values pushed to the SDK by ApplyOverrides. -1 means "not
            // yet pushed". Settings are pause-menu-only so races here are
            // benign; atomics keep it simple without needing a mutex.
            std::atomic<int>      lastRangeDeg{-1};
            std::atomic<int>      lastSpringPct{-1};
            std::atomic<bool>     lastOverrideEnabled{false};
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
                // 300 BPM: quarter note = 200ms. Rhythm per measure is
                // quarter quarter quarter rest. Each beat fires a short
                // kick of force then releases for the remainder of the beat
                // so the pulse feels like a tap rather than a held push.
                constexpr int kTriplets[2][3] = {
                    { +1, -1, +1 },   // R L R
                    { -1, +1, -1 },   // L R L
                };
                constexpr auto kPulseMs   = 40ms;               // active kick
                constexpr auto kBeatMs    = 200ms;              // quarter @ 300 BPM
                constexpr auto kGapMs     = kBeatMs - kPulseMs; // silence within beat
                constexpr auto kRestMs    = 200ms;              // quarter rest between triplets
                constexpr int  kMagnitude = 45;                 // percent
                constexpr int  kNumTriplets = 4;                // R-L-R, L-R-L, R-L-R, L-R-L

                for (int t = 0; t < kNumTriplets; ++t)
                {
                    for (int b = 0; b < 3; ++b)
                    {
                        LogiPlayConstantForce(idx, kTriplets[t % 2][b] * kMagnitude);
                        std::this_thread::sleep_for(kPulseMs);
                        LogiStopConstantForce(idx);
                        std::this_thread::sleep_for(kGapMs);
                    }
                    std::this_thread::sleep_for(kRestMs);
                }

                // Final step: hold a 100% centering spring for 3s, then
                // release so game-driven FFB can take over.
                LogiStopConstantForce(idx);
                LogiStopDamperForce(idx);
                const bool springOk = LogiPlaySpringForce(idx, 0, 100, 100);
                log::InfoF("[gwheel] hello centering begin (spring=%d) - holding 3s",
                           springOk ? 1 : 0);
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

            // ignoreXInput=true: on the G923 Xbox variant (and any other Logi
            // wheel that exposes an XInput face), CP2077 registers an XInput
            // gamepad which siphons the standard-gamepad bits (ABXY / D-pad /
            // LSB / RSB / paddles as triggers / Start / Back) away from
            // DInput. The Logi SDK reads from DInput, so with ignoreXInput=
            // false we only see the non-XInput extras (scroll rotary, +/-,
            // Xbox button). Passing true forces the SDK to surface the full
            // DInput button set even when an XInput client is active in-
            // process. Standalone (input_probe) saw 20/20 buttons because no
            // XInput client was live; in-game we saw 7/20 because CP2077's
            // XInput layer was intercepting. Verified by tools/input_probe.
            bool ok = false;
            if (hwnd)
            {
                ok = LogiSteeringInitializeWithWindow(true, hwnd);
                if (verbose || ok)
                    log::InfoF("[gwheel] LogiSteeringInitializeWithWindow(ignoreXInput=true, hwnd=0x%p) -> %s",
                               static_cast<void*>(hwnd), ok ? "true" : "false");
            }
            else
            {
                ok = LogiSteeringInitialize(true);
                if (verbose)
                    log::InfoF("[gwheel] LogiSteeringInitialize(ignoreXInput=true) -> %s (no game window yet)",
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

            // Pick the per-wheel button/POV layout based on friendly name.
            input_bindings::SetDeviceLayout(snap.productName);

            if (snap.hasFFB)
            {
                if (config::Current().hello.playOnStart)
                    FireHelloPulse(idx);
                else
                    log::Info("[gwheel] hello pulse disabled by config (hello.playOnStart=false)");
            }

            return true;
        }

        // Range limits we expose to the user. G-series wheels (G29/G920/G923)
        // support hardware ranges down to ~40 deg and up to 900 deg.
        constexpr int kMinRangeDeg = 40;
        constexpr int kMaxRangeDeg = 900;

        // Push config::override_ changes down to the Logitech SDK. Called
        // once per pump tick after the wheel is bound.
        //
        // Strictly edge-triggered. When override is off and has never been
        // turned on this session, this function does nothing at all — G HUB
        // stays in complete control of the wheel. SDK writes only happen on:
        //   - off -> on edge (apply range + spring, capture G HUB's pre-
        //     override range so we can restore later)
        //   - on -> off edge (stop spring, restore captured range)
        //   - config value changed while on (push the delta)
        //
        // Division of labor:
        //   - hardware operating range: LogiSetOperatingRange / restored from
        //     the value captured at first off->on edge this session
        //   - centering spring: continuous LogiPlaySpringForce / Stop
        //   - sensitivity: NOT applied here; vehicle_hook reads it per-tick
        //     and multiplies the normalized steer before writing the input.
        void ApplyOverrides(int idx)
        {
            auto& st = S();
            const auto cfg = config::Current();
            const bool enabled = cfg.override_.enabled;
            const int  range   = std::clamp(cfg.override_.rangeDeg, kMinRangeDeg, kMaxRangeDeg);
            const int  spring  = std::clamp(cfg.override_.centeringSpringPct, 0, 100);

            const bool wasEnabled = st.lastOverrideEnabled.load(std::memory_order_relaxed);
            const int  lastRange  = st.lastRangeDeg.load(std::memory_order_relaxed);
            const int  lastSpring = st.lastSpringPct.load(std::memory_order_relaxed);
            const bool edgeOn     = enabled && !wasEnabled;
            const bool edgeOff    = !enabled && wasEnabled;

            if (enabled)
            {
                if (edgeOn)
                {
                    // Capture G HUB's current range so we can restore it on
                    // edgeOff. We do this here (not at bind) so that a user
                    // who never touches override never has us read or write
                    // SDK range state at all.
                    int current = 0;
                    if (LogiGetOperatingRange(idx, current) && current > 0)
                    {
                        st.originalRangeDeg.store(current, std::memory_order_release);
                        st.haveOriginalRange.store(true, std::memory_order_release);
                        log::InfoF("[gwheel] override ON: captured pre-override range = %d deg",
                                   current);
                    }
                    else
                    {
                        log::Warn("[gwheel] override ON: LogiGetOperatingRange failed; "
                                  "will not be able to restore G HUB's range on override-off");
                    }
                }

                if (edgeOn || range != lastRange)
                {
                    const bool ok = LogiSetOperatingRange(idx, range);
                    log::InfoF("[gwheel] override: LogiSetOperatingRange(%d deg) -> %s",
                               range, ok ? "ok" : "FAILED");
                    st.lastRangeDeg.store(range, std::memory_order_relaxed);
                }
                if (edgeOn || spring != lastSpring)
                {
                    // offset=0 (centered), saturation=100, coefficient=spring.
                    const bool ok = LogiPlaySpringForce(idx, 0, 100, spring);
                    log::InfoF("[gwheel] override: centering spring %d%% -> %s",
                               spring, ok ? "ok" : "FAILED");
                    st.lastSpringPct.store(spring, std::memory_order_relaxed);
                }
            }
            else if (edgeOff)
            {
                LogiStopSpringForce(idx);
                log::Info("[gwheel] override disabled: centering spring stopped");

                const int  orig = st.originalRangeDeg.load(std::memory_order_acquire);
                const bool have = st.haveOriginalRange.load(std::memory_order_acquire);
                if (have && orig > 0)
                {
                    const bool ok = LogiSetOperatingRange(idx, orig);
                    log::InfoF("[gwheel] override disabled: restoring pre-override range=%d deg -> %s",
                               orig, ok ? "ok" : "FAILED");
                }
                else
                {
                    log::Info("[gwheel] override disabled: no captured range to restore");
                }
                st.lastRangeDeg.store(-1, std::memory_order_relaxed);
                st.lastSpringPct.store(-1, std::memory_order_relaxed);
            }
            // else: override is off and has never been on this session — do
            // nothing. G HUB remains fully in charge of the wheel.

            st.lastOverrideEnabled.store(enabled, std::memory_order_relaxed);
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
                // Always stop any forces we might have kicked off.
                LogiStopSpringForce(idx);
                LogiStopDamperForce(idx);
                LogiStopConstantForce(idx);

                // Return the wheel to Logitech's out-of-box hardware range
                // (900 deg) on shutdown. The SDK's Properties API for
                // restoring G HUB's exact values isn't functional with modern
                // G HUB - LogiGet/SetPreferredControllerProperties both
                // return false - so 900 is the pragmatic "vanilla" state.
                // Runs unconditionally so crashes-to-desktop mid-override
                // don't leave the wheel stuck at a narrow range.
                const bool ok = LogiSetOperatingRange(idx, 900);
                log::InfoF("[gwheel] shutdown: LogiSetOperatingRange(900) -> %s", ok ? "ok" : "FAILED");
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

        ApplyOverrides(idx);

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
