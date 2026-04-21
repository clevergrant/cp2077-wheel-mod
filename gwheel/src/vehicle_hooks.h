#pragma once

#include <RED4ext/CName.hpp>

namespace gwheel::vehicle
{
    // Called from a redscript @wrapMethod override on
    // VehicleComponent::GetInputValueFloat. Returns the wheel-derived value
    // when (a) wheel input is enabled, (b) a device is acquired, and (c) the
    // input name is one we own (Steer / Accelerate / Brake). Otherwise returns
    // the original value unchanged.
    float MaybeOverrideFloat(RED4ext::CName inputName, float original);
}
