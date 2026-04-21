#include "rtti.h"
#include "config.h"
#include "wheel.h"
#include "button_map.h"
#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "rtti_dump.h"

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

        // -------- Button bindings -------------------------------------------

        void SetButtonBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t button = -1;
            RED4ext::CString action;
            RED4ext::GetParameter(aFrame, &button);
            RED4ext::GetParameter(aFrame, &action);
            aFrame->code++;
            config::SetButtonBinding(button, std::string_view(action.c_str()));
            if (aOut) *aOut = true;
        }

        void ClearButtonBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t button = -1;
            RED4ext::GetParameter(aFrame, &button);
            aFrame->code++;
            config::ClearButtonBinding(button);
            if (aOut) *aOut = true;
        }

        void GetButtonBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            int32_t button = -1;
            RED4ext::GetParameter(aFrame, &button);
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(button_map::Get(button).c_str());
        }

        void IsButtonPressed(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t button = -1;
            RED4ext::GetParameter(aFrame, &button);
            aFrame->code++;
            if (aOut) *aOut = button_map::IsPressed(button);
        }

        void GetLastPressedButton(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, int32_t* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = button_map::LastPressed();
        }

        void GetButtonBindingsJson(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(button_map::SerializeJson().c_str());
        }

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
            RegisterGlobal(rtti, "GWheel_SetFfbStrengthPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetFfbStrengthPct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "GWheel_SetFfbDebugLogging",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbDebugLogging>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "GWheel_SetOverrideEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetOverrideEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetOverrideSensitivity",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetOverrideSensitivity>),
                           "Bool", {{ "Float", "v" }});
            RegisterGlobal(rtti, "GWheel_SetOverrideRangeDeg",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetOverrideRangeDeg>),
                           "Bool", {{ "Int32", "deg" }});
            RegisterGlobal(rtti, "GWheel_SetOverrideCenteringSpringPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetOverrideCenteringSpringPct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "GWheel_SetButtonBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetButtonBinding),
                           "Bool", {{ "Int32", "button" }, { "String", "action" }});
            RegisterGlobal(rtti, "GWheel_ClearButtonBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ClearButtonBinding),
                           "Bool", {{ "Int32", "button" }});
            RegisterGlobal(rtti, "GWheel_GetButtonBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetButtonBinding),
                           "String", {{ "Int32", "button" }});
            RegisterGlobal(rtti, "GWheel_IsButtonPressed",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsButtonPressed),
                           "Bool", {{ "Int32", "button" }});
            RegisterGlobal(rtti, "GWheel_GetLastPressedButton",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetLastPressedButton),
                           "Int32", {});
            RegisterGlobal(rtti, "GWheel_GetButtonBindingsJson",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetButtonBindingsJson),
                           "String", {});

            log::Info("[gwheel] native functions registered for redscript");

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
