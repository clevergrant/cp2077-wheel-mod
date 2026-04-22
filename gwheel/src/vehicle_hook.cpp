#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "wheel.h"
#include "config.h"

#include <RED4ext/Relocation.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace gwheel::vehicle_hook
{
    namespace
    {
        // vehicle::BaseObject::UpdateVehicleCameraInput(self)
        // Hash from Let There Be Flight (MIT licensed). Resolved per-patch
        // by RED4ext.dll's address database. Fires per-vehicle per-tick;
        // used here to write wheel axis values into the player vehicle's
        // input struct each frame. Non-player vehicles are skipped so
        // traffic AI keeps driving normally.
        using UpdateVehicleCameraInputFn = void (*)(void*);
        constexpr uint32_t kUpdateVehicleCameraInputHash = 501486464u;

        // Offsets into vehicle::BaseObject for game build 5294808 (CP2077
        // v2.31). Found empirically via field-probe sweep 2026-04-21 -
        // LTBF's SDK labels didn't match the actual layout in this build.
        // Re-probe if the game patches the vehicleBaseObject struct.
        namespace off
        {
            constexpr std::ptrdiff_t kInputThrottle = 0x264; // float, [0..1]
            constexpr std::ptrdiff_t kInputBrake    = 0x268; // float, [0..1] (also drives reverse while stationary)
            constexpr std::ptrdiff_t kInputSteer    = 0x278; // float, [-1..1], +=right
        }

        UpdateVehicleCameraInputFn g_original = nullptr;
        void* g_target = nullptr;

        std::atomic<uint64_t> g_fireCount{0};
        std::atomic<uint64_t> g_injectCount{0};
        std::atomic<bool>     g_attached{false};

        inline float* FloatFieldAt(void* base, std::ptrdiff_t off)
        {
            return reinterpret_cast<float*>(static_cast<char*>(base) + off);
        }

        inline float Clamp(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        void DetourUpdateVehicleCameraInput(void* self)
        {
            if (g_original) g_original(self);

            const auto n = g_fireCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == 1)
                log::InfoF("[gwheel:hook] UpdateVehicleCameraInput fired for the first time (self=%p)", self);
            if (!self) return;

            const auto snap = wheel::CurrentSnapshot();
            if (!snap.connected) return;
            const auto cfg = config::Current();
            if (!cfg.input.enabled) return;

            // Inject the wheel axes into the player vehicle's input
            // struct. The hook only fires for the player's vehicle
            // (verified 2026-04-21 - traffic AI pass through untouched).
            float steer = snap.steer;
            if (cfg.override_.enabled && cfg.override_.sensitivity != 1.0f)
                steer = steer * cfg.override_.sensitivity;
            steer = Clamp(steer, -1.0f, 1.0f);
            const float throttle = Clamp(snap.throttle,  0.0f, 1.0f);
            const float brake    = Clamp(snap.brake,     0.0f, 1.0f);

            *FloatFieldAt(self, off::kInputSteer)    = steer;
            *FloatFieldAt(self, off::kInputThrottle) = throttle;
            *FloatFieldAt(self, off::kInputBrake)    = brake;

            const auto m = g_injectCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (m == 1)
                log::InfoF("[gwheel:hook] first injection: steer=%.3f throttle=%.3f brake=%.3f",
                           steer, throttle, brake);
            else if (m == 5000 || m == 50000 || m == 500000)
                log::InfoF("[gwheel:hook] inject count = %llu (steer=%.3f throttle=%.3f brake=%.3f)",
                           static_cast<unsigned long long>(m), steer, throttle, brake);
        }
    }

    bool Init()
    {
        auto& ctx = Ctx();
        if (!ctx.sdk || !ctx.sdk->hooking || !ctx.sdk->hooking->Attach)
        {
            log::Error("[gwheel:hook] RED4ext hooking API unavailable");
            return false;
        }

        // Resolving the hash terminates the game with a RED4ext MessageBox
        // if the address db doesn't know it for the current build. That's
        // the ecosystem convention - users get a clear "which mod is
        // broken" dialog rather than a silent crash later.
        const auto addr = RED4ext::UniversalRelocBase::Resolve(kUpdateVehicleCameraInputHash);
        g_target = reinterpret_cast<void*>(addr);
        log::InfoF("[gwheel:hook] UpdateVehicleCameraInput resolved: hash=%u addr=%p",
                   kUpdateVehicleCameraInputHash, g_target);

        const bool ok = ctx.sdk->hooking->Attach(
            ctx.handle,
            g_target,
            reinterpret_cast<void*>(&DetourUpdateVehicleCameraInput),
            reinterpret_cast<void**>(&g_original));

        if (!ok)
        {
            log::Error("[gwheel:hook] Attach returned false for UpdateVehicleCameraInput");
            g_target = nullptr;
            return false;
        }

        g_attached.store(true, std::memory_order_release);
        log::Info("[gwheel:hook] UpdateVehicleCameraInput detour installed "
                  "(player vehicle input override: steer/throttle/brake)");
        return true;
    }

    void Shutdown()
    {
        auto& ctx = Ctx();
        if (!g_attached.exchange(false)) return;
        if (ctx.sdk && ctx.sdk->hooking && ctx.sdk->hooking->Detach && g_target)
        {
            ctx.sdk->hooking->Detach(ctx.handle, g_target);
            log::Info("[gwheel:hook] UpdateVehicleCameraInput detour detached");
        }
        g_target = nullptr;
        g_original = nullptr;
    }

    bool IsInstalled() { return g_attached.load(std::memory_order_acquire); }

    uint64_t FireCount() { return g_fireCount.load(std::memory_order_relaxed); }
}
