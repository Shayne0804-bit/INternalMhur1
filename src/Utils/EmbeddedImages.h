#pragma once

#include <cstddef>

namespace EmbeddedImages
{
    struct ImageBytes
    {
        const unsigned char* data = nullptr;
        std::size_t size = 0;
    };

    enum class AssetId
    {
        None = 0,
        Rugir,
        PlatformPsn,
        PlatformXbox,
        PlatformSteam,
        PlatformSwitch
    };

    ImageBytes FindAsset(AssetId assetId);
    ImageBytes FindCharacterIcon(int characterId);
}
