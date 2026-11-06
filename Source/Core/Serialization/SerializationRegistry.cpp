#include "SerializationRegistry.h"

#include <functional>
#include <string>
#include <unordered_set>

namespace GE::Serialization
{
    namespace
    {
        [[nodiscard]] Core::Result<void> Failure(Core::ErrorCode code, std::string operation, std::string message,
                                                 std::string resource = {})
        {
            Core::EngineError error;
            error.domain = Core::ErrorDomain::Serialization;
            error.code = code;
            error.operation = std::move(operation);
            error.message = std::move(message);
            if (!resource.empty())
                error.resource = std::move(resource);
            return Core::Result<void>::Failure(std::move(error));
        }
    } // namespace

    Core::Result<void> SerializationRegistry::Register(ValueCodec codec)
    {
        if (m_frozen)
            return Failure(Core::ErrorCode::InvalidState, "SerializationRegistry::Register", "Registry is frozen");
        if (!codec.type_id.IsValid() || codec.encode == nullptr || codec.decode == nullptr)
            return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Register", "Codec is incomplete");
        if (Find(codec.type_id) != nullptr)
            return Failure(Core::ErrorCode::Conflict, "SerializationRegistry::Register", "Codec is already registered",
                           std::to_string(codec.type_id.Value()));

        const std::size_t index = m_codecs.size();
        m_indices.emplace(codec.type_id, index);
        m_codecs.push_back(codec);
        return Core::Result<void>::Success();
    }

    Core::Result<void> SerializationRegistry::Freeze(const Reflection::TypeRegistry& type_registry)
    {
        if (m_frozen)
            return Failure(Core::ErrorCode::InvalidState, "SerializationRegistry::Freeze",
                           "Registry is already frozen");
        if (!type_registry.IsFrozen())
            return Failure(Core::ErrorCode::InvalidState, "SerializationRegistry::Freeze",
                           "Type registry must be frozen first");

        for (const ValueCodec& codec : m_codecs)
        {
            if (type_registry.FindType(codec.type_id) == nullptr)
                return Failure(Core::ErrorCode::NotFound, "SerializationRegistry::Freeze",
                               "Codec type is not registered", std::to_string(codec.type_id.Value()));
        }

        std::unordered_set<Reflection::TypeId> visiting;
        std::unordered_set<Reflection::TypeId> validated;
        std::function<Core::Result<void>(const Reflection::TypeInfo&)> validate_type;
        validate_type = [&](const Reflection::TypeInfo& type_info) -> Core::Result<void>
        {
            if (validated.contains(type_info.GetTypeId()))
                return Core::Result<void>::Success();
            if (!visiting.emplace(type_info.GetTypeId()).second)
                return Failure(Core::ErrorCode::Conflict, "SerializationRegistry::Freeze",
                               "Recursive value schema is not supported", std::string(type_info.GetStableName()));
            if (!type_info.GetOperations().IsComplete())
                return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Freeze",
                               "Serializable type lacks complete type operations",
                               std::string(type_info.GetStableName()));

            std::unordered_set<std::string_view> serialized_names;
            for (const Reflection::FieldInfo& field : type_info.GetFields())
            {
                const std::string path = std::string(type_info.GetStableName()) + "." + std::string(field.GetName());
                if (field.IsTransient())
                {
                    if (field.IsRequired())
                        return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Freeze",
                                       "Transient field cannot be required", path);
                    continue;
                }
                if (field.IsReadOnly())
                    return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Freeze",
                                   "Persistent read-only field is not supported", path);
                if (field.GetSerializedName().empty())
                    return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Freeze",
                                   "Persistent field lacks SerializedName", path);
                if (!serialized_names.emplace(field.GetSerializedName()).second)
                    return Failure(Core::ErrorCode::Conflict, "SerializationRegistry::Freeze",
                                   "Duplicate SerializedName", path);

                const Reflection::TypeInfo* field_type = type_registry.FindType(field.GetValueTypeId());
                if (field_type == nullptr)
                    return Failure(Core::ErrorCode::NotFound, "SerializationRegistry::Freeze",
                                   "Field type is not registered", path);
                if (Find(field_type->GetTypeId()) != nullptr || field_type->GetTypeKind() == Reflection::TypeKind::Enum)
                    continue;
                if (!field_type->IsSerializable())
                    return Failure(Core::ErrorCode::InvalidArgument, "SerializationRegistry::Freeze",
                                   "Field type is not serializable", path);
                Core::Result<void> nested = validate_type(*field_type);
                if (!nested)
                    return nested;
            }

            visiting.erase(type_info.GetTypeId());
            validated.emplace(type_info.GetTypeId());
            return Core::Result<void>::Success();
        };

        for (const Reflection::TypeInfo* type_info : type_registry.GetTypes())
        {
            if (type_info->IsSerializable())
            {
                Core::Result<void> result = validate_type(*type_info);
                if (!result)
                    return result;
            }
        }

        m_frozen = true;
        return Core::Result<void>::Success();
    }

    bool SerializationRegistry::IsFrozen() const noexcept
    {
        return m_frozen;
    }

    const ValueCodec* SerializationRegistry::Find(Reflection::TypeId type_id) const noexcept
    {
        const auto found = m_indices.find(type_id);
        return found == m_indices.end() ? nullptr : &m_codecs[found->second];
    }
} // namespace GE::Serialization
