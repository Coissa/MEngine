#include "Serializer.h"

#include <new>
#include <string>
#include <vector>

namespace GE::Serialization
{
    namespace
    {
        [[nodiscard]] Core::Result<void> Failure(
            Core::ErrorCode code, std::string operation, std::string message,
            std::string resource = {})
        {
            Core::EngineError error;
            error.domain = Core::ErrorDomain::Serialization;
            error.code = code;
            error.operation = std::move(operation);
            error.message = std::move(message);
            if (!resource.empty()) error.resource = std::move(resource);
            return Core::Result<void>::Failure(std::move(error));
        }

        [[nodiscard]] Core::Result<void> AtPath(
            Core::Result<void> result, const std::string& path)
        {
            if (result) return result;
            Core::EngineError error = std::move(result.Error());
            if (!error.resource.has_value()) error.resource = path;
            return Core::Result<void>::Failure(std::move(error));
        }

        [[nodiscard]] Core::Result<void> SerializeValue(
            Reflection::ConstValueView value, ArchiveWriter& archive,
            const Reflection::TypeRegistry& types,
            const SerializationRegistry& codecs,
            const std::string& path)
        {
            const Reflection::TypeInfo* type_info = types.FindType(value.GetTypeId());
            if (!value.IsValid() || type_info == nullptr)
                return Failure(Core::ErrorCode::InvalidArgument,
                    "Serialize", "Value or reflected type is invalid", path);

            if (const ValueCodec* codec = codecs.Find(value.GetTypeId()))
                return AtPath(codec->encode(value, archive), path);

            if (type_info->GetTypeKind() == Reflection::TypeKind::Enum)
            {
                const Reflection::EnumInfo* enum_info = type_info->GetEnumInfo();
                const std::uint64_t raw = enum_info->read_raw_value(value.Data());
                for (const Reflection::EnumValueInfo& entry : enum_info->values)
                {
                    if (entry.raw_value == raw)
                        return AtPath(archive.WriteString(entry.name), path);
                }
                return Failure(Core::ErrorCode::InvalidArgument,
                    "Serialize", "Enum value has no registered name", path);
            }

            if (!type_info->IsSerializable())
                return Failure(Core::ErrorCode::InvalidArgument,
                    "Serialize", "Type is not marked Serializable", path);
            Core::Result<void> result = archive.BeginObject(type_info->GetStableName());
            if (!result) return AtPath(std::move(result), path);
            for (const Reflection::FieldInfo& field : type_info->GetFields())
            {
                if (field.IsTransient()) continue;
                const std::string field_path = path + "."
                    + std::string(field.GetSerializedName());
                result = archive.BeginField(field.GetSerializedName());
                if (!result) return AtPath(std::move(result), field_path);
                result = SerializeValue(field.GetValue(value), archive, types,
                    codecs, field_path);
                if (!result) return result;
                result = archive.EndField();
                if (!result) return AtPath(std::move(result), field_path);
            }
            return AtPath(archive.EndObject(), path);
        }

        [[nodiscard]] Core::Result<void> DeserializeValue(
            Reflection::ValueView value, ArchiveReader& archive,
            const Reflection::TypeRegistry& types,
            const SerializationRegistry& codecs,
            const std::string& path)
        {
            const Reflection::TypeInfo* type_info = types.FindType(value.GetTypeId());
            if (!value.IsValid() || type_info == nullptr)
                return Failure(Core::ErrorCode::InvalidArgument,
                    "DeserializeInto", "Value or reflected type is invalid", path);

            if (const ValueCodec* codec = codecs.Find(value.GetTypeId()))
                return AtPath(codec->decode(value, archive), path);

            if (type_info->GetTypeKind() == Reflection::TypeKind::Enum)
            {
                Core::Result<std::string> name_result = archive.ReadString();
                if (!name_result)
                    return AtPath(Core::Result<void>::Failure(
                        std::move(name_result.Error())), path);
                const Reflection::EnumInfo* enum_info = type_info->GetEnumInfo();
                for (const Reflection::EnumValueInfo& entry : enum_info->values)
                {
                    if (entry.name == name_result.Value())
                    {
                        if (enum_info->write_raw_value(value.Data(), entry.raw_value))
                            return Core::Result<void>::Success();
                        break;
                    }
                }
                return Failure(Core::ErrorCode::ParseFailure,
                    "DeserializeInto", "Unknown or unrepresentable enum name", path);
            }

            if (!type_info->IsSerializable())
                return Failure(Core::ErrorCode::InvalidArgument,
                    "DeserializeInto", "Type is not marked Serializable", path);
            Core::Result<void> result = archive.BeginObject(type_info->GetStableName());
            if (!result) return AtPath(std::move(result), path);

            const auto fields = type_info->GetFields();
            std::vector<bool> seen(fields.size(), false);
            while (true)
            {
                Core::Result<std::optional<std::string>> next = archive.NextField();
                if (!next)
                    return AtPath(Core::Result<void>::Failure(
                        std::move(next.Error())), path);
                if (!next.Value().has_value()) break;
                const std::string& serialized_name = *next.Value();
                const Reflection::FieldInfo* field =
                    type_info->FindFieldBySerializedName(serialized_name);
                if (field == nullptr || field->IsTransient())
                    return Failure(Core::ErrorCode::ParseFailure,
                        "DeserializeInto", "Unknown field", path + "." + serialized_name);
                const std::size_t index = static_cast<std::size_t>(field - fields.data());
                if (seen[index])
                    return Failure(Core::ErrorCode::Conflict,
                        "DeserializeInto", "Duplicate field", path + "." + serialized_name);
                seen[index] = true;
                result = DeserializeValue(field->GetValue(value), archive, types,
                    codecs, path + "." + serialized_name);
                if (!result) return result;
                result = archive.EndField();
                if (!result) return AtPath(std::move(result), path + "." + serialized_name);
            }
            result = archive.EndObject();
            if (!result) return AtPath(std::move(result), path);
            for (std::size_t index = 0; index < fields.size(); ++index)
            {
                if (!fields[index].IsTransient() && fields[index].IsRequired()
                    && !seen[index])
                    return Failure(Core::ErrorCode::NotFound,
                        "DeserializeInto", "Required field is missing",
                        path + "." + std::string(fields[index].GetSerializedName()));
            }
            return Core::Result<void>::Success();
        }
    }

    Core::Result<void> Serialize(
        Reflection::ConstValueView value, ArchiveWriter& archive,
        const Reflection::TypeRegistry& type_registry,
        const SerializationRegistry& serialization_registry)
    {
        if (!type_registry.IsFrozen() || !serialization_registry.IsFrozen())
            return Failure(Core::ErrorCode::InvalidState, "Serialize",
                "Type and serialization registries must be frozen");
        const Reflection::TypeInfo* type_info =
            type_registry.FindType(value.GetTypeId());
        const std::string path = type_info == nullptr
            ? "<unknown>" : std::string(type_info->GetStableName());
        return SerializeValue(value, archive, type_registry,
            serialization_registry, path);
    }

    Core::Result<void> DeserializeInto(
        Reflection::ValueView destination, ArchiveReader& archive,
        const Reflection::TypeRegistry& type_registry,
        const SerializationRegistry& serialization_registry)
    {
        if (!type_registry.IsFrozen() || !serialization_registry.IsFrozen())
            return Failure(Core::ErrorCode::InvalidState, "DeserializeInto",
                "Type and serialization registries must be frozen");
        if (!destination.IsValid())
            return Failure(Core::ErrorCode::InvalidArgument,
                "DeserializeInto", "Destination is invalid");
        const Reflection::TypeInfo* type_info =
            type_registry.FindType(destination.GetTypeId());
        if (type_info == nullptr || !type_info->GetOperations().IsComplete())
            return Failure(Core::ErrorCode::InvalidArgument,
                "DeserializeInto", "Destination type lacks type operations");

        const Reflection::TypeOperations& operations = type_info->GetOperations();
        void* temporary = nullptr;
        bool constructed = false;
        try
        {
            temporary = ::operator new(type_info->GetSize(),
                std::align_val_t(type_info->GetAlignment()));
            operations.default_construct(temporary);
            constructed = true;
            Core::Result<void> result = DeserializeValue(
                Reflection::ValueView(temporary, type_info->GetTypeId()), archive,
                type_registry, serialization_registry,
                std::string(type_info->GetStableName()));
            if (result) operations.copy_assign(destination.Data(), temporary);
            operations.destroy(temporary);
            ::operator delete(temporary, std::align_val_t(type_info->GetAlignment()));
            return result;
        }
        catch (...)
        {
            if (constructed) operations.destroy(temporary);
            if (temporary != nullptr)
                ::operator delete(temporary,
                    std::align_val_t(type_info->GetAlignment()));
            return Failure(Core::ErrorCode::InvalidState, "DeserializeInto",
                "Type operation threw an exception",
                std::string(type_info->GetStableName()));
        }
    }
}
