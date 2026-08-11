#include "ReflectionSample.generated.h"

#include "Engine/Core/Reflection/FunctionInfo.h"

#include <string>
#include <utility>
#include <vector>

void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetHealthMutable(
    void* owner) noexcept
{
    return &static_cast<ReflectionSample*>(owner)->m_health;
}

const void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetHealthConst(
    const void* owner) noexcept
{
    return &static_cast<const ReflectionSample*>(owner)->m_health;
}

void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetNameMutable(
    void* owner) noexcept
{
    return &static_cast<ReflectionSample*>(owner)->m_name;
}

const void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetNameConst(
    const void* owner) noexcept
{
    return &static_cast<const ReflectionSample*>(owner)->m_name;
}

void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetRuntimeCacheMutable(
    void* owner) noexcept
{
    return &static_cast<ReflectionSample*>(owner)->m_runtime_cache;
}

const void* ReflectionTests::ReflectionSampleGeneratedAccessor::GetRuntimeCacheConst(
    const void* owner) noexcept
{
    return &static_cast<const ReflectionSample*>(owner)->m_runtime_cache;
}

void ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeAddHealth(
    void* owner,
    std::span<const GE::Reflection::ConstValueView> arguments,
    void*)
{
    const int amount = *static_cast<const int*>(arguments[0].Data());
    static_cast<ReflectionSample*>(owner)->AddHealth(amount);
}

void ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeGetHealthWithBonus(
    const void* owner,
    std::span<const GE::Reflection::ConstValueView> arguments,
    void* return_storage)
{
    const int bonus = *static_cast<const int*>(arguments[0].Data());
    *static_cast<int*>(return_storage) =
        static_cast<const ReflectionSample*>(owner)->GetHealthWithBonus(bonus);
}

GE::Reflection::TypeRegistry::RegisterTypeResult
GE::Reflection::RegisterReflectionSample(TypeRegistry& registry)
{
    const TypeId owner_type_id = GetTypeId<ReflectionTests::ReflectionSample>();

    std::vector<FieldInfo> fields;
    fields.emplace_back(owner_type_id,
                        GetTypeId<int>(),
                        "m_health",
                        FieldInfo::FieldFlags::None,
                        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetHealthMutable,
                        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetHealthConst);
    fields.emplace_back(owner_type_id,
                        GetTypeId<std::string>(),
                        "m_name",
                        FieldInfo::FieldFlags::None,
                        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetNameMutable,
                        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetNameConst);
    fields.emplace_back(
        owner_type_id,
        GetTypeId<float>(),
        "m_runtime_cache",
        FieldInfo::FieldFlags::Transient,
        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetRuntimeCacheMutable,
        &ReflectionTests::ReflectionSampleGeneratedAccessor::GetRuntimeCacheConst);

    std::vector<FunctionInfo> functions;

    std::vector<ParameterInfo> add_health_parameters;
    add_health_parameters.emplace_back(GetTypeId<int>(), "amount");
    functions.emplace_back(
        owner_type_id,
        GetTypeId<void>(),
        std::move(add_health_parameters),
        "AddHealth",
        FunctionInfo::FunctionFlags::None,
        &ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeAddHealth,
        nullptr);

    std::vector<ParameterInfo> get_health_parameters;
    get_health_parameters.emplace_back(GetTypeId<int>(), "bonus");
    functions.emplace_back(
        owner_type_id,
        GetTypeId<int>(),
        std::move(get_health_parameters),
        "GetHealthWithBonus",
        FunctionInfo::FunctionFlags::Const,
        nullptr,
        &ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeGetHealthWithBonus);

    return registry.RegisterType(TypeInfo(
        owner_type_id,
        TypeKind::Class,
        std::string(GetStableTypeName<ReflectionTests::ReflectionSample>()),
        sizeof(ReflectionTests::ReflectionSample),
        alignof(ReflectionTests::ReflectionSample),
        std::move(fields),
        std::move(functions)));
}
