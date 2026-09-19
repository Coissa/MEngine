#pragma once

#include "Component.h"
#include "ComponentRegistry.h"
#include "GameObjectHandle.h"

#include <memory>
#include <span>
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

        [[nodiscard]] Component* AddComponent(Reflection::TypeId type_id);
        [[nodiscard]] Component* GetComponent(Reflection::TypeId type_id) noexcept;
        [[nodiscard]] const Component* GetComponent(Reflection::TypeId type_id) const noexcept;
        [[nodiscard]] bool RemoveComponent(Reflection::TypeId type_id) noexcept;
        [[nodiscard]] std::vector<Component*> EnumerateComponents() noexcept;
        [[nodiscard]] std::vector<const Component*> EnumerateComponents() const;

        template<typename T>
        [[nodiscard]] T* AddComponent()
        {
            return static_cast<T*>(AddComponent(T::StaticTypeId));
        }

        template<typename T>
        [[nodiscard]] T* GetComponent() noexcept
        {
            return static_cast<T*>(GetComponent(T::StaticTypeId));
        }

        template<typename T>
        [[nodiscard]] const T* GetComponent() const noexcept
        {
            return static_cast<const T*>(GetComponent(T::StaticTypeId));
        }

        template<typename T>
        [[nodiscard]] bool RemoveComponent() noexcept
        {
            return RemoveComponent(T::StaticTypeId);
        }

    private:
        friend class World;

        GameObject(
            World& world,
            const ComponentRegistry& registry,
            GameObjectId id,
            GameObjectHandle handle,
            std::string name);

        [[nodiscard]] Component* InsertConfiguredComponent(
            std::unique_ptr<Component> component);
        [[nodiscard]] bool StartComponents();
        void FixedTickComponents(float delta_seconds);
        void TickComponents(float delta_seconds);
        void StopComponents() noexcept;
        void DetachComponents() noexcept;
        World& m_world;
        const ComponentRegistry& m_registry;
        GameObjectId m_id;
        GameObjectHandle m_handle;
        std::string m_name;
        std::vector<std::unique_ptr<Component>> m_components;
    };
}
