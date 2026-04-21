#include "device_table.h"

#include <array>

namespace gwheel
{
    namespace
    {
        constexpr std::array<ModelInfo, 18> kTable = {{
            { 0xC291, Model::WingmanFormulaForce,      "WingMan Formula Force",       true,  false, false, 240 },
            { 0xC293, Model::WingmanFormulaForceGp,    "WingMan Formula Force GP",    true,  false, false, 240 },
            { 0xC294, Model::DrivingForce,             "Driving Force",               false, false, false, 240 },
            { 0xC295, Model::MomoForce,                "Momo Force",                  true,  false, false, 270 },
            { 0xC298, Model::DrivingForcePro,          "Driving Force Pro",           true,  false, false, 900 },
            { 0xC299, Model::G25,                      "G25 Racing Wheel",            true,  true,  true,  900 },
            { 0xC29A, Model::DrivingForceGt,           "Driving Force GT",            true,  false, false, 900 },
            { 0xC29B, Model::G27,                      "G27 Racing Wheel",            true,  true,  true,  900 },
            { 0xC24F, Model::G29Native,                "G29 Driving Force",           true,  true,  false, 900 },
            { 0xC260, Model::G29Ps,                    "G29 Driving Force (PS mode)", true,  true,  false, 900 },
            { 0xC261, Model::G920Variant,              "G920 Driving Force",          true,  true,  false, 900 },
            { 0xC262, Model::G920,                     "G920 Driving Force",          true,  true,  false, 900 },
            { 0xC266, Model::G923Xbox,                 "G923 (Xbox)",                 true,  true,  false, 900 },
            { 0xC267, Model::G923PsPc,                 "G923 (PS/PC)",                true,  true,  false, 900 },
            { 0xC26D, Model::G923Ps,                   "G923 (PS mode)",              true,  true,  false, 900 },
            { 0xC26E, Model::G923,                     "G923 (PC/USB)",               true,  true,  false, 900 },
            { 0xCA03, Model::MomoRacing,               "Momo Racing",                 true,  false, false, 270 },
            { 0xCA04, Model::FormulaVibrationFeedback, "Formula Vibration Feedback",  true,  false, false, 240 },
        }};
    }

    const ModelInfo* LookupByPid(uint32_t pid)
    {
        for (const auto& row : kTable)
        {
            if (row.pid == pid) return &row;
        }
        return nullptr;
    }
}
