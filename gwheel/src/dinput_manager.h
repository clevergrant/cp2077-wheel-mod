#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include "wheel.h"

namespace gwheel
{
    struct DInputDeviceInfo
    {
        GUID instanceGuid = {};
        GUID productGuid = {};
        uint16_t vid = 0;
        uint16_t pid = 0;
        std::string productName;
        bool isFlightYoke = false;
        bool isRudderPedals = false;
        bool isWheel = false;
        bool hasFFB = false;
    };

    class DInputManager
    {
    public:
        static DInputManager& Instance();

        bool Init(HWND hwnd);
        void Shutdown();
        void Poll();

        bool IsConnected() const;
        bool HasRudderPedals() const;
        bool HasFlightYoke() const;
        
        wheel::Snapshot GetSnapshot() const;
        const std::vector<DInputDeviceInfo>& GetConnectedDevices() const;

        LPDIRECTINPUTDEVICE8 GetPrimaryDevice() const;
        HWND GetHwnd() const { return m_hwnd; }

    private:
        DInputManager() = default;
        ~DInputManager() { Shutdown(); }

        static BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);

        void AcquireDevices();
        void ProcessPrimaryDevice(const DIJOYSTATE2& js, wheel::Snapshot& snap);
        void ProcessRudderPedals(const DIJOYSTATE2& js, wheel::Snapshot& snap);

        mutable std::recursive_mutex m_mutex;
        HWND m_hwnd = nullptr;
        HWND m_hostHwnd = nullptr;
        LPDIRECTINPUT8 m_dinput = nullptr;

        LPDIRECTINPUTDEVICE8 m_primaryDevice = nullptr;
        LPDIRECTINPUTDEVICE8 m_pedalDevice = nullptr;

        DInputDeviceInfo m_primaryInfo = {};
        DInputDeviceInfo m_pedalInfo = {};
        std::vector<DInputDeviceInfo> m_devices;

        wheel::Snapshot m_snapshot = {};
        bool m_initialized = false;
    };
}
