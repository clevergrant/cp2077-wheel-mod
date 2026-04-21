#pragma once

#include <cstdint>

namespace gwheel::ffb
{
    // Initialize the FFB subsystem. Returns true if the acquired device has
    // DIDC_FORCEFEEDBACK and all effects were created. Returns false otherwise
    // (no device, no FFB caps, or effect creation failed). When it returns
    // false, every Play*/Set* call is a safe no-op.
    bool Init();
    void Shutdown();

    // True iff Init() succeeded and effects are armed.
    bool IsReady();

    // Stop all running effects without releasing them.
    void StopAll();

    // Fire a short constant-force jolt. `magnitude` in [-1, 1]; `duration_ms`
    // measured in milliseconds. Used for collision spikes and similar.
    void PlayConstant(float magnitude, uint32_t duration_ms);

    // Continuous damper effect. `coefficient` in [0, 1]; 0 stops the damper.
    void SetDamper(float coefficient);

    // Continuous sinusoidal road texture. `frequency_hz` is the oscillation
    // rate; `magnitude` in [0, 1]. `magnitude = 0` stops the effect.
    void PlayTexture(float frequency_hz, float magnitude);

    // Apply a global strength multiplier to every subsequent effect. Value is
    // clamped to [0, 1]. Drive from config's ffb.strengthPct / 100.
    void SetGlobalStrength(float scalar);
}
