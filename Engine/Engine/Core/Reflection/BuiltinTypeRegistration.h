#pragma once
//注册基本类型
#include "TypeRegistry.h"

namespace GE::Reflection
{
    [[nodiscard]]
    Core::Result<void> RegisterBuiltinTypes(
        TypeRegistry& registry);
}
