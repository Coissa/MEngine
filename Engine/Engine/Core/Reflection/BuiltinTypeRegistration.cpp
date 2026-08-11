#include "BuiltinTypeRegistration.h"
#include <string>
#include <utility>

namespace GE::Reflection
{

    [[nodiscard]] GE::Reflection::TypeRegistry::RegisterTypeResult RegisterBuiltinTypes(GE::Reflection::TypeRegistry& registry)
    {
        TypeInfo void_type(MakeTypeId("void"), TypeKind::Void, "void", 0, 0);
        TypeInfo bool_type(MakeTypeId("bool"), TypeKind::Primitive, "bool", sizeof(bool), alignof(bool));
        TypeInfo char_type(MakeTypeId("char"), TypeKind::Primitive, "char", sizeof(char), alignof(char));
        TypeInfo signed_char_type(MakeTypeId("signed char"), TypeKind::Primitive, "signed char", sizeof(signed char), alignof(signed char));
        TypeInfo unsigned_char_type(MakeTypeId("unsigned char"), TypeKind::Primitive, "unsigned char", sizeof(unsigned char), alignof(unsigned char));
        TypeInfo short_type(MakeTypeId("short"), TypeKind::Primitive, "short", sizeof(short), alignof(short));
        TypeInfo unsigned_short_type(MakeTypeId("unsigned short"), TypeKind::Primitive, "unsigned short", sizeof(unsigned short), alignof(unsigned short));
        TypeInfo int_type(MakeTypeId("int"), TypeKind::Primitive, "int", sizeof(int), alignof(int));
        TypeInfo unsigned_int_type(MakeTypeId("unsigned int"), TypeKind::Primitive, "unsigned int", sizeof(unsigned int), alignof(unsigned int));
        TypeInfo long_type(MakeTypeId("long"), TypeKind::Primitive, "long", sizeof(long), alignof(long));
        TypeInfo unsigned_long_type(MakeTypeId("unsigned long"), TypeKind::Primitive, "unsigned long", sizeof(unsigned long), alignof(unsigned long));
        TypeInfo long_long_type(MakeTypeId("long long"), TypeKind::Primitive, "long long", sizeof(long long), alignof(long long));
        TypeInfo unsigned_long_long_type(MakeTypeId("unsigned long long"), TypeKind::Primitive, "unsigned long long", sizeof(unsigned long long), alignof(unsigned long long));
        TypeInfo float_type(MakeTypeId("float"), TypeKind::Primitive, "float", sizeof(float), alignof(float));
        TypeInfo string_type(MakeTypeId("std::string"), TypeKind::String, "std::string", sizeof(std::string), alignof(std::string));
        TypeInfo double_type(MakeTypeId("double"), TypeKind::Primitive, "double", sizeof(double), alignof(double));

        TypeRegistry::RegisterTypeResult result;

        result = registry.RegisterType(std::move(void_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }

        result = registry.RegisterType(std::move(bool_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(char_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(signed_char_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(unsigned_char_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(short_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(unsigned_short_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(int_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(unsigned_int_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(long_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(unsigned_long_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(long_long_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(unsigned_long_long_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(float_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(string_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }
        result = registry.RegisterType(std::move(double_type));
        if (result != TypeRegistry::RegisterTypeResult::Success)
        {
            return result;
        }

        return TypeRegistry::RegisterTypeResult::Success;

    }
}