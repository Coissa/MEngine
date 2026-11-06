#pragma once

#include "StaticAssetFormats.h"

#include <Assets/Cook/CookedContainerPublisher.h>

namespace GE::AssetFormats
{
    [[nodiscard]] Core::Result<FileSystem::ByteBuffer> CookStaticMesh(const StaticMeshAsset& asset);
    [[nodiscard]] Core::Result<FileSystem::ByteBuffer> CookTexture(const TextureAsset& asset);
    [[nodiscard]] Core::Result<FileSystem::ByteBuffer> CookMaterial(const MaterialAsset& asset);
} // namespace GE::AssetFormats
