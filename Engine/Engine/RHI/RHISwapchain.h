#pragma once

#include "RHIFormat.h"
#include "RHIResult.h"
#include "RHISurface.h"
#include "RHITexture.h"

#include <cstdint>

namespace GE::RHI
{
    // 呈现模式是 D3D12/Vulkan 都能表达的共同语义。
    // Fifo 保证垂直同步；Immediate 允许立即呈现，但后端可能明确返回 Unsupported。
    enum class PresentMode : std::uint8_t
    {
        Fifo,
        Immediate
    };

    // 创建交换链所需的纯值描述；其中的窗口表面仍然只是借用。
    struct SwapchainCreateInfo final
    {
        NativeWindowSurface surface;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        RHIFormat format = RHIFormat::B8G8R8A8_UNorm;
        std::uint32_t image_count = 2;
        PresentMode present_mode = PresentMode::Fifo;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return surface.IsValid()
                && width != 0
                && height != 0
                && format != RHIFormat::Unknown
                && image_count >= 2;
        }
    };

    // 进程渲染服务拥有的呈现资源抽象；具体后端持有 DXGI/Vulkan 交换链。
    class Swapchain
    {
    public:
        virtual ~Swapchain() noexcept = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] virtual RHIFormat GetFormat() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t GetWidth() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t GetHeight() const noexcept = 0;
        [[nodiscard]] virtual std::uint32_t GetImageCount() const noexcept = 0;
        [[nodiscard]] virtual PresentMode GetPresentMode() const noexcept = 0;

        // 任意索引查询可失败；返回值只是借用，不允许调用方 delete。
        [[nodiscard]] virtual Texture* GetImage(
            std::uint32_t index) noexcept = 0;

        [[nodiscard]] virtual std::uint32_t
        GetCurrentImageIndex() const noexcept = 0;

        // 返回当前可渲染图片；借用在 Resize 或 Swapchain 销毁时失效。
        [[nodiscard]] virtual Texture& GetCurrentImage() noexcept = 0;

        // 最小化时调用方应跳过 Resize；零尺寸属于无效参数。
        // 调用前必须通过 Fence 确认 GPU 不再使用旧交换链图片，并停止持有借用引用。
        // 失败后调用方应销毁并重建交换链，不再查询旧图片包装。
        [[nodiscard]] virtual RHIResult Resize(
            std::uint32_t width,
            std::uint32_t height) noexcept = 0;

        // 呈现当前帧；同步间隔等后端细节由创建时选择的 PresentMode 决定。
        [[nodiscard]] virtual RHIResult Present() noexcept = 0;

    protected:
        Swapchain() noexcept = default;
    };
}
