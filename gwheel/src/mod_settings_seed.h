#pragma once

namespace gwheel::mod_settings_seed
{
    // Probe attached HID devices for a Logitech wheel and write the three
    // hidden capability flags (hasFfbHardware / hasRevLeds / hasRightCluster)
    // into mod_settings' user.ini before mod_settings reads it during
    // ProcessScriptData. With the values pre-seeded, mod_settings' built-in
    // dependency evaluator hides every section that depends on a capability
    // the current wheel doesn't have (or hides everything when no wheel is
    // attached).
    //
    // Must be called from plugin OnLoad. Plugin OnLoads complete before
    // RED4ext processes script data, so our writes are always in place when
    // mod_settings parses the file. Probe is synchronous, ~tens of ms,
    // independent of the deferred Logitech SDK init in wheel::Init.
    //
    // Plug/unplug a wheel = restart the game once for the UI to catch up;
    // the file is read once at process start.
    void Run();
}
