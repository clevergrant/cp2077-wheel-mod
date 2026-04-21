#include "plugin.h"

#include <RED4ext/RED4ext.hpp>

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle,
                                        RED4ext::v1::EMainReason aReason,
                                        const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
        gwheel::OnLoad(aHandle, aSdk);
        break;

    case RED4ext::v1::EMainReason::Unload:
        gwheel::OnUnload();
        break;
    }

    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = gwheel::kPluginName;
    aInfo->author = gwheel::kPluginAuthor;
    aInfo->version = RED4EXT_V1_SEMVER(gwheel::kVersionMajor,
                                       gwheel::kVersionMinor,
                                       gwheel::kVersionPatch);
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
