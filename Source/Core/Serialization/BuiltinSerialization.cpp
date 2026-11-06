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
        [[nodiscard]] Core::Result<void> Failure(std::string operation, std::string message)
        {
            return Core::Result<void>::Failure(Core::EngineError{.domain = Core::ErrorDomain::Serialization,
                                                                 .code = Core::ErrorCode::ParseFailure,
                                                                 .operation = std::move(operation),
                                                                 .message = std::move(message)});
        }

        template <typename T> T* Require(Reflection::ValueView value) noexcept
        {
            return value.IsValid() && value.GetTypeId() == Reflection::GetTypeId<T>() ? static_cast<T*>(value.Data())
                                                                                      : nullptr;
        }

        template <typename T> const T* Require(Reflection::ConstValueView value) noexcept
        {
            return value.IsValid() && value.GetTypeId() == Reflection::GetTypeId<T>()
                       ? static_cast<const T*>(value.Data())
                       : nullptr;
        }

        template <typename T> Core::Result<void> EncodeInteger(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const T* typed = Require<T>(value);
            if (typed == nullptr)
                return Failure("EncodeInteger", "Type mismatch");
            if constexpr (std::is_signed_v<T>)
                return archive.WriteSigned(static_cast<std::int64_t>(*typed));
            else
                return archive.WriteUnsigned(static_cast<std::uint64_t>(*typed));
        }

        template <typename T> Core::Result<void> DecodeInteger(Reflection::ValueView value, ArchiveReader& archive)
        {
            T* typed = Require<T>(value);
            if (typed == nullptr)
                return Failure("DecodeInteger", "Type mismatch");
            if constexpr (std::is_signed_v<T>)
            {
                Core::Result<std::int64_t> result = archive.ReadSigned();
                if (!result)
                    return Core::Result<void>::Failure(std::move(result.Error()));
                if (result.Value() < static_cast<std::int64_t>((std::numeric_limits<T>::min)()) ||
                    result.Value() > static_cast<std::int64_t>((std::numeric_limits<T>::max)()))
                    return Failure("DecodeInteger", "Signed integer is out of range");
                *typed = static_cast<T>(result.Value());
            }
            else
            {
                Core::Result<std::uint64_t> result = archive.ReadUnsigned();
                if (!result)
                    return Core::Result<void>::Failure(std::move(result.Error()));
                if (result.Value() > static_cast<std::uint64_t>((std::numeric_limits<T>::max)()))
                    return Failure("DecodeInteger", "Unsigned integer is out of range");
                *typed = static_cast<T>(result.Value());
            }
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeBool(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const bool* typed = Require<bool>(value);
            return typed == nullptr ? Failure("EncodeBool", "Type mismatch") : archive.WriteBool(*typed);
        }

        Core::Result<void> DecodeBool(Reflection::ValueView value, ArchiveReader& archive)
        {
            bool* typed = Require<bool>(value);
            if (typed == nullptr)
                return Failure("DecodeBool", "Type mismatch");
            Core::Result<bool> result = archive.ReadBool();
            if (!result)
                return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = result.Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeFloat(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const float* typed = Require<float>(value);
            return typed == nullptr ? Failure("EncodeFloat", "Type mismatch") : archive.WriteFloat(*typed);
        }

        Core::Result<void> DecodeFloat(Reflection::ValueView value, ArchiveReader& archive)
        {
            float* typed = Require<float>(value);
            if (typed == nullptr)
                return Failure("DecodeFloat", "Type mismatch");
            Core::Result<float> result = archive.ReadFloat();
            if (!result)
                return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = result.Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeDouble(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            return archive.WriteDouble(*static_cast<const double*>(value.Data()));
        }
        Core::Result<void> DecodeDouble(Reflection::ValueView value, ArchiveReader& archive)
        {
            auto result = archive.ReadDouble();
            if (!result)
                return Core::Result<void>::Failure(std::move(result.Error()));
            *static_cast<double*>(value.Data()) = result.Value();
            return Core::Result<void>::Success();
        }

        Core::Result<void> EncodeString(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const std::string* typed = Require<std::string>(value);
            return typed == nullptr ? Failure("EncodeString", "Type mismatch") : archive.WriteString(*typed);
        }

        Core::Result<void> DecodeString(Reflection::ValueView value, ArchiveReader& archive)
        {
            std::string* typed = Require<std::string>(value);
            if (typed == nullptr)
                return Failure("DecodeString", "Type mismatch");
            Core::Result<std::string> result = archive.ReadString();
            if (!result)
                return Core::Result<void>::Failure(std::move(result.Error()));
            *typed = std::move(result).Value();
            return Core::Result<void>::Success();
        }

        template <typename T, std::size_t N>
        Core::Result<void> EncodeVector(Reflection::ConstValueView value, ArchiveWriter& archive)
        {
            const T* typed = Require<T>(value);
            if (typed == nullptr)
                return Failure("EncodeVector", "Type mismatch");
            Core::Result<void> result = archive.BeginObject(Reflection::GetStableTypeName<T>());
            if (!result)
                return result;
            constexpr const char* names[] = {"x", "y", "z", "w"};
            for (std::size_t index = 0; index < N; ++index)
            {
                result = archive.BeginField(names[index]);
                if (!result)
                    return result;
                result = archive.WriteFloat((*typed)[index]);
                if (!result)
                    return result;
                result = archive.EndField();
                if (!result)
                    return result;
            }
            return archive.EndObject();
        }

        template <typename T, std::size_t N>
        Core::Result<void> DecodeVector(Reflection::ValueView value, ArchiveReader& archive)
        {
            T* typed = Require<T>(value);
            if (typed == nullptr)
                return Failure("DecodeVector", "Type mismatch");
            Core::Result<void> result = archive.BeginObject(Reflection::GetStableTypeName<T>());
            if (!result)
                return result;
            bool seen[N]{};
            while (true)
            {
                Core::Result<std::optional<std::string>> next = archive.NextField();
                if (!next)
                    return Core::Result<void>::Failure(std::move(next.Error()));
                if (!next.Value().has_value())
                    break;
                const std::string& name = *next.Value();
                std::size_t index = name == "x" ? 0 : name == "y" ? 1 : name == "z" ? 2 : name == "w" ? 3 : N;
                if (index >= N)
                    return Failure("DecodeVector", "Unknown vector field");
                if (seen[index])
                    return Failure("DecodeVector", "Duplicate vector field");
                seen[index] = true;
                Core::Result<float> component = archive.ReadFloat();
                if (!component)
                    return Core::Result<void>::Failure(std::move(component.Error()));
                (*typed)[index] = component.Value();
                result = archive.EndField();
                if (!result)
                    return result;
            }
            result = archive.EndObject();
            if (!result)
                return result;
            for (bool present : seen)
                if (!present)
                    return Failure("DecodeVector", "Vector component is missing");
            return Core::Result<void>::Success();
        }

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

        template <typename T> Core::Result<void> AddIntegerCodec(SerializationRegistry& registry)
        {
            return registry.Register({Reflection::GetTypeId<T>(), &EncodeInteger<T>, &DecodeInteger<T>});
        }
    } // namespace

    Core::Result<void> RegisterBuiltinSerialization(Reflection::TypeRegistry& type_registry,
                                                    SerializationRegistry& serialization_registry)
    {
        Core::Result<void> result = type_registry.RegisterType(
            Reflection::TypeInfo(Reflection::GetTypeId<Math::Vec3>(), Reflection::TypeKind::Struct,
                                 std::string(Reflection::GetStableTypeName<Math::Vec3>()), sizeof(Math::Vec3),
                                 alignof(Math::Vec3), {}, {}, Reflection::TypeFlags::None,
                                 {&DefaultConstruct<Math::Vec3>, &Destroy<Math::Vec3>, &CopyAssign<Math::Vec3>}));
        if (!result)
            return result;

        result = serialization_registry.Register({Reflection::GetTypeId<bool>(), &EncodeBool, &DecodeBool});
        if (!result)
            return result;
        result = AddIntegerCodec<char>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<signed char>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<unsigned char>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<short>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<unsigned short>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<int>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<unsigned int>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<long>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<unsigned long>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<long long>(serialization_registry);
        if (!result)
            return result;
        result = AddIntegerCodec<unsigned long long>(serialization_registry);
        if (!result)
            return result;
        result = serialization_registry.Register({Reflection::GetTypeId<float>(), &EncodeFloat, &DecodeFloat});
        if (!result)
            return result;
        result = serialization_registry.Register({Reflection::GetTypeId<std::string>(), &EncodeString, &DecodeString});
        if (!result)
            return result;
        result = serialization_registry.Register({Reflection::GetTypeId<double>(), &EncodeDouble, &DecodeDouble});
        if (!result)
            return result;
        result = serialization_registry.Register(
            {Reflection::GetTypeId<Math::Vec3>(), &EncodeVector<Math::Vec3, 3>, &DecodeVector<Math::Vec3, 3>});
        if (!result)
            return result;
        result = type_registry.RegisterType(
            Reflection::TypeInfo(Reflection::GetTypeId<Math::Vec2>(), Reflection::TypeKind::Struct,
                                 std::string(Reflection::GetStableTypeName<Math::Vec2>()), sizeof(Math::Vec2),
                                 alignof(Math::Vec2), {}, {}, Reflection::TypeFlags::None,
                                 {&DefaultConstruct<Math::Vec2>, &Destroy<Math::Vec2>, &CopyAssign<Math::Vec2>}));
        if (!result)
            return result;
        result = serialization_registry.Register(
            {Reflection::GetTypeId<Math::Vec2>(), &EncodeVector<Math::Vec2, 2>, &DecodeVector<Math::Vec2, 2>});
        if (!result)
            return result;
        result = type_registry.RegisterType(
            Reflection::TypeInfo(Reflection::GetTypeId<Math::Vec4>(), Reflection::TypeKind::Struct,
                                 std::string(Reflection::GetStableTypeName<Math::Vec4>()), sizeof(Math::Vec4),
                                 alignof(Math::Vec4), {}, {}, Reflection::TypeFlags::None,
                                 {&DefaultConstruct<Math::Vec4>, &Destroy<Math::Vec4>, &CopyAssign<Math::Vec4>}));
        if (!result)
            return result;
        result = serialization_registry.Register(
            {Reflection::GetTypeId<Math::Vec4>(), &EncodeVector<Math::Vec4, 4>, &DecodeVector<Math::Vec4, 4>});
        if (!result)
            return result;
        return Core::Result<void>::Success();
    }
} // namespace GE::Serialization
