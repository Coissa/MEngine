#pragma once

#include "Engine/Engine/Core/Reflection/ReflectionMacros.h"
#include "ClangIncludedTypes.h"

namespace ReflectionGeneratorTests
{
    class MENGINE_REFLECT_TYPE ReflectedClass
    {
        MENGINE_REFLECT_BODY(ReflectedClass)

    public:
        MENGINE_REFLECT_FIELD int public_value;
        int ignored_value;

    protected:
        MENGINE_REFLECT_FIELD const int fixed_value = 7;

    private:
        MENGINE_REFLECT_FIELD unsigned int flags : 3;
    };

    struct MENGINE_REFLECT_TYPE EmptyReflectedStruct
    {
        MENGINE_REFLECT_BODY(EmptyReflectedStruct)
    };

    struct NotReflected
    {
        int value;
    };

    struct [[clang::annotate("some.other.annotation")]] OtherAnnotation
    {
        int value;
    };
}
