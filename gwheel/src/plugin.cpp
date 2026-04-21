#include "plugin.h"
#include "logging.h"
#include "wheel_device.h"
#include "ffb.h"
#include "config.h"
#include "rtti.h"

namespace gwheel
{
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

        if (!aSdk)
        {
            // We have no logger yet — nothing we can say.
            return;
        }
        if (!aSdk->logger)
        {
            // The SDK is present but logging isn't wired up. Continue; nothing
            // good will come of aborting here.
        }

        log::InfoF("[gwheel] ========================================");
        log::InfoF("[gwheel] loaded v%s", kVersionString);
        log::InfoF("[gwheel] ========================================");

        log::Info("[gwheel] step 1/4: loading config");
        config::Load();

        log::Info("[gwheel] step 2/4: registering redscript natives");
        rtti::Register();

        log::Info("[gwheel] step 3/4: acquiring wheel device");
        const bool gotDevice = device::Init();
        if (!gotDevice)
        {
            log::Warn("[gwheel] no wheel acquired — mod will idle. The game will play normally "
                      "with mouse/keyboard/gamepad. If you expected a wheel, check: "
                      "(a) wheel is plugged in, (b) G HUB is running, (c) on G29/G923, the PS/Xbox/PC switch is set correctly.");
        }

        log::Info("[gwheel] step 4/4: initializing force feedback");
        if (gotDevice)
        {
            if (ffb::Init())
            {
                log::Info("[gwheel] force feedback online");
            }
            else
            {
                log::Info("[gwheel] force feedback unavailable — input-only mode");
            }
        }
        else
        {
            log::Debug("[gwheel] skipping FFB init (no device)");
        }

        log::InfoF("[gwheel] ready. Device=%s FFB=%s Plugin=%s",
                   device::IsAcquired() ? "yes" : "no",
                   ffb::IsReady() ? "yes" : "no",
                   kVersionString);
    }

    void OnUnload()
    {
        log::Info("[gwheel] unloading");

        ffb::Shutdown();
        device::Shutdown();

        auto& ctx = Ctx();
        ctx.handle = {};
        ctx.sdk = nullptr;
    }
}
