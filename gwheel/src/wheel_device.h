#pragma once

#include "device_table.h"

#include <atomic>
#include <cstdint>

namespace gwheel::device
{
    struct Snapshot
    {
        // Normalized values. Steer is bipolar, others unipolar.
        float    steer    = 0.f; // -1..1
        float    throttle = 0.f; //  0..1
        float    brake    = 0.f; //  0..1
        float    clutch   = 0.f; //  0..1
        uint32_t buttons_lo = 0;
        uint32_t buttons_hi = 0;
        int8_t   shifter_gear = 0;
        bool     connected = false;
    };

    struct Caps
    {
        const ModelInfo* model = nullptr;
        uint32_t         pid = 0;
        bool             ffb_runtime = false; // DIDC_FORCEFEEDBACK from DIDEVCAPS
        uint8_t          num_axes = 0;
        uint8_t          num_buttons = 0;
    };

    // Attempt to enumerate and acquire a supported Logitech wheel. On success
    // starts a background polling thread. Safe to call once; subsequent calls
    // no-op until Shutdown.
    bool Init();

    // Stop polling, release the device, release the IDirectInput8 interface.
    void Shutdown();

    // True if a device has been successfully acquired.
    bool IsAcquired();

    // Copy the latest polled snapshot. Non-blocking; may return a stale value
    // for a few ms around a producer swap — callers should treat values as
    // best-effort eventually-consistent.
    Snapshot CurrentSnapshot();

    // Plugin-wide capability record. Zero-initialized until Init() succeeds.
    const Caps& GetCaps();

    // Raw IDirectInputDevice8W pointer. Returned as void* to keep <dinput.h>
    // out of this header. Returns nullptr if the device is not acquired.
    void* GetRawDevice();
}
