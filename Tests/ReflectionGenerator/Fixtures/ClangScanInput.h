#pragma once

#include "Source/Core/Reflection/ReflectionMacros.h"
#include "ClangIncludedTypes.h"

#include <cstdint>

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

    struct MENGINE_REFLECT_TYPE MENGINE_REFLECT_STABLE_NAME("MEngine.Tests.SerializableRecord")
        MENGINE_REFLECT_SERIALIZABLE SerializableRecord
    {
        MENGINE_REFLECT_BODY(SerializableRecord)

        MENGINE_REFLECT_FIELD MENGINE_REFLECT_SERIALIZED_NAME("value") MENGINE_REFLECT_REQUIRED int value = 0;
        MENGINE_REFLECT_FIELD MENGINE_REFLECT_TRANSIENT int cache = 0;
    };

    enum class MENGINE_REFLECT_ENUM MENGINE_REFLECT_STABLE_NAME("MEngine.Tests.ScanMode") ScanMode : std::uint8_t{
        Disabled = 0, Enabled = 1};

    struct NotReflected
    {
        int value;
    };

    struct [[clang::annotate("some.other.annotation")]] OtherAnnotation
    {
        int value;
    };
} // namespace ReflectionGeneratorTests
