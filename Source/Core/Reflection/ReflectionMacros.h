#pragma once

namespace GE::Reflection
{
    template <typename T> struct GeneratedTypeAccess;
}

#if defined(__clang__)
#define MENGINE_REFLECT_TYPE [[clang::annotate("mengine.reflect.type")]]
#define MENGINE_REFLECT_COMPONENT [[clang::annotate("mengine.reflect.component")]]
#define MENGINE_REFLECT_VERSION(Value) [[clang::annotate("mengine.reflect.version:" #Value)]]
#define MENGINE_REFLECT_ESSENTIAL [[clang::annotate("mengine.reflect.essential")]]
#define MENGINE_REFLECT_COPY [[clang::annotate("mengine.reflect.copy")]]
#define MENGINE_REFLECT_EDITOR(Name, Speed, Min, Max)                                                                  \
    [[clang::annotate("mengine.reflect.editor:" Name ":" #Speed ":" #Min ":" #Max)]]
#define MENGINE_REFLECT_ENUM [[clang::annotate("mengine.reflect.enum")]]
#define MENGINE_REFLECT_STABLE_NAME(Name) [[clang::annotate("mengine.reflect.stable_name:" Name)]]
#define MENGINE_REFLECT_SERIALIZABLE [[clang::annotate("mengine.reflect.serializable")]]
#define MENGINE_REFLECT_FIELD [[clang::annotate("mengine.reflect.field")]]
#define MENGINE_REFLECT_SERIALIZED_NAME(Name) [[clang::annotate("mengine.reflect.serialized_name:" Name)]]
#define MENGINE_REFLECT_REQUIRED [[clang::annotate("mengine.reflect.required")]]
#define MENGINE_REFLECT_TRANSIENT [[clang::annotate("mengine.reflect.transient")]]
#define MENGINE_REFLECT_FUNCTION [[clang::annotate("mengine.reflect.function")]]
#else
#define MENGINE_REFLECT_TYPE
#define MENGINE_REFLECT_COMPONENT
#define MENGINE_REFLECT_VERSION(Value)
#define MENGINE_REFLECT_ESSENTIAL
#define MENGINE_REFLECT_COPY
#define MENGINE_REFLECT_EDITOR(Name, Speed, Min, Max)
#define MENGINE_REFLECT_ENUM
#define MENGINE_REFLECT_STABLE_NAME(Name)
#define MENGINE_REFLECT_SERIALIZABLE
#define MENGINE_REFLECT_FIELD
#define MENGINE_REFLECT_SERIALIZED_NAME(Name)
#define MENGINE_REFLECT_REQUIRED
#define MENGINE_REFLECT_TRANSIENT
#define MENGINE_REFLECT_FUNCTION
#endif

#define MENGINE_REFLECT_BODY(Type) friend struct ::GE::Reflection::GeneratedTypeAccess<Type>;
