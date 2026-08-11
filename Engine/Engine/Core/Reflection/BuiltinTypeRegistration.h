#pragma once
//注册基本类型
#include "TypeRegistry.h"

namespace GE::Reflection
{
    [[nodiscard]]
    TypeRegistry::RegisterTypeResult RegisterBuiltinTypes(
        TypeRegistry& registry);
}