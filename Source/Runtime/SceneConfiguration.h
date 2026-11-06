#pragma once

#include <Assets/Registry/AssetId.h>
#include <Renderer/RenderData.h>
#include <vector>

namespace GE::Runtime
{
    // The number is local to this scene, while AssetId identifies a project asset on disk.
    struct SceneAssetReference
    {
        std::uint32_t local_id = 0;
        Asset::AssetId asset;
        bool operator==(const SceneAssetReference&) const noexcept = default;
    };

    struct SceneConfiguration
    {
        std::vector<SceneAssetReference> meshes, materials;
        RenderCore::SceneRenderSettings rendering{.normal_debug = false};
        bool legacy_resource_ids = false;
    };
} // namespace GE::Runtime
