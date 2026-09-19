#pragma once

#include "GltfImporter.h"

#include <Engine/Asset/AssetRegistry.h>

#include <vector>

namespace GE::AssetFormats
{
    [[nodiscard]] Core::Result<std::vector<MaterialAsset>>
    ResolveGltfMaterialDependencies(
        const ImportedGltf& imported,
        const Asset::AssetRegistry& registry);
}
