#include "rtti.h"
#include "config.h"
#include "wheel.h"
#include "input_bindings.h"
#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "rtti_dump.h"
#include "rtti_offsets.h"
#include "sources.h"
#include "device_table.h"
#include "dinput_manager.h"

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
                *aOut = RED4ext::CString("no controller connected");
                return;
            }
            const auto& caps = wheel::GetCaps();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "%s (%d deg, FFB=%s) -> hook:%s fireCount=%llu",
                          caps.productName,
                          caps.operatingRangeDeg,
                          caps.hasFFB ? "yes" : "no",
                          vehicle_hook::IsInstalled() ? "installed" : "not-installed",
                          static_cast<unsigned long long>(vehicle_hook::FireCount()));
            *aOut = RED4ext::CString(buf);
        }

        void HasFFB(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = wheel::IsReady() && wheel::GetCaps().hasFFB;
        }

        bool DetectedHasRightClusterImpl()
        {
            if (!wheel::IsReady()) return false;
            const auto& caps = wheel::GetCaps();
            const auto* info = LookupByPid(caps.pid);
            if (!info) return false;
            return info->has_right_cluster;
        }

        bool DetectedHasFfbHardwareImpl()
        {
            if (!wheel::IsReady()) return false;
            return wheel::GetCaps().hasFFB;
        }

        bool DetectedHasRevLedsImpl()
        {
            if (!wheel::IsReady()) return false;
            const auto& caps = wheel::GetCaps();
            const auto* info = LookupByPid(caps.pid);
            if (!info) return false;
            return info->has_rev_leds;
        }

        bool DetectedHasYokeHardwareImpl()
        {
            return DInputManager::Instance().HasFlightYoke();
        }

        bool DetectedHasRudderPedalsImpl()
        {
            return DInputManager::Instance().HasRudderPedals();
        }

        void DetectedHasRightCluster(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasRightClusterImpl();
        }

        void DetectedHasFfbHardware(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasFfbHardwareImpl();
        }

        void DetectedHasRevLeds(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasRevLedsImpl();
        }

        void DetectedHasYokeHardware(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasYokeHardwareImpl();
        }

        void DetectedHasRudderPedals(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasRudderPedalsImpl();
        }

        void DetectedModelName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (!aOut) return;
            if (!wheel::IsReady()) { *aOut = RED4ext::CString("(no controller bound)"); return; }
            const auto& caps = wheel::GetCaps();
            *aOut = RED4ext::CString(caps.productName);
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

        void SetResponseCurve(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            std::string s = ReadString(aFrame); aFrame->code++;
            config::SetResponseCurve(s);
            if (aOut) *aOut = true;
        }

        void SetMusicProcessName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            std::string s = ReadString(aFrame); aFrame->code++;
            config::SetMusicProcessName(s);
            if (aOut) *aOut = true;
        }

        void SetInputBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t inputId = 0; int32_t action = 0;
            RED4ext::GetParameter(aFrame, &inputId);
            RED4ext::GetParameter(aFrame, &action);
            aFrame->code++;
            config::SetInputBinding(inputId, action);
            if (aOut) *aOut = true;
        }

        void SetPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::IScriptable> handle;
            RED4ext::GetParameter(aFrame, &handle);
            aFrame->code++;
            if (handle)
            {
                vehicle_hook::SetPlayerVehicle(handle.instance);
                sources::SetInVehicle(true);
            }
            if (aOut) *aOut = true;
        }

        void ClearPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            vehicle_hook::SetPlayerVehicle(nullptr);
            sources::SetInVehicle(false);
            if (aOut) *aOut = true;
        }

        void SetEngineRpmNormalized(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            float v = 0.f; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            wheel::PlayLeds(v);
            if (aOut) *aOut = true;
        }

        void SetRadioActive(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            (void)v;
            if (aOut) *aOut = true;
        }

        void OnVehicleBump(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::IScriptable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            wheel::TriggerJolt(kick, 150);
            if (aOut) *aOut = true;
        }

        void OnVehicleHit(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::IScriptable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            wheel::TriggerJolt(kick, 250);
            if (aOut) *aOut = true;
        }

        void OnWheelMaterial(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t wheelIdx = 0; RED4ext::CName mat;
            RED4ext::GetParameter(aFrame, &wheelIdx);
            RED4ext::GetParameter(aFrame, &mat);
            aFrame->code++;
            (void)wheelIdx; (void)mat;
            if (aOut) *aOut = true;
        }

        void RegisterGlobal(RED4ext::CRTTISystem* rtti,
                            const char* name,
                            RED4ext::ScriptingFunction_t<void*> func,
                            const char* returnType,
                            std::initializer_list<std::pair<const char*, const char*>> params)
        {
            auto f = RED4ext::CGlobalFunction::Create(name, name, func);
            if (!f)
            {
                log::ErrorF("[gwheel] Failed to create native function: %s", name);
                return;
            }
            if (returnType && std::strlen(returnType) > 0)
            {
                f->SetReturnType(RED4ext::CName(returnType));
            }
            for (const auto& [type, pName] : params)
            {
                f->AddParam(RED4ext::CName(type), pName);
            }
            rtti->RegisterFunction(f);
        }

        void RegisterTypes()
        {
        }

        void PostRegisterTypes()
        {
            auto rtti = RED4ext::CRTTISystem::Get();
            if (!rtti) return;

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

            RegisterGlobal(rtti, "GWheel_DetectedHasRightCluster",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasRightCluster),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_DetectedHasFfbHardware",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasFfbHardware),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_DetectedHasRevLeds",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasRevLeds),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_DetectedHasYokeHardware",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasYokeHardware),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_DetectedHasRudderPedals",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasRudderPedals),
                           "Bool", {});
            RegisterGlobal(rtti, "GWheel_DetectedModelName",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedModelName),
                           "String", {});

            RegisterGlobal(rtti, "GWheel_ReadConfig",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ReadConfig),
                           "String", {});

            RegisterGlobal(rtti, "GWheel_SetInputEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputEnabled),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetClutchAsBrake",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetClutchAsBrake>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetResponseCurve",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetResponseCurve),
                           "Bool", {{ "String", "v" }});

            RegisterGlobal(rtti, "GWheel_SetFfbEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetFfbDebugLogging",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbDebugLogging>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetFfbTorquePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetFfbTorquePct>),
                           "Bool", {{ "Int32", "pct" }});

            // Flight Yoke & Rudder Pedals
            RegisterGlobal(rtti, "GWheel_SetPitchSpringStrength",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetPitchSpringStrength>),
                           "Bool", {{ "Float", "v" }});
            RegisterGlobal(rtti, "GWheel_SetPitchDamperStrength",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetPitchDamperStrength>),
                           "Bool", {{ "Float", "v" }});
            RegisterGlobal(rtti, "GWheel_SetPitchAction",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetPitchAction>),
                           "Bool", {{ "Int32", "v" }});
            RegisterGlobal(rtti, "GWheel_SetLeftToeBrakeInvert",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLeftToeBrakeInvert>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetRightToeBrakeInvert",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetRightToeBrakeInvert>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "GWheel_SetHandshakePlayOnStart",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetHandshakePlayOnStart>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "GWheel_SetStationaryThresholdMps",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetStationaryThresholdMps>),
                           "Bool", {{ "Float", "mps" }});
            RegisterGlobal(rtti, "GWheel_SetYawFeedbackPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetYawFeedbackPct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "GWheel_SetActiveTorqueStrengthPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetActiveTorqueStrengthPct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "GWheel_SetLedEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "GWheel_SetLedVisualizerWhileMusic",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedVisualizerWhileMusic>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "GWheel_SetMusicProcessName",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetMusicProcessName),
                           "Bool", {{ "String", "v" }});

            RegisterGlobal(rtti, "GWheel_SetInputBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputBinding),
                           "Bool", {{ "Int32", "inputId" }, { "Int32", "action" }});

            RegisterGlobal(rtti, "GWheel_SetPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetPlayerVehicle),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }});
            RegisterGlobal(rtti, "GWheel_ClearPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ClearPlayerVehicle),
                           "Bool", {});

            RegisterGlobal(rtti, "GWheel_SetEngineRpmNormalized",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetEngineRpmNormalized),
                           "Bool", {{ "Float", "v" }});
            RegisterGlobal(rtti, "GWheel_SetRadioActive",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetRadioActive),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "GWheel_OnVehicleBump",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleBump),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});
            RegisterGlobal(rtti, "GWheel_OnVehicleHit",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleHit),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});

            RegisterGlobal(rtti, "GWheel_OnWheelMaterial",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnWheelMaterial),
                           "Bool", {{ "Int32", "wheelIdx" }, { "CName", "material" }});

            log::Info("[gwheel] native functions registered for redscript");
            rtti_offsets::Init();
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
