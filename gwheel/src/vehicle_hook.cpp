#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"

#include <RED4ext/Relocation.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>

#include <atomic>

namespace gwheel::vehicle_hook
{
    namespace
    {
        // vehicle::BaseObject::UpdateVehicleCameraInput(self)
        // Hash from Let There Be Flight (MIT). Vehicle-only per-tick
        // camera-input update; a safe canary to prove the hash-resolved
        // hooking architecture works. Does not affect character, camera
        // when on-foot, or ADS.
        using UpdateVehicleCameraInputFn = void (*)(void*);
        constexpr uint32_t kUpdateVehicleCameraInputHash = 501486464u;

        UpdateVehicleCameraInputFn g_original = nullptr;
        void* g_target = nullptr;

        std::atomic<uint64_t> g_fireCount{0};
        std::atomic<bool>     g_attached{false};

        void DetourUpdateVehicleCameraInput(void* self)
        {
            if (g_original) g_original(self);

            const auto n = g_fireCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == 1)
                log::InfoF("[gwheel:hook] UpdateVehicleCameraInput fired for the first time (self=%p)", self);
            else if (n == 500 || n == 5000 || n == 50000)
                log::InfoF("[gwheel:hook] UpdateVehicleCameraInput fire count = %llu",
                           static_cast<unsigned long long>(n));
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
        log::Info("[gwheel:hook] UpdateVehicleCameraInput detour installed");
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
