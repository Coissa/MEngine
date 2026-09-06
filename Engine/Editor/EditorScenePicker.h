#pragma once

#include "EditorCamera.h"

#include <Engine/RenderCore/RenderExtent.h>
#include <Engine/RenderCore/RenderHandles.h>
#include <Engine/Runtime/GameObjectHandle.h>

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
        std::array<float, 3> minimum{};
        std::array<float, 3> maximum{};

        [[nodiscard]] bool IsValid() const noexcept;
    };

    class EditorScenePicker final
    {
    public:
        [[nodiscard]] bool RegisterMeshBounds(
            GE::RenderCore::MeshHandle mesh,
            EditorMeshBounds bounds) noexcept;

        [[nodiscard]] std::optional<GE::Runtime::GameObjectId> Pick(
            const GE::Runtime::World& world,
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

        [[nodiscard]] const EditorMeshBounds* FindBounds(
            GE::RenderCore::MeshHandle mesh) const noexcept;

        std::vector<Entry> m_entries;
    };
}
