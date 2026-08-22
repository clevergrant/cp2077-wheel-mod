#pragma once

#include <cstdint>
#include <string_view>

namespace gwheel
{
    inline constexpr uint32_t kLogitechVid     = 0x046D;
    inline constexpr uint32_t kMozaVid         = 0x346E;
    inline constexpr uint32_t kMozaAltVid      = 0x0483;
    inline constexpr uint32_t kThrustmasterVid = 0x044F;

    enum class Model : uint8_t
    {
        Unknown = 0,
        WingmanFormulaYellow,
        WingmanFormulaGp,
        WingmanFormulaForce,
        WingmanFormulaForceGp,
        DrivingForce,
        MomoForce,
        DrivingForcePro,
        G25,
        DrivingForceGt,
        G27,
        G29Native,
        G29Ps,
        G920Variant,
        G920,
        G923Xbox,
        G923PsPc,
        G923Ps,
        G923,
        GProRacingPs,
        GProRacingXbox,
        MomoRacing,
        FormulaVibrationFeedback,

        // MOZA hardware
        MozaAy90,
        MozaAy210,
        MozaAb9,
        MozaMfy,
        MozaR3,
        MozaR5,
        MozaR9,
        MozaR12,
        MozaR16,
        MozaR21,

        // Rudder Pedals
        ThrustmasterTfrp,
        ThrustmasterTpr,
    };

    struct ModelInfo
    {
        uint32_t         vid;
        uint32_t         pid;
        Model            id;
        std::string_view name;
        bool             ffb_default;
        bool             has_clutch;
        bool             has_shifter;
        bool             has_right_cluster;
        bool             has_rev_leds;
        bool             is_flight_yoke;
        bool             is_rudder_pedals;
        uint16_t         steering_range_deg;
    };

    // Lookup device by PID and optional VID
    const ModelInfo* LookupDevice(uint32_t vid, uint32_t pid);
    const ModelInfo* LookupByPid(uint32_t pid);
}
