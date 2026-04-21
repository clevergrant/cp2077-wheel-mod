#include "wheel_device.h"
#include "logging.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

namespace gwheel::device
{
    namespace
    {
        struct State
        {
            IDirectInput8W*       dinput = nullptr;
            IDirectInputDevice8W* device = nullptr;

            Caps caps{};

            std::thread           pollThread;
            std::atomic<bool>     running{false};
            std::atomic<int>      publishedIdx{0};
            Snapshot              slots[2]{};

            std::atomic<uint64_t> pollCount{0};
            std::atomic<uint64_t> pollErrors{0};
            std::atomic<uint64_t> reacquireAttempts{0};

            std::mutex            lifecycleMutex;
        };

        State& S()
        {
            static State s;
            return s;
        }

        float NormalizeBipolar(LONG v, LONG lo, LONG hi)
        {
            if (hi <= lo) return 0.f;
            const float mid = (float(hi) + float(lo)) * 0.5f;
            const float half = (float(hi) - float(lo)) * 0.5f;
            return (float(v) - mid) / half;
        }

        float NormalizeUnipolar(LONG v, LONG lo, LONG hi)
        {
            if (hi <= lo) return 0.f;
            const float range = float(hi) - float(lo);
            const float delta = float(hi) - float(v);
            float n = delta / range;
            if (n < 0.f) n = 0.f;
            if (n > 1.f) n = 1.f;
            return n;
        }

        BOOL CALLBACK EnumDevicesCb(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef)
        {
            auto* foundGuid = reinterpret_cast<GUID*>(pvRef);
            const DWORD vid = LOWORD(lpddi->guidProduct.Data1);
            const DWORD pid = HIWORD(lpddi->guidProduct.Data1);

            char productName[256];
            WideCharToMultiByte(CP_UTF8, 0, lpddi->tszProductName, -1,
                                productName, sizeof(productName), nullptr, nullptr);

            log::DebugF("[gwheel] enum device: VID=0x%04lX PID=0x%04lX name=\"%s\"",
                        vid, pid, productName);

            if (vid != kLogitechVid)
            {
                log::DebugF("[gwheel] skipping non-Logitech device VID=0x%04lX", vid);
                return DIENUM_CONTINUE;
            }

            if (const auto* info = LookupByPid(pid))
            {
                log::InfoF("[gwheel] matched supported wheel: %s (PID 0x%04lX, reported name \"%s\")",
                           info->name.data(), pid, productName);
                *foundGuid = lpddi->guidInstance;
                auto& st = S();
                st.caps.model = info;
                st.caps.pid = pid;
                return DIENUM_STOP;
            }

            log::WarnF("[gwheel] Logitech device with unknown PID 0x%04lX (name \"%s\") — not in supported table, skipping",
                       pid, productName);
            return DIENUM_CONTINUE;
        }

        bool InitializeInterface()
        {
            auto& st = S();
            log::Debug("[gwheel] creating IDirectInput8 interface");
            HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr),
                                            DIRECTINPUT_VERSION,
                                            IID_IDirectInput8W,
                                            reinterpret_cast<void**>(&st.dinput),
                                            nullptr);
            if (FAILED(hr))
            {
                log::ErrorF("[gwheel] DirectInput8Create failed: %s — is dinput8.dll present on the system?",
                            log::HresultName(hr));
                return false;
            }
            log::Debug("[gwheel] IDirectInput8 interface created");
            return true;
        }

        bool AcquireDevice()
        {
            auto& st = S();
            GUID guid{};
            log::Info("[gwheel] enumerating attached game controllers...");
            HRESULT hr = st.dinput->EnumDevices(DI8DEVCLASS_GAMECTRL,
                                                EnumDevicesCb,
                                                &guid,
                                                DIEDFL_ATTACHEDONLY);
            if (FAILED(hr))
            {
                log::ErrorF("[gwheel] EnumDevices failed: %s", log::HresultName(hr));
                return false;
            }
            if (!st.caps.model)
            {
                log::Warn("[gwheel] no supported Logitech G-series wheel detected. "
                          "Is the wheel plugged in? Is G HUB running? Is the wheel in the "
                          "correct mode (PS/Xbox/PC switch on G29/G923)?");
                return false;
            }

            log::DebugF("[gwheel] creating DirectInput device for %s", st.caps.model->name.data());
            hr = st.dinput->CreateDevice(guid, &st.device, nullptr);
            if (FAILED(hr) || !st.device)
            {
                log::ErrorF("[gwheel] CreateDevice failed: %s", log::HresultName(hr));
                return false;
            }

            hr = st.device->SetDataFormat(&c_dfDIJoystick2);
            if (FAILED(hr))
            {
                log::ErrorF("[gwheel] SetDataFormat(c_dfDIJoystick2) failed: %s", log::HresultName(hr));
                return false;
            }

            // Non-exclusive background by default. G HUB keeps ownership of
            // rotation range / sensitivity / centering spring. Override
            // behavior (exclusive foreground) is a separate subsystem.
            hr = st.device->SetCooperativeLevel(GetDesktopWindow(),
                                                DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            if (FAILED(hr))
            {
                log::ErrorF("[gwheel] SetCooperativeLevel(BACKGROUND|NONEXCLUSIVE) failed: %s — "
                            "another process may already hold exclusive access to this device.",
                            log::HresultName(hr));
                return false;
            }

            DIDEVCAPS caps{};
            caps.dwSize = sizeof(caps);
            hr = st.device->GetCapabilities(&caps);
            if (SUCCEEDED(hr))
            {
                st.caps.ffb_runtime = (caps.dwFlags & DIDC_FORCEFEEDBACK) != 0;
                st.caps.num_axes    = static_cast<uint8_t>(caps.dwAxes);
                st.caps.num_buttons = static_cast<uint8_t>(caps.dwButtons);
                log::DebugF("[gwheel] device caps: axes=%u buttons=%u POVs=%u flags=0x%08lX",
                            caps.dwAxes, caps.dwButtons, caps.dwPOVs, caps.dwFlags);
            }
            else
            {
                log::WarnF("[gwheel] GetCapabilities failed: %s — assuming no FFB",
                           log::HresultName(hr));
            }

            hr = st.device->Acquire();
            if (FAILED(hr))
            {
                log::WarnF("[gwheel] initial Acquire failed: %s — poll loop will retry",
                           log::HresultName(hr));
            }

            log::InfoF("[gwheel] device acquired: %s (axes=%u buttons=%u FFB=%s)",
                       st.caps.model->name.data(),
                       st.caps.num_axes, st.caps.num_buttons,
                       st.caps.ffb_runtime ? "yes" : "no");
            return true;
        }

        void PollLoop()
        {
            using namespace std::chrono_literals;
            auto& st = S();

            log::Info("[gwheel] polling thread started (target 250 Hz)");

            while (st.running.load(std::memory_order_acquire))
            {
                DIJOYSTATE2 js{};
                HRESULT hr = st.device->Poll();
                if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
                {
                    st.reacquireAttempts.fetch_add(1, std::memory_order_relaxed);
                    HRESULT ar = st.device->Acquire();
                    if (FAILED(ar) && ar != S_FALSE)
                    {
                        log::DebugF("[gwheel] reacquire failed: %s", log::HresultName(ar));
                    }
                    std::this_thread::sleep_for(4ms);
                    continue;
                }

                hr = st.device->GetDeviceState(sizeof(js), &js);
                if (FAILED(hr))
                {
                    st.pollErrors.fetch_add(1, std::memory_order_relaxed);
                    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
                    {
                        st.device->Acquire();
                    }
                    else
                    {
                        log::DebugF("[gwheel] GetDeviceState failed: %s", log::HresultName(hr));
                    }
                    std::this_thread::sleep_for(4ms);
                    continue;
                }

                const int writeIdx = 1 - st.publishedIdx.load(std::memory_order_relaxed);
                Snapshot& slot = st.slots[writeIdx];

                slot.steer    = NormalizeBipolar(js.lX, -32767, 32767);
                slot.throttle = NormalizeUnipolar(js.lY, -32767, 32767);
                slot.brake    = NormalizeUnipolar(js.lRz, -32767, 32767);
                slot.clutch   = st.caps.model && st.caps.model->has_clutch
                                ? NormalizeUnipolar(js.rglSlider[0], -32767, 32767)
                                : 0.f;

                slot.buttons_lo = 0;
                slot.buttons_hi = 0;
                for (int i = 0; i < 32; ++i)
                {
                    if (js.rgbButtons[i] & 0x80) slot.buttons_lo |= (1u << i);
                }
                for (int i = 32; i < 64; ++i)
                {
                    if (js.rgbButtons[i] & 0x80) slot.buttons_hi |= (1u << (i - 32));
                }

                slot.shifter_gear = 0; // H-pattern decode deferred
                slot.connected = true;

                st.publishedIdx.store(writeIdx, std::memory_order_release);

                const uint64_t n = st.pollCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (log::DebugEnabled() && (n % 250 == 0))
                {
                    log::DebugF("[gwheel] poll %llu: steer=%+.3f thr=%.3f brk=%.3f (err=%llu reacquire=%llu)",
                                static_cast<unsigned long long>(n),
                                slot.steer, slot.throttle, slot.brake,
                                static_cast<unsigned long long>(st.pollErrors.load()),
                                static_cast<unsigned long long>(st.reacquireAttempts.load()));
                }

                std::this_thread::sleep_for(4ms);
            }

            log::InfoF("[gwheel] polling thread stopped (polls=%llu errors=%llu reacquires=%llu)",
                       static_cast<unsigned long long>(st.pollCount.load()),
                       static_cast<unsigned long long>(st.pollErrors.load()),
                       static_cast<unsigned long long>(st.reacquireAttempts.load()));
        }
    }

    bool Init()
    {
        std::lock_guard lock(S().lifecycleMutex);
        auto& st = S();
        if (st.running.load()) { log::Debug("[gwheel] device::Init called while already running — skipping"); return true; }

        log::Info("[gwheel] device::Init starting");
        if (!InitializeInterface()) return false;
        if (!AcquireDevice())
        {
            if (st.dinput) { st.dinput->Release(); st.dinput = nullptr; }
            return false;
        }

        st.running.store(true, std::memory_order_release);
        st.pollThread = std::thread(PollLoop);
        log::Info("[gwheel] device::Init complete");
        return true;
    }

    void Shutdown()
    {
        std::lock_guard lock(S().lifecycleMutex);
        auto& st = S();

        log::Info("[gwheel] device::Shutdown starting");
        st.running.store(false, std::memory_order_release);
        if (st.pollThread.joinable()) st.pollThread.join();

        if (st.device)
        {
            st.device->Unacquire();
            st.device->Release();
            st.device = nullptr;
            log::Debug("[gwheel] device released");
        }
        if (st.dinput)
        {
            st.dinput->Release();
            st.dinput = nullptr;
            log::Debug("[gwheel] dinput interface released");
        }
        st.caps = {};
        log::Info("[gwheel] device::Shutdown complete");
    }

    bool IsAcquired()
    {
        return S().device != nullptr;
    }

    Snapshot CurrentSnapshot()
    {
        auto& st = S();
        const int idx = st.publishedIdx.load(std::memory_order_acquire);
        return st.slots[idx];
    }

    const Caps& GetCaps()
    {
        return S().caps;
    }

    void* GetRawDevice()
    {
        return S().device;
    }
}
