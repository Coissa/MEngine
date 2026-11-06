#pragma once

#include "GameObjectHandle.h"

#include <Core/Math/vector.h>
#include <Renderer/RenderData.h>
#include <ReflectionMacros.h>
#include <TypeId.h>
#include <string_view>
#include <iosfwd>
#include <string>

namespace GE::Serialization
{
    struct Context;
}

// The generator reads identity/version from these annotated declarations.
#define MENGINE_COMPONENT_BODY(Type, Name, Version)                                                                    \
    MENGINE_REFLECT_BODY(Type)                                                                                         \
public:                                                                                                                \
    MENGINE_REFLECT_STABLE_NAME(Name)                                                                                  \
    MENGINE_REFLECT_VERSION(Version) static constexpr ::GE::Reflection::TypeId StaticTypeId =                          \
        ::GE::Reflection::MakeTypeId(Name);                                                                            \
    static constexpr std::uint32_t SerializationVersion = Version;                                                     \
    [[nodiscard]] ::GE::Reflection::TypeId GetTypeId() const noexcept override;                                        \
    [[nodiscard]] bool Save(std::ostream&, const ::GE::Serialization::Context&, std::string&) const override;          \
    [[nodiscard]] bool Load(std::istream&, std::uint32_t, const ::GE::Serialization::Context&, std::string&) override; \
    void CopyConfigurationFrom(const ::GE::Runtime::Component&) override;

namespace GE::Runtime
{
    class GameObject;
    class World;

    class Component
    {
    public:
        virtual ~Component() noexcept = default;

        Component(const Component&) noexcept;
        Component& operator=(const Component&) noexcept;
        Component(Component&&) noexcept;
        Component& operator=(Component&&) noexcept;

        [[nodiscard]] virtual Reflection::TypeId GetTypeId() const noexcept = 0;
        [[nodiscard]] virtual bool Save(std::ostream& output, const Serialization::Context& context,
                                        std::string& error) const = 0;
        [[nodiscard]] virtual bool Load(std::istream& input, std::uint32_t version,
                                        const Serialization::Context& context, std::string& error) = 0;

        virtual void CopyConfigurationFrom(const Component& source) = 0;

        virtual void OnValidate() noexcept;
        [[nodiscard]] virtual bool OnAttach();
        virtual void OnDetach() noexcept;
        [[nodiscard]] virtual bool OnStart();
        virtual void FixedTick(float delta_seconds);
        virtual void Tick(float delta_seconds);
        virtual void OnStop() noexcept;

        [[nodiscard]] World* GetOwnerWorld() noexcept;
        [[nodiscard]] const World* GetOwnerWorld() const noexcept;
        [[nodiscard]] GameObjectHandle GetOwnerHandle() const noexcept;
        [[nodiscard]] GameObject* GetOwner() noexcept;
        [[nodiscard]] const GameObject* GetOwner() const noexcept;
        [[nodiscard]] bool IsAttached() const noexcept;
        [[nodiscard]] bool IsStarted() const noexcept;

    protected:
        Component() noexcept = default;
        [[nodiscard]] virtual bool LoadLegacy(std::istream& input, std::uint32_t version, std::string& error);

    private:
        friend class GameObject;
        friend class World;

        void BindOwner(World* world, GameObjectHandle handle) noexcept;
        void ClearOwner() noexcept;
        void SetAttached(bool attached) noexcept;
        void SetStarted(bool started) noexcept;

        World* m_owner_world = nullptr;
        GameObjectHandle m_owner_handle;
        bool m_attached = false;
        bool m_started = false;
    };

    class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_ESSENTIAL MENGINE_REFLECT_SERIALIZABLE
        TransformComponent final : public Component
    {
        MENGINE_COMPONENT_BODY(TransformComponent, "MEngine.Runtime.TransformComponent", 2)
    public:
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("translation")
            MENGINE_REFLECT_REQUIRED Math::Vec3 translation{0.0F, 0.0F, 0.0F};
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("rotation_degrees")
            MENGINE_REFLECT_REQUIRED Math::Vec3 rotation_degrees{0.0F, 0.0F, 0.0F};
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("scale") MENGINE_REFLECT_REQUIRED Math::Vec3 scale{
            1.0F, 1.0F, 1.0F};

        [[nodiscard]] bool LoadLegacy(std::istream& input, std::uint32_t version, std::string& error) override;
        void OnValidate() noexcept override;
    };

    class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_SERIALIZABLE MeshRendererComponent final
        : public Component
    {
        MENGINE_COMPONENT_BODY(MeshRendererComponent, "MEngine.Runtime.MeshRendererComponent", 2)
    public:
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("mesh")
            MENGINE_REFLECT_REQUIRED RenderCore::MeshHandle mesh;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("material")
            MENGINE_REFLECT_REQUIRED RenderCore::MaterialHandle material;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("visible") bool visible = true;

        [[nodiscard]] bool LoadLegacy(std::istream& input, std::uint32_t version, std::string& error) override;
    };

    class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_SERIALIZABLE RotatorComponent final
        : public Component
    {
        MENGINE_COMPONENT_BODY(RotatorComponent, "MEngine.Runtime.RotatorComponent", 2)
    public:
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("degrees_per_second")
            MENGINE_REFLECT_EDITOR("Speed", 0.1, 0, 0) Math::Vec3 degrees_per_second{0, 37.242256f, 0};
        [[nodiscard]] bool LoadLegacy(std::istream& input, std::uint32_t version, std::string& error) override;
        void Tick(float delta_seconds) override;
    };

    class MENGINE_REFLECT_TYPE MENGINE_REFLECT_COMPONENT MENGINE_REFLECT_SERIALIZABLE CameraComponent final
        : public Component
    {
        MENGINE_COMPONENT_BODY(CameraComponent, "MEngine.Runtime.CameraComponent", 2)
    public:
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("vertical_field_of_view_degrees") MENGINE_REFLECT_REQUIRED
            float vertical_field_of_view_degrees = 60.0F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("near_plane") MENGINE_REFLECT_REQUIRED
            float near_plane = 0.1F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("far_plane") MENGINE_REFLECT_REQUIRED
            float far_plane = 1'000.0F;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("primary") bool primary = false;

        [[nodiscard]] bool LoadLegacy(std::istream& input, std::uint32_t version, std::string& error) override;
        void OnValidate() noexcept override;
    };
} // namespace GE::Runtime
