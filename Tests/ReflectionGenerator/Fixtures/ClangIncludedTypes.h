#pragma once

#include "Source/Core/Reflection/ReflectionMacros.h"

namespace ReflectionGeneratorTests
{
    struct MENGINE_REFLECT_TYPE IncludedReflectedType
    {
        MENGINE_REFLECT_BODY(IncludedReflectedType)

        MENGINE_REFLECT_FIELD int included_value;
    };
} // namespace ReflectionGeneratorTests
