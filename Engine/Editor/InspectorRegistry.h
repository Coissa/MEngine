#pragma once

#include <TypeId.h>
#include <unordered_map>

namespace GE::Reflection { class TypeRegistry; }

namespace GE::Editor
{
    enum class InspectorDrawMode
    {
        None,
        Specialized,
        ReflectionFallback
    };

    class InspectorRegistry final
    {
    public:
        [[nodiscard]] bool RegisterSpecialized(GE::Reflection::TypeId type_id);
        [[nodiscard]] bool Freeze() noexcept;
        [[nodiscard]] InspectorDrawMode Resolve(
            GE::Reflection::TypeId type_id,
            const GE::Reflection::TypeRegistry& reflection) const noexcept;
        [[nodiscard]] bool IsFrozen() const noexcept;

    private:
        std::unordered_map<GE::Reflection::TypeId, InspectorDrawMode> m_entries;
        bool m_frozen = false;
    };
}
