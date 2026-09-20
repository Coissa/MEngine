
#include "D3D12Device.h"
#include "D3D12Buffer.h"
#include "D3D12Fence.h"
#include "D3D12Pipeline.h"
#include "D3D12Queue.h"
#include "D3D12Sampler.h"
#include "D3D12Swapchain.h"
#include "D3D12Command.h"
#include "D3D12Texture.h"
#include "D3D12TextureView.h"
#include "D3D12Shader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    [[nodiscard]] DXGI_FORMAT ToNativeFormat(GE::RHI::RHIFormat format) noexcept
    {
        switch (format)
        {
        case GE::RHI::RHIFormat::R16_UInt: return DXGI_FORMAT_R16_UINT;
        case GE::RHI::RHIFormat::R32_UInt: return DXGI_FORMAT_R32_UINT;
        case GE::RHI::RHIFormat::R32G32_Float: return DXGI_FORMAT_R32G32_FLOAT;
        case GE::RHI::RHIFormat::R32G32B32_Float: return DXGI_FORMAT_R32G32B32_FLOAT;
        case GE::RHI::RHIFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case GE::RHI::RHIFormat::R8G8B8A8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GE::RHI::RHIFormat::B8G8R8A8_UNorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case GE::RHI::RHIFormat::D32_Float: return DXGI_FORMAT_D32_FLOAT;
        case GE::RHI::RHIFormat::D24_UNorm_S8_UInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    [[nodiscard]] std::uint32_t BytesPerTexel(
        GE::RHI::RHIFormat format) noexcept
    {
        switch (format)
        {
        case GE::RHI::RHIFormat::R8G8B8A8_UNorm:
        case GE::RHI::RHIFormat::B8G8R8A8_UNorm:
            return 4;
        case GE::RHI::RHIFormat::R32G32B32A32_Float:
            return 16;
        default:
            return 0;
        }
    }

    [[nodiscard]] bool TryMapTextureState(
        GE::RHI::TextureState state,
        D3D12_RESOURCE_STATES& out_state) noexcept
    {
        switch (state)
        {
        case GE::RHI::TextureState::Present: out_state = D3D12_RESOURCE_STATE_PRESENT; return true;
        case GE::RHI::TextureState::ColorAttachment: out_state = D3D12_RESOURCE_STATE_RENDER_TARGET; return true;
        case GE::RHI::TextureState::DepthWrite: out_state = D3D12_RESOURCE_STATE_DEPTH_WRITE; return true;
        case GE::RHI::TextureState::ShaderRead: out_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE; return true;
        case GE::RHI::TextureState::Storage: out_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; return true;
        case GE::RHI::TextureState::CopySource: out_state = D3D12_RESOURCE_STATE_COPY_SOURCE; return true;
        case GE::RHI::TextureState::CopyDestination: out_state = D3D12_RESOURCE_STATE_COPY_DEST; return true;
        default: return false;
        }
    }

    [[nodiscard]] bool TryMapBufferState(
        GE::RHI::BufferState state,
        D3D12_RESOURCE_STATES& out_state) noexcept
    {
        switch (state)
        {
        case GE::RHI::BufferState::Common: out_state = D3D12_RESOURCE_STATE_COMMON; return true;
        case GE::RHI::BufferState::Vertex:
        case GE::RHI::BufferState::Constant: out_state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; return true;
        case GE::RHI::BufferState::Index: out_state = D3D12_RESOURCE_STATE_INDEX_BUFFER; return true;
        case GE::RHI::BufferState::ShaderRead: out_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE; return true;
        case GE::RHI::BufferState::Storage: out_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; return true;
        case GE::RHI::BufferState::Indirect: out_state = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT; return true;
        case GE::RHI::BufferState::CopySource: out_state = D3D12_RESOURCE_STATE_COPY_SOURCE; return true;
        case GE::RHI::BufferState::CopyDestination: out_state = D3D12_RESOURCE_STATE_COPY_DEST; return true;
        default: return false;
        }
    }

    [[nodiscard]] D3D12_SHADER_VISIBILITY ToNativeVisibility(
        GE::RHI::ShaderVisibility visibility) noexcept
    {
        switch (visibility)
        {
        case GE::RHI::ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
        case GE::RHI::ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
        default: return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    [[nodiscard]] D3D12_COMPARISON_FUNC ToNativeCompare(
        GE::RHI::CompareOperation operation) noexcept
    {
        switch (operation)
        {
        case GE::RHI::CompareOperation::Never: return D3D12_COMPARISON_FUNC_NEVER;
        case GE::RHI::CompareOperation::Less: return D3D12_COMPARISON_FUNC_LESS;
        case GE::RHI::CompareOperation::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case GE::RHI::CompareOperation::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
        case GE::RHI::CompareOperation::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case GE::RHI::CompareOperation::Greater: return D3D12_COMPARISON_FUNC_GREATER;
        default: return D3D12_COMPARISON_FUNC_ALWAYS;
        }
    }

    // 在 D3D12Fence 正式接管句柄前提供异常安全，避免分配失败时泄漏事件。
    struct CompletionEventCloser final
    {
        void operator()(void* handle) const noexcept
        {
            if (handle != nullptr)
            {
                CloseHandle(handle);
            }
        }
    };
}

namespace GE::D3D12RHI
{
    D3D12Device::D3D12Device(
        RHI::AdapterId adapter_id,
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory,
        Microsoft::WRL::ComPtr<ID3D12Device> native_device,
        std::unique_ptr<D3D12Queue> graphics_queue) noexcept
        : factory_(std::move(factory)),
          native_device_(std::move(native_device)),
          graphics_queue_(std::move(graphics_queue))
    {
        capabilities_.adapter_id = adapter_id;
        capabilities_.features.graphics_pipeline = {
            RHI::FeatureSupport::Native, 1};
        capabilities_.features.storage_buffer = {
            RHI::FeatureSupport::Native, 1};

        // 这些是 D3D12 核心规范保证的固定限制，不需要额外查询 GPU。
        capabilities_.limits.max_texture_2d_dimension =
            D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;

        capabilities_.limits.max_color_attachments =
            D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;

        capabilities_.limits.constant_buffer_offset_alignment =
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    }

    D3D12Device::~D3D12Device() noexcept = default;

    const RHI::DeviceCapabilities&
    D3D12Device::GetCapabilities() const noexcept
    {
        // 返回借用的只读引用，其生命周期与 D3D12Device 相同。
        return capabilities_;
    }

    RHI::Queue& D3D12Device::GetGraphicsQueue() noexcept
    {
        // Backend 仅在设备和队列均初始化成功后发布 Device，因此此处保证非空。
        return *graphics_queue_;
    }

    RHI::RHIResult D3D12Device::CreateFence(
        std::unique_ptr<RHI::Fence>& out_fence)
    {
        out_fence.reset();

        // 原生对象通过输出参数取得，HRESULT 单独保存。
        Microsoft::WRL::ComPtr<ID3D12Fence> native_fence;

        const HRESULT create_result = native_device_->CreateFence(
            0,                       // 初始完成值
            D3D12_FENCE_FLAG_NONE,   // 不启用跨设备或跨进程共享
            IID_PPV_ARGS(native_fence.ReleaseAndGetAddressOf()));

        if (FAILED(create_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] CreateFence failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(create_result));
            return RHI::RHIResult::BackendFailure;
        }

        const HRESULT name_result =
            native_fence->SetName(L"MEngine.GraphicsFence");
        if (FAILED(name_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Fence SetName failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(name_result));
        }

        // 自动重置、初始无信号、仅进程内使用；Fence 在整个生命周期复用它。
        HANDLE raw_completion_event =
            CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (raw_completion_event == nullptr)
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] CreateEventW for Fence failed, Win32Error=%lu\n",
                static_cast<unsigned long>(GetLastError()));
            return RHI::RHIResult::BackendFailure;
        }

        std::unique_ptr<void, CompletionEventCloser> completion_event(
            raw_completion_event);
        auto fence = std::make_unique<D3D12Fence>(
            std::move(native_fence),
            *graphics_queue_,
            completion_event.get());
        completion_event.release();
        out_fence = std::move(fence);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateSwapchain(
        const RHI::SwapchainCreateInfo& create_info,
        std::unique_ptr<RHI::Swapchain>& out_swapchain)
    {
        out_swapchain.reset();

        if (!create_info.IsValid()
            || create_info.surface.system
                != RHI::NativeWindowSystem::Win32)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        const HWND window = reinterpret_cast<HWND>(
            create_info.surface.window_handle);
        if (!IsWindow(window))
        {
            return RHI::RHIResult::InvalidArgument;
        }

        DXGI_FORMAT native_format = DXGI_FORMAT_UNKNOWN;
        switch (create_info.format)
        {
        case RHI::RHIFormat::R8G8B8A8_UNorm:
            native_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case RHI::RHIFormat::B8G8R8A8_UNorm:
            native_format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            return RHI::RHIResult::Unsupported;
        }

        UINT swapchain_flags = 0;
        if (create_info.present_mode == RHI::PresentMode::Immediate)
        {
            BOOL allow_tearing = FALSE;
            const HRESULT tearing_result = factory_->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allow_tearing,
                sizeof(allow_tearing));
            if (FAILED(tearing_result) || allow_tearing == FALSE)
            {
                return RHI::RHIResult::Unsupported;
            }
            swapchain_flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_DESC1 native_desc{};
        native_desc.Width = create_info.width;
        native_desc.Height = create_info.height;
        native_desc.Format = native_format;
        native_desc.Stereo = FALSE;
        native_desc.SampleDesc.Count = 1;
        native_desc.SampleDesc.Quality = 0;
        native_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        native_desc.BufferCount = create_info.image_count;
        native_desc.Scaling = DXGI_SCALING_STRETCH;
        native_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        native_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        native_desc.Flags = swapchain_flags;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> native_swapchain1;
        const HRESULT create_result = factory_->CreateSwapChainForHwnd(
            graphics_queue_->native_queue_.Get(),
            window,
            &native_desc,
            nullptr,
            nullptr,
            native_swapchain1.ReleaseAndGetAddressOf());
        if (create_result == DXGI_ERROR_INVALID_CALL)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        if (FAILED(create_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] CreateSwapChainForHwnd failed, "
                "HRESULT=0x%08lX\n",
                static_cast<unsigned long>(create_result));
            return RHI::RHIResult::BackendFailure;
        }

        const HRESULT association_result = factory_->MakeWindowAssociation(
            window,
            DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(association_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] MakeWindowAssociation failed, "
                "HRESULT=0x%08lX\n",
                static_cast<unsigned long>(association_result));
            return RHI::RHIResult::BackendFailure;
        }

        Microsoft::WRL::ComPtr<IDXGISwapChain3> native_swapchain;
        const HRESULT interface_result = native_swapchain1.As(&native_swapchain);
        if (FAILED(interface_result))
        {
            std::fprintf(
                stderr,
                "[D3D12RHI] Query IDXGISwapChain3 failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(interface_result));
            return RHI::RHIResult::BackendFailure;
        }

        auto swapchain = std::make_unique<D3D12Swapchain>(
            std::move(native_swapchain),
            create_info.width,
            create_info.height,
            create_info.format,
            create_info.image_count,
            create_info.present_mode,
            swapchain_flags);

        const RHI::RHIResult image_result = swapchain->InitializeImages();
        if (image_result != RHI::RHIResult::Success)
        {
            return image_result;
        }

        out_swapchain = std::move(swapchain);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateColorAttachmentView(
        RHI::Texture& texture,
        std::unique_ptr<RHI::ColorAttachmentView>& out_view)
    {
        out_view.reset();
        auto* native_texture = dynamic_cast<D3D12Texture*>(&texture);
        if (native_texture == nullptr
            || !RHI::HasTextureUsage(
                texture.GetDescription().usage,
                RHI::TextureUsage::ColorAttachment))
        {
            return RHI::RHIResult::InvalidArgument;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.NumDescriptors = 1;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        const HRESULT heap_result = native_device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(heap_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE descriptor =
            heap->GetCPUDescriptorHandleForHeapStart();
        native_device_->CreateRenderTargetView(
            native_texture->native_resource_.Get(), nullptr, descriptor);
        out_view = std::make_unique<D3D12ColorAttachmentView>(
            std::move(heap), descriptor, *native_texture);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateSampledTextureView(
        RHI::Texture& texture,
        const RHI::SampledTextureViewDescription& view,
        std::unique_ptr<RHI::SampledTextureView>& out_view)
    {
        out_view.reset();
        auto* native_texture = dynamic_cast<D3D12Texture*>(&texture);
        const RHI::TextureDescription& description = texture.GetDescription();
        const DXGI_FORMAT native_format = ToNativeFormat(description.format);
        const std::uint32_t mip_count = view.mip_level_count == 0
            && view.base_mip_level < description.mip_levels
            ? description.mip_levels - view.base_mip_level
            : view.mip_level_count;
        const bool valid_dimension =
            (view.dimension == RHI::SampledTextureViewDimension::Texture2D
                && description.array_layers == 1)
            || (view.dimension
                    == RHI::SampledTextureViewDimension::TextureCube
                && description.array_layers == 6
                && description.width == description.height);
        if (native_texture == nullptr
            || !RHI::HasTextureUsage(
                description.usage, RHI::TextureUsage::Sampled)
            || native_format == DXGI_FORMAT_UNKNOWN
            || description.sample_count != 1
            || !valid_dimension
            || view.base_mip_level >= description.mip_levels
            || mip_count == 0
            || mip_count > description.mip_levels - view.base_mip_level
            || description.format == RHI::RHIFormat::D32_Float
            || description.format == RHI::RHIFormat::D24_UNorm_S8_UInt)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
        heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_description.NumDescriptors = 1;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        const HRESULT heap_result = native_device_->CreateDescriptorHeap(
            &heap_description, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(heap_result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE descriptor =
            heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_SHADER_RESOURCE_VIEW_DESC view_description{};
        view_description.Format = native_format;
        view_description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (view.dimension == RHI::SampledTextureViewDimension::TextureCube)
        {
            view_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view_description.TextureCube.MostDetailedMip = view.base_mip_level;
            view_description.TextureCube.MipLevels = mip_count;
            view_description.TextureCube.ResourceMinLODClamp = 0.0F;
        }
        else
        {
            view_description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view_description.Texture2D.MostDetailedMip = view.base_mip_level;
            view_description.Texture2D.MipLevels = mip_count;
            view_description.Texture2D.ResourceMinLODClamp = 0.0F;
        }
        native_device_->CreateShaderResourceView(
            native_texture->native_resource_.Get(),
            &view_description,
            descriptor);
        out_view = std::make_unique<D3D12SampledTextureView>(
            std::move(heap), descriptor, *native_texture);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateTexture(
        const RHI::TextureDescription& description,
        RHI::TextureState initial_state,
        std::unique_ptr<RHI::Texture>& out_texture)
    {
        out_texture.reset();
        D3D12_RESOURCE_STATES native_state{};
        const DXGI_FORMAT native_format = ToNativeFormat(description.format);
        const bool is_depth_format =
            description.format == RHI::RHIFormat::D32_Float
            || description.format == RHI::RHIFormat::D24_UNorm_S8_UInt;
        if (!description.IsValid() || native_format == DXGI_FORMAT_UNKNOWN
            || RHI::HasTextureUsage(description.usage, RHI::TextureUsage::Present)
            || !TryMapTextureState(initial_state, native_state)
            || description.array_layers > (std::numeric_limits<UINT16>::max)()
            || description.mip_levels > (std::numeric_limits<UINT16>::max)()
            || (RHI::HasTextureUsage(
                    description.usage, RHI::TextureUsage::DepthStencilAttachment)
                != is_depth_format)
            || (is_depth_format && RHI::HasTextureUsage(
                    description.usage, RHI::TextureUsage::ColorAttachment)))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        if (description.sample_count != 1
            || (RHI::HasTextureUsage(
                    description.usage, RHI::TextureUsage::DepthStencilAttachment)
                && RHI::HasTextureUsage(
                    description.usage, RHI::TextureUsage::Storage)))
        {
            return RHI::RHIResult::Unsupported;
        }

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if (RHI::HasTextureUsage(description.usage, RHI::TextureUsage::ColorAttachment))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (RHI::HasTextureUsage(description.usage, RHI::TextureUsage::DepthStencilAttachment))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (RHI::HasTextureUsage(description.usage, RHI::TextureUsage::Storage))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC native_desc{};
        native_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        native_desc.Width = description.width;
        native_desc.Height = description.height;
        native_desc.DepthOrArraySize = static_cast<UINT16>(description.array_layers);
        native_desc.MipLevels = static_cast<UINT16>(description.mip_levels);
        native_desc.Format = native_format;
        native_desc.SampleDesc.Count = description.sample_count;
        native_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        native_desc.Flags = flags;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        const HRESULT result = native_device_->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &native_desc,
            native_state,
            nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            std::fprintf(stderr,
                "[D3D12RHI] Create texture failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(result));
            return RHI::RHIResult::BackendFailure;
        }
        out_texture = std::make_unique<D3D12Texture>(
            std::move(resource), description);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::GetTextureUploadLayout(
        const RHI::TextureDescription& description,
        RHI::TextureSubresource subresource,
        std::uint64_t base_offset,
        RHI::TextureUploadLayout& out_layout) const noexcept
    {
        out_layout = {};
        if (!description.IsValid()
            || !RHI::HasTextureUsage(
                description.usage, RHI::TextureUsage::CopyDestination)
            || subresource.mip_level >= description.mip_levels
            || subresource.array_layer >= description.array_layers)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        const DXGI_FORMAT native_format = ToNativeFormat(description.format);
        const std::uint32_t bytes_per_texel = BytesPerTexel(description.format);
        if (native_format == DXGI_FORMAT_UNKNOWN
            || bytes_per_texel == 0
            || description.sample_count != 1)
        {
            return RHI::RHIResult::Unsupported;
        }

        D3D12_RESOURCE_DESC native_description{};
        native_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        native_description.Width = description.width;
        native_description.Height = description.height;
        native_description.DepthOrArraySize =
            static_cast<UINT16>(description.array_layers);
        native_description.MipLevels =
            static_cast<UINT16>(description.mip_levels);
        native_description.Format = native_format;
        native_description.SampleDesc.Count = 1;
        native_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        // D3D12 子资源编号：mip + arrayLayer * mipLevels；当前 plane 固定为 0。
        const UINT subresource_index = subresource.mip_level
            + subresource.array_layer * description.mip_levels;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT row_count = 0;
        UINT64 row_size = 0;
        UINT64 total_size = 0;
        native_device_->GetCopyableFootprints(
            &native_description,
            subresource_index,
            1,
            base_offset,
            &footprint,
            &row_count,
            &row_size,
            &total_size);
        if (total_size == (std::numeric_limits<UINT64>::max)()
            || row_count == 0
            || row_size == 0
            || footprint.Footprint.RowPitch == 0
            || total_size > (std::numeric_limits<UINT64>::max)()
                - base_offset)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        out_layout.subresource = subresource;
        out_layout.buffer_offset = footprint.Offset;
        // GetCopyableFootprints reports the byte count of the requested
        // subresource, excluding BaseOffset.  The RHI contract exposes the
        // minimum size of the complete upload buffer, so include it here.
        out_layout.required_buffer_size = base_offset + total_size;
        out_layout.row_size = row_size;
        out_layout.row_pitch = footprint.Footprint.RowPitch;
        out_layout.row_count = row_count;
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateDepthStencilView(
        RHI::Texture& texture,
        std::unique_ptr<RHI::DepthStencilView>& out_view)
    {
        out_view.reset();
        auto* native_texture = dynamic_cast<D3D12Texture*>(&texture);
        if (native_texture == nullptr
            || !RHI::HasTextureUsage(
                texture.GetDescription().usage,
                RHI::TextureUsage::DepthStencilAttachment))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heap_desc.NumDescriptors = 1;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        const HRESULT result = native_device_->CreateDescriptorHeap(
            &heap_desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptor =
            heap->GetCPUDescriptorHandleForHeapStart();
        native_device_->CreateDepthStencilView(
            native_texture->native_resource_.Get(), nullptr, descriptor);
        out_view = std::make_unique<D3D12DepthStencilView>(
            std::move(heap), descriptor, *native_texture);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateBuffer(
        const RHI::BufferDescription& description,
        RHI::BufferState initial_state,
        std::unique_ptr<RHI::Buffer>& out_buffer)
    {
        out_buffer.reset();
        D3D12_RESOURCE_STATES native_state{};
        if (!description.IsValid() || !TryMapBufferState(initial_state, native_state))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        if (description.memory == RHI::MemoryType::Upload)
        {
            if (initial_state == RHI::BufferState::Storage
                || initial_state == RHI::BufferState::CopyDestination)
            {
                return RHI::RHIResult::Unsupported;
            }
            native_state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        if (RHI::HasBufferUsage(description.usage, RHI::BufferUsage::Constant)
            && description.size > (std::numeric_limits<std::uint64_t>::max)() - 255u)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        const std::uint64_t allocation_size =
            RHI::HasBufferUsage(description.usage, RHI::BufferUsage::Constant)
            ? (description.size + 255u) & ~std::uint64_t{255u}
            : description.size;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = description.memory == RHI::MemoryType::Upload
            ? D3D12_HEAP_TYPE_UPLOAD
            : D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC native_desc{};
        native_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        native_desc.Width = allocation_size;
        native_desc.Height = 1;
        native_desc.DepthOrArraySize = 1;
        native_desc.MipLevels = 1;
        native_desc.SampleDesc.Count = 1;
        native_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        native_desc.Flags =
            RHI::HasBufferUsage(description.usage, RHI::BufferUsage::Storage)
            && description.memory == RHI::MemoryType::DeviceLocal
            ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            : D3D12_RESOURCE_FLAG_NONE;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        const HRESULT result = native_device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &native_desc, native_state, nullptr,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            std::fprintf(stderr,
                "[D3D12RHI] Create buffer failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(result));
            return RHI::RHIResult::BackendFailure;
        }
        if (!description.debug_name.empty())
        {
            const std::wstring name(
                description.debug_name.begin(), description.debug_name.end());
            (void)resource->SetName(name.c_str());
        }
        out_buffer = std::make_unique<D3D12Buffer>(
            std::move(resource), description);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateSampler(
        const RHI::SamplerDescription& description,
        std::unique_ptr<RHI::Sampler>& out_sampler)
    {
        out_sampler.reset();
        const auto map_address = [](RHI::SamplerAddressMode mode,
                                    D3D12_TEXTURE_ADDRESS_MODE& out_mode)
        {
            switch (mode)
            {
            case RHI::SamplerAddressMode::Repeat:
                out_mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                return true;
            case RHI::SamplerAddressMode::ClampToEdge:
                out_mode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                return true;
            default:
                return false;
            }
        };

        D3D12_SAMPLER_DESC native_description{};
        switch (description.filter)
        {
        case RHI::SamplerFilter::Nearest:
            native_description.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            break;
        case RHI::SamplerFilter::Linear:
            native_description.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        default:
            return RHI::RHIResult::InvalidArgument;
        }
        if (!map_address(description.address_u, native_description.AddressU)
            || !map_address(description.address_v, native_description.AddressV)
            || !map_address(description.address_w, native_description.AddressW))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        native_description.MaxAnisotropy = 1;
        native_description.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        native_description.MinLOD = 0.0F;
        native_description.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
        heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heap_description.NumDescriptors = 1;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        const HRESULT heap_result = native_device_->CreateDescriptorHeap(
            &heap_description, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()));
        if (FAILED(heap_result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        const D3D12_CPU_DESCRIPTOR_HANDLE descriptor =
            heap->GetCPUDescriptorHandleForHeapStart();
        native_device_->CreateSampler(&native_description, descriptor);
        out_sampler = std::make_unique<D3D12Sampler>(
            std::move(heap), descriptor, description);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateShader(
        const RHI::ShaderDescription& description,
        std::unique_ptr<RHI::Shader>& out_shader)
    {
        out_shader.reset();
        if (!description.IsValid())
        {
            return RHI::RHIResult::InvalidArgument;
        }
        if (description.format != RHI::ShaderBytecodeFormat::D3DBytecode)
        {
            return RHI::RHIResult::Unsupported;
        }
        std::vector<std::byte> bytes(
            description.bytecode.begin(), description.bytecode.end());
        out_shader = std::make_unique<D3D12Shader>(
            description.stage, std::move(bytes));
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreatePipelineLayout(
        const RHI::PipelineLayoutDescription& description,
        std::unique_ptr<RHI::PipelineLayout>& out_layout)
    {
        out_layout.reset();
        std::vector<D3D12_ROOT_PARAMETER> parameters(description.bindings.size());
        // 每个纹理/Sampler 绑定暂用独立的单描述符 Table；数组生命周期覆盖序列化调用。
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges(description.bindings.size());
        std::vector<D3D12RootBinding> bindings;
        bindings.reserve(description.bindings.size());
        for (std::size_t index = 0; index < description.bindings.size(); ++index)
        {
            const RHI::ResourceBindingLayout& binding = description.bindings[index];
            if (std::any_of(bindings.begin(), bindings.end(),
                [&binding](const D3D12RootBinding& value)
                { return value.binding == binding.binding; }))
            {
                return RHI::RHIResult::InvalidArgument;
            }
            D3D12_ROOT_PARAMETER& parameter = parameters[index];
            parameter.ShaderVisibility = ToNativeVisibility(binding.visibility);
            switch (binding.type)
            {
            case RHI::ResourceBindingType::ConstantBuffer:
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.Descriptor.ShaderRegister = binding.binding;
                parameter.Descriptor.RegisterSpace = 0;
                break;
            case RHI::ResourceBindingType::ReadOnlyStorageBuffer:
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                parameter.Descriptor.ShaderRegister = binding.binding;
                parameter.Descriptor.RegisterSpace = 0;
                break;
            case RHI::ResourceBindingType::ReadWriteStorageBuffer:
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
                parameter.Descriptor.ShaderRegister = binding.binding;
                parameter.Descriptor.RegisterSpace = 0;
                break;
            case RHI::ResourceBindingType::SampledTexture:
            case RHI::ResourceBindingType::Sampler:
            {
                D3D12_DESCRIPTOR_RANGE& range = ranges[index];
                range.RangeType = binding.type
                    == RHI::ResourceBindingType::SampledTexture
                    ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV
                    : D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                range.NumDescriptors = 1;
                range.BaseShaderRegister = binding.binding;
                range.OffsetInDescriptorsFromTableStart = 0;
                parameter.ParameterType =
                    D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                parameter.DescriptorTable.NumDescriptorRanges = 1;
                parameter.DescriptorTable.pDescriptorRanges = &range;
                break;
            }
            default:
                return RHI::RHIResult::InvalidArgument;
            }
            bindings.push_back({
                binding.binding,
                static_cast<std::uint32_t>(index),
                binding.type});
        }

        D3D12_ROOT_SIGNATURE_DESC native_desc{};
        native_desc.NumParameters = static_cast<UINT>(parameters.size());
        native_desc.pParameters = parameters.data();
        native_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        Microsoft::WRL::ComPtr<ID3DBlob> serialized;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        const HRESULT serialize_result = D3D12SerializeRootSignature(
            &native_desc, D3D_ROOT_SIGNATURE_VERSION_1,
            serialized.ReleaseAndGetAddressOf(), errors.ReleaseAndGetAddressOf());
        if (FAILED(serialize_result))
        {
            if (errors)
            {
                std::fprintf(stderr, "[D3D12RHI] Root signature: %s\n",
                    static_cast<const char*>(errors->GetBufferPointer()));
            }
            return RHI::RHIResult::BackendFailure;
        }
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature;
        const HRESULT create_result = native_device_->CreateRootSignature(
            0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
            IID_PPV_ARGS(root_signature.ReleaseAndGetAddressOf()));
        if (FAILED(create_result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        out_layout = std::make_unique<D3D12PipelineLayout>(
            std::move(root_signature), std::move(bindings));
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateResourceSet(
        const RHI::ResourceSetDescription& description,
        std::unique_ptr<RHI::ResourceSet>& out_resource_set)
    {
        out_resource_set.reset();
        auto* layout = dynamic_cast<D3D12PipelineLayout*>(description.layout);
        if (layout == nullptr
            || description.buffers.size()
                > (std::numeric_limits<UINT>::max)()
            || description.textures.size()
                > (std::numeric_limits<UINT>::max)()
            || description.samplers.size()
                > (std::numeric_limits<UINT>::max)()
            || description.buffers.size() + description.textures.size()
                + description.samplers.size() != layout->bindings_.size())
        {
            return RHI::RHIResult::InvalidArgument;
        }

        std::vector<D3D12BoundBuffer> buffers;
        buffers.reserve(description.buffers.size());
        std::vector<D3D12BoundDescriptor> descriptors;
        descriptors.reserve(
            description.textures.size() + description.samplers.size());
        std::vector<bool> bound_parameters(layout->bindings_.size(), false);

        for (const RHI::BufferBinding& binding : description.buffers)
        {
            auto layout_binding = std::find_if(
                layout->bindings_.begin(), layout->bindings_.end(),
                [&binding](const D3D12RootBinding& value)
                { return value.binding == binding.binding; });
            auto* buffer = dynamic_cast<D3D12Buffer*>(binding.buffer);
            if (layout_binding == layout->bindings_.end() || buffer == nullptr
                || (layout_binding->type
                        != RHI::ResourceBindingType::ConstantBuffer
                    && layout_binding->type
                        != RHI::ResourceBindingType::ReadOnlyStorageBuffer
                    && layout_binding->type
                        != RHI::ResourceBindingType::ReadWriteStorageBuffer)
                || binding.size == 0 || binding.offset > buffer->description_.size
                || binding.size > buffer->description_.size - binding.offset
                || bound_parameters[layout_binding->root_parameter])
            {
                return RHI::RHIResult::InvalidArgument;
            }
            const RHI::BufferUsage required_usage =
                layout_binding->type == RHI::ResourceBindingType::ConstantBuffer
                ? RHI::BufferUsage::Constant
                : RHI::BufferUsage::Storage;
            const std::uint64_t required_alignment =
                layout_binding->type == RHI::ResourceBindingType::ConstantBuffer
                ? 256u : 16u;
            if (!RHI::HasBufferUsage(buffer->description_.usage, required_usage)
                || binding.offset % required_alignment != 0
                || (layout_binding->type
                        == RHI::ResourceBindingType::ReadWriteStorageBuffer
                    && buffer->description_.memory != RHI::MemoryType::DeviceLocal))
            {
                return RHI::RHIResult::InvalidArgument;
            }
            buffers.push_back({
                layout_binding->root_parameter,
                layout_binding->type,
                buffer->native_resource_->GetGPUVirtualAddress() + binding.offset});
            bound_parameters[layout_binding->root_parameter] = true;
        }

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> resource_heap;
        if (!description.textures.empty())
        {
            D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
            heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heap_description.NumDescriptors =
                static_cast<UINT>(description.textures.size());
            heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            const HRESULT result = native_device_->CreateDescriptorHeap(
                &heap_description,
                IID_PPV_ARGS(resource_heap.ReleaseAndGetAddressOf()));
            if (FAILED(result))
            {
                return RHI::RHIResult::BackendFailure;
            }
        }

        const UINT resource_increment = description.textures.empty()
            ? 0
            : native_device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for (std::size_t index = 0;
             index < description.textures.size();
             ++index)
        {
            const RHI::TextureBinding& binding = description.textures[index];
            const auto layout_binding = std::find_if(
                layout->bindings_.begin(), layout->bindings_.end(),
                [&binding](const D3D12RootBinding& value)
                { return value.binding == binding.binding; });
            auto* view = dynamic_cast<D3D12SampledTextureView*>(binding.view);
            if (layout_binding == layout->bindings_.end() || view == nullptr
                || layout_binding->type
                    != RHI::ResourceBindingType::SampledTexture
                || bound_parameters[layout_binding->root_parameter])
            {
                return RHI::RHIResult::InvalidArgument;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE destination =
                resource_heap->GetCPUDescriptorHandleForHeapStart();
            destination.ptr += index * resource_increment;
            native_device_->CopyDescriptorsSimple(
                1,
                destination,
                view->descriptor_,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            D3D12_GPU_DESCRIPTOR_HANDLE visible =
                resource_heap->GetGPUDescriptorHandleForHeapStart();
            visible.ptr += index * resource_increment;
            descriptors.push_back({layout_binding->root_parameter, visible});
            bound_parameters[layout_binding->root_parameter] = true;
        }

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> sampler_heap;
        if (!description.samplers.empty())
        {
            D3D12_DESCRIPTOR_HEAP_DESC heap_description{};
            heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            heap_description.NumDescriptors =
                static_cast<UINT>(description.samplers.size());
            heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            const HRESULT result = native_device_->CreateDescriptorHeap(
                &heap_description,
                IID_PPV_ARGS(sampler_heap.ReleaseAndGetAddressOf()));
            if (FAILED(result))
            {
                return RHI::RHIResult::BackendFailure;
            }
        }

        const UINT sampler_increment = description.samplers.empty()
            ? 0
            : native_device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        for (std::size_t index = 0;
             index < description.samplers.size();
             ++index)
        {
            const RHI::SamplerBinding& binding = description.samplers[index];
            const auto layout_binding = std::find_if(
                layout->bindings_.begin(), layout->bindings_.end(),
                [&binding](const D3D12RootBinding& value)
                { return value.binding == binding.binding; });
            auto* sampler = dynamic_cast<D3D12Sampler*>(binding.sampler);
            if (layout_binding == layout->bindings_.end() || sampler == nullptr
                || layout_binding->type != RHI::ResourceBindingType::Sampler
                || bound_parameters[layout_binding->root_parameter])
            {
                return RHI::RHIResult::InvalidArgument;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE destination =
                sampler_heap->GetCPUDescriptorHandleForHeapStart();
            destination.ptr += index * sampler_increment;
            native_device_->CopyDescriptorsSimple(
                1,
                destination,
                sampler->descriptor_,
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            D3D12_GPU_DESCRIPTOR_HANDLE visible =
                sampler_heap->GetGPUDescriptorHandleForHeapStart();
            visible.ptr += index * sampler_increment;
            descriptors.push_back({layout_binding->root_parameter, visible});
            bound_parameters[layout_binding->root_parameter] = true;
        }

        if (!std::all_of(
                bound_parameters.begin(),
                bound_parameters.end(),
                [](bool value) { return value; }))
        {
            return RHI::RHIResult::InvalidArgument;
        }
        out_resource_set = std::make_unique<D3D12ResourceSet>(
            *layout,
            std::move(buffers),
            std::move(resource_heap),
            std::move(sampler_heap),
            std::move(descriptors));
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateGraphicsPipeline(
        const RHI::GraphicsPipelineDescription& description,
        std::unique_ptr<RHI::GraphicsPipeline>& out_pipeline)
    {
        out_pipeline.reset();
        auto* layout = dynamic_cast<D3D12PipelineLayout*>(description.layout);
        auto* vertex_shader = dynamic_cast<D3D12Shader*>(description.vertex_shader);
        auto* pixel_shader = dynamic_cast<D3D12Shader*>(description.pixel_shader);
        const DXGI_FORMAT color_format = ToNativeFormat(description.color_format);
        const DXGI_FORMAT depth_format = ToNativeFormat(description.depth_format);
        if (!description.IsValid() || layout == nullptr
            || vertex_shader == nullptr || pixel_shader == nullptr
            || vertex_shader->stage_ != RHI::ShaderStage::Vertex
            || pixel_shader->stage_ != RHI::ShaderStage::Pixel
            || color_format == DXGI_FORMAT_UNKNOWN
            || (description.depth.test_enabled && depth_format == DXGI_FORMAT_UNKNOWN))
        {
            return RHI::RHIResult::InvalidArgument;
        }

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
        inputs.reserve(description.vertex_attributes.size());
        for (const RHI::VertexAttribute& attribute : description.vertex_attributes)
        {
            const DXGI_FORMAT format = ToNativeFormat(attribute.format);
            if (format == DXGI_FORMAT_UNKNOWN)
            {
                return RHI::RHIResult::Unsupported;
            }
            inputs.push_back({
                "ATTRIBUTE", attribute.location, format, 0, attribute.offset,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC native_desc{};
        native_desc.pRootSignature = layout->root_signature_.Get();
        native_desc.VS = {vertex_shader->bytecode_.data(), vertex_shader->bytecode_.size()};
        native_desc.PS = {pixel_shader->bytecode_.data(), pixel_shader->bytecode_.size()};
        native_desc.BlendState.AlphaToCoverageEnable = FALSE;
        native_desc.BlendState.IndependentBlendEnable = FALSE;
        if (description.blend.alpha_blending_enabled)
        {
            D3D12_RENDER_TARGET_BLEND_DESC& blend =
                native_desc.BlendState.RenderTarget[0];
            blend.BlendEnable = TRUE;
            blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blend.BlendOp = D3D12_BLEND_OP_ADD;
            blend.SrcBlendAlpha = D3D12_BLEND_ONE;
            blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }
        native_desc.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        native_desc.SampleMask = UINT_MAX;
        native_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        native_desc.RasterizerState.CullMode =
            description.cull_mode == RHI::CullMode::None
            ? D3D12_CULL_MODE_NONE
            : description.cull_mode == RHI::CullMode::Front
                ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
        native_desc.RasterizerState.FrontCounterClockwise =
            description.front_face == RHI::FrontFace::CounterClockwise;
        native_desc.RasterizerState.DepthClipEnable = TRUE;
        native_desc.DepthStencilState.DepthEnable = description.depth.test_enabled;
        native_desc.DepthStencilState.DepthWriteMask = description.depth.write_enabled
            ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        native_desc.DepthStencilState.DepthFunc = ToNativeCompare(description.depth.compare);
        native_desc.DepthStencilState.StencilEnable = FALSE;
        native_desc.InputLayout = {inputs.data(), static_cast<UINT>(inputs.size())};
        native_desc.PrimitiveTopologyType =
            description.topology == RHI::PrimitiveTopology::LineList
            ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
            : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        native_desc.NumRenderTargets = 1;
        native_desc.RTVFormats[0] = color_format;
        native_desc.DSVFormat = description.depth.test_enabled
            ? depth_format : DXGI_FORMAT_UNKNOWN;
        native_desc.SampleDesc.Count = 1;

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
        const HRESULT result = native_device_->CreateGraphicsPipelineState(
            &native_desc, IID_PPV_ARGS(pipeline_state.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            std::fprintf(stderr,
                "[D3D12RHI] Create graphics pipeline failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(result));
            return RHI::RHIResult::BackendFailure;
        }
        out_pipeline = std::make_unique<D3D12GraphicsPipeline>(
            std::move(pipeline_state), layout->root_signature_,
            *layout, description.topology);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateComputePipeline(
        const RHI::ComputePipelineDescription& description,
        std::unique_ptr<RHI::ComputePipeline>& out_pipeline)
    {
        out_pipeline.reset();
        auto* layout = dynamic_cast<D3D12PipelineLayout*>(description.layout);
        auto* shader = dynamic_cast<D3D12Shader*>(description.compute_shader);
        if (!description.IsValid() || layout == nullptr || shader == nullptr
            || shader->stage_ != RHI::ShaderStage::Compute)
        {
            return RHI::RHIResult::InvalidArgument;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC native_desc{};
        native_desc.pRootSignature = layout->root_signature_.Get();
        native_desc.CS = {shader->bytecode_.data(), shader->bytecode_.size()};
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state;
        const HRESULT result = native_device_->CreateComputePipelineState(
            &native_desc, IID_PPV_ARGS(pipeline_state.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            std::fprintf(stderr,
                "[D3D12RHI] Create compute pipeline failed, HRESULT=0x%08lX\n",
                static_cast<unsigned long>(result));
            return RHI::RHIResult::BackendFailure;
        }
        out_pipeline = std::make_unique<D3D12ComputePipeline>(
            std::move(pipeline_state), layout->root_signature_, *layout);
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateCommandAllocator(
        std::unique_ptr<RHI::CommandAllocator>& out_allocator)
    {
        out_allocator.reset();
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> native_allocator;
        const HRESULT result = native_device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(native_allocator.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        out_allocator = std::make_unique<D3D12CommandAllocator>(
            std::move(native_allocator));
        return RHI::RHIResult::Success;
    }

    RHI::RHIResult D3D12Device::CreateCommandList(
        RHI::CommandAllocator& allocator,
        std::unique_ptr<RHI::CommandList>& out_command_list)
    {
        out_command_list.reset();
        auto* native_allocator = dynamic_cast<D3D12CommandAllocator*>(&allocator);
        if (native_allocator == nullptr)
        {
            return RHI::RHIResult::InvalidArgument;
        }
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> native_list;
        const HRESULT result = native_device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            native_allocator->native_allocator_.Get(),
            nullptr,
            IID_PPV_ARGS(native_list.ReleaseAndGetAddressOf()));
        if (FAILED(result))
        {
            return RHI::RHIResult::BackendFailure;
        }

        D3D12_INDIRECT_ARGUMENT_DESC argument{};
        argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
        D3D12_COMMAND_SIGNATURE_DESC signature_description{};
        signature_description.ByteStride =
            sizeof(RHI::DrawIndexedIndirectArguments);
        signature_description.NumArgumentDescs = 1;
        signature_description.pArgumentDescs = &argument;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> signature;
        const HRESULT signature_result = native_device_->CreateCommandSignature(
            &signature_description, nullptr,
            IID_PPV_ARGS(signature.ReleaseAndGetAddressOf()));
        if (FAILED(signature_result))
        {
            return RHI::RHIResult::BackendFailure;
        }
        out_command_list = std::make_unique<D3D12CommandList>(
            std::move(native_list), std::move(signature));
        return RHI::RHIResult::Success;
    }
}
