#pragma once

#include "Archive.h"

#include <Core/Reflection/TypeRegistry.h>
#include <Core/Reflection/ValueView.h>

#include <span>
#include <unordered_map>
#include <vector>

namespace GE::Serialization
{
    struct ValueCodec final
    {
        using Encode = Core::Result<void> (*)(Reflection::ConstValueView value, ArchiveWriter& archive);
        using Decode = Core::Result<void> (*)(Reflection::ValueView value, ArchiveReader& archive);

        Reflection::TypeId type_id;
        Encode encode = nullptr;
        Decode decode = nullptr;
    };

    class SerializationRegistry final
    {
    public:
        [[nodiscard]] Core::Result<void> Register(ValueCodec codec);
        [[nodiscard]] Core::Result<void> Freeze(const Reflection::TypeRegistry& type_registry);
        [[nodiscard]] bool IsFrozen() const noexcept;
        [[nodiscard]] const ValueCodec* Find(Reflection::TypeId type_id) const noexcept;

    private:
        std::vector<ValueCodec> m_codecs;
        std::unordered_map<Reflection::TypeId, std::size_t> m_indices;
        bool m_frozen = false;
    };
} // namespace GE::Serialization
