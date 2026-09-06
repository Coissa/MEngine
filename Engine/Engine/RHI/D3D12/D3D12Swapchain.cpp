#include "D3D12Swapchain.h"
#include "D3D12Texture.h"

#include <cassert>
#include <cstdio>
#include <new>
#include <utility>

namespace GE::D3D12RHI
{
    D3D12Swapchain::D3D12Swapchain(
        Microsoft::WRL::ComPtr<IDXGISwapChain3> native_swapchain,
        std::uint32_t width,
        std::uint32_t height,
        RHI::RHIFormat format,
        std::uint32_t image_count,
        RHI::PresentMode present_mode,
        UINT native_flags) noexcept
        : native_swapchain_(std::move(native_swapchain)),
          width_(width),
          height_(height),
          format_(format),
          image_count_(image_count),
          present_mode_(present_mode),
          native_flags_(native_flags)
    {
    }

    D3D12Swapchain::~D3D12Swapchain() noexcept = default;

    RHI::RHIResult D3D12Swapchain::InitializeImages() noexcept
    {
        return RebuildImages();
    }

    RHI::RHIFormat D3D12Swapchain::GetFormat() const noexcept
    {
        return format_;
    }

    std::uint32_t D3D12Swapchain::GetWidth() const noexcept
    {
        return width_;
    }

    std::uint32_t D3D12Swapchain::GetHeight() const noexcept
    {
        return height_;
    }

    std::uint32_t D3D12Swapchain::GetImageCount() const noexcept
    {
        return image_count_;
    }

    RHI::PresentMode D3D12Swapchain::GetPresentMode() const noexcept
    {
        return present_mode_;
    }

    RHI::Texture* D3D12Swapchain::GetImage(
        std::uint32_t index) noexcept
    {
        return index < images_.size() ? images_[index].get() : nullptr;
    }

    std::uint32_t
    D3D12Swapchain::GetCurrentImageIndex() const noexcept
    {
        return native_swapchain_->GetCurrentBackBufferIndex();
    }

    RHI::Texture& D3D12Swapchain::GetCurrentImage() noexcept
    {
        const std::uint32_t index = GetCurrentImageIndex();
        assert(index < images_.size());
        return *images_[index];
    }

    RHI::RHIResult D3D12Swapchain::RebuildImages() noexcept
    {
        std::vector<std::unique_ptr<D3D12Texture>> rebuilt_images;

        try
        {
            rebuilt_images.reserve(image_count_);
            for (std::uint32_t index = 0; index < image_count_; ++index)
            {
                Microsoft::WRL::ComPtr<ID3D12Resource> native_resource;
                const HRESULT buffer_result = native_swapchain_->GetBuffer(
                    index,
                    IID_PPV_ARGS(native_resource.ReleaseAndGetAddressOf()));
                if (FAILED(buffer_result))
                {
                    std::fprintf(
                        stderr,
                        "[D3D12RHI] Swapchain GetBuffer failed, "
                        "index=%u, HRESULT=0x%08lX\n",
                        index,
                        static_cast<unsigned long>(buffer_result));
                    return RHI::RHIResult::BackendFailure;
                }

                RHI::TextureDescription description;
                description.width = width_;
                description.height = height_;
                description.format = format_;
                description.usage = RHI::TextureUsage::ColorAttachment
                    | RHI::TextureUsage::Present;

                rebuilt_images.push_back(
                    std::make_unique<D3D12Texture>(
                        std::move(native_resource),
                        description));
            }
        }
        catch (const std::bad_alloc&)
        {
            return RHI::RHIResult::BackendFailure;
        }

        images_ = std::move(rebuilt_images);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Swapchain::Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept
    {
        if (width == 0 || height == 0)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        // 释放所有应用侧 back-buffer COM 引用，满足 ResizeBuffers 前置条件。
        images_.clear();

        // 0/UNKNOWN 表示保留原交换链的图片数量与格式，只替换尺寸。
        const HRESULT resize_result = native_swapchain_->ResizeBuffers(
            0,
            width,
            height,
            DXGI_FORMAT_UNKNOWN,
            native_flags_);
        if (resize_result == DXGI_ERROR_INVALID_CALL)
        {
            // Resize 失败时 DXGI 保留旧缓冲；尽力恢复旧包装供诊断/销毁。
            static_cast<void>(RebuildImages());
            return RHI::RHIResult::InvalidArgument;
        }
        if (FAILED(resize_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Swapchain ResizeBuffers failed, "
                "HRESULT=0x%08lX\n",
                static_cast<unsigned long>(resize_result));
            static_cast<void>(RebuildImages());
            return RHI::RHIResult::BackendFailure;
        }

        width_ = width;
        height_ = height;
        return RebuildImages();
    }

    RHI::RHIResult D3D12Swapchain::Present() noexcept
    {
        const UINT sync_interval =
            present_mode_ == RHI::PresentMode::Fifo ? 1u : 0u;
        const UINT present_flags =
            present_mode_ == RHI::PresentMode::Immediate
                ? DXGI_PRESENT_ALLOW_TEARING
                : 0u;

        const HRESULT present_result = native_swapchain_->Present(
            sync_interval,
            present_flags);
        if (FAILED(present_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Swapchain Present failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(present_result));
            return RHI::RHIResult::BackendFailure;
        }

        // DXGI_STATUS_OCCLUDED 等成功状态表示窗口当前不可见，不是设备故障。
        return RHI::RHIResult::Success;
    }
}
