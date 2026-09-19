#include "BuiltinSerialization.h"

#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>

namespace GE::Serialization
{
    namespace
    {
        [[nodiscard]] Core::Result<void> Failure(
            std::string operation, std::string message)
        {
            return Core::Result<void>::Failure(Core::EngineError{
                .domain = Core::ErrorDomain::Serialization,
                .code = Core::ErrorCode::ParseFailure,
                .operation = std::move(operation),
                .message = std::move(message)});
        }

        template<typename T>
        T* Require(Reflection::ValueView value) noexcept
        {
            return value.IsValid() && value.GetTypeId() == Reflection::GetTypeId<T>()
                ? static_cast<T*>(value.Data()) : nullptr;
        }

        template<typename T>
        const T* Require(Reflection::ConstValueView value) noexcept
        {
            return value.IsValid() && value.GetTypeId() == Reflection::GetTypeId<T>()
                ? static_cast<const T*>(value.Data()) : nullptr;
        }

        template<typename T>
        Core::Result<void> EncodeInteger(
            Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const T* typed = Require<T>(value);
            if (typed == nullptr) return Failure("EncodeInteger", "Type mismatch");
            if constexpr (std::is_signed_v<T>)
                return archive.WriteSigned(static_cast<std::int64_t>(*typed));
            else
                return archive.WriteUnsigned(static_cast<std::uint64_t>(*typed));
        }

        template<typename T>
        Core::Result<void> DecodeInteger(
            Reflection::ValueView value, ArchiveReader& archive)
        {
            T* typed = Require<T>(value);
            if (typed == nullptr) return Failure("DecodeInteger", "Type mismatch");
            if constexpr (std::is_signed_v<T>)
            {
                Core::Result<std::int64_t> result = archive.ReadSigned();
                if (!result) return Core::Result<void>::Failure(std::move(result.Error()));
                if (result.Value() < static_cast<std::int64_t>((std::numeric_limits<T>::min)())
                    || result.Value() > static_cast<std::int64_t>((std::numeric_limits<T>::max)()))
                    return Failure("DecodeInteger", "Signed integer is out of range");
                *typed = static_cast<T>(result.Value());
            }
            else
            {
                Core::Result<std::uint64_t> result = archive.ReadUnsigned();
                if (!result) return Core::Result<void>::Failure(std::move(result.Error()));
                if (result.Value() > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))
                    return Failure("DecodeInteger", "Unsigned integer is out of range");
                *typed = static_cast<T>(result.Value());
            }
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeBool(
            Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const bool* typed = Require<bool>(value);
            return typed == nullptr ? Failure("EncodeBool", "Type mismatch")
                : archive.WriteBool(*typed);
        }

        Core::Result<void> DecodeBool(
            Reflection::ValueView value, ArchiveReader& archive)
        {
            bool* typed = Require<bool>(value);
            if (typed == nullptr) return Failure("DecodeBool", "Type mismatch");
            Core::Result<bool> result = archive.ReadBool();
            if (!result) return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = result.Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeFloat(
            Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const float* typed = Require<float>(value);
            return typed == nullptr ? Failure("EncodeFloat", "Type mismatch")
                : archive.WriteFloat(*typed);
        }

        Core::Result<void> DecodeFloat(
            Reflection::ValueView value, ArchiveReader& archive)
        {
            float* typed = Require<float>(value);
            if (typed == nullptr) return Failure("DecodeFloat", "Type mismatch");
            Core::Result<float> result = archive.ReadFloat();
            if (!result) return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = result.Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeString(
            Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const std::string* typed = Require<std::string>(value);
            return typed == nullptr ? Failure("EncodeString", "Type mismatch")
                : archive.WriteString(*typed);
        }

        Core::Result<void> DecodeString(
            Reflection::ValueView value, ArchiveReader& archive)
        {
            std::string* typed = Require<std::string>(value);
            if (typed == nullptr) return Failure("DecodeString", "Type mismatch");
            Core::Result<std::string> result = archive.ReadString();
            if (!result) return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = std::move(result).Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeVec3(
            Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const Math::Vec3* typed = Require<Math::Vec3>(value);
            if (typed == nullptr) return Failure("EncodeVec3", "Type mismatch");
            Core::Result<void> result = archive.BeginObject("MEngine.Math.Vec3");
            if (!result) return result;
            constexpr const char* names[] = {"x", "y", "z"};
            for (std::size_t index = 0; index < 3; ++index)
            {
                result = archive.BeginField(names[index]);
                if (!result) return result;
                result = archive.WriteFloat((*typed)[index]);
                if (!result) return result;
                result = archive.EndField();
                if (!result) return result;
            }
            return archive.EndObject();
        }

        Core::Result<void> DecodeVec3(
            Reflection::ValueView value, ArchiveReader& archive)
        {
            Math::Vec3* typed = Require<Math::Vec3>(value);
            if (typed == nullptr) return Failure("DecodeVec3", "Type mismatch");
            Core::Result<void> result = archive.BeginObject("MEngine.Math.Vec3");
            if (!result) return result;
            bool seen[3]{};
            while (true)
            {
                Core::Result<std::optional<std::string>> next = archive.NextField();
                if (!next) return Core::Result<void>::Failure(std::move(next.Error()));
                if (!next.Value().has_value()) break;
                const std::string& name = *next.Value();
                std::size_t index = name == "x" ? 0 : name == "y" ? 1 : name == "z" ? 2 : 3;
                if (index == 3) return Failure("DecodeVec3", "Unknown Vec3 field");
                if (seen[index]) return Failure("DecodeVec3", "Duplicate Vec3 field");
                seen[index] = true;
                Core::Result<float> component = archive.ReadFloat();
                if (!component)
                    return Core::Result<void>::Failure(std::move(component.Error()));
                (*typed)[index] = component.Value();
                result = archive.EndField();
                if (!result) return result;
            }
            result = archive.EndObject();
            if (!result) return result;
            if (!seen[0] || !seen[1] || !seen[2])
                return Failure("DecodeVec3", "Vec3 requires x, y, and z");
            return Core::Result<void>::Success();
        }

        template<typename T>
        void DefaultConstruct(void* storage)
        {
            std::construct_at(static_cast<T*>(storage));
        }
        template<typename T>
        void Destroy(void* object) noexcept { std::destroy_at(static_cast<T*>(object)); }
        template<typename T>
        void CopyAssign(void* target, const void* source)
        { *static_cast<T*>(target) = *static_cast<const T*>(source); }

        template<typename T>
        Core::Result<void> AddIntegerCodec(SerializationRegistry& registry)
        {
            return registry.Register({Reflection::GetTypeId<T>(),
                &EncodeInteger<T>, &DecodeInteger<T>});
        }
    }

    Core::Result<void> RegisterBuiltinSerialization(
        Reflection::TypeRegistry& type_registry,
        SerializationRegistry& serialization_registry)
    {
        Core::Result<void> result = type_registry.RegisterType(Reflection::TypeInfo(
            Reflection::GetTypeId<Math::Vec3>(), Reflection::TypeKind::Struct,
            std::string(Reflection::GetStableTypeName<Math::Vec3>()),
            sizeof(Math::Vec3), alignof(Math::Vec3), {}, {},
            Reflection::TypeFlags::None,
            {&DefaultConstruct<Math::Vec3>, &Destroy<Math::Vec3>,
                &CopyAssign<Math::Vec3>}));
        if (!result) return result;

        result = serialization_registry.Register({Reflection::GetTypeId<bool>(),
            &EncodeBool, &DecodeBool});
        if (!result) return result;
        result = AddIntegerCodec<std::int8_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::uint8_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::int16_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::uint16_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::int32_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::uint32_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::int64_t>(serialization_registry); if (!result) return result;
        result = AddIntegerCodec<std::uint64_t>(serialization_registry); if (!result) return result;
        result = serialization_registry.Register({Reflection::GetTypeId<float>(),
            &EncodeFloat, &DecodeFloat}); if (!result) return result;
        result = serialization_registry.Register({Reflection::GetTypeId<std::string>(),
            &EncodeString, &DecodeString}); if (!result) return result;
        return serialization_registry.Register({Reflection::GetTypeId<Math::Vec3>(),
            &EncodeVec3, &DecodeVec3});
    }
}
