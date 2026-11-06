#include <Assets/Registry/AssetSerialization.h>
#include <Core/Reflection/BuiltinTypeRegistration.h>
#include <Runtime/ComponentRegistry.h>
#include <Runtime/Generated/Reflection/MEngine.Runtime.reflection.module.generated.h>
#include <Core/Serialization/BuiltinSerialization.h>
#include <Core/Serialization/Serializer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <sstream>

namespace
{
    enum class TestMode : std::uint8_t
    {
        Disabled = 0,
        Enabled = 1
    };

    struct OptionalRecord final
    {
        std::int32_t required = 1;
        std::int32_t optional = 42;
    };
} // namespace

namespace GE::Reflection
{
    template <> struct TypeTraits<::TestMode>
    {
        static constexpr std::string_view StableName = "MEngine.Tests.TestMode";
    };

    template <> struct TypeTraits<::OptionalRecord>
    {
        static constexpr std::string_view StableName = "MEngine.Tests.OptionalRecord";
    };
} // namespace GE::Reflection

namespace
{
    using namespace GE;

    struct Node final
    {
        enum class Kind
        {
            Empty,
            Object,
            Bool,
            Signed,
            Unsigned,
            Float,
            String
        };

        Kind kind = Kind::Empty;
        std::string type_name;
        std::vector<std::pair<std::string, Node>> fields;
        bool bool_value = false;
        std::int64_t signed_value = 0;
        std::uint64_t unsigned_value = 0;
        float float_value = 0.0F;
        std::string string_value;
    };

    Core::EngineError ArchiveError(std::string operation, std::string message)
    {
        return {.domain = Core::ErrorDomain::Serialization,
                .code = Core::ErrorCode::ParseFailure,
                .operation = std::move(operation),
                .message = std::move(message)};
    }

    class MemoryWriter final : public Serialization::ArchiveWriter
    {
    public:
        [[nodiscard]] const Node& Root() const noexcept { return m_root; }

        Core::Result<void> BeginObject(std::string_view stable_type_name) override
        {
            Node* target = Target();
            if (target == nullptr || target->kind != Node::Kind::Empty)
                return Fail("BeginObject", "No empty value is available");
            target->kind = Node::Kind::Object;
            target->type_name = stable_type_name;
            m_objects.push_back(target);
            m_field_target = nullptr;
            return Core::Result<void>::Success();
        }

        Core::Result<void> EndObject() override
        {
            if (m_objects.empty() || m_field_target != nullptr)
                return Fail("EndObject", "Object or field nesting is invalid");
            m_objects.pop_back();
            return Core::Result<void>::Success();
        }

        Core::Result<void> BeginField(std::string_view serialized_name) override
        {
            if (m_objects.empty() || m_field_target != nullptr)
                return Fail("BeginField", "Field nesting is invalid");
            Node& object = *m_objects.back();
            object.fields.emplace_back(std::string(serialized_name), Node{});
            m_field_target = &object.fields.back().second;
            return Core::Result<void>::Success();
        }

        Core::Result<void> EndField() override
        {
            if (m_objects.empty())
                return Fail("EndField", "No field is open");
            m_field_target = nullptr;
            return Core::Result<void>::Success();
        }

        Core::Result<void> WriteBool(bool value) override
        {
            return WriteScalar(Node::Kind::Bool, [value](Node& node) { node.bool_value = value; }, "WriteBool");
        }

        Core::Result<void> WriteSigned(std::int64_t value) override
        {
            return WriteScalar(Node::Kind::Signed, [value](Node& node) { node.signed_value = value; }, "WriteSigned");
        }

        Core::Result<void> WriteUnsigned(std::uint64_t value) override
        {
            return WriteScalar(
                Node::Kind::Unsigned, [value](Node& node) { node.unsigned_value = value; }, "WriteUnsigned");
        }

        Core::Result<void> WriteFloat(float value) override
        {
            return WriteScalar(Node::Kind::Float, [value](Node& node) { node.float_value = value; }, "WriteFloat");
        }

        Core::Result<void> WriteString(std::string_view value) override
        {
            return WriteScalar(Node::Kind::String, [value](Node& node) { node.string_value = value; }, "WriteString");
        }

    private:
        Core::Result<void> Fail(std::string operation, std::string message)
        {
            return Core::Result<void>::Failure(ArchiveError(std::move(operation), std::move(message)));
        }

        Node* Target() noexcept
        {
            if (m_field_target != nullptr)
                return m_field_target;
            return m_objects.empty() ? &m_root : nullptr;
        }

        template <typename Write> Core::Result<void> WriteScalar(Node::Kind kind, Write write, std::string operation)
        {
            Node* target = Target();
            if (target == nullptr || target->kind != Node::Kind::Empty)
                return Fail(std::move(operation), "No empty value is available");
            target->kind = kind;
            write(*target);
            m_field_target = nullptr;
            return Core::Result<void>::Success();
        }

        Node m_root;
        std::vector<Node*> m_objects;
        Node* m_field_target = nullptr;
    };

    class MemoryReader final : public Serialization::ArchiveReader
    {
    public:
        explicit MemoryReader(const Node& root) noexcept : m_current(&root) {}

        Core::Result<void> BeginObject(std::string_view expected_stable_type_name) override
        {
            if (m_current == nullptr || m_current->kind != Node::Kind::Object ||
                m_current->type_name != expected_stable_type_name)
                return Fail<void>("BeginObject", "Object type does not match");
            m_objects.push_back({m_current, 0});
            return Core::Result<void>::Success();
        }

        Core::Result<std::optional<std::string>> NextField() override
        {
            if (m_objects.empty())
                return Fail<std::optional<std::string>>("NextField", "No object is open");
            Context& context = m_objects.back();
            if (context.next_field == context.object->fields.size())
                return Core::Result<std::optional<std::string>>::Success(std::nullopt);
            const auto& field = context.object->fields[context.next_field++];
            m_current = &field.second;
            return Core::Result<std::optional<std::string>>::Success(field.first);
        }

        Core::Result<void> EndObject() override
        {
            if (m_objects.empty() || m_objects.back().next_field != m_objects.back().object->fields.size())
                return Fail<void>("EndObject", "Object was not fully read");
            m_current = m_objects.back().object;
            m_objects.pop_back();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EndField() override
        {
            if (m_objects.empty())
                return Fail<void>("EndField", "No parent object is open");
            m_current = m_objects.back().object;
            return Core::Result<void>::Success();
        }

        Core::Result<bool> ReadBool() override
        {
            if (!Is(Node::Kind::Bool))
                return Fail<bool>("ReadBool", "Expected bool");
            return Core::Result<bool>::Success(m_current->bool_value);
        }

        Core::Result<std::int64_t> ReadSigned() override
        {
            if (!Is(Node::Kind::Signed))
                return Fail<std::int64_t>("ReadSigned", "Expected signed integer");
            return Core::Result<std::int64_t>::Success(m_current->signed_value);
        }

        Core::Result<std::uint64_t> ReadUnsigned() override
        {
            if (!Is(Node::Kind::Unsigned))
                return Fail<std::uint64_t>("ReadUnsigned", "Expected unsigned integer");
            return Core::Result<std::uint64_t>::Success(m_current->unsigned_value);
        }

        Core::Result<float> ReadFloat() override
        {
            if (!Is(Node::Kind::Float))
                return Fail<float>("ReadFloat", "Expected float");
            return Core::Result<float>::Success(m_current->float_value);
        }

        Core::Result<std::string> ReadString() override
        {
            if (!Is(Node::Kind::String))
                return Fail<std::string>("ReadString", "Expected string");
            return Core::Result<std::string>::Success(m_current->string_value);
        }

    private:
        struct Context final
        {
            const Node* object = nullptr;
            std::size_t next_field = 0;
        };

        template <typename T> Core::Result<T> Fail(std::string operation, std::string message)
        {
            return Core::Result<T>::Failure(ArchiveError(std::move(operation), std::move(message)));
        }

        [[nodiscard]] bool Is(Node::Kind kind) const noexcept
        {
            return m_current != nullptr && m_current->kind == kind;
        }

        const Node* m_current = nullptr;
        std::vector<Context> m_objects;
    };

    template <typename T> void DefaultConstruct(void* storage)
    {
        std::construct_at(static_cast<T*>(storage));
    }

    template <typename T> void Destroy(void* object) noexcept
    {
        std::destroy_at(static_cast<T*>(object));
    }

    template <typename T> void CopyAssign(void* target, const void* source)
    {
        *static_cast<T*>(target) = *static_cast<const T*>(source);
    }

    std::uint64_t ReadMode(const void* value) noexcept
    {
        return static_cast<std::uint8_t>(*static_cast<const TestMode*>(value));
    }

    bool WriteMode(void* value, std::uint64_t raw) noexcept
    {
        if (raw > (std::numeric_limits<std::uint8_t>::max)())
            return false;
        *static_cast<TestMode*>(value) = static_cast<TestMode>(raw);
        return true;
    }

    void* RequiredMutable(void* owner) noexcept
    {
        return &static_cast<OptionalRecord*>(owner)->required;
    }

    const void* RequiredConst(const void* owner) noexcept
    {
        return &static_cast<const OptionalRecord*>(owner)->required;
    }

    void* OptionalMutable(void* owner) noexcept
    {
        return &static_cast<OptionalRecord*>(owner)->optional;
    }

    const void* OptionalConst(const void* owner) noexcept
    {
        return &static_cast<const OptionalRecord*>(owner)->optional;
    }

    bool Vec3Equals(const Math::Vec3& lhs, const Math::Vec3& rhs)
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
    }

    int FailTest(int code, std::string_view message)
    {
        std::cerr << message << '\n';
        return code;
    }
} // namespace

int main()
{
    using namespace GE;

    Reflection::TypeRegistry types;
    Serialization::SerializationRegistry serialization;
    if (!Reflection::RegisterBuiltinTypes(types) ||
        !Serialization::RegisterBuiltinSerialization(types, serialization) ||
        !Asset::RegisterAssetSerialization(types, serialization) || !Runtime::RegisterBuiltinComponentReflection(types))
        return FailTest(1, "reflection registration failed");

    const auto text_round_trip = [&]<typename T>(const T& source)
    {
        std::stringstream stream;
        Serialization::TextArchiveWriter writer(stream);
        if (!Serialization::Serialize({&source, Reflection::GetTypeId<T>()}, writer, types, serialization))
            return false;
        T target{};
        Serialization::TextArchiveReader reader(stream);
        return Serialization::DeserializeInto({&target, Reflection::GetTypeId<T>()}, reader, types, serialization) &&
               reader.Finish() && target == source;
    };

    Reflection::EnumInfo mode_info;
    mode_info.underlying_type_id = Reflection::GetTypeId<std::uint8_t>();
    mode_info.values = {{"Disabled", 0}, {"Enabled", 1}};
    mode_info.read_raw_value = &ReadMode;
    mode_info.write_raw_value = &WriteMode;
    if (!types.RegisterType(Reflection::TypeInfo(
            Reflection::GetTypeId<TestMode>(), Reflection::TypeKind::Enum,
            std::string(Reflection::GetStableTypeName<TestMode>()), sizeof(TestMode), alignof(TestMode), {}, {},
            Reflection::TypeFlags::None, {&DefaultConstruct<TestMode>, &Destroy<TestMode>, &CopyAssign<TestMode>},
            std::move(mode_info))))
        return FailTest(2, "enum registration failed");

    const Reflection::TypeId optional_id = Reflection::GetTypeId<OptionalRecord>();
    std::vector<Reflection::FieldInfo> optional_fields;
    optional_fields.emplace_back(optional_id, Reflection::GetTypeId<std::int32_t>(), "required",
                                 Reflection::FieldInfo::FieldFlags::Required, &RequiredMutable, &RequiredConst,
                                 "required");
    optional_fields.emplace_back(optional_id, Reflection::GetTypeId<std::int32_t>(), "optional",
                                 Reflection::FieldInfo::FieldFlags::None, &OptionalMutable, &OptionalConst, "optional");
    if (!types.RegisterType(Reflection::TypeInfo(
            optional_id, Reflection::TypeKind::Struct, std::string(Reflection::GetStableTypeName<OptionalRecord>()),
            sizeof(OptionalRecord), alignof(OptionalRecord), std::move(optional_fields), {},
            Reflection::TypeFlags::Serializable,
            {&DefaultConstruct<OptionalRecord>, &Destroy<OptionalRecord>, &CopyAssign<OptionalRecord>})))
        return FailTest(3, "optional record registration failed");

    if (!types.Freeze() || !serialization.Freeze(types))
        return FailTest(4, "registry freeze failed");

    if (!text_round_trip(1.2345678901234567) || !text_round_trip(char(-12)) || !text_round_trip(long(-123456)) ||
        !text_round_trip(Math::Vec2{1.25F, -3.5F}) || !text_round_trip(Math::Vec4{1, 2, 3, 4}))
        return FailTest(25, "text scalar or vector round trip failed");

    Runtime::TransformComponent transform;
    transform.translation = {1.0F, 2.0F, 3.0F};
    transform.rotation_degrees = {-10.0F, 20.0F, 30.0F};
    transform.scale = {4.0F, 5.0F, 6.0F};
    MemoryWriter transform_writer;
    if (!Serialization::Serialize({&transform, Reflection::GetTypeId<Runtime::TransformComponent>()}, transform_writer,
                                  types, serialization))
        return FailTest(5, "Transform serialization failed");

    Runtime::TransformComponent decoded_transform;
    MemoryReader transform_reader(transform_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_transform, Reflection::GetTypeId<Runtime::TransformComponent>()},
                                        transform_reader, types, serialization) ||
        !Vec3Equals(decoded_transform.translation, transform.translation) ||
        !Vec3Equals(decoded_transform.rotation_degrees, transform.rotation_degrees) ||
        !Vec3Equals(decoded_transform.scale, transform.scale))
        return FailTest(6, "Transform round trip failed");

    Runtime::CameraComponent camera;
    camera.vertical_field_of_view_degrees = 75.0F;
    camera.near_plane = 0.25F;
    camera.far_plane = 500.0F;
    camera.primary = true;
    MemoryWriter camera_writer;
    Runtime::CameraComponent decoded_camera;
    if (!Serialization::Serialize({&camera, Reflection::GetTypeId<Runtime::CameraComponent>()}, camera_writer, types,
                                  serialization))
        return FailTest(7, "Camera serialization failed");
    MemoryReader camera_reader(camera_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_camera, Reflection::GetTypeId<Runtime::CameraComponent>()},
                                        camera_reader, types, serialization) ||
        decoded_camera.vertical_field_of_view_degrees != 75.0F || decoded_camera.near_plane != 0.25F ||
        decoded_camera.far_plane != 500.0F || !decoded_camera.primary)
        return FailTest(8, "Camera round trip failed");

    Runtime::MeshRendererComponent mesh;
    MemoryWriter mesh_writer;
    mesh.mesh.value = 7;
    mesh.material.value = 9;
    mesh.visible = false;
    Runtime::MeshRendererComponent decoded_mesh;
    if (!Serialization::Serialize({&mesh, Reflection::GetTypeId<Runtime::MeshRendererComponent>()}, mesh_writer, types,
                                  serialization))
        return FailTest(9, "MeshRenderer serialization failed");
    MemoryReader mesh_reader(mesh_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_mesh, Reflection::GetTypeId<Runtime::MeshRendererComponent>()},
                                        mesh_reader, types, serialization) ||
        decoded_mesh.mesh.value != 7 || decoded_mesh.material.value != 9 || decoded_mesh.visible)
        return FailTest(9, "MeshRenderer round trip failed");

    Node unknown = transform_writer.Root();
    unknown.fields.emplace_back("future_field", Node{});
    unknown.fields.back().second.kind = Node::Kind::Float;
    unknown.fields.back().second.float_value = 1.0F;
    Runtime::TransformComponent unchanged;
    unchanged.translation = {91.0F, 92.0F, 93.0F};
    MemoryReader unknown_reader(unknown);
    Core::Result<void> unknown_result = Serialization::DeserializeInto(
        {&unchanged, Reflection::GetTypeId<Runtime::TransformComponent>()}, unknown_reader, types, serialization);
    if (unknown_result || !Vec3Equals(unchanged.translation, {91.0F, 92.0F, 93.0F}) ||
        !unknown_result.Error().resource.has_value() ||
        unknown_result.Error().resource->find("future_field") == std::string::npos)
        return FailTest(10, "unknown-field failure was not transactional or path-aware");

    Node missing = transform_writer.Root();
    std::erase_if(missing.fields, [](const auto& field) { return field.first == "scale"; });
    unchanged.translation = {81.0F, 82.0F, 83.0F};
    MemoryReader missing_reader(missing);
    Core::Result<void> missing_result = Serialization::DeserializeInto(
        {&unchanged, Reflection::GetTypeId<Runtime::TransformComponent>()}, missing_reader, types, serialization);
    if (missing_result || !Vec3Equals(unchanged.translation, {81.0F, 82.0F, 83.0F}) ||
        !missing_result.Error().resource.has_value() ||
        missing_result.Error().resource->find("scale") == std::string::npos)
        return FailTest(11, "missing required field was not rejected transactionally");

    Node duplicate = transform_writer.Root();
    duplicate.fields.push_back(duplicate.fields.front());
    MemoryReader duplicate_reader(duplicate);
    if (Serialization::DeserializeInto({&unchanged, Reflection::GetTypeId<Runtime::TransformComponent>()},
                                       duplicate_reader, types, serialization))
        return FailTest(12, "duplicate field was accepted");

    OptionalRecord optional_source;
    optional_source.required = 99;
    optional_source.optional = 100;
    MemoryWriter optional_writer;
    if (!Serialization::Serialize({&optional_source, Reflection::GetTypeId<OptionalRecord>()}, optional_writer, types,
                                  serialization))
        return FailTest(13, "optional record serialization failed");
    Node without_optional = optional_writer.Root();
    std::erase_if(without_optional.fields, [](const auto& field) { return field.first == "optional"; });
    OptionalRecord optional_destination;
    optional_destination.required = -1;
    optional_destination.optional = -1;
    MemoryReader optional_reader(without_optional);
    if (!Serialization::DeserializeInto({&optional_destination, Reflection::GetTypeId<OptionalRecord>()},
                                        optional_reader, types, serialization) ||
        optional_destination.required != 99 || optional_destination.optional != 42)
        return FailTest(14, "missing optional field did not retain its type default");

    Math::Vec3 special{(std::numeric_limits<float>::quiet_NaN)(), (std::numeric_limits<float>::infinity)(),
                       -(std::numeric_limits<float>::infinity)()};
    MemoryWriter vec_writer;
    Math::Vec3 decoded_special{};
    if (!Serialization::Serialize({&special, Reflection::GetTypeId<Math::Vec3>()}, vec_writer, types, serialization))
        return FailTest(15, "Vec3 serialization failed");
    MemoryReader vec_reader(vec_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_special, Reflection::GetTypeId<Math::Vec3>()}, vec_reader, types,
                                        serialization) ||
        !std::isnan(decoded_special[0]) || !std::isinf(decoded_special[1]) || !std::isinf(decoded_special[2]) ||
        decoded_special[2] > 0.0F)
        return FailTest(16, "Vec3 non-finite values did not round trip");

    Asset::AssetId zero_id;
    Asset::AssetId decoded_id(std::array<std::uint8_t, 16>{1});
    MemoryWriter asset_writer;
    if (!Serialization::Serialize({&zero_id, Reflection::GetTypeId<Asset::AssetId>()}, asset_writer, types,
                                  serialization))
        return FailTest(17, "AssetId serialization failed");
    MemoryReader asset_reader(asset_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_id, Reflection::GetTypeId<Asset::AssetId>()}, asset_reader, types,
                                        serialization) ||
        decoded_id != zero_id)
        return FailTest(18, "zero AssetId did not round trip");

    Node overflow;
    overflow.kind = Node::Kind::Signed;
    overflow.signed_value = 128;
    std::int8_t byte_value = 7;
    MemoryReader overflow_reader(overflow);
    if (Serialization::DeserializeInto({&byte_value, Reflection::GetTypeId<std::int8_t>()}, overflow_reader, types,
                                       serialization) ||
        byte_value != 7)
        return FailTest(19, "integer overflow was accepted or changed the destination");

    TestMode mode = TestMode::Enabled;
    TestMode decoded_mode = TestMode::Disabled;
    MemoryWriter enum_writer;
    if (!Serialization::Serialize({&mode, Reflection::GetTypeId<TestMode>()}, enum_writer, types, serialization))
        return FailTest(20, "enum serialization failed");
    MemoryReader enum_reader(enum_writer.Root());
    if (!Serialization::DeserializeInto({&decoded_mode, Reflection::GetTypeId<TestMode>()}, enum_reader, types,
                                        serialization) ||
        decoded_mode != TestMode::Enabled)
        return FailTest(21, "enum round trip failed");

    Reflection::TypeRegistry invalid_types;
    Serialization::SerializationRegistry invalid_serialization;
    if (!Reflection::RegisterBuiltinTypes(invalid_types) ||
        !Serialization::RegisterBuiltinSerialization(invalid_types, invalid_serialization))
        return FailTest(22, "invalid-schema test setup failed");
    std::vector<Reflection::FieldInfo> read_only_fields;
    read_only_fields.emplace_back(optional_id, Reflection::GetTypeId<std::int32_t>(), "required",
                                  Reflection::FieldInfo::FieldFlags::ReadOnly, nullptr, &RequiredConst, "required");
    if (!invalid_types.RegisterType(Reflection::TypeInfo(
            optional_id, Reflection::TypeKind::Struct, std::string(Reflection::GetStableTypeName<OptionalRecord>()),
            sizeof(OptionalRecord), alignof(OptionalRecord), std::move(read_only_fields), {},
            Reflection::TypeFlags::Serializable,
            {&DefaultConstruct<OptionalRecord>, &Destroy<OptionalRecord>, &CopyAssign<OptionalRecord>})) ||
        !invalid_types.Freeze())
        return FailTest(23, "invalid-schema reflection setup failed");
    Core::Result<void> invalid_freeze = invalid_serialization.Freeze(invalid_types);
    if (invalid_freeze || invalid_freeze.Error().domain != Core::ErrorDomain::Serialization ||
        !invalid_freeze.Error().resource.has_value() ||
        invalid_freeze.Error().resource->find("required") == std::string::npos)
        return FailTest(24, "persistent read-only field passed schema freeze");

    return 0;
}
