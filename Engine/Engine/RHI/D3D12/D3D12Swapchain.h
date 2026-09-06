#pragma once

#include <Engine/RHI/RHISwapchain.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace GE::D3D12RHI
{
    class D3D12Texture;

    // DXGI flip-model 交换链；拥有 COM 引用，但不拥有 HWND、Device 或 Queue。
    class D3D12Swapchain final : public RHI::Swapchain
    {
    public:
        D3D12Swapchain(
            Microsoft::WRL::ComPtr<IDXGISwapChain3> native_swapchain,
            std::uint32_t width,
            std::uint32_t height,
            RHI::RHIFormat format,
            std::uint32_t image_count,
            RHI::PresentMode present_mode,
            UINT native_flags) noexcept;

        ~D3D12Swapchain() noexcept override;

        // 在发布给通用 RHI 前取得全部 DXGI back buffer。
        [[nodiscard]] RHI::RHIResult InitializeImages() noexcept;

        [[nodiscard]] RHI::RHIFormat GetFormat() const noexcept override;
        [[nodiscard]] std::uint32_t GetWidth() const noexcept override;
        [[nodiscard]] std::uint32_t GetHeight() const noexcept override;
        [[nodiscard]] std::uint32_t GetImageCount() const noexcept override;
        [[nodiscard]] RHI::PresentMode GetPresentMode() const noexcept override;
        [[nodiscard]] RHI::Texture* GetImage(
            std::uint32_t index) noexcept override;
        [[nodiscard]] std::uint32_t
        GetCurrentImageIndex() const noexcept override;
        [[nodiscard]] RHI::Texture& GetCurrentImage() noexcept override;

        [[nodiscard]] RHI::RHIResult Resize(
            std::uint32_t width,
            std::uint32_t height) noexcept override;

        [[nodiscard]] RHI::RHIResult Present() noexcept override;

    private:
        [[nodiscard]] RHI::RHIResult RebuildImages() noexcept;

        Microsoft::WRL::ComPtr<IDXGISwapChain3> native_swapchain_;
        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;
        RHI::RHIFormat format_ = RHI::RHIFormat::Unknown;
        std::uint32_t image_count_ = 0;
        RHI::PresentMode present_mode_ = RHI::PresentMode::Fifo;
        UINT native_flags_ = 0;

        // 包装持有 back buffer 的 COM 引用；必须在 ResizeBuffers 前全部释放。
        std::vector<std::unique_ptr<D3D12Texture>> images_;
    };
}
