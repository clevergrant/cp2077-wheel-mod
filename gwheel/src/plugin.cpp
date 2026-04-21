#include "plugin.h"
#include "logging.h"
#include "wheel.h"
#include "button_map.h"
#include "vehicle_hook.h"
#include "config.h"
#include "rtti.h"
#include "rtti_dump.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace gwheel
{
    namespace
    {
        std::atomic<bool> g_pumpRunning{false};
        std::thread       g_pumpThread;

        void PumpLoop()
        {
            using namespace std::chrono_literals;
            log::Info("[gwheel] pump thread started (250 Hz)");
            while (g_pumpRunning.load(std::memory_order_acquire))
            {
                wheel::Pump();
                const auto snap = wheel::CurrentSnapshot();
                if (snap.connected)
                    button_map::OnWheelTick(snap.buttons);
                std::this_thread::sleep_for(4ms);
            }
            log::Info("[gwheel] pump thread stopped");
        }
    }

    PluginContext& Ctx()
    {
        static PluginContext ctx;
        return ctx;
    }

    void OnLoad(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk)
    {
        auto& ctx = Ctx();
        ctx.handle = aHandle;
        ctx.sdk = aSdk;

        log::InfoF("[gwheel] ========================================");
        log::InfoF("[gwheel] loaded v%s", kVersionString);
        log::InfoF("[gwheel] ========================================");

        log::Info("[gwheel] step 1/5: loading config");
        config::Load();

        log::Info("[gwheel] step 2/5: registering redscript natives");
        rtti::Register();

        log::Info("[gwheel] step 3/5: initializing Logitech SDK wheel layer (deferred)");
        wheel::Init();

        log::Info("[gwheel] step 4/5: installing vehicle-input detour (hash-resolved)");
        vehicle_hook::Init();

        log::Info("[gwheel] step 5/5: starting 250 Hz pump thread");
        g_pumpRunning.store(true, std::memory_order_release);
        g_pumpThread = std::thread(PumpLoop);

        log::InfoF("[gwheel] ready: hook=%s",
                   vehicle_hook::IsInstalled() ? "installed" : "not-installed");
    }

    void OnUnload()
    {
        log::Info("[gwheel] unloading");

        g_pumpRunning.store(false, std::memory_order_release);
        if (g_pumpThread.joinable()) g_pumpThread.join();

        vehicle_hook::Shutdown();
        wheel::Shutdown();

        auto& ctx = Ctx();
        ctx.handle = {};
        ctx.sdk = nullptr;
    }
}
