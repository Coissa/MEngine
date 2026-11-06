#include "Fixtures/ReflectionSample.generated.h"

#include "Core/Reflection/BuiltinTypeRegistration.h"
#include "Core/Reflection/FieldInfo.h"
#include "Core/Reflection/FunctionInfo.h"
#include "Core/Reflection/ParameterInfo.h"
#include "Core/Reflection/TypeRegistry.h"
#include "Core/Reflection/TypeTraits.h"
#include "Core/Reflection/ValueView.h"
#include "Core/Reflection/ValueViewFactory.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ReflectionTestTypes
{
    struct OverloadedAddress
    {
        int value{11};

        OverloadedAddress* operator&() noexcept { return nullptr; }

        const OverloadedAddress* operator&() const noexcept { return nullptr; }
    };
} // namespace ReflectionTestTypes

namespace GE::Reflection
{
    template <> struct TypeTraits<ReflectionTestTypes::OverloadedAddress>
    {
        static constexpr std::string_view StableName = "ReflectionTestTypes::OverloadedAddress";
    };
} // namespace GE::Reflection

namespace
{
    using GE::Reflection::ConstValueView;
    using GE::Reflection::FieldInfo;
    using GE::Reflection::FunctionInfo;
    using GE::Reflection::GetStableTypeName;
    using GE::Reflection::GetTypeId;
    using GE::Reflection::MakeConstValueView;
    using GE::Reflection::MakeTypeId;
    using GE::Reflection::MakeValueView;
    using GE::Reflection::ParameterInfo;
    using GE::Reflection::RegisterBuiltinTypes;
    using GE::Reflection::RegisterReflectionSample;
    using GE::Reflection::TypeId;
    using GE::Reflection::TypeInfo;
    using GE::Reflection::TypeKind;
    using GE::Reflection::TypeRegistry;
    using GE::Reflection::ValueView;
    using ReflectionTests::ReflectionSample;

    enum class RegisterResult
    {
        Success,
        Frozen,
        InvalidTypeId,
        EmptyName,
        IdNameMismatch,
        InvalidSize,
        InvalidAlignment,
        FieldsNotAllowed,
        InvalidField,
        FieldOwnerMismatch,
        DuplicateFieldName,
        FunctionsNotAllowed,
        InvalidFunction,
        FunctionOwnerMismatch,
        DuplicateFunctionName,
        DuplicateType
    };

    enum class FreezeResult
    {
        Success,
        AlreadyFrozen,
        MissingFieldType,
        MissingFunctionReturnType,
        MissingFunctionParameterType
    };

    bool operator==(const GE::Core::Result<void>& result, RegisterResult expected) noexcept
    {
        return expected == RegisterResult::Success ? static_cast<bool>(result) : !result;
    }

    bool operator==(const GE::Core::Result<void>& result, FreezeResult expected) noexcept
    {
        return expected == FreezeResult::Success ? static_cast<bool>(result) : !result;
    }
    using FieldFlags = FieldInfo::FieldFlags;
    using FunctionFlags = FunctionInfo::FunctionFlags;
    using InvokeResult = FunctionInfo::InvokeResult;
    int g_failure_count = 0;

    struct ReflectedSample
    {
        int health{100};
        int identifier{7};
    };

    void* GetHealthMutable(void* owner) noexcept
    {
        return &static_cast<ReflectedSample*>(owner)->health;
    }

    const void* GetHealthConst(const void* owner) noexcept
    {
        return &static_cast<const ReflectedSample*>(owner)->health;
    }

    void* GetIdentifierMutable(void* owner) noexcept
    {
        return &static_cast<ReflectedSample*>(owner)->identifier;
    }

    const void* GetIdentifierConst(const void* owner) noexcept
    {
        return &static_cast<const ReflectedSample*>(owner)->identifier;
    }

    void DummyMutableInvoker(void*, std::span<const ConstValueView>, void*) {}

    void DummyConstInvoker(const void*, std::span<const ConstValueView>, void*) {}

#define CHECK(expression)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expression))                                                                                             \
        {                                                                                                              \
            std::cerr << "CHECK failed: " #expression << " (line " << __LINE__ << ")\n";                               \
            ++g_failure_count;                                                                                         \
        }                                                                                                              \
    } while (false)

    void TestTypeId()
    {
        CHECK(!TypeId{}.IsValid());
        CHECK(MakeTypeId("").IsValid() == false);
        CHECK(MakeTypeId("a").Value() == 0xaf63dc4c8601ec8cull);
        CHECK(MakeTypeId("hello").Value() == 0xa430d84680aabd0bull);
        CHECK(MakeTypeId("GE::Vector3") == MakeTypeId("GE::Vector3"));
        CHECK(MakeTypeId("GE::Vector3") != MakeTypeId("GE::Vector4"));

        std::unordered_map<TypeId, int> values;
        values.emplace(MakeTypeId("int"), 42);
        CHECK(values.at(MakeTypeId("int")) == 42);
    }

    void TestTypeTraits()
    {
        CHECK(GetStableTypeName<void>() == std::string_view{"void"});
        CHECK(GetStableTypeName<bool>() == std::string_view{"bool"});
        CHECK(GetStableTypeName<char>() == std::string_view{"char"});
        CHECK(GetStableTypeName<signed char>() == std::string_view{"signed char"});
        CHECK(GetStableTypeName<unsigned char>() == std::string_view{"unsigned char"});
        CHECK(GetStableTypeName<short>() == std::string_view{"short"});
        CHECK(GetStableTypeName<unsigned short>() == std::string_view{"unsigned short"});
        CHECK(GetStableTypeName<int>() == std::string_view{"int"});
        CHECK(GetStableTypeName<unsigned int>() == std::string_view{"unsigned int"});
        CHECK(GetStableTypeName<long>() == std::string_view{"long"});
        CHECK(GetStableTypeName<unsigned long>() == std::string_view{"unsigned long"});
        CHECK(GetStableTypeName<long long>() == std::string_view{"long long"});
        CHECK(GetStableTypeName<unsigned long long>() == std::string_view{"unsigned long long"});
        CHECK(GetStableTypeName<float>() == std::string_view{"float"});
        CHECK(GetStableTypeName<double>() == std::string_view{"double"});
        CHECK(GetStableTypeName<std::string>() == std::string_view{"std::string"});

        CHECK(GetStableTypeName<const int&>() == std::string_view{"int"});
        CHECK(GetTypeId<const int&>() == GetTypeId<int>());
        CHECK(GetTypeId<volatile double&&>() == MakeTypeId("double"));
        CHECK(GetTypeId<const std::string>() == MakeTypeId("std::string"));
    }

    void TestParameterInfo()
    {
        const ParameterInfo amount(GetTypeId<int>(), std::string{"amount"});
        CHECK(amount.IsValid());
        CHECK(amount.GetTypeId() == GetTypeId<int>());
        CHECK(amount.GetName() == std::string_view{"amount"});

        const ParameterInfo invalid_type(TypeId{}, "value");
        CHECK(!invalid_type.IsValid());

        const ParameterInfo empty_name(GetTypeId<float>(), "");
        CHECK(!empty_name.IsValid());
    }

    void TestFunctionInfoMetadata()
    {
        const TypeId owner_type_id = MakeTypeId("Tests::FunctionOwner");
        std::vector<ParameterInfo> parameters;
        parameters.emplace_back(GetTypeId<int>(), "amount");

        const FunctionInfo mutable_function(owner_type_id, GetTypeId<void>(), parameters, "TakeDamage",
                                            FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(mutable_function.IsValid());
        CHECK(!mutable_function.IsConst());
        CHECK(mutable_function.GetOwnerTypeId() == owner_type_id);
        CHECK(mutable_function.GetReturnTypeId() == GetTypeId<void>());
        CHECK(mutable_function.GetName() == std::string_view{"TakeDamage"});
        CHECK(mutable_function.GetParameters().size() == 1);
        CHECK(mutable_function.GetParameters()[0].GetTypeId() == GetTypeId<int>());
        CHECK(mutable_function.GetParameters()[0].GetName() == std::string_view{"amount"});

        const FunctionInfo const_function(owner_type_id, GetTypeId<int>(), {}, "GetHealth", FunctionFlags::Const,
                                          nullptr, &DummyConstInvoker);
        CHECK(const_function.IsValid());
        CHECK(const_function.IsConst());

        const FunctionInfo invalid_owner(TypeId{}, GetTypeId<void>(), {}, "Run", FunctionFlags::None,
                                         &DummyMutableInvoker, nullptr);
        CHECK(!invalid_owner.IsValid());

        std::vector<ParameterInfo> invalid_parameters;
        invalid_parameters.emplace_back(TypeId{}, "value");
        const FunctionInfo invalid_parameter(owner_type_id, GetTypeId<void>(), std::move(invalid_parameters), "Run",
                                             FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(!invalid_parameter.IsValid());

        const FunctionInfo const_flag_with_mutable_invoker(owner_type_id, GetTypeId<void>(), {}, "Run",
                                                           FunctionFlags::Const, &DummyMutableInvoker, nullptr);
        CHECK(!const_flag_with_mutable_invoker.IsValid());

        const FunctionInfo both_invokers(owner_type_id, GetTypeId<void>(), {}, "Run", FunctionFlags::None,
                                         &DummyMutableInvoker, &DummyConstInvoker);
        CHECK(!both_invokers.IsValid());
    }

    void TestFunctionInvocation()
    {
        const TypeId owner_type_id = GetTypeId<ReflectionSample>();

        std::vector<ParameterInfo> add_parameters;
        add_parameters.emplace_back(GetTypeId<int>(), "amount");
        const FunctionInfo add_health(owner_type_id, GetTypeId<void>(), std::move(add_parameters), "AddHealth",
                                      FunctionFlags::None,
                                      &ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeAddHealth, nullptr);

        std::vector<ParameterInfo> get_parameters;
        get_parameters.emplace_back(GetTypeId<int>(), "bonus");
        const FunctionInfo get_health_with_bonus(
            owner_type_id, GetTypeId<int>(), std::move(get_parameters), "GetHealthWithBonus", FunctionFlags::Const,
            nullptr, &ReflectionTests::ReflectionSampleGeneratedAccessor::InvokeGetHealthWithBonus);

        ReflectionSample sample;
        ValueView owner = MakeValueView(sample);

        const int amount = 25;
        const std::array<ConstValueView, 1> add_arguments{
            MakeConstValueView(amount),
        };
        CHECK(add_health.Invoke(owner, add_arguments) == InvokeResult::Success);
        CHECK(sample.GetHealth() == 125);

        const int bonus = 5;
        const std::array<ConstValueView, 1> get_arguments{
            MakeConstValueView(bonus),
        };
        int result = 0;
        CHECK(get_health_with_bonus.Invoke(owner, get_arguments, MakeValueView(result)) == InvokeResult::Success);
        CHECK(result == 130);

        const ReflectionSample& const_sample = sample;
        result = 0;
        CHECK(get_health_with_bonus.Invoke(MakeConstValueView(const_sample), get_arguments, MakeValueView(result)) ==
              InvokeResult::Success);
        CHECK(result == 130);

        CHECK(add_health.Invoke(ValueView{}, add_arguments) == InvokeResult::InvalidOwner);
        CHECK(add_health.Invoke(ValueView(nullptr, owner_type_id), add_arguments) == InvokeResult::InvalidOwner);

        int wrong_owner = 0;
        CHECK(add_health.Invoke(MakeValueView(wrong_owner), add_arguments) == InvokeResult::OwnerTypeMismatch);

        CHECK(add_health.Invoke(owner, {}) == InvokeResult::ArgumentCountMismatch);

        const std::array<ConstValueView, 1> invalid_arguments{ConstValueView{}};
        CHECK(add_health.Invoke(owner, invalid_arguments) == InvokeResult::InvalidArgument);

        const float wrong_argument = 2.0F;
        const std::array<ConstValueView, 1> wrong_arguments{
            MakeConstValueView(wrong_argument),
        };
        CHECK(add_health.Invoke(owner, wrong_arguments) == InvokeResult::ArgumentTypeMismatch);

        CHECK(add_health.Invoke(owner, add_arguments, MakeValueView(result)) == InvokeResult::UnexpectedReturnStorage);
        CHECK(get_health_with_bonus.Invoke(owner, get_arguments) == InvokeResult::MissingReturnStorage);

        float wrong_result = 0.0F;
        CHECK(get_health_with_bonus.Invoke(owner, get_arguments, MakeValueView(wrong_result)) ==
              InvokeResult::ReturnTypeMismatch);

        CHECK(add_health.Invoke(MakeConstValueView(const_sample), add_arguments) ==
              InvokeResult::NonConstFunctionOnConstObject);

        const FunctionInfo invalid_function(TypeId{}, GetTypeId<void>(), {}, "Invalid", FunctionFlags::None,
                                            &DummyMutableInvoker, nullptr);
        CHECK(invalid_function.Invoke(owner, {}) == InvokeResult::InvalidFunction);
    }

    void TestValueViews()
    {
        const TypeId int_type_id = MakeTypeId("int");

        const ConstValueView empty_const_view;
        CHECK(!empty_const_view.IsValid());
        CHECK(empty_const_view.Data() == nullptr);
        CHECK(!empty_const_view.GetTypeId().IsValid());

        const ConstValueView typed_null_view(nullptr, int_type_id);
        CHECK(!typed_null_view.IsValid());
        CHECK(typed_null_view.Data() == nullptr);
        CHECK(typed_null_view.GetTypeId() == int_type_id);

        const int const_value = 17;
        const ConstValueView untyped_view(&const_value, TypeId{});
        CHECK(!untyped_view.IsValid());
        CHECK(untyped_view.Data() == &const_value);

        const ConstValueView const_view(&const_value, int_type_id);
        CHECK(const_view.IsValid());
        CHECK(const_view.Data() == &const_value);
        CHECK(const_view.GetTypeId() == int_type_id);
        CHECK(*static_cast<const int*>(const_view.Data()) == 17);

        int mutable_value = 21;
        ValueView mutable_view(&mutable_value, int_type_id);
        CHECK(mutable_view.IsValid());
        CHECK(mutable_view.Data() == &mutable_value);
        *static_cast<int*>(mutable_view.Data()) = 42;
        CHECK(mutable_value == 42);

        const ConstValueView converted_view = mutable_view.AsConst();
        CHECK(converted_view.IsValid());
        CHECK(converted_view.Data() == &mutable_value);
        CHECK(converted_view.GetTypeId() == int_type_id);
        CHECK(*static_cast<const int*>(converted_view.Data()) == 42);

        ValueView assigned_mutable_view;
        assigned_mutable_view = mutable_view;
        CHECK(assigned_mutable_view.Data() == &mutable_value);
        CHECK(assigned_mutable_view.GetTypeId() == int_type_id);

        ConstValueView assigned_const_view;
        assigned_const_view = const_view;
        CHECK(assigned_const_view.Data() == &const_value);
        CHECK(assigned_const_view.GetTypeId() == int_type_id);
    }

    void TestValueViewFactories()
    {
        int value = 12;
        ValueView mutable_view = MakeValueView(value);
        CHECK(mutable_view.IsValid());
        CHECK(mutable_view.Data() == std::addressof(value));
        CHECK(mutable_view.GetTypeId() == GetTypeId<int>());
        *static_cast<int*>(mutable_view.Data()) = 24;
        CHECK(value == 24);

        ConstValueView mutable_as_const = MakeConstValueView(value);
        CHECK(mutable_as_const.IsValid());
        CHECK(mutable_as_const.Data() == std::addressof(value));
        CHECK(mutable_as_const.GetTypeId() == GetTypeId<int>());

        const int const_value = 36;
        ConstValueView const_view = MakeConstValueView(const_value);
        CHECK(const_view.IsValid());
        CHECK(const_view.Data() == std::addressof(const_value));
        CHECK(const_view.GetTypeId() == GetTypeId<const int&>());

        ReflectionTestTypes::OverloadedAddress overloaded_address;
        CHECK(&overloaded_address == nullptr);

        ValueView overloaded_view = MakeValueView(overloaded_address);
        CHECK(overloaded_view.IsValid());
        CHECK(overloaded_view.Data() == std::addressof(overloaded_address));
        CHECK(overloaded_view.GetTypeId() == GetTypeId<ReflectionTestTypes::OverloadedAddress>());

        const ReflectionTestTypes::OverloadedAddress const_overloaded_address;
        CHECK(&const_overloaded_address == nullptr);

        ConstValueView const_overloaded_view = MakeConstValueView(const_overloaded_address);
        CHECK(const_overloaded_view.IsValid());
        CHECK(const_overloaded_view.Data() == std::addressof(const_overloaded_address));
    }

    void TestFieldInfo()
    {
        const TypeId owner_type_id = MakeTypeId("Tests::ReflectedSample");
        const TypeId int_type_id = MakeTypeId("int");

        const FieldInfo health_field(owner_type_id, int_type_id, "health", FieldFlags::None, &GetHealthMutable,
                                     &GetHealthConst);

        CHECK(health_field.IsValid());
        CHECK(health_field.GetOwnerTypeId() == owner_type_id);
        CHECK(health_field.GetValueTypeId() == int_type_id);
        CHECK(health_field.GetName() == std::string_view{"health"});
        CHECK(!health_field.IsReadOnly());
        CHECK(health_field.IsMutable());
        CHECK(!health_field.IsTransient());

        ReflectedSample sample;
        ValueView owner_view(&sample, owner_type_id);
        ValueView health_view = health_field.GetValue(owner_view);
        CHECK(health_view.IsValid());
        CHECK(health_view.GetTypeId() == int_type_id);
        CHECK(health_view.Data() == &sample.health);
        if (health_view.IsValid())
        {
            *static_cast<int*>(health_view.Data()) = 250;
        }
        CHECK(sample.health == 250);

        const ReflectedSample const_sample{.health = 80, .identifier = 9};
        const ConstValueView const_owner_view(&const_sample, owner_type_id);
        const ConstValueView const_health_view = health_field.GetValue(const_owner_view);
        CHECK(const_health_view.IsValid());
        CHECK(const_health_view.Data() == &const_sample.health);
        if (const_health_view.IsValid())
        {
            CHECK(*static_cast<const int*>(const_health_view.Data()) == 80);
        }

        const TypeId wrong_owner_type_id = MakeTypeId("Tests::WrongOwner");
        CHECK(!health_field.GetValue(ValueView(&sample, wrong_owner_type_id)).IsValid());
        CHECK(!health_field.GetValue(ConstValueView(&sample, wrong_owner_type_id)).IsValid());
        CHECK(!health_field.GetValue(ValueView{}).IsValid());
        CHECK(!health_field.GetValue(ConstValueView{}).IsValid());

        const FieldInfo read_only_transient_field(owner_type_id, int_type_id, "identifier",
                                                  FieldFlags::ReadOnly | FieldFlags::Transient, &GetIdentifierMutable,
                                                  &GetIdentifierConst);

        CHECK(read_only_transient_field.IsValid());
        CHECK(read_only_transient_field.IsReadOnly());
        CHECK(!read_only_transient_field.IsMutable());
        CHECK(read_only_transient_field.IsTransient());
        CHECK(!read_only_transient_field.GetValue(owner_view).IsValid());

        const ConstValueView identifier_view = read_only_transient_field.GetValue(owner_view.AsConst());
        CHECK(identifier_view.IsValid());
        CHECK(identifier_view.Data() == &sample.identifier);

        const FieldInfo implicitly_read_only_field(owner_type_id, int_type_id, "identifier", FieldFlags::None, nullptr,
                                                   &GetIdentifierConst);
        CHECK(implicitly_read_only_field.IsValid());
        CHECK(implicitly_read_only_field.IsReadOnly());
        CHECK(!implicitly_read_only_field.GetValue(owner_view).IsValid());
        CHECK(implicitly_read_only_field.GetValue(owner_view.AsConst()).IsValid());

        const FieldInfo invalid_field(owner_type_id, int_type_id, "", FieldFlags::None, &GetHealthMutable,
                                      &GetHealthConst);
        CHECK(!invalid_field.IsValid());

        const FieldInfo unreadable_field(owner_type_id, int_type_id, "health", FieldFlags::None, &GetHealthMutable,
                                         nullptr);
        CHECK(!unreadable_field.IsValid());
    }

    void TestTypeInfoFields()
    {
        const TypeId owner_type_id = MakeTypeId("Tests::ReflectedSample");
        const TypeId int_type_id = MakeTypeId("int");

        std::vector<FieldInfo> fields;
        fields.emplace_back(owner_type_id, int_type_id, "health", FieldFlags::None, &GetHealthMutable, &GetHealthConst);
        fields.emplace_back(owner_type_id, int_type_id, "identifier", FieldFlags::ReadOnly | FieldFlags::Transient,
                            &GetIdentifierMutable, &GetIdentifierConst);

        TypeInfo sample_type(owner_type_id, TypeKind::Class, "Tests::ReflectedSample", sizeof(ReflectedSample),
                             alignof(ReflectedSample), std::move(fields));

        CHECK(sample_type.GetFields().size() == 2);
        CHECK(sample_type.FindField("health") != nullptr);
        CHECK(sample_type.FindField("identifier") != nullptr);
        CHECK(sample_type.FindField("missing") == nullptr);
        CHECK(sample_type.FindField("health") == &sample_type.GetFields()[0]);
        CHECK(sample_type.FindField("identifier") == &sample_type.GetFields()[1]);

        TypeRegistry registry;
        CHECK(registry.RegisterType(std::move(sample_type)) == RegisterResult::Success);

        const TypeInfo* const registered_type = registry.FindType(owner_type_id);
        CHECK(registered_type != nullptr);
        if (registered_type == nullptr)
        {
            return;
        }

        CHECK(registered_type->GetFields().size() == 2);
        const FieldInfo* const health_field = registered_type->FindField("health");
        CHECK(health_field != nullptr);

        ReflectedSample sample;
        if (health_field != nullptr)
        {
            ValueView health_view = health_field->GetValue(ValueView(&sample, owner_type_id));
            CHECK(health_view.IsValid());
            if (health_view.IsValid())
            {
                *static_cast<int*>(health_view.Data()) = 315;
            }
        }
        CHECK(sample.health == 315);

        const TypeInfo primitive_type(int_type_id, TypeKind::Primitive, "int", sizeof(int), alignof(int));
        CHECK(primitive_type.GetFields().empty());
        CHECK(primitive_type.FindField("anything") == nullptr);
    }

    void TestFieldRegistrationValidation()
    {
        const TypeId int_type_id = MakeTypeId("int");
        TypeRegistry registry;

        const TypeId primitive_owner_id = MakeTypeId("Tests::PrimitiveWithField");
        std::vector<FieldInfo> primitive_fields;
        primitive_fields.emplace_back(primitive_owner_id, int_type_id, "value", FieldFlags::None, &GetHealthMutable,
                                      &GetHealthConst);
        CHECK(registry.RegisterType(TypeInfo(primitive_owner_id, TypeKind::Primitive, "Tests::PrimitiveWithField",
                                             sizeof(ReflectedSample), alignof(ReflectedSample),
                                             std::move(primitive_fields))) == RegisterResult::FieldsNotAllowed);
        CHECK(registry.FindType(primitive_owner_id) == nullptr);

        const TypeId invalid_field_owner_id = MakeTypeId("Tests::InvalidFieldOwner");
        std::vector<FieldInfo> invalid_fields;
        invalid_fields.emplace_back(invalid_field_owner_id, int_type_id, "", FieldFlags::None, &GetHealthMutable,
                                    &GetHealthConst);
        CHECK(registry.RegisterType(TypeInfo(invalid_field_owner_id, TypeKind::Class, "Tests::InvalidFieldOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample),
                                             std::move(invalid_fields))) == RegisterResult::InvalidField);
        CHECK(registry.FindType(invalid_field_owner_id) == nullptr);

        const TypeId actual_owner_id = MakeTypeId("Tests::ActualOwner");
        const TypeId wrong_owner_id = MakeTypeId("Tests::WrongOwner");
        std::vector<FieldInfo> wrong_owner_fields;
        wrong_owner_fields.emplace_back(wrong_owner_id, int_type_id, "health", FieldFlags::None, &GetHealthMutable,
                                        &GetHealthConst);
        CHECK(registry.RegisterType(TypeInfo(actual_owner_id, TypeKind::Class, "Tests::ActualOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample),
                                             std::move(wrong_owner_fields))) == RegisterResult::FieldOwnerMismatch);
        CHECK(registry.FindType(actual_owner_id) == nullptr);

        const TypeId duplicate_owner_id = MakeTypeId("Tests::DuplicateFieldOwner");
        std::vector<FieldInfo> duplicate_fields;
        duplicate_fields.emplace_back(duplicate_owner_id, int_type_id, "value", FieldFlags::None, &GetHealthMutable,
                                      &GetHealthConst);
        duplicate_fields.emplace_back(duplicate_owner_id, int_type_id, "value", FieldFlags::None, &GetIdentifierMutable,
                                      &GetIdentifierConst);
        CHECK(registry.RegisterType(TypeInfo(duplicate_owner_id, TypeKind::Class, "Tests::DuplicateFieldOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample),
                                             std::move(duplicate_fields))) == RegisterResult::DuplicateFieldName);
        CHECK(registry.FindType(duplicate_owner_id) == nullptr);
    }

    void TestFunctionRegistrationValidation()
    {
        TypeRegistry registry;

        const TypeId primitive_owner_id = MakeTypeId("Tests::PrimitiveWithFunction");
        std::vector<FunctionInfo> primitive_functions;
        primitive_functions.emplace_back(primitive_owner_id, GetTypeId<void>(), std::vector<ParameterInfo>{}, "Run",
                                         FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(registry.RegisterType(TypeInfo(primitive_owner_id, TypeKind::Primitive, "Tests::PrimitiveWithFunction",
                                             sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                             std::move(primitive_functions))) == RegisterResult::FunctionsNotAllowed);
        CHECK(registry.FindType(primitive_owner_id) == nullptr);

        const TypeId invalid_function_owner_id = MakeTypeId("Tests::InvalidFunctionOwner");
        std::vector<FunctionInfo> invalid_functions;
        invalid_functions.emplace_back(invalid_function_owner_id, GetTypeId<void>(), std::vector<ParameterInfo>{}, "",
                                       FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(registry.RegisterType(TypeInfo(invalid_function_owner_id, TypeKind::Class, "Tests::InvalidFunctionOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                             std::move(invalid_functions))) == RegisterResult::InvalidFunction);
        CHECK(registry.FindType(invalid_function_owner_id) == nullptr);

        const TypeId actual_owner_id = MakeTypeId("Tests::ActualFunctionOwner");
        const TypeId wrong_owner_id = MakeTypeId("Tests::WrongFunctionOwner");
        std::vector<FunctionInfo> wrong_owner_functions;
        wrong_owner_functions.emplace_back(wrong_owner_id, GetTypeId<void>(), std::vector<ParameterInfo>{}, "Run",
                                           FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(registry.RegisterType(TypeInfo(actual_owner_id, TypeKind::Class, "Tests::ActualFunctionOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                             std::move(wrong_owner_functions))) ==
              RegisterResult::FunctionOwnerMismatch);
        CHECK(registry.FindType(actual_owner_id) == nullptr);

        const TypeId duplicate_owner_id = MakeTypeId("Tests::DuplicateFunctionOwner");
        std::vector<FunctionInfo> duplicate_functions;
        duplicate_functions.emplace_back(duplicate_owner_id, GetTypeId<void>(), std::vector<ParameterInfo>{}, "Run",
                                         FunctionFlags::None, &DummyMutableInvoker, nullptr);
        duplicate_functions.emplace_back(duplicate_owner_id, GetTypeId<void>(), std::vector<ParameterInfo>{}, "Run",
                                         FunctionFlags::None, &DummyMutableInvoker, nullptr);
        CHECK(registry.RegisterType(TypeInfo(duplicate_owner_id, TypeKind::Class, "Tests::DuplicateFunctionOwner",
                                             sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                             std::move(duplicate_functions))) == RegisterResult::DuplicateFunctionName);
        CHECK(registry.FindType(duplicate_owner_id) == nullptr);
    }

    void TestFreezeValidation()
    {
        const TypeId owner_type_id = MakeTypeId("Tests::FreezeSample");
        const TypeId int_type_id = MakeTypeId("int");

        std::vector<FieldInfo> fields;
        fields.emplace_back(owner_type_id, int_type_id, "health", FieldFlags::None, &GetHealthMutable, &GetHealthConst);

        TypeRegistry registry;
        CHECK(registry.RegisterType(TypeInfo(owner_type_id, TypeKind::Class, "Tests::FreezeSample",
                                             sizeof(ReflectedSample), alignof(ReflectedSample), std::move(fields))) ==
              RegisterResult::Success);

        CHECK(registry.Freeze() == FreezeResult::MissingFieldType);
        CHECK(!registry.IsFrozen());

        CHECK(registry.RegisterType(TypeInfo(int_type_id, TypeKind::Primitive, "int", sizeof(int), alignof(int))) ==
              RegisterResult::Success);

        CHECK(registry.Freeze() == FreezeResult::Success);
        CHECK(registry.IsFrozen());
        CHECK(registry.Freeze() == FreezeResult::AlreadyFrozen);
        CHECK(registry.IsFrozen());

        CHECK(registry.RegisterType(TypeInfo(MakeTypeId("float"), TypeKind::Primitive, "float", sizeof(float),
                                             alignof(float))) == RegisterResult::Frozen);
    }

    void TestFunctionFreezeValidation()
    {
        {
            const TypeId owner_type_id = MakeTypeId("Tests::MissingReturnOwner");
            const TypeId return_type_id = MakeTypeId("Tests::MissingReturnType");

            std::vector<FunctionInfo> functions;
            functions.emplace_back(owner_type_id, return_type_id, std::vector<ParameterInfo>{}, "CreateValue",
                                   FunctionFlags::None, &DummyMutableInvoker, nullptr);

            TypeRegistry registry;
            CHECK(registry.RegisterType(TypeInfo(owner_type_id, TypeKind::Class, "Tests::MissingReturnOwner",
                                                 sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                                 std::move(functions))) == RegisterResult::Success);

            CHECK(registry.Freeze() == FreezeResult::MissingFunctionReturnType);
            CHECK(!registry.IsFrozen());

            CHECK(registry.RegisterType(TypeInfo(return_type_id, TypeKind::Struct, "Tests::MissingReturnType",
                                                 sizeof(int), alignof(int))) == RegisterResult::Success);
            CHECK(registry.Freeze() == FreezeResult::Success);
            CHECK(registry.IsFrozen());
        }

        {
            const TypeId owner_type_id = MakeTypeId("Tests::MissingParameterOwner");
            const TypeId parameter_type_id = MakeTypeId("Tests::MissingParameterType");

            std::vector<ParameterInfo> parameters;
            parameters.emplace_back(parameter_type_id, "value");

            std::vector<FunctionInfo> functions;
            functions.emplace_back(owner_type_id, GetTypeId<void>(), std::move(parameters), "SetValue",
                                   FunctionFlags::None, &DummyMutableInvoker, nullptr);

            TypeRegistry registry;
            CHECK(registry.RegisterType(TypeInfo(GetTypeId<void>(), TypeKind::Void, "void", 0, 0)) ==
                  RegisterResult::Success);
            CHECK(registry.RegisterType(TypeInfo(owner_type_id, TypeKind::Class, "Tests::MissingParameterOwner",
                                                 sizeof(ReflectedSample), alignof(ReflectedSample), {},
                                                 std::move(functions))) == RegisterResult::Success);

            CHECK(registry.Freeze() == FreezeResult::MissingFunctionParameterType);
            CHECK(!registry.IsFrozen());

            CHECK(registry.RegisterType(TypeInfo(parameter_type_id, TypeKind::Struct, "Tests::MissingParameterType",
                                                 sizeof(int), alignof(int))) == RegisterResult::Success);
            CHECK(registry.Freeze() == FreezeResult::Success);
            CHECK(registry.IsFrozen());
        }
    }

    void TestBuiltinTypeRegistration()
    {
        struct ExpectedBuiltin
        {
            std::string_view name;
            TypeKind kind;
            std::size_t size;
            std::size_t alignment;
        };

        const std::array<ExpectedBuiltin, 16> expected{{
            {"void", TypeKind::Void, 0, 0},
            {"bool", TypeKind::Primitive, sizeof(bool), alignof(bool)},
            {"char", TypeKind::Primitive, sizeof(char), alignof(char)},
            {"signed char", TypeKind::Primitive, sizeof(signed char), alignof(signed char)},
            {"unsigned char", TypeKind::Primitive, sizeof(unsigned char), alignof(unsigned char)},
            {"short", TypeKind::Primitive, sizeof(short), alignof(short)},
            {"unsigned short", TypeKind::Primitive, sizeof(unsigned short), alignof(unsigned short)},
            {"int", TypeKind::Primitive, sizeof(int), alignof(int)},
            {"unsigned int", TypeKind::Primitive, sizeof(unsigned int), alignof(unsigned int)},
            {"long", TypeKind::Primitive, sizeof(long), alignof(long)},
            {"unsigned long", TypeKind::Primitive, sizeof(unsigned long), alignof(unsigned long)},
            {"long long", TypeKind::Primitive, sizeof(long long), alignof(long long)},
            {"unsigned long long", TypeKind::Primitive, sizeof(unsigned long long), alignof(unsigned long long)},
            {"float", TypeKind::Primitive, sizeof(float), alignof(float)},
            {"double", TypeKind::Primitive, sizeof(double), alignof(double)},
            {"std::string", TypeKind::String, sizeof(std::string), alignof(std::string)},
        }};

        TypeRegistry registry;
        CHECK(RegisterBuiltinTypes(registry) == RegisterResult::Success);

        for (const ExpectedBuiltin& expected_type : expected)
        {
            const TypeInfo* const type_info = registry.FindType(expected_type.name);
            CHECK(type_info != nullptr);
            if (type_info == nullptr)
            {
                continue;
            }

            CHECK(type_info->GetTypeId() == MakeTypeId(expected_type.name));
            CHECK(type_info->GetStableName() == expected_type.name);
            CHECK(type_info->GetTypeKind() == expected_type.kind);
            CHECK(type_info->GetSize() == expected_type.size);
            CHECK(type_info->GetAlignment() == expected_type.alignment);
            CHECK(type_info->GetFields().empty());
        }

        CHECK(registry.Freeze() == FreezeResult::Success);

        TypeRegistry duplicate_registry;
        CHECK(duplicate_registry.RegisterType(TypeInfo(MakeTypeId("void"), TypeKind::Void, "void", 0, 0)) ==
              RegisterResult::Success);
        CHECK(RegisterBuiltinTypes(duplicate_registry) == RegisterResult::DuplicateType);

        TypeRegistry frozen_registry;
        CHECK(frozen_registry.Freeze() == FreezeResult::Success);
        CHECK(RegisterBuiltinTypes(frozen_registry) == RegisterResult::Frozen);
    }

    void TestGeneratedReflectionSample()
    {
        TypeRegistry registry;
        CHECK(RegisterBuiltinTypes(registry) == RegisterResult::Success);
        CHECK(RegisterReflectionSample(registry) == RegisterResult::Success);
        CHECK(RegisterReflectionSample(registry) == RegisterResult::DuplicateType);

        const TypeId sample_type_id = GetTypeId<ReflectionSample>();
        const TypeInfo* const sample_type = registry.FindType(sample_type_id);
        CHECK(sample_type != nullptr);
        CHECK(registry.FindType(GetStableTypeName<ReflectionSample>()) == sample_type);

        if (sample_type == nullptr)
        {
            return;
        }

        CHECK(sample_type->GetTypeKind() == TypeKind::Class);
        CHECK(sample_type->GetStableName() == std::string_view{"ReflectionTests::ReflectionSample"});
        CHECK(sample_type->GetSize() == sizeof(ReflectionSample));
        CHECK(sample_type->GetAlignment() == alignof(ReflectionSample));
        CHECK(sample_type->GetFields().size() == 3);
        CHECK(sample_type->GetFunctions().size() == 2);

        const FunctionInfo* const add_health_function = sample_type->FindFunction("AddHealth");
        const FunctionInfo* const get_health_function = sample_type->FindFunction("GetHealthWithBonus");
        CHECK(add_health_function != nullptr);
        CHECK(get_health_function != nullptr);
        CHECK(sample_type->FindFunction("MissingFunction") == nullptr);

        const FieldInfo* const health_field = sample_type->FindField("m_health");
        const FieldInfo* const name_field = sample_type->FindField("m_name");
        const FieldInfo* const cache_field = sample_type->FindField("m_runtime_cache");
        CHECK(health_field != nullptr);
        CHECK(name_field != nullptr);
        CHECK(cache_field != nullptr);

        ReflectionSample sample;
        CHECK(sample.GetHealth() == 100);
        CHECK(sample.GetName() == std::string_view{"Sample"});
        CHECK(sample.GetRuntimeCache() == 0.0F);

        ValueView owner_view = MakeValueView(sample);
        CHECK(owner_view.GetTypeId() == sample_type_id);

        if (health_field != nullptr)
        {
            ValueView health_view = health_field->GetValue(owner_view);
            CHECK(health_view.IsValid());
            CHECK(health_view.GetTypeId() == GetTypeId<int>());
            if (health_view.IsValid())
            {
                *static_cast<int*>(health_view.Data()) = 275;
            }
        }
        CHECK(sample.GetHealth() == 275);

        if (add_health_function != nullptr)
        {
            const int amount = 10;
            const std::array<ConstValueView, 1> arguments{
                MakeConstValueView(amount),
            };
            CHECK(add_health_function->Invoke(owner_view, arguments) == InvokeResult::Success);
        }
        CHECK(sample.GetHealth() == 285);

        if (get_health_function != nullptr)
        {
            const int bonus = 15;
            const std::array<ConstValueView, 1> arguments{
                MakeConstValueView(bonus),
            };
            int result = 0;
            CHECK(get_health_function->Invoke(owner_view, arguments, MakeValueView(result)) == InvokeResult::Success);
            CHECK(result == 300);
        }

        if (name_field != nullptr)
        {
            ValueView name_view = name_field->GetValue(owner_view);
            CHECK(name_view.IsValid());
            CHECK(name_view.GetTypeId() == GetTypeId<std::string>());
            if (name_view.IsValid())
            {
                *static_cast<std::string*>(name_view.Data()) = "Reflected";
            }
        }
        CHECK(sample.GetName() == std::string_view{"Reflected"});

        if (cache_field != nullptr)
        {
            CHECK(cache_field->IsTransient());
            ValueView cache_view = cache_field->GetValue(owner_view);
            CHECK(cache_view.IsValid());
            CHECK(cache_view.GetTypeId() == GetTypeId<float>());
            if (cache_view.IsValid())
            {
                *static_cast<float*>(cache_view.Data()) = 3.5F;
            }
        }
        CHECK(sample.GetRuntimeCache() == 3.5F);

        const ReflectionSample& const_sample = sample;
        const ConstValueView const_owner_view = MakeConstValueView(const_sample);
        if (health_field != nullptr)
        {
            const ConstValueView health_view = health_field->GetValue(const_owner_view);
            CHECK(health_view.IsValid());
            if (health_view.IsValid())
            {
                CHECK(*static_cast<const int*>(health_view.Data()) == 285);
            }
        }

        CHECK(registry.Freeze() == FreezeResult::Success);
        CHECK(registry.IsFrozen());
    }

    void TestRegistrationAndLookup()
    {
        TypeRegistry registry;
        CHECK(!registry.IsFrozen());

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("void"), TypeKind::Void, "void", 0, 0}) ==
              RegisterResult::Success);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("int"), TypeKind::Primitive, "int", sizeof(int),
                                             alignof(int)}) == RegisterResult::Success);

        const TypeInfo* const by_id = registry.FindType(MakeTypeId("int"));
        const TypeInfo* const by_name = registry.FindType("int");

        CHECK(by_id != nullptr);
        CHECK(by_name != nullptr);
        CHECK(by_id == by_name);
        CHECK(by_id->GetTypeId() == MakeTypeId("int"));
        CHECK(by_id->GetTypeKind() == TypeKind::Primitive);
        CHECK(by_id->GetStableName() == std::string_view{"int"});
        CHECK(by_id->GetSize() == sizeof(int));
        CHECK(by_id->GetAlignment() == alignof(int));
        CHECK(registry.FindType("float") == nullptr);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("int"), TypeKind::Primitive, "int", sizeof(int),
                                             alignof(int)}) == RegisterResult::DuplicateType);

        CHECK(registry.Freeze() == FreezeResult::Success);
        CHECK(registry.IsFrozen());
        CHECK(registry.Freeze() == FreezeResult::AlreadyFrozen);
        CHECK(registry.IsFrozen());

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("float"), TypeKind::Primitive, "float", sizeof(float),
                                             alignof(float)}) == RegisterResult::Frozen);
        CHECK(registry.FindType("int") == by_id);
    }

    void TestInvalidMetadata()
    {
        TypeRegistry registry;

        CHECK(registry.RegisterType(TypeInfo{TypeId{}, TypeKind::Primitive, "invalid-id", 1, 1}) ==
              RegisterResult::InvalidTypeId);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("placeholder"), TypeKind::Primitive, "", 1, 1}) ==
              RegisterResult::EmptyName);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("GE::A"), TypeKind::Primitive, "GE::B", 1, 1}) ==
              RegisterResult::IdNameMismatch);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("zero-size"), TypeKind::Primitive, "zero-size", 0, 1}) ==
              RegisterResult::InvalidSize);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("zero-alignment"), TypeKind::Primitive, "zero-alignment", 1,
                                             0}) == RegisterResult::InvalidAlignment);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("bad-void-size"), TypeKind::Void, "bad-void-size", 1, 0}) ==
              RegisterResult::InvalidSize);

        CHECK(registry.RegisterType(TypeInfo{MakeTypeId("bad-void-alignment"), TypeKind::Void, "bad-void-alignment", 0,
                                             1}) == RegisterResult::InvalidAlignment);
    }

#undef CHECK
} // namespace

int main()
{
    TestTypeId();
    TestTypeTraits();
    TestParameterInfo();
    TestFunctionInfoMetadata();
    TestFunctionInvocation();
    TestValueViews();
    TestValueViewFactories();
    TestFieldInfo();
    TestTypeInfoFields();
    TestFieldRegistrationValidation();
    TestFunctionRegistrationValidation();
    TestFreezeValidation();
    TestFunctionFreezeValidation();
    TestBuiltinTypeRegistration();
    TestGeneratedReflectionSample();
    TestRegistrationAndLookup();
    TestInvalidMetadata();

    if (g_failure_count != 0)
    {
        std::cerr << g_failure_count << " reflection registry test(s) failed.\n";
        return 1;
    }

    std::cout << "All reflection registry tests passed.\n";
    return 0;
}
