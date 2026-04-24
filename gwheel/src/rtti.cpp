#include "rtti.h"
#include "config.h"
#include "wheel.h"
#include "input_bindings.h"
#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "rtti_dump.h"
#include "rtti_offsets.h"

#include <RED4ext/RED4ext.hpp>

#include <cstdio>
#include <string>

namespace gwheel::rtti
{
    namespace
    {
        std::string ReadString(RED4ext::CStackFrame* aFrame)
        {
            RED4ext::CString s;
            RED4ext::GetParameter(aFrame, &s);
            return std::string(s.c_str());
        }

        // -------- Read-only natives -----------------------------------------

        void GetVersion(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(kVersionString);
        }

        void IsPluginReady(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = wheel::IsReady();
        }

        void GetDeviceInfo(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (!aOut) return;
            if (!wheel::IsReady())
            {
                *aOut = RED4ext::CString("no wheel connected (Logitech SDK has not bound a device yet)");
                return;
            }
            const auto& caps = wheel::GetCaps();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "%s (%d deg, FFB=%s, SDK=%d.%d.%d) -> hook:%s fireCount=%llu",
                          caps.productName,
                          caps.operatingRangeDeg,
                          caps.hasFFB ? "yes" : "no",
                          caps.sdkMajor, caps.sdkMinor, caps.sdkBuild,
                          vehicle_hook::IsInstalled() ? "installed" : "not-installed",
                          static_cast<unsigned long long>(vehicle_hook::FireCount()));
            *aOut = RED4ext::CString(buf);
        }

        void HasFFB(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = wheel::IsReady() && wheel::GetCaps().hasFFB;
        }

        void ReadConfig(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(config::ReadAsJson().c_str());
        }

        // -------- Config setters --------------------------------------------

        void SetInputEnabled(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            config::SetInputEnabled(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(int32_t)>
        void SetInt(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t v = 0; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(float)>
        void SetFloat(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            float v = 0.f; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(bool)>
        void SetBool(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        // -------- Input bindings --------------------------------------------

        void SetInputBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t inputId = -1;
            int32_t action = 0;
            RED4ext::GetParameter(aFrame, &inputId);
            RED4ext::GetParameter(aFrame, &action);
            aFrame->code++;
            config::SetInputBinding(inputId, action);
            if (aOut) *aOut = true;
        }

        // -------- Player-vehicle mount tracking -----------------------------
        //
        // Called from redscript VehicleComponent mount/unmount wrappers so
        // the hook knows which vehicle is "the one the player is driving"
        // and doesn't write inputs into all the other vehicles the
        // UpdateVehicleCameraInput detour fires on each tick.

        void SetPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            RED4ext::GetParameter(aFrame, &handle);
            aFrame->code++;
            void* ptr = static_cast<void*>(handle.instance);
            vehicle_hook::SetPlayerVehicle(ptr);
            if (aOut) *aOut = true;
        }

        void ClearPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            vehicle_hook::SetPlayerVehicle(nullptr);
            if (aOut) *aOut = true;
        }

        // -------- Collision / bump feedback natives -------------------------
        //
        // Called from redscript @wrapMethod handlers on VehicleObject
        // collision events (see gwheel_reds/gwheel_events.reds). Each
        // native receives the vehicle handle + a signed lateral kick
        // in [-1..+1] (negative = left-side hit, positive = right-side).
        // The vehicle handle lets us filter out events from NPC cars /
        // traffic — only the player's vehicle feeds the wheel.

        void OnVehicleBump(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            void* ptr = static_cast<void*>(handle.instance);
            const bool isPlayer = vehicle_hook::IsPlayerVehicle(ptr);
            if (log::DebugEnabled())
                log::DebugF("[gwheel:evt] bump: kick=%+.3f vehicle=%p isPlayer=%d",
                            kick, ptr, isPlayer ? 1 : 0);
            if (isPlayer)
                wheel::TriggerJolt(kick, 120); // short scrape
            if (aOut) *aOut = true;
        }

        void OnVehicleHit(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            void* ptr = static_cast<void*>(handle.instance);
            const bool isPlayer = vehicle_hook::IsPlayerVehicle(ptr);
            if (log::DebugEnabled())
                log::DebugF("[gwheel:evt] hit: kick=%+.3f vehicle=%p isPlayer=%d",
                            kick, ptr, isPlayer ? 1 : 0);
            if (isPlayer)
                wheel::TriggerJolt(kick, 280); // heavier impact
            if (aOut) *aOut = true;
        }

        // -------- Menu-state tracking ---------------------------------------
        //
        // Tells the plugin whether any gameplay-blocking menu is showing,
        // so it can override D-pad + ABXY with gamepad-nav actions. Driven
        // by a redscript IsPausedState polling loop (gwheel_menu.reds).


        // -------- Registration ----------------------------------------------

        using FuncFlags = RED4ext::CBaseFunction::Flags;

        void RegisterTypes() {}

        void RegisterGlobal(RED4ext::CRTTISystem* rtti,
                            const char* name,
                            RED4ext::ScriptingFunction_t<void*> fn,
                            const char* returnType,
                            std::initializer_list<std::pair<const char*, const char*>> params)
        {
            auto func = RED4ext::CGlobalFunction::Create(name, name, fn);
            func->flags = FuncFlags{ .isNative = true, .isStatic = true };
            if (returnType && *returnType) func->SetReturnType(returnType);
            for (const auto& p : params) func->AddParam(p.first, p.second);
            rtti->RegisterFunction(func);
        }

        void PostRegisterTypes()
        {
            auto rtti = RED4ext::CRTTISystem::Get();

            RegisterGlobal(rtti, "GWheel_GetVersion",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetVersion),
                           "String", {});
            RegisterGlobal(rtti, "GWheel_IsPluginReady",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsPluginReady),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_GetDeviceInfo",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDeviceInfo),
                           "String", {});
            RegisterGlobal(rtti, "GWheel_HasFFB",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&HasFFB),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_ReadConfig",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ReadConfig),
                           "String", {});

            RegisterGlobal(rtti, "GWheel_SetInputEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputEnabled),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetSteerDeadzonePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteerDeadzonePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "GWheel_SetThrottleDeadzonePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetThrottleDeadzonePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "GWheel_SetBrakeDeadzonePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetBrakeDeadzonePct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "GWheel_SetFfbEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetFfbDebugLogging",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbDebugLogging>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetFfbTorquePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetFfbTorquePct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "GWheel_SetStationaryThresholdMps",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetStationaryThresholdMps>),
                           "Bool", {{ "Float", "mps" }});
            RegisterGlobal(rtti, "GWheel_SetYawFeedbackPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetYawFeedbackPct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "GWheel_SetActiveTorqueStrengthPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetActiveTorqueStrengthPct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "GWheel_SetSteeringSensitivity",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetSteeringSensitivity>),
                           "Bool", {{ "Float", "v" }});

            RegisterGlobal(rtti, "GWheel_SetInputBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputBinding),
                           "Bool", {{ "Int32", "inputId" }, { "Int32", "action" }});

            RegisterGlobal(rtti, "GWheel_SetPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetPlayerVehicle),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }});
            RegisterGlobal(rtti, "GWheel_ClearPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ClearPlayerVehicle),
                           "Bool", {});

            RegisterGlobal(rtti, "GWheel_OnVehicleBump",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleBump),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});
            RegisterGlobal(rtti, "GWheel_OnVehicleHit",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleHit),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});


            log::Info("[gwheel] native functions registered for redscript");

            // Resolve vehicle struct offsets dynamically from RTTI — this
            // sidesteps the "offsets drifted between game patches" problem
            // by asking the game itself where each field lives. See
            // rtti_offsets.cpp for what gets resolved.
            rtti_offsets::Init();

            // BISECT: RTTI dump disabled. Still registering natives above;
            // just skipping the full class enumeration pass.
            log::Info("[gwheel] RTTI dump DISABLED this build (bisect)");
        }
    }

    void Register()
    {
        auto rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Error("[gwheel] CRTTISystem::Get() returned null - native functions will not be registered.");
            return;
        }
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);
        log::Debug("[gwheel] RTTI register callbacks queued");
    }
}
