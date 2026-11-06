#pragma once

#include "EditorCamera.h"

#include <Core/Math/vector.h>
#include <Renderer/RenderData.h>
#include <Runtime/GameObjectHandle.h>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace GE::Runtime
{
    class World;
}

namespace GE::Editor
{
    struct EditorMeshBounds final
    {
        GE::Math::Vec3 minimum{};
        GE::Math::Vec3 maximum{};

        [[nodiscard]] bool IsValid() const noexcept;
    };

    class EditorScenePicker final
    {
    public:
        [[nodiscard]] bool RegisterMeshBounds(GE::RenderCore::MeshHandle mesh, EditorMeshBounds bounds) noexcept;

        [[nodiscard]] std::optional<GE::Runtime::GameObjectId> Pick(const GE::Runtime::World& world,
                                                                    const EditorCamera& camera,
                                                                    const std::array<float, 2>& viewport_uv,
                                                                    GE::RenderCore::RenderExtent extent) const noexcept;

        [[nodiscard]] std::size_t GetMeshBoundsCount() const noexcept;

    private:
        struct Entry final
        {
            GE::RenderCore::MeshHandle mesh;
            EditorMeshBounds bounds;
        };

        [[nodiscard]] const EditorMeshBounds* FindBounds(GE::RenderCore::MeshHandle mesh) const noexcept;

        std::vector<Entry> m_entries;
    };
} // namespace GE::Editor
