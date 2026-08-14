#pragma once

#include "Engine/Engine/Core/Reflection/ReflectionMacros.h"

namespace ReflectionRendererTests
{
    class MENGINE_REFLECT_TYPE RenderSample
    {
        MENGINE_REFLECT_BODY(RenderSample)

    public:
        [[nodiscard]] int GetValue() const noexcept
        {
            return m_value;
        }

        [[nodiscard]] float GetWeight() const noexcept
        {
            return m_weight;
        }

    private:
        MENGINE_REFLECT_FIELD int m_value {42};
        MENGINE_REFLECT_FIELD const float m_weight {1.5F};
    };
}
