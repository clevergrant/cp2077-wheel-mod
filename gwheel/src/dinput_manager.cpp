#include "dinput_manager.h"
#include "logging.h"
#include "config.h"
#include "device_table.h"
#include <algorithm>
#include <cmath>

namespace gwheel
{
    namespace
    {
        float NormalizeAxisBipolar(LONG v)
        {
            // DirectInput default range is [0 .. 65535] with center at 32767
            return std::clamp((static_cast<float>(v) - 32767.5f) / 32767.5f, -1.f, 1.f);
        }

        float NormalizeAxisUnipolar(LONG v, bool invert)
        {
            // DirectInput default range is [0 .. 65535]
            float t = std::clamp(static_cast<float>(v) / 65535.f, 0.f, 1.f);
            if (invert)
            {
                t = 1.0f - t;
            }
            return t;
        }

        // Thrustmaster TFRP physical potentiometer characteristics:
        // Mechanical rest (unpressed) is 65535.
        // Tilting pedal forward moves from 65535 down towards 0.
        float NormalizeTFRPPedal(LONG raw)
        {
            if (raw >= 60000) return 0.0f;
            float norm = static_cast<float>(60000 - raw) / 55000.0f;
            return std::clamp(norm, 0.0f, 1.0f);
        }

        float NormalizeLeftBrake(LONG v)
        {
            return NormalizeTFRPPedal(v);
        }

        float NormalizeRightGas(LONG v)
        {
            return NormalizeTFRPPedal(v);
        }
    }

    DInputManager& DInputManager::Instance()
    {
        static DInputManager s_instance;
        return s_instance;
    }

    bool DInputManager::Init(HWND hwnd)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        m_hwnd = hwnd;
        HINSTANCE hInstance = GetModuleHandleW(nullptr);
        HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                       reinterpret_cast<void**>(&m_dinput), nullptr);
        if (FAILED(hr) || !m_dinput)
        {
            log::WarnF("[gwheel] DirectInput8Create failed: hr=0x%08lX", static_cast<unsigned long>(hr));
            return false;
        }

        m_devices.clear();
        m_primaryDevice = nullptr;
        m_pedalDevice = nullptr;
        m_primaryInfo = {};
        m_pedalInfo = {};

        hr = m_dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDevicesCallback, this, DIEDFL_ATTACHEDONLY);
        if (FAILED(hr))
        {
            log::WarnF("[gwheel] EnumDevices failed: hr=0x%08lX", static_cast<unsigned long>(hr));
        }

        AcquireDevices();
        m_initialized = true;
        log::InfoF("[gwheel] DInputManager initialized with %zu device(s). Primary: '%s' (vid=0x%04X pid=0x%04X yoke=%d ffb=%d), Pedals: '%s' (vid=0x%04X pid=0x%04X)",
                   m_devices.size(),
                   m_primaryInfo.productName.empty() ? "None" : m_primaryInfo.productName.c_str(),
                   m_primaryInfo.vid, m_primaryInfo.pid,
                   m_primaryInfo.isFlightYoke ? 1 : 0, m_primaryInfo.hasFFB ? 1 : 0,
                   m_pedalInfo.productName.empty() ? "None" : m_pedalInfo.productName.c_str(),
                   m_pedalInfo.vid, m_pedalInfo.pid);
        return true;
    }

    void DInputManager::Shutdown()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_primaryDevice)
        {
            m_primaryDevice->Unacquire();
            m_primaryDevice->Release();
            m_primaryDevice = nullptr;
        }
        if (m_pedalDevice)
        {
            m_pedalDevice->Unacquire();
            m_pedalDevice->Release();
            m_pedalDevice = nullptr;
        }
        if (m_dinput)
        {
            m_dinput->Release();
            m_dinput = nullptr;
        }
        m_devices.clear();
        m_primaryInfo = {};
        m_pedalInfo = {};
        m_initialized = false;
    }

    BOOL CALLBACK DInputManager::EnumDevicesCallback(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef)
    {
        auto* self = static_cast<DInputManager*>(pvRef);
        DInputDeviceInfo info;
        info.instanceGuid = lpddi->guidInstance;
        info.vid = static_cast<uint16_t>(lpddi->guidProduct.Data1 & 0xFFFF);
        info.pid = static_cast<uint16_t>(lpddi->guidProduct.Data1 >> 16);
        char nameBuf[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, lpddi->tszProductName, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
        info.productName = nameBuf;

        std::string lowerName = info.productName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

        const auto* tableInfo = LookupDevice(info.vid, info.pid);
        if (tableInfo)
        {
            info.isFlightYoke = tableInfo->is_flight_yoke;
            info.isRudderPedals = tableInfo->is_rudder_pedals;
            info.isWheel = !tableInfo->is_flight_yoke && !tableInfo->is_rudder_pedals;
            info.hasFFB = tableInfo->ffb_default;
        }
        else
        {
            if (info.vid == kThrustmasterVid || lowerName.find("rudder") != std::string::npos ||
                lowerName.find("pedal") != std::string::npos || lowerName.find("tfrp") != std::string::npos ||
                lowerName.find("tpr") != std::string::npos)
            {
                info.isRudderPedals = true;
            }
            else if (info.vid == kMozaVid || info.vid == kMozaAltVid || lowerName.find("moza") != std::string::npos)
            {
                if (lowerName.find("ay") != std::string::npos || lowerName.find("yoke") != std::string::npos ||
                    lowerName.find("flight") != std::string::npos || lowerName.find("ab9") != std::string::npos ||
                    info.pid == 0x1001 || info.pid == 0x1002 || info.pid == 0x0020 || info.pid == 0x0021 ||
                    info.pid == 0x0022 || info.pid == 0x0023)
                {
                    info.isFlightYoke = true;
                }
                else
                {
                    info.isWheel = true;
                }
                info.hasFFB = true;
            }
            else if (lowerName.find("yoke") != std::string::npos)
            {
                info.isFlightYoke = true;
            }
            else if (lowerName.find("wheel") != std::string::npos)
            {
                info.isWheel = true;
            }
        }

        log::InfoF("[gwheel] DInput enumerated: [0x%04X:0x%04X] '%s' (yoke=%d rudder=%d wheel=%d ffb=%d)",
                   info.vid, info.pid, info.productName.c_str(),
                   info.isFlightYoke ? 1 : 0, info.isRudderPedals ? 1 : 0,
                   info.isWheel ? 1 : 0, info.hasFFB ? 1 : 0);

        self->m_devices.push_back(info);
        return DIENUM_CONTINUE;
    }

    void DInputManager::AcquireDevices()
    {
        if (!m_dinput) return;

        if (!m_hostHwnd)
        {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"GWheelDirectInputHost";
            RegisterClassExW(&wc);
            m_hostHwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"GWheel DirectInput FFB Host",
                                         WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        }

        HWND targetHwnd = m_hwnd ? m_hwnd : m_hostHwnd;

        // Pass 1: Bind Rudder Pedals
        for (const auto& info : m_devices)
        {
            if (info.isRudderPedals && !m_pedalDevice)
            {
                LPDIRECTINPUTDEVICE8 dev = nullptr;
                HRESULT hr = m_dinput->CreateDevice(info.instanceGuid, &dev, nullptr);
                if (SUCCEEDED(hr) && dev)
                {
                    dev->SetDataFormat(&c_dfDIJoystick2);
                    dev->SetCooperativeLevel(targetHwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
                    dev->Acquire();
                    m_pedalDevice = dev;
                    m_pedalInfo = info;
                    log::InfoF("[gwheel] Bound rudder pedal device: '%s' (0x%04X:0x%04X)",
                               info.productName.c_str(), info.vid, info.pid);
                    break;
                }
            }
        }

        // Pass 2: Bind Primary Controller with explicit priority for Flight Yoke or Wheel
        for (const auto& info : m_devices)
        {
            if (!m_primaryDevice && (info.isFlightYoke || info.isWheel))
            {
                LPDIRECTINPUTDEVICE8 dev = nullptr;
                HRESULT hr = m_dinput->CreateDevice(info.instanceGuid, &dev, nullptr);
                if (SUCCEEDED(hr) && dev)
                {
                    dev->SetDataFormat(&c_dfDIJoystick2);
                    DWORD coop = info.hasFFB ? (DISCL_BACKGROUND | DISCL_EXCLUSIVE) : (DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
                    HRESULT coopHr = dev->SetCooperativeLevel(targetHwnd, coop);
                    HRESULT acqHr = dev->Acquire();
                    if (FAILED(acqHr) && info.hasFFB)
                    {
                        dev->SetCooperativeLevel(targetHwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
                        dev->Acquire();
                    }
                    m_primaryDevice = dev;
                    m_primaryInfo = info;
                    log::InfoF("[gwheel] Bound primary control device: '%s' (0x%04X:0x%04X, ffb=%d, coop=0x%08lX, acq=0x%08lX)",
                               info.productName.c_str(), info.vid, info.pid, info.hasFFB ? 1 : 0, coopHr, acqHr);
                    break;
                }
            }
        }

        // Pass 3: Fallback for primary controller if no flight yoke or wheel was detected
        if (!m_primaryDevice)
        {
            for (const auto& info : m_devices)
            {
                if (!info.isRudderPedals)
                {
                    LPDIRECTINPUTDEVICE8 dev = nullptr;
                    HRESULT hr = m_dinput->CreateDevice(info.instanceGuid, &dev, nullptr);
                    if (SUCCEEDED(hr) && dev)
                    {
                        dev->SetDataFormat(&c_dfDIJoystick2);
                        dev->SetCooperativeLevel(m_hwnd ? m_hwnd : GetDesktopWindow(),
                                                DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
                        dev->Acquire();
                        m_primaryDevice = dev;
                        m_primaryInfo = info;
                        log::InfoF("[gwheel] Bound generic primary control device: '%s' (0x%04X:0x%04X)",
                                   info.productName.c_str(), info.vid, info.pid);
                        break;
                    }
                }
            }
        }
    }

    void DInputManager::ProcessPrimaryDevice(const DIJOYSTATE2& js, wheel::Snapshot& snap)
    {
        snap.steer = NormalizeAxisBipolar(js.lX);

        uint32_t buttons = 0;
        for (int i = 0; i < 32; ++i)
        {
            if (js.rgbButtons[i] & 0x80)
            {
                buttons |= (1u << i);
            }
        }
        snap.buttons = buttons;

        if (LOWORD(js.rgdwPOV[0]) != 0xFFFF)
        {
            snap.pov = static_cast<uint16_t>(js.rgdwPOV[0]);
        }
        else
        {
            snap.pov = 0xFFFF;
        }

        if (!m_pedalDevice)
        {
            snap.throttle = NormalizeAxisUnipolar(js.lY, true);
            snap.brake = NormalizeAxisUnipolar(js.lRz, true);
            snap.clutch = NormalizeAxisUnipolar(js.rglSlider[0], true);
        }
    }

    void DInputManager::ProcessRudderPedals(const DIJOYSTATE2& js, wheel::Snapshot& snap)
    {
        // Thrustmaster TFRP Rudder Pedals physical axis layout:
        //   js.lX = Right Toe Brake (Throttle / Gas) - rests at 65535, pressed -> 0
        //   js.lY = Left Toe Brake (Brake) - rests at 65535, pressed -> 0
        //   js.lZ = Sliding Rudder (Yaw)
        float rightToe = NormalizeRightGas(js.lX);
        float leftToe = NormalizeLeftBrake(js.lY);
        float rudderYaw = NormalizeAxisBipolar(js.lZ);

        snap.throttle = rightToe;
        snap.brake = leftToe;
        snap.clutch = 0.0f; // Prevent any clutch-as-brake cross-bleed
        (void)rudderYaw;
    }

    void DInputManager::Poll()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (!m_initialized) return;

        wheel::Snapshot snap{};

        if (m_primaryDevice)
        {
            DIJOYSTATE2 js{};
            HRESULT hr = m_primaryDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js);
            if (FAILED(hr))
            {
                m_primaryDevice->Acquire();
                hr = m_primaryDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js);
            }
            if (SUCCEEDED(hr))
            {
                snap.connected = true;
                ProcessPrimaryDevice(js, snap);
            }
        }

        if (m_pedalDevice)
        {
            DIJOYSTATE2 js{};
            HRESULT hr = m_pedalDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js);
            if (FAILED(hr))
            {
                m_pedalDevice->Acquire();
                hr = m_pedalDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js);
            }
            if (SUCCEEDED(hr))
            {
                ProcessRudderPedals(js, snap);
            }
        }

        m_snapshot = snap;
    }

    bool DInputManager::IsConnected() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_primaryDevice != nullptr || m_pedalDevice != nullptr;
    }

    bool DInputManager::HasRudderPedals() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_pedalDevice != nullptr;
    }

    bool DInputManager::HasFlightYoke() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_primaryInfo.isFlightYoke;
    }

    wheel::Snapshot DInputManager::GetSnapshot() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_snapshot;
    }

    const std::vector<DInputDeviceInfo>& DInputManager::GetConnectedDevices() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_devices;
    }

    LPDIRECTINPUTDEVICE8 DInputManager::GetPrimaryDevice() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_primaryDevice;
    }
}
