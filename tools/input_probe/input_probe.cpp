// input_probe — standalone DirectInput hardware diagnostic and telemetry probe
// for the Cyberpunk 2077 Flight Yoke and Steering Wheel mod.
//
// Usage:
//   input_probe.exe          -> live real-time hardware telemetry monitor (Press 'q' or Esc to quit)
//   input_probe.exe once     -> captures 1 frame snapshot and exits immediately
//   input_probe.exe 5        -> runs telemetry for 5 seconds and exits

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <conio.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct DeviceEntry
    {
        GUID instanceGuid = {};
        uint16_t vid = 0;
        uint16_t pid = 0;
        std::string name;
        std::string role;
        bool isFlightYoke = false;
        bool isRudderPedals = false;
        bool isWheel = false;
        bool hasFFB = false;
        LPDIRECTINPUTDEVICE8 dev = nullptr;
    };

    float NormalizeBipolar(LONG v)
    {
        return std::clamp((static_cast<float>(v) - 32767.5f) / 32767.5f, -1.f, 1.f);
    }

    float NormalizeUnipolar(LONG v, bool invert)
    {
        float t = std::clamp(static_cast<float>(v) / 65535.f, 0.f, 1.f);
        if (invert) t = 1.0f - t;
        return t;
    }

    float NormalizeTFRPPedal(LONG raw)
    {
        if (raw >= 60000) return 0.0f;
        float norm = static_cast<float>(60000 - raw) / 55000.0f;
        return std::clamp(norm, 0.0f, 1.0f);
    }

    std::string MakeBar(float val, float minVal, float maxVal, int width = 24)
    {
        float norm = (val - minVal) / (maxVal - minVal);
        norm = std::clamp(norm, 0.0f, 1.0f);
        int pos = static_cast<int>(norm * width);
        std::string s(width, ' ');
        if (minVal < 0.0f && maxVal > 0.0f)
        {
            int center = width / 2;
            s[center] = '|';
            if (pos < center)
            {
                for (int i = pos; i < center; ++i) s[i] = '<';
            }
            else if (pos > center)
            {
                for (int i = center + 1; i <= pos && i < width; ++i) s[i] = '>';
            }
        }
        else
        {
            for (int i = 0; i < pos && i < width; ++i) s[i] = '#';
        }
        return "[" + s + "]";
    }
}

int main(int argc, char** argv)
{
    int runSeconds = 0;
    bool once = false;
    if (argc > 1)
    {
        if (std::strcmp(argv[1], "once") == 0) once = true;
        else runSeconds = std::atoi(argv[1]);
    }

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
    {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    CONSOLE_CURSOR_INFO cci;
    cci.dwSize = 25;
    cci.bVisible = once ? TRUE : FALSE;
    if (!once) SetConsoleCursorInfo(hOut, &cci);

    HINSTANCE hinst = GetModuleHandleW(nullptr);

    LPDIRECTINPUT8 dinput = nullptr;
    HRESULT hr = DirectInput8Create(hinst, DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                   reinterpret_cast<void**>(&dinput), nullptr);
    if (FAILED(hr) || !dinput)
    {
        std::printf("Error: DirectInput8Create failed: 0x%08lX\n", static_cast<unsigned long>(hr));
        return 1;
    }

    std::vector<DeviceEntry> devices;

    dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCEW inst, LPVOID ud) -> BOOL {
        auto* list = reinterpret_cast<std::vector<DeviceEntry>*>(ud);
        DeviceEntry e;
        e.instanceGuid = inst->guidInstance;
        e.vid = static_cast<uint16_t>(inst->guidProduct.Data1 & 0xFFFF);
        e.pid = static_cast<uint16_t>(inst->guidProduct.Data1 >> 16);
        char nameBuf[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, inst->tszProductName, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
        e.name = nameBuf;

        std::string lower = e.name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

        if (e.vid == 0x044F || lower.find("rudder") != std::string::npos ||
            lower.find("pedal") != std::string::npos || lower.find("tfrp") != std::string::npos ||
            lower.find("tpr") != std::string::npos)
        {
            e.isRudderPedals = true;
            e.role = "RUDDER PEDALS";
        }
        else if (e.vid == 0x346E || e.vid == 0x0483 || lower.find("moza") != std::string::npos)
        {
            if (lower.find("ay") != std::string::npos || lower.find("yoke") != std::string::npos ||
                lower.find("flight") != std::string::npos || lower.find("ab9") != std::string::npos ||
                e.pid == 0x1001 || e.pid == 0x1002 || e.pid == 0x0020 || e.pid == 0x0021 ||
                e.pid == 0x0022 || e.pid == 0x0023)
            {
                e.isFlightYoke = true;
                e.role = "FLIGHT YOKE";
            }
            else
            {
                e.isWheel = true;
                e.role = "RACING WHEEL";
            }
            e.hasFFB = true;
        }
        else if (e.vid == 0x046D || lower.find("logitech") != std::string::npos ||
                 lower.find("g29") != std::string::npos || lower.find("g920") != std::string::npos ||
                 lower.find("g923") != std::string::npos || lower.find("g pro") != std::string::npos)
        {
            e.isWheel = true;
            e.hasFFB = true;
            e.role = "RACING WHEEL";
        }
        else if (lower.find("yoke") != std::string::npos)
        {
            e.isFlightYoke = true;
            e.role = "FLIGHT YOKE";
        }
        else if (lower.find("wheel") != std::string::npos)
        {
            e.isWheel = true;
            e.role = "RACING WHEEL";
        }
        else
        {
            e.role = "GAME CONTROLLER";
        }

        list->push_back(e);
        return DIENUM_CONTINUE;
    }, &devices, DIEDFL_ATTACHEDONLY);

    if (devices.empty())
    {
        std::printf("No DirectInput game controllers found.\n");
        dinput->Release();
        return 0;
    }

    int primaryIdx = -1;
    int pedalIdx = -1;

    for (size_t i = 0; i < devices.size(); ++i)
    {
        if (devices[i].isRudderPedals && pedalIdx < 0)
        {
            pedalIdx = static_cast<int>(i);
            break;
        }
    }

    for (size_t i = 0; i < devices.size(); ++i)
    {
        if ((devices[i].isFlightYoke || devices[i].isWheel) && primaryIdx < 0)
        {
            primaryIdx = static_cast<int>(i);
            break;
        }
    }

    if (primaryIdx < 0)
    {
        for (size_t i = 0; i < devices.size(); ++i)
        {
            if (!devices[i].isRudderPedals && primaryIdx < 0)
            {
                primaryIdx = static_cast<int>(i);
                break;
            }
        }
    }

    // Create background host window for device acquisition
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hinst;
    wc.lpszClassName = L"GWheelProbeHost";
    RegisterClassExW(&wc);
    HWND hostHwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"GWheel Probe Host",
                                    WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, hinst, nullptr);

    for (size_t i = 0; i < devices.size(); ++i)
    {
        auto& d = devices[i];
        LPDIRECTINPUTDEVICE8 dev = nullptr;
        if (SUCCEEDED(dinput->CreateDevice(d.instanceGuid, &dev, nullptr)) && dev)
        {
            dev->SetDataFormat(&c_dfDIJoystick2);
            dev->SetCooperativeLevel(hostHwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            dev->Acquire();

            DIDEVCAPS caps{};
            caps.dwSize = sizeof(DIDEVCAPS);
            if (SUCCEEDED(dev->GetCapabilities(&caps)))
            {
                if (caps.dwFlags & DIDC_FORCEFEEDBACK)
                {
                    d.hasFFB = true;
                }
            }
            d.dev = dev;
        }
    }

    if (!once)
    {
        std::printf("\033[2J\033[H");
    }

    auto startTime = std::chrono::steady_clock::now();
    std::vector<DIJOYSTATE2> states(devices.size());
    std::vector<LONG> minLX(devices.size(), 65535), maxLX(devices.size(), 0);
    std::vector<LONG> minLY(devices.size(), 65535), maxLY(devices.size(), 0);
    std::vector<LONG> minLZ(devices.size(), 65535), maxLZ(devices.size(), 0);

    while (true)
    {
        for (size_t i = 0; i < devices.size(); ++i)
        {
            if (devices[i].dev)
            {
                DIJOYSTATE2 js{};
                devices[i].dev->Poll();
                HRESULT getHr = devices[i].dev->GetDeviceState(sizeof(DIJOYSTATE2), &js);
                if (FAILED(getHr))
                {
                    devices[i].dev->Acquire();
                    getHr = devices[i].dev->GetDeviceState(sizeof(DIJOYSTATE2), &js);
                }
                if (SUCCEEDED(getHr))
                {
                    states[i] = js;
                    minLX[i] = std::min(minLX[i], js.lX); maxLX[i] = std::max(maxLX[i], js.lX);
                    minLY[i] = std::min(minLY[i], js.lY); maxLY[i] = std::max(maxLY[i], js.lY);
                    minLZ[i] = std::min(minLZ[i], js.lZ); maxLZ[i] = std::max(maxLZ[i], js.lZ);
                }
            }
        }

        if (_kbhit())
        {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q' || ch == 27) // 'q' or Esc
                break;
        }

        if (!once)
        {
            SetConsoleCursorPosition(hOut, { 0, 0 });
        }

        std::string frame;
        frame.reserve(4096);

        frame += "===============================================================================\033[K\n";
        frame += "          CYBERPUNK 2077 FLIGHT YOKE & WHEEL DIRECTINPUT PROBE                 \033[K\n";
        frame += "===============================================================================\033[K\n";
        frame += "Detected " + std::to_string(devices.size()) + " connected device(s):\033[K\n";
        for (size_t i = 0; i < devices.size(); ++i)
        {
            const auto& d = devices[i];
            char devLine[256];
            std::snprintf(devLine, sizeof(devLine), "  [%zu] [VID:0x%04X PID:0x%04X] %-32s [%s]%s\033[K\n",
                          i, d.vid, d.pid, d.name.c_str(), d.role.c_str(),
                          d.hasFFB ? " (FFB Capable)" : "");
            frame += devLine;
        }
        frame += "-------------------------------------------------------------------------------\033[K\n";
        frame += ">>> LIVE DEVICE TELEMETRY <<<\033[K\n";
        for (size_t i = 0; i < devices.size(); ++i)
        {
            const auto& js = states[i];
            uint32_t btnMask = 0;
            for (int b = 0; b < 32; ++b) if (js.rgbButtons[b] & 0x80) btnMask |= (1u << b);

            uint32_t pov = (LOWORD(js.rgdwPOV[0]) != 0xFFFF) ? js.rgdwPOV[0] : 0xFFFF;

            char rawLine[512];
            std::snprintf(rawLine, sizeof(rawLine),
                          "  [%zu] %-18s | X:%6ld [%5ld-%5ld] | Y:%6ld [%5ld-%5ld] | Z:%6ld [%5ld-%5ld] | Btn:0x%08X | POV:%5u\033[K\n",
                          i, devices[i].name.substr(0, 18).c_str(),
                          js.lX, minLX[i], maxLX[i],
                          js.lY, minLY[i], maxLY[i],
                          js.lZ, minLZ[i], maxLZ[i],
                          btnMask, pov);
            frame += rawLine;
        }

        frame += "-------------------------------------------------------------------------------\033[K\n";
        if (primaryIdx >= 0)
        {
            const auto& js = states[primaryIdx];
            float steer = NormalizeBipolar(js.lX);
            float pitch = NormalizeBipolar(js.lY);
            char line[256];
            std::snprintf(line, sizeof(line), "  PRIMARY STEER / ROLL (lX) : %+.3f  %-26s\033[K\n",
                          steer, MakeBar(steer, -1.0f, 1.0f).c_str());
            frame += line;
            if (devices[primaryIdx].isFlightYoke)
            {
                std::snprintf(line, sizeof(line), "  PRIMARY PITCH / ELEV (lY) : %+.3f  %-26s\033[K\n",
                              pitch, MakeBar(pitch, -1.0f, 1.0f).c_str());
                frame += line;
            }
        }

        if (pedalIdx >= 0)
        {
            const auto& js = states[pedalIdx];
            float rightToe = NormalizeTFRPPedal(js.lX);
            float leftToe = NormalizeTFRPPedal(js.lY);
            float rudderYaw = NormalizeBipolar(js.lZ);
            char line[256];
            std::snprintf(line, sizeof(line), "  THROTTLE / RIGHT TOE (lX) : %5.3f  %-26s (Raw: %5ld)\033[K\n",
                          rightToe, MakeBar(rightToe, 0.0f, 1.0f).c_str(), js.lX);
            frame += line;
            std::snprintf(line, sizeof(line), "  BRAKE / LEFT TOE     (lY) : %5.3f  %-26s (Raw: %5ld)\033[K\n",
                          leftToe, MakeBar(leftToe, 0.0f, 1.0f).c_str(), js.lY);
            frame += line;
            std::snprintf(line, sizeof(line), "  RUDDER SLIDER / YAW  (lZ) : %+.3f  %-26s (Raw: %5ld)\033[K\n",
                          rudderYaw, MakeBar(rudderYaw, -1.0f, 1.0f).c_str(), js.lZ);
            frame += line;
        }

        if (!once)
        {
            frame += "-------------------------------------------------------------------------------\033[K\n";
            frame += "  Press [Q] or [Esc] to exit.\033[K\n";
        }

        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);

        if (once) break;

        auto now = std::chrono::steady_clock::now();
        if (runSeconds > 0 && (now - startTime) > std::chrono::seconds(runSeconds))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 Hz
    }

    for (auto& d : devices)
    {
        if (d.dev)
        {
            d.dev->Unacquire();
            d.dev->Release();
        }
    }
    dinput->Release();
    DestroyWindow(hostHwnd);

    if (!once)
    {
        cci.bVisible = TRUE;
        SetConsoleCursorInfo(hOut, &cci);
        std::printf("\nProbe finished.\n");
    }

    return 0;
}
