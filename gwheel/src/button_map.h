#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gwheel::button_map
{
    // Bindings are wheel-button-index -> free-form action name. The action
    // name is whatever the user wants to see in logs and, once sigs.h is
    // populated, whatever CP2077's action-dispatch function compares against
    // (name hash). Unbound slots store an empty string.
    inline constexpr size_t kMaxButtons = 32;

    struct Binding
    {
        int32_t     button = -1;
        std::string action;
    };

    // Replace / clear a single binding. `button` is 0..kMaxButtons-1.
    void Set(int32_t button, std::string_view action);
    void Clear(int32_t button);

    // Read the current action bound to `button`, or empty string if unbound.
    std::string Get(int32_t button);

    // Snapshot of all bindings in button-index order (bindings with empty
    // action are omitted).
    std::vector<Binding> Snapshot();

    // Returns true iff the button index is in-range and currently held.
    bool IsPressed(int32_t button);

    // Feed the latest wheel snapshot; emits log lines on rising/falling
    // edges for bound actions and forwards to the hook layer's action
    // substitution once sigs.h is populated.
    void OnWheelTick(uint32_t currentButtonBits);

    // Report the most recently pressed (rising edge) button - useful for a
    // "teach-in" UI in redscript where the user presses the wheel button
    // they want bound to an action.
    int32_t LastPressed();

    // Replace the entire binding set - used by config load/save.
    void ReplaceAll(const std::vector<Binding>& bindings);
    std::string SerializeJson();
}
