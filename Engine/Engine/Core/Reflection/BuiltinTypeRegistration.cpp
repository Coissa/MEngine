#include "BuiltinTypeRegistration.h"

#include <memory>
#include <new>
#include <string>
#include <type_traits>

namespace GE::Reflection
{
    namespace
    {
        template<typename T>
        void DefaultConstruct(void* storage)
        {
            std::construct_at(static_cast<T*>(storage));
        }

        template<typename T>
        void Destroy(void* object) noexcept
        {
            std::destroy_at(static_cast<T*>(object));
        }

        template<typename T>
        void CopyAssign(void* target, const void* source)
        {
            *static_cast<T*>(target) = *static_cast<const T*>(source);
        }

        template<typename T>
        TypeOperations Operations() noexcept
        {
            return {&DefaultConstruct<T>, &Destroy<T>, &CopyAssign<T>};
        }
    }

    Core::Result<void> RegisterBuiltinTypes(TypeRegistry& registry)
    {
        Core::Result<void> result = registry.RegisterType(TypeInfo(
            MakeTypeId("void"), TypeKind::Void, "void", 0, 0));
        if (!result) return result;

#define MENGINE_REGISTER_BUILTIN(cpp_type, stable_name, kind) \
        result = registry.RegisterType(TypeInfo( \
            MakeTypeId(stable_name), kind, stable_name, \
            sizeof(cpp_type), alignof(cpp_type), {}, {}, TypeFlags::None, \
            Operations<cpp_type>())); \
        if (!result) return result

        MENGINE_REGISTER_BUILTIN(bool, "bool", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(char, "char", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(signed char, "signed char", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(unsigned char, "unsigned char", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(short, "short", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(unsigned short, "unsigned short", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(int, "int", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(unsigned int, "unsigned int", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(long, "long", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(unsigned long, "unsigned long", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(long long, "long long", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(unsigned long long, "unsigned long long", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(float, "float", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(double, "double", TypeKind::Primitive);
        MENGINE_REGISTER_BUILTIN(std::string, "std::string", TypeKind::String);

#undef MENGINE_REGISTER_BUILTIN
        return Core::Result<void>::Success();
    }
}
