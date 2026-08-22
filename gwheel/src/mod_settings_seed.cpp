#include "mod_settings_seed.h"
#include "config.h"
#include "logging.h"
#include "device_table.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace gwheel::mod_settings_seed
{
    namespace
    {
        struct DetectedDevices
        {
            bool     found = false;
            bool     hasFfb = false;
            bool     hasRevLeds = false;
            bool     hasRightCluster = false;
            bool     hasYoke = false;
            bool     hasRudder = false;
            uint16_t primaryVid = 0;
            uint16_t primaryPid = 0;
            char     primaryName[256] = {};
        };

        BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEW inst, LPVOID ud)
        {
            auto* result = reinterpret_cast<DetectedDevices*>(ud);

            // DIDEVICEINSTANCE.guidProduct.Data1 packs (PID << 16) | VID.
            const uint16_t vid = static_cast<uint16_t>(inst->guidProduct.Data1 & 0xFFFF);
            const uint16_t pid = static_cast<uint16_t>(inst->guidProduct.Data1 >> 16);

            const auto* info = LookupDevice(vid, pid);
            if (info)
            {
                result->found = true;
                if (info->is_flight_yoke) result->hasYoke = true;
                if (info->is_rudder_pedals) result->hasRudder = true;
                if (info->ffb_default) result->hasFfb = true;
                if (info->has_rev_leds) result->hasRevLeds = true;
                if (info->has_right_cluster) result->hasRightCluster = true;

                if (result->primaryVid == 0)
                {
                    result->primaryVid = vid;
                    result->primaryPid = pid;
                    WideCharToMultiByte(CP_UTF8, 0, inst->tszProductName, -1,
                                        result->primaryName, sizeof(result->primaryName),
                                        nullptr, nullptr);
                }
            }
            else
            {
                char nameBuf[512] = {};
                WideCharToMultiByte(CP_UTF8, 0, inst->tszProductName, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
                std::string lower = nameBuf;
                for (char& c : lower) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

                if (vid == kMozaVid || vid == kMozaAltVid || lower.find("moza") != std::string::npos)
                {
                    result->found = true;
                    result->hasFfb = true;
                    if (lower.find("ay") != std::string::npos || lower.find("yoke") != std::string::npos)
                    {
                        result->hasYoke = true;
                    }
                }
                else if (vid == kThrustmasterVid || lower.find("rudder") != std::string::npos || lower.find("tfrp") != std::string::npos)
                {
                    result->hasRudder = true;
                }
            }

            return DIENUM_CONTINUE;
        }

        DetectedDevices ProbeDevices()
        {
            DetectedDevices out;
            IDirectInput8W* dinput = nullptr;
            const HRESULT hr = DirectInput8Create(
                GetModuleHandleW(nullptr),
                DIRECTINPUT_VERSION,
                IID_IDirectInput8W,
                reinterpret_cast<LPVOID*>(&dinput),
                nullptr);
            if (FAILED(hr) || !dinput)
            {
                log::WarnF("[gwheel] mod_settings_seed: DirectInput8Create failed (hr=0x%08lX)",
                           static_cast<unsigned long>(hr));
                return out;
            }
            dinput->EnumDevices(DI8DEVCLASS_GAMECTRL,
                                EnumDevicesCallback,
                                &out,
                                DIEDFL_ATTACHEDONLY);
            dinput->Release();
            return out;
        }

        std::filesystem::path ResolveUserIniPath()
        {
            HMODULE mod = nullptr;
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&ResolveUserIniPath),
                &mod);
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(mod, buf, MAX_PATH);
            std::filesystem::path p(buf);
            p = p.parent_path().parent_path();
            p /= L"mod_settings";
            p /= L"user.ini";
            return p;
        }

        void WriteFlag(const std::wstring& path, const wchar_t* key, bool value)
        {
            const wchar_t* str = value ? L"1" : L"0";
            WritePrivateProfileStringW(L"GWheelSettings", key, str, path.c_str());
        }
    }

    void Run()
    {
        const DetectedDevices det = ProbeDevices();

        const auto path = ResolveUserIniPath();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        const std::wstring wpath = path.wstring();
        WriteFlag(wpath, L"hasFfbHardware",  det.hasFfb);
        WriteFlag(wpath, L"hasRevLeds",      det.hasRevLeds);
        WriteFlag(wpath, L"hasRightCluster", det.hasRightCluster);
        WriteFlag(wpath, L"hasYokeHardware", det.hasYoke);
        WriteFlag(wpath, L"hasRudderPedals", det.hasRudder);

        const bool handshake = config::Current().handshake.playOnStart;
        WriteFlag(wpath, L"handshakePlayOnStart", handshake);

        log::InfoF("[gwheel] mod_settings_seed: wrote capabilities hasFfb=%d hasYoke=%d hasRudder=%d -> %ls",
                   det.hasFfb ? 1 : 0,
                   det.hasYoke ? 1 : 0,
                   det.hasRudder ? 1 : 0,
                   wpath.c_str());
    }
}
