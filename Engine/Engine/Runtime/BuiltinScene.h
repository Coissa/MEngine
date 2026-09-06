#pragma once

namespace GE::Runtime
{
    class World;

    // 为尚未指定场景文件的 Game/Editor 建立最小可见内容。
    [[nodiscard]] bool PopulateBuiltinScene(World& world);
}
