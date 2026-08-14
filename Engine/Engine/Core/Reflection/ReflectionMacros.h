#pragma once

namespace GE::Reflection
{
    template <typename T>
    struct GeneratedTypeAccess;
}

#if defined(__clang__)
#define MENGINE_REFLECT_TYPE [[clang::annotate("mengine.reflect.type")]]
#define MENGINE_REFLECT_FIELD [[clang::annotate("mengine.reflect.field")]]
#define MENGINE_REFLECT_FUNCTION [[clang::annotate("mengine.reflect.function")]]
#else
#define MENGINE_REFLECT_TYPE
#define MENGINE_REFLECT_FIELD
#define MENGINE_REFLECT_FUNCTION
#endif

#define MENGINE_REFLECT_BODY(Type) \
    friend struct ::GE::Reflection::GeneratedTypeAccess<Type>;
