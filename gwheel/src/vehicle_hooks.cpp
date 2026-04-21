#include "vehicle_hooks.h"
#include "wheel_device.h"
#include "logging.h"

#include <atomic>
#include <cmath>

namespace gwheel::vehicle
{
    namespace
    {
        struct Names
        {
            RED4ext::CName steer;
            RED4ext::CName accelerate;
            RED4ext::CName brake;
            RED4ext::CName vehicleSteer;
        };

        const Names& GetNames()
        {
            static const Names n{
                RED4ext::CName("Steer"),
                RED4ext::CName("Accelerate"),
                RED4ext::CName("Brake"),
                RED4ext::CName("VehicleSteer"),
            };
            return n;
        }

        // Symmetric deadzone applied to a bipolar [-1..1] value with smooth
        // taper so the output remains continuous at the deadzone boundary.
        float ApplyDeadzoneBipolar(float v, float dz)
        {
            if (dz <= 0.f) return v;
            const float mag = std::fabs(v);
            if (mag <= dz) return 0.f;
            const float sign = v < 0.f ? -1.f : 1.f;
            return sign * (mag - dz) / (1.f - dz);
        }

        float ApplyDeadzoneUnipolar(float v, float dz)
        {
            if (dz <= 0.f) return v;
            if (v <= dz) return 0.f;
            return (v - dz) / (1.f - dz);
        }
    }

    float MaybeOverrideFloat(RED4ext::CName inputName, float original)
    {
        static std::atomic<uint64_t> callCount{0};
        const uint64_t n = callCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (log::DebugEnabled() && (n % 500 == 0))
        {
            log::DebugF("[gwheel] vehicle_hooks: %llu override queries observed",
                        static_cast<unsigned long long>(n));
        }

        if (!device::IsAcquired()) return original;

        const auto& names = GetNames();
        const auto snap = device::CurrentSnapshot();

        // Deadzones are read from config in a later iteration. Using small
        // conservative defaults here so wheel centre jitter doesn't leak.
        constexpr float kSteerDz = 0.02f;
        constexpr float kThrottleDz = 0.02f;
        constexpr float kBrakeDz = 0.02f;

        if (inputName == names.steer || inputName == names.vehicleSteer)
        {
            return ApplyDeadzoneBipolar(snap.steer, kSteerDz);
        }
        if (inputName == names.accelerate)
        {
            return ApplyDeadzoneUnipolar(snap.throttle, kThrottleDz);
        }
        if (inputName == names.brake)
        {
            return ApplyDeadzoneUnipolar(snap.brake, kBrakeDz);
        }

        return original;
    }
}
