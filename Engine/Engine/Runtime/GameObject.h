#pragma once

#include "Component.h"
#include "GameObjectHandle.h"

#include <memory>
#include <string>
#include <vector>

namespace GE::Runtime
{
    class World;

    class GameObject final
    {
    public:
        ~GameObject() noexcept = default;

        GameObject(const GameObject&) = delete;
        GameObject& operator=(const GameObject&) = delete;
        GameObject(GameObject&&) = delete;
        GameObject& operator=(GameObject&&) = delete;

        [[nodiscard]] GameObjectId GetId() const noexcept;
        [[nodiscard]] GameObjectHandle GetHandle() const noexcept;
        [[nodiscard]] const std::string& GetName() const noexcept;
        void SetName(std::string name);

        [[nodiscard]] TransformComponent& GetTransform() noexcept;
        [[nodiscard]] const TransformComponent& GetTransform() const noexcept;

        [[nodiscard]] MeshRendererComponent* AddMeshRenderer();
        [[nodiscard]] MeshRendererComponent* GetMeshRenderer() noexcept;
        [[nodiscard]] const MeshRendererComponent* GetMeshRenderer() const noexcept;
        [[nodiscard]] bool RemoveMeshRenderer() noexcept;

        [[nodiscard]] CameraComponent* AddCamera();
        [[nodiscard]] CameraComponent* GetCamera() noexcept;
        [[nodiscard]] const CameraComponent* GetCamera() const noexcept;
        [[nodiscard]] bool RemoveCamera() noexcept;

    private:
        friend class World;

        GameObject(
            GameObjectId id,
            GameObjectHandle handle,
            std::string name);

        [[nodiscard]] Component* FindComponent(ComponentType type) noexcept;
        [[nodiscard]] const Component* FindComponent(
            ComponentType type) const noexcept;
        [[nodiscard]] bool RemoveComponent(ComponentType type) noexcept;
        [[nodiscard]] std::unique_ptr<GameObject> Clone(
            GameObjectHandle target_handle) const;

        GameObjectId m_id;
        GameObjectHandle m_handle;
        std::string m_name;
        std::vector<std::unique_ptr<Component>> m_components;
    };
}
