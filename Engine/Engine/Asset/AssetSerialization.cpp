#include "AssetSerialization.h"

#include <array>
#include <memory>
#include <string>

namespace GE::Asset
{
    namespace
    {
        int Hex(char value) noexcept
        {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        }

        Core::Result<AssetId> ParseSyntacticAssetId(std::string_view text)
        {
            if (text.size() != 36 || text[8] != '-' || text[13] != '-'
                || text[18] != '-' || text[23] != '-')
                return Core::Result<AssetId>::Failure({
                    .domain = Core::ErrorDomain::Serialization,
                    .code = Core::ErrorCode::ParseFailure,
                    .operation = "AssetIdCodec::Decode",
                    .message = "Expected canonical UUID text"});
            std::array<std::uint8_t, 16> bytes{};
            std::size_t byte_index = 0;
            for (std::size_t index = 0; index < text.size();)
            {
                if (text[index] == '-') { ++index; continue; }
                const int high = Hex(text[index++]);
                const int low = index < text.size() ? Hex(text[index++]) : -1;
                if (high < 0 || low < 0 || byte_index >= bytes.size())
                    return Core::Result<AssetId>::Failure({
                        .domain = Core::ErrorDomain::Serialization,
                        .code = Core::ErrorCode::ParseFailure,
                        .operation = "AssetIdCodec::Decode",
                        .message = "Expected canonical UUID text"});
                bytes[byte_index++] = static_cast<std::uint8_t>((high << 4) | low);
            }
            return Core::Result<AssetId>::Success(AssetId(bytes));
        }

        Core::Result<void> Encode(
            Reflection::ConstValueView value, Serialization::ArchiveWriter& archive)
        {
            if (!value.IsValid()
                || value.GetTypeId() != Reflection::GetTypeId<AssetId>())
                return Core::Result<void>::Failure({
                    .domain = Core::ErrorDomain::Serialization,
                    .code = Core::ErrorCode::InvalidArgument,
                    .operation = "AssetIdCodec::Encode",
                    .message = "Type mismatch"});
            return archive.WriteString(ToString(*static_cast<const AssetId*>(value.Data())));
        }

        Core::Result<void> Decode(
            Reflection::ValueView value, Serialization::ArchiveReader& archive)
        {
            if (!value.IsValid()
                || value.GetTypeId() != Reflection::GetTypeId<AssetId>())
                return Core::Result<void>::Failure({
                    .domain = Core::ErrorDomain::Serialization,
                    .code = Core::ErrorCode::InvalidArgument,
                    .operation = "AssetIdCodec::Decode",
                    .message = "Type mismatch"});
            Core::Result<std::string> text = archive.ReadString();
            if (!text) return Core::Result<void>::Failure(std::move(text.Error()));
            Core::Result<AssetId> parsed = ParseSyntacticAssetId(text.Value());
            if (!parsed) return Core::Result<void>::Failure(std::move(parsed.Error()));
            *static_cast<AssetId*>(value.Data()) = std::move(parsed).Value();
            return Core::Result<void>::Success();
        }

        void DefaultConstruct(void* storage) { std::construct_at(static_cast<AssetId*>(storage)); }
        void Destroy(void* object) noexcept { std::destroy_at(static_cast<AssetId*>(object)); }
        void CopyAssign(void* target, const void* source)
        { *static_cast<AssetId*>(target) = *static_cast<const AssetId*>(source); }
    }

    Core::Result<void> RegisterAssetSerialization(
        Reflection::TypeRegistry& type_registry,
        Serialization::SerializationRegistry& serialization_registry)
    {
        Core::Result<void> result = type_registry.RegisterType(Reflection::TypeInfo(
            Reflection::GetTypeId<AssetId>(), Reflection::TypeKind::Struct,
            std::string(Reflection::GetStableTypeName<AssetId>()),
            sizeof(AssetId), alignof(AssetId), {}, {}, Reflection::TypeFlags::None,
            {&DefaultConstruct, &Destroy, &CopyAssign}));
        if (!result) return result;
        return serialization_registry.Register({Reflection::GetTypeId<AssetId>(),
            &Encode, &Decode});
    }
}
