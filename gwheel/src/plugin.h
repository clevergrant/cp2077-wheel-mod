#pragma once

#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>

namespace gwheel
{
    inline constexpr const wchar_t* kPluginName = L"gwheel";
    inline constexpr const wchar_t* kPluginAuthor = L"Grant Perdue";
    inline constexpr int kVersionMajor = 0;
    inline constexpr int kVersionMinor = 1;
    inline constexpr int kVersionPatch = 0;
    inline constexpr const char* kVersionString = "0.1.0";

    struct PluginContext
    {
        RED4ext::v1::PluginHandle handle{};
        const RED4ext::v1::Sdk* sdk{};
    };

    PluginContext& Ctx();

    void OnLoad(RED4ext::v1::PluginHandle aHandle, const RED4ext::v1::Sdk* aSdk);
    void OnUnload();
}
