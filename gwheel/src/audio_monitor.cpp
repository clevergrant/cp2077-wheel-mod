#include "audio_monitor.h"
#include "config.h"
#include "logging.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <thread>

namespace gwheel::audio_monitor
{
    namespace
    {
        std::atomic<bool>  g_running{false};
        std::atomic<float> g_level{0.f};
        std::thread        g_thread;

        // Rolling window for dynamic-range normalisation. At 10ms
        // chunks, 300 chunks = 3 seconds — long enough to stretch a
        // quiet intro to full scale, short enough to feel reactive.
        constexpr int   kWindowChunks  = 300;

        // Asymmetric envelope smoothing on the per-chunk RMS. Attack
        // responds fast to transients (bass hits punch), release decays
        // fast enough that quiet valleys between beats remain visible
        // instead of filling in.
        constexpr float kAttackAlpha   = 0.35f;
        constexpr float kReleaseAlpha  = 0.15f;

        template <typename T>
        void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

        // -----------------------------------------------------------------
        // Bass-band IIR bandpass filter.
        //
        // The in-car LED visualizer runs on the full CP2077 main audio
        // bus (music + engine + SFX + dialogue, all mixed inside the
        // game process). We can't isolate the music bus — that requires
        // tapping Wwise internally, which is blocked on stripped-symbol
        // reverse engineering. Bandpass around the 40-160 Hz kick/bass
        // region is the practical workaround: music's bass line and
        // percussion produce sharp transients there, while engine audio
        // and dialogue mostly sit in different spectral regions (mid
        // for dialogue, broadband rumble for engine — the rumble's
        // fundamental passes through but without the transient punch).
        //
        // Transposed Direct Form II. Coefficients designed once at
        // init time from the WASAPI mix-format sample rate.
        struct Biquad
        {
            float b0{0.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
            float z1{0.f}, z2{0.f};

            void DesignBandpass(float sampleRate, float centerHz, float q)
            {
                const float omega = 2.f * 3.14159265f * centerHz / sampleRate;
                const float sinw  = std::sin(omega);
                const float cosw  = std::cos(omega);
                const float alpha = sinw / (2.f * q);
                const float a0    = 1.f + alpha;
                b0 =  alpha / a0;
                b1 =  0.f;
                b2 = -alpha / a0;
                a1 = -2.f * cosw / a0;
                a2 = (1.f - alpha) / a0;
                z1 = z2 = 0.f;
            }

            float Process(float x)
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }

            void Reset() { z1 = z2 = 0.f; }
        };

        // One filter, mono-post-mixdown. 80 Hz center covers kick drums
        // and bass; Q=0.7 gives about an octave of pass width (~55–115 Hz
        // at -3 dB). Designed lazily at format discovery time so we
        // adapt to whatever sample rate WASAPI hands us (typically
        // 44100 or 48000).
        Biquad g_bassFilter{};
        bool   g_bassFilterDesigned = false;

        bool IsFloatFormat(const WAVEFORMATEX* fmt)
        {
            if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
            if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                && fmt->cbSize >= 22)
            {
                const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
                return ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            }
            return false;
        }

        bool IsPcm16Format(const WAVEFORMATEX* fmt)
        {
            if (fmt->wBitsPerSample != 16) return false;
            if (fmt->wFormatTag == WAVE_FORMAT_PCM) return true;
            if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                && fmt->cbSize >= 22)
            {
                const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
                return ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
            }
            return false;
        }

        // Compute mean-square (not RMS — sqrt comes after accumulation
        // across all chunks in the packet). Each frame is mono-averaged
        // across channels then run through the bass-band bandpass filter;
        // the filtered sample is what gets squared. End result is an
        // energy reading of only the 40-160 Hz band, which is where
        // music kick/bass transients live while engine rumble is more
        // spread-spectrum.
        //
        // Filter state (biquad delays) persists across chunks via the
        // file-scope g_bassFilter, so cross-packet continuity is correct.
        double ChunkMeanSquare(const BYTE* data, uint32_t numFrames, const WAVEFORMATEX* fmt)
        {
            if (numFrames == 0) return 0.0;
            const uint32_t ch = fmt->nChannels;
            if (ch == 0) return 0.0;

            double sumSq = 0.0;

            if (IsFloatFormat(fmt))
            {
                const float* samples = reinterpret_cast<const float*>(data);
                for (uint32_t f = 0; f < numFrames; ++f)
                {
                    float mono = 0.f;
                    for (uint32_t c = 0; c < ch; ++c) mono += samples[f * ch + c];
                    mono /= static_cast<float>(ch);
                    const float y = g_bassFilter.Process(mono);
                    sumSq += static_cast<double>(y) * y;
                }
            }
            else if (IsPcm16Format(fmt))
            {
                const int16_t* samples = reinterpret_cast<const int16_t*>(data);
                for (uint32_t f = 0; f < numFrames; ++f)
                {
                    float mono = 0.f;
                    for (uint32_t c = 0; c < ch; ++c)
                        mono += static_cast<float>(samples[f * ch + c]) / 32768.f;
                    mono /= static_cast<float>(ch);
                    const float y = g_bassFilter.Process(mono);
                    sumSq += static_cast<double>(y) * y;
                }
            }
            // Unknown format → treat as silent. Modern Windows mix format
            // is effectively always float32, so this path is rare.

            return sumSq / static_cast<double>(numFrames);
        }

        // Case-insensitive lookup of a running process by its exe name
        // (e.g. "Spotify.exe"). Returns 0 if no match is found. We use
        // Toolhelp32 rather than WTSEnumerateProcesses so we can match
        // without needing session-query privileges.
        DWORD FindProcessIdByName(const std::string& name)
        {
            if (name.empty()) return 0;

            // Widen ASCII-ish name for Process32FirstW. Full UTF-8 isn't
            // required — exe filenames on Windows are typically ASCII.
            std::wstring wname;
            wname.reserve(name.size());
            for (char c : name) wname.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));

            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            DWORD found = 0;
            if (Process32FirstW(snap, &entry))
            {
                do {
                    if (_wcsicmp(entry.szExeFile, wname.c_str()) == 0)
                    {
                        found = entry.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snap, &entry));
            }
            CloseHandle(snap);
            return found;
        }

        // Minimal IActivateAudioInterfaceCompletionHandler. The per-process
        // activation API is async; we marshal the completion onto an event
        // and wait synchronously on the init thread.
        struct ActivationHandler : public IActivateAudioInterfaceCompletionHandler
        {
            LONG            m_refs   = 1;
            HANDLE          m_event  = nullptr;
            HRESULT         m_result = E_FAIL;
            IAudioClient*   m_client = nullptr;

            ActivationHandler() { m_event = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
            virtual ~ActivationHandler()
            {
                if (m_client) m_client->Release();
                if (m_event)  CloseHandle(m_event);
            }

            ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refs); }
            ULONG STDMETHODCALLTYPE Release() override
            {
                const LONG n = InterlockedDecrement(&m_refs);
                if (n == 0) delete this;
                return static_cast<ULONG>(n);
            }
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
            {
                if (!ppv) return E_POINTER;
                if (riid == __uuidof(IUnknown) ||
                    riid == __uuidof(IActivateAudioInterfaceCompletionHandler) ||
                    riid == __uuidof(IAgileObject))
                {
                    *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
                    AddRef();
                    return S_OK;
                }
                *ppv = nullptr;
                return E_NOINTERFACE;
            }

            HRESULT STDMETHODCALLTYPE ActivateCompleted(
                IActivateAudioInterfaceAsyncOperation* op) override
            {
                HRESULT actResult = E_UNEXPECTED;
                IUnknown* unk = nullptr;
                m_result = op->GetActivateResult(&actResult, &unk);
                if (SUCCEEDED(m_result)) m_result = actResult;
                if (SUCCEEDED(m_result) && unk)
                    unk->QueryInterface(__uuidof(IAudioClient),
                                        reinterpret_cast<void**>(&m_client));
                if (unk) unk->Release();
                SetEvent(m_event);
                return S_OK;
            }
        };

        // Open a per-process loopback capture attached to `pid`. On
        // success returns a started IAudioClient + its manually-specified
        // PCM format. Windows' per-process API requires a known integer-
        // PCM format (won't honour GetMixFormat), so we ask for the same
        // 16-bit 44.1 kHz stereo shape the official MS ApplicationLoopback
        // sample uses. ChunkMeanSquare handles this format path natively.
        bool OpenPerProcessLoopback(DWORD pid,
                                    IAudioClient** outClient,
                                    WAVEFORMATEX** outFormat)
        {
            AUDIOCLIENT_ACTIVATION_PARAMS params{};
            params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
            params.ProcessLoopbackParams.TargetProcessId  = pid;
            params.ProcessLoopbackParams.ProcessLoopbackMode =
                PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

            PROPVARIANT pv{};
            pv.vt              = VT_BLOB;
            pv.blob.cbSize     = sizeof(params);
            pv.blob.pBlobData  = reinterpret_cast<BYTE*>(&params);

            auto* handler = new ActivationHandler();
            IActivateAudioInterfaceAsyncOperation* op = nullptr;
            HRESULT hr = ActivateAudioInterfaceAsync(
                VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
                __uuidof(IAudioClient),
                &pv,
                handler,
                &op);

            if (FAILED(hr))
            {
                log::WarnF("[gwheel:audio] ActivateAudioInterfaceAsync failed hr=0x%08lX", hr);
                handler->Release();
                if (op) op->Release();
                return false;
            }

            const bool signalled = WaitForSingleObject(handler->m_event, 5000) == WAIT_OBJECT_0;
            HRESULT result = handler->m_result;
            IAudioClient* client = nullptr;
            if (signalled)
            {
                client = handler->m_client;
                handler->m_client = nullptr;
            }
            handler->Release();
            if (op) op->Release();

            if (!signalled || FAILED(result) || !client)
            {
                log::WarnF("[gwheel:audio] per-process activation did not complete (signalled=%d, hr=0x%08lX)",
                           signalled ? 1 : 0, result);
                if (client) client->Release();
                return false;
            }

            auto* fmt = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
            if (!fmt) { client->Release(); return false; }
            *fmt = {};
            fmt->wFormatTag      = WAVE_FORMAT_PCM;
            fmt->nChannels       = 2;
            fmt->nSamplesPerSec  = 44100;
            fmt->wBitsPerSample  = 16;
            fmt->nBlockAlign     = static_cast<WORD>(fmt->nChannels * fmt->wBitsPerSample / 8);
            fmt->nAvgBytesPerSec = fmt->nSamplesPerSec * fmt->nBlockAlign;
            fmt->cbSize          = 0;

            constexpr REFERENCE_TIME kHnsBuffer = 2'000'000; // 200 ms
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    kHnsBuffer, 0, fmt, nullptr);
            if (FAILED(hr))
            {
                log::WarnF("[gwheel:audio] per-process IAudioClient::Initialize failed hr=0x%08lX", hr);
                CoTaskMemFree(fmt);
                client->Release();
                return false;
            }

            *outClient = client;
            *outFormat = fmt;
            return true;
        }

        void CaptureLoop()
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            const bool comOk = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
            if (!comOk)
            {
                log::WarnF("[gwheel:audio] CoInitializeEx failed (hr=0x%08lX) — visualizer disabled", hr);
                return;
            }

            IMMDeviceEnumerator* enumerator = nullptr;
            IMMDevice*           device     = nullptr;
            IAudioClient*        client     = nullptr;
            IAudioCaptureClient* capture    = nullptr;
            WAVEFORMATEX*        mixFormat  = nullptr;

            auto cleanup = [&]() {
                if (client) client->Stop();
                SafeRelease(capture);
                SafeRelease(client);
                SafeRelease(device);
                SafeRelease(enumerator);
                if (mixFormat) { CoTaskMemFree(mixFormat); mixFormat = nullptr; }
                if (comOk) CoUninitialize();
            };

            // Pick a capture source. Prefer per-process loopback when the
            // user has configured music.processName; fall back to system-
            // wide loopback if the target process isn't running or the
            // per-process activation fails. When processName is empty,
            // skip the per-process attempt entirely.
            const auto cfg = config::Current();
            bool perProcess = false;
            if (!cfg.music.processName.empty())
            {
                const DWORD pid = FindProcessIdByName(cfg.music.processName);
                if (pid == 0)
                {
                    log::WarnF("[gwheel:audio] music.processName=\"%s\" not running — falling back to system loopback",
                               cfg.music.processName.c_str());
                }
                else if (OpenPerProcessLoopback(pid, &client, &mixFormat))
                {
                    log::InfoF("[gwheel:audio] per-process loopback attached to \"%s\" (pid=%u, 44100 Hz 16-bit stereo)",
                               cfg.music.processName.c_str(), pid);
                    perProcess = true;
                }
                else
                {
                    log::WarnF("[gwheel:audio] per-process loopback failed for \"%s\" (pid=%u) — falling back to system loopback",
                               cfg.music.processName.c_str(), pid);
                }
            }

            if (!perProcess)
            {
                hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                      CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                      reinterpret_cast<void**>(&enumerator));
                if (FAILED(hr)) {
                    log::WarnF("[gwheel:audio] CoCreateInstance(MMDeviceEnumerator) failed hr=0x%08lX", hr);
                    cleanup(); return;
                }

                hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
                if (FAILED(hr)) {
                    log::WarnF("[gwheel:audio] GetDefaultAudioEndpoint failed hr=0x%08lX", hr);
                    cleanup(); return;
                }

                hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(&client));
                if (FAILED(hr)) {
                    log::WarnF("[gwheel:audio] IAudioClient::Activate failed hr=0x%08lX", hr);
                    cleanup(); return;
                }

                hr = client->GetMixFormat(&mixFormat);
                if (FAILED(hr) || !mixFormat) {
                    log::WarnF("[gwheel:audio] GetMixFormat failed hr=0x%08lX", hr);
                    cleanup(); return;
                }

                // Loopback captures must be shared-mode and do not support
                // event-driven callbacks. Allocate a 200ms ring buffer and
                // poll on a 10ms timer below.
                constexpr REFERENCE_TIME kHnsBuffer = 2'000'000; // 200 ms in 100ns units
                hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_LOOPBACK,
                                        kHnsBuffer, 0, mixFormat, nullptr);
                if (FAILED(hr)) {
                    log::WarnF("[gwheel:audio] IAudioClient::Initialize failed hr=0x%08lX", hr);
                    cleanup(); return;
                }
            }

            hr = client->GetService(__uuidof(IAudioCaptureClient),
                                    reinterpret_cast<void**>(&capture));
            if (FAILED(hr)) {
                log::WarnF("[gwheel:audio] GetService(IAudioCaptureClient) failed hr=0x%08lX", hr);
                cleanup(); return;
            }

            hr = client->Start();
            if (FAILED(hr)) {
                log::WarnF("[gwheel:audio] IAudioClient::Start failed hr=0x%08lX", hr);
                cleanup(); return;
            }

            log::InfoF("[gwheel:audio] WASAPI %s loopback started (%u Hz, %u ch, tag=0x%04X)",
                       perProcess ? "per-process" : "system",
                       mixFormat->nSamplesPerSec, mixFormat->nChannels, mixFormat->wFormatTag);

            // Design the bass-band bandpass once the sample rate is known.
            // 80 Hz center, Q=0.7 → pass band ~55-115 Hz (-3 dB).
            g_bassFilter.DesignBandpass(
                static_cast<float>(mixFormat->nSamplesPerSec), 80.f, 0.7f);
            g_bassFilterDesigned = true;
            log::Info("[gwheel:audio] bass-band bandpass filter designed "
                      "(80 Hz center, Q=0.7)");

            std::deque<float> rollingBuf;
            float smoothed = 0.f;

            // Periodic level log counter. At 10ms chunks, 500 = 5 seconds.
            int logCounter = 0;

            while (g_running.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // Drain every packet available since the last tick.
                double sumSq = 0.0;
                uint32_t totalFrames = 0;

                UINT32 packetFrames = 0;
                HRESULT ph = capture->GetNextPacketSize(&packetFrames);
                if (FAILED(ph)) break;

                while (packetFrames > 0)
                {
                    BYTE*  data   = nullptr;
                    UINT32 frames = 0;
                    DWORD  flags  = 0;
                    ph = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                    if (FAILED(ph)) break;

                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0 && frames > 0 && data)
                    {
                        const double ms = ChunkMeanSquare(data, frames, mixFormat);
                        sumSq       += ms * static_cast<double>(frames);
                        totalFrames += frames;
                    }

                    capture->ReleaseBuffer(frames);
                    ph = capture->GetNextPacketSize(&packetFrames);
                    if (FAILED(ph)) break;
                }

                const float chunkRms = (totalFrames > 0)
                    ? static_cast<float>(std::sqrt(sumSq / static_cast<double>(totalFrames)))
                    : 0.f;

                // Asymmetric envelope.
                const float alpha = (chunkRms > smoothed) ? kAttackAlpha : kReleaseAlpha;
                smoothed += alpha * (chunkRms - smoothed);

                // Rolling min + max over the recent window. The pair is
                // the dynamic range we stretch across the LED bar, so
                // "quietest recent moment" lands on dark and "loudest
                // recent moment" lands on full.
                rollingBuf.push_back(smoothed);
                while (static_cast<int>(rollingBuf.size()) > kWindowChunks)
                    rollingBuf.pop_front();
                float rollingPeak = 0.f;
                float rollingMin  = std::numeric_limits<float>::max();
                for (float v : rollingBuf)
                {
                    if (v > rollingPeak) rollingPeak = v;
                    if (v < rollingMin)  rollingMin  = v;
                }
                if (rollingBuf.empty()) rollingMin = 0.f;

                // Dynamic-range normalisation: stretch the recent
                // window's quietest-to-loudest span across [0..1]. A
                // tiny-range guard avoids amplifying the noise floor
                // into a jittery full-scale bar when the signal is
                // essentially silent. The LED controller decides when
                // to consume this level (only while the radio is on
                // per the game's Blackboard), so we don't need any
                // music-vs-silence classification here.
                const float range = rollingPeak - rollingMin;
                float level = 0.f;
                if (range > 0.0005f)
                    level = std::clamp((smoothed - rollingMin) / range, 0.f, 1.f);
                g_level.store(level, std::memory_order_release);

                // Periodic telemetry. Gated on the FFB debug-log toggle
                // so release logs stay quiet.
                if (++logCounter >= 500 && log::DebugEnabled()) {
                    logCounter = 0;
                    log::DebugF("[gwheel:audio] rms=%.5f smoothed=%.5f min=%.5f peak=%.5f range=%.5f level=%.2f",
                                chunkRms, smoothed, rollingMin, rollingPeak, range, level);
                }
            }

            log::Info("[gwheel:audio] WASAPI loopback stopping");
            cleanup();
        }
    }

    void Init()
    {
        if (g_running.exchange(true, std::memory_order_acq_rel)) return;
        g_thread = std::thread(CaptureLoop);
    }

    void Shutdown()
    {
        if (!g_running.exchange(false, std::memory_order_acq_rel)) return;
        if (g_thread.joinable()) g_thread.join();
        g_level.store(0.f, std::memory_order_release);
    }

    float CurrentLevel() { return g_level.load(std::memory_order_acquire); }
}
