#include "device_table.h"
#include <array>

namespace gwheel
{
    namespace
    {
        // Columns: vid, pid, id, name, ffb, clutch, shifter, right_cluster, rev_leds, is_yoke, is_rudder, steering_range_deg
        constexpr std::array<ModelInfo, 36> kTable = {{
            // Logitech
            { kLogitechVid, 0xC202, Model::WingmanFormulaYellow,     "WingMan Formula (Yellow)",    false, false, false, false, false, false, false, 200 },
            { kLogitechVid, 0xC20E, Model::WingmanFormulaGp,         "WingMan Formula GP",          false, false, false, false, false, false, false, 200 },
            { kLogitechVid, 0xC291, Model::WingmanFormulaForce,      "WingMan Formula Force",       true,  false, false, false, false, false, false, 240 },
            { kLogitechVid, 0xC293, Model::WingmanFormulaForceGp,    "WingMan Formula Force GP",    true,  false, false, false, false, false, false, 240 },
            { kLogitechVid, 0xC294, Model::DrivingForce,             "Driving Force",               false, false, false, false, false, false, false, 240 },
            { kLogitechVid, 0xC295, Model::MomoForce,                "Momo Force",                  true,  false, false, false, false, false, false, 270 },
            { kLogitechVid, 0xC298, Model::DrivingForcePro,          "Driving Force Pro",           true,  false, false, false, false, false, false, 900 },
            { kLogitechVid, 0xC299, Model::G25,                      "G25 Racing Wheel",            true,  true,  true,  false, false, false, false, 900 },
            { kLogitechVid, 0xC29A, Model::DrivingForceGt,           "Driving Force GT",            true,  false, false, true,  false, false, false, 900 },
            { kLogitechVid, 0xC29B, Model::G27,                      "G27 Racing Wheel",            true,  true,  true,  false, false, false, false, 900 },
            { kLogitechVid, 0xC24F, Model::G29Native,                "G29 Driving Force",           true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC260, Model::G29Ps,                    "G29 Driving Force (PS mode)", true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC261, Model::G920Variant,              "G920 Driving Force",          true,  true,  false, false, false, false, false, 900 },
            { kLogitechVid, 0xC262, Model::G920,                     "G920 Driving Force",          true,  true,  false, false, false, false, false, 900 },
            { kLogitechVid, 0xC266, Model::G923Xbox,                 "G923 (Xbox)",                 true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC267, Model::G923PsPc,                 "G923 (PS/PC)",                true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC268, Model::GProRacingPs,             "G PRO Racing Wheel (PS/PC)",  true,  true,  false, true,  true,  false, false, 1080 },
            { kLogitechVid, 0xC26D, Model::G923Ps,                   "G923 (PS mode)",              true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC26E, Model::G923,                     "G923 (PC/USB)",               true,  true,  false, true,  true,  false, false, 900 },
            { kLogitechVid, 0xC272, Model::GProRacingXbox,           "G PRO Racing Wheel (Xbox/PC)",true,  true,  false, true,  true,  false, false, 1080 },
            { kLogitechVid, 0xCA03, Model::MomoRacing,               "Momo Racing",                 true,  false, false, false, false, false, false, 240 },
            { kLogitechVid, 0xCA04, Model::FormulaVibrationFeedback, "Formula Vibration Feedback",  false, false, false, false, false, false, false, 200 },

            // MOZA Flight Hardware
            { kMozaVid, 0x0020, Model::MozaAy90,                     "MOZA AY90 Force Feedback Yoke",  true, false, false, false, false, true,  false, 180 },
            { kMozaVid, 0x0021, Model::MozaAy90,                     "MOZA AY90 Flight Base",          true, false, false, false, false, true,  false, 180 },
            { kMozaVid, 0x0022, Model::MozaAy210,                    "MOZA AY210 Force Feedback Yoke", true, false, false, false, false, true,  false, 180 },
            { kMozaVid, 0x0023, Model::MozaAy210,                    "MOZA AY210 Flight Base",         true, false, false, false, false, true,  false, 180 },
            { kMozaVid, 0x1001, Model::MozaAb9,                      "MOZA AB9 Flight Base / Yoke",    true, false, false, false, false, true,  false, 180 },
            { kMozaVid, 0x1002, Model::MozaMfy,                      "MOZA MFY Flight Hardware",       true, false, false, false, false, true,  false, 180 },

            // MOZA Racing Wheelbases
            { kMozaVid, 0x0002, Model::MozaR3,                       "MOZA R3 Wheelbase",              true, true,  false, true,  false, false, false, 900 },
            { kMozaVid, 0x0003, Model::MozaR5,                       "MOZA R5 Wheelbase",              true, true,  false, true,  false, false, false, 900 },
            { kMozaVid, 0x0004, Model::MozaR9,                       "MOZA R9 Wheelbase",              true, true,  false, true,  false, false, false, 900 },
            { kMozaVid, 0x0005, Model::MozaR12,                      "MOZA R12 Wheelbase",             true, true,  false, true,  false, false, false, 900 },
            { kMozaVid, 0x0006, Model::MozaR16,                      "MOZA R16 Wheelbase",             true, true,  false, true,  false, false, false, 900 },
            { kMozaVid, 0x0007, Model::MozaR21,                      "MOZA R21 Wheelbase",             true, true,  false, true,  false, false, false, 900 },

            // Thrustmaster Rudder Pedals
            { kThrustmasterVid, 0xB679, Model::ThrustmasterTfrp,     "Thrustmaster TFRP Rudder Pedals",false, false, false, false, false, false, true, 0 },
            { kThrustmasterVid, 0xB68F, Model::ThrustmasterTpr,      "Thrustmaster TPR Pendular Rudder",false,false, false, false, false, false, true, 0 },
        }};
    }

    const ModelInfo* LookupDevice(uint32_t vid, uint32_t pid)
    {
        for (const auto& row : kTable)
        {
            if (row.pid == pid && (vid == 0 || row.vid == vid)) return &row;
        }
        return nullptr;
    }

    const ModelInfo* LookupByPid(uint32_t pid)
    {
        return LookupDevice(0, pid);
    }
}
