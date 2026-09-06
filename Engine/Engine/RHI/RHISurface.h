#pragma once

#include <cstdint>

namespace GE::RHI
{
    // 标识窗口句柄来自哪一种原生窗口系统；后端只解释自己支持的类型。
    enum class NativeWindowSystem : std::uint8_t
    {
        Unknown,
        Win32
    };

    // 创建呈现表面所需的借用值；不拥有窗口，也不负责销毁任何平台对象。
    struct NativeWindowSurface final
    {
        NativeWindowSystem system = NativeWindowSystem::Unknown;

        // Win32 分别保存 HINSTANCE 和 HWND 转换后的整数值。
        // 使用整数而不是 Windows 类型，使公共 RHI 头不依赖 Windows.h。
        std::uintptr_t display_handle = 0;
        std::uintptr_t window_handle = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return system == NativeWindowSystem::Win32
                && display_handle != 0
                && window_handle != 0;
        }
    };
}
