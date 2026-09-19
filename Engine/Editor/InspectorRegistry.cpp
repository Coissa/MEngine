#include "InspectorRegistry.h"
#include <TypeRegistry.h>

namespace GE::Editor
{
    bool InspectorRegistry::RegisterSpecialized(GE::Reflection::TypeId type_id)
    {
        return !m_frozen && type_id.IsValid()
            && m_entries.emplace(type_id, InspectorDrawMode::Specialized).second;
    }

    bool InspectorRegistry::Freeze() noexcept
    {
        if (m_frozen) return false;
        m_frozen = true;
        return true;
    }

    InspectorDrawMode InspectorRegistry::Resolve(GE::Reflection::TypeId type_id,
        const GE::Reflection::TypeRegistry& reflection) const noexcept
    {
        const auto found = m_entries.find(type_id);
        if (found != m_entries.end()) return found->second;
        return reflection.FindType(type_id) == nullptr
            ? InspectorDrawMode::None : InspectorDrawMode::ReflectionFallback;
    }

    bool InspectorRegistry::IsFrozen() const noexcept { return m_frozen; }
}
