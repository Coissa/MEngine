#include "ClangRenderInput.h.reflection.generated.h"

#include "FieldInfo.h"
#include "TypeInfo.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace GE::Reflection
{
    void* GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField0Mutable(void* owner) noexcept
    {
        return std::addressof(
            static_cast<::ReflectionRendererTests::RenderSample*>(owner)->m_value);
    }

    const void* GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField0Const(const void* owner) noexcept
    {
        return std::addressof(
            static_cast<const ::ReflectionRendererTests::RenderSample*>(owner)->m_value);
    }

    const void* GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField1Const(const void* owner) noexcept
    {
        return std::addressof(
            static_cast<const ::ReflectionRendererTests::RenderSample*>(owner)->m_weight);
    }

    TypeRegistry::RegisterTypeResult RegisterGeneratedReflection_f57f7e125e8196b3(
        TypeRegistry& registry)
    {
        {
            const TypeId owner_type_id = GetTypeId<::ReflectionRendererTests::RenderSample>();
            std::vector<FieldInfo> fields;
            fields.reserve(2);
            fields.emplace_back(
                owner_type_id,
                GetTypeId<int>(),
                "m_value",
                FieldInfo::FieldFlags::None,
                &GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField0Mutable,
                &GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField0Const);
            fields.emplace_back(
                owner_type_id,
                GetTypeId<const float>(),
                "m_weight",
                FieldInfo::FieldFlags::ReadOnly,
                nullptr,
                &GeneratedTypeAccess<::ReflectionRendererTests::RenderSample>::GetField1Const);

            const TypeRegistry::RegisterTypeResult result =
                registry.RegisterType(TypeInfo(
                    owner_type_id,
                    TypeKind::Class,
                    std::string(GetStableTypeName<::ReflectionRendererTests::RenderSample>()),
                    sizeof(::ReflectionRendererTests::RenderSample),
                    alignof(::ReflectionRendererTests::RenderSample),
                    std::move(fields)));
            if (result != TypeRegistry::RegisterTypeResult::Success)
            {
                return result;
            }
        }

        return TypeRegistry::RegisterTypeResult::Success;
    }
}
