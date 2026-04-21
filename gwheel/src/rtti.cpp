#include "rtti.h"
#include "config.h"
#include "vehicle_hooks.h"
#include "wheel_device.h"
#include "plugin.h"
#include "logging.h"

#include <RED4ext/RED4ext.hpp>

#include <cstdio>
#include <string>

namespace gwheel::rtti
{
    namespace
    {
        // -------- Helper: read a String (CString) parameter. ------------------

        std::string ReadString(RED4ext::CStackFrame* aFrame)
        {
            RED4ext::CString s;
            RED4ext::GetParameter(aFrame, &s);
            return std::string(s.c_str());
        }

        // -------- Read-only natives -----------------------------------------------

        void GetVersion(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++; // ParamEnd
            if (aOut) *aOut = RED4ext::CString(kVersionString);
        }

        void IsPluginReady(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = device::IsAcquired();
        }

        void GetDeviceInfo(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (!aOut) return;
            const auto& caps = device::GetCaps();
            if (!caps.model)
            {
                *aOut = RED4ext::CString("no device");
                return;
            }
            char buf[192];
            std::snprintf(buf, sizeof(buf),
                "%.*s (PID 0x%04X) — FFB: %s, clutch: %s, shifter: %s",
                static_cast<int>(caps.model->name.size()), caps.model->name.data(),
                caps.pid,
                caps.ffb_runtime ? "yes" : "no",
                caps.model->has_clutch ? "yes" : "no",
                caps.model->has_shifter ? "yes" : "no");
            *aOut = RED4ext::CString(buf);
        }

        void HasFFB(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = device::GetCaps().ffb_runtime;
        }

        void ReadConfig(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(config::ReadAsJson().c_str());
        }

        // -------- Vehicle input hot path --------------------------------------

        void MaybeOverrideFloat(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t)
        {
            RED4ext::CName inputName;
            float original = 0.f;
            RED4ext::GetParameter(aFrame, &inputName);
            RED4ext::GetParameter(aFrame, &original);
            aFrame->code++;
            if (aOut) *aOut = vehicle::MaybeOverrideFloat(inputName, original);
        }

        // -------- Config setters ----------------------------------------------

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

        void SetResponseCurve(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto s = ReadString(aFrame);
            aFrame->code++;
            config::SetResponseCurve(s);
            if (aOut) *aOut = true;
        }

        // -------- Registration --------------------------------------------------

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

            RegisterGlobal(rtti, "GWheel_GetVersion",      reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetVersion),      "String", {});
            RegisterGlobal(rtti, "GWheel_IsPluginReady",   reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsPluginReady),   "Bool",   {});
            RegisterGlobal(rtti, "GWheel_GetDeviceInfo",   reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDeviceInfo),   "String", {});
            RegisterGlobal(rtti, "GWheel_HasFFB",          reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&HasFFB),          "Bool",   {});
            RegisterGlobal(rtti, "GWheel_ReadConfig",      reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ReadConfig),      "String", {});

            RegisterGlobal(rtti, "GWheel_MaybeOverrideFloat",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&MaybeOverrideFloat),
                           "Float",
                           {{ "CName", "inputName" }, { "Float", "original" }});

            // Setters — all return Bool so redscript can pipeline without ignoring.
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
            RegisterGlobal(rtti, "GWheel_SetResponseCurve",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetResponseCurve),
                           "Bool", {{ "String", "curve" }});

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

            log::Info("[gwheel] 17 native functions registered for redscript");
        }
    }

    void Register()
    {
        auto rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Error("[gwheel] CRTTISystem::Get() returned null — native functions will not be registered. "
                       "This means RED4ext is loaded but the game's scripting system isn't ready yet.");
            return;
        }
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);
        log::Debug("[gwheel] RTTI register callbacks queued");
    }
}
