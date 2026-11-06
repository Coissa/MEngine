#pragma once
#include "../RHICommandContext.h"
#include "D3D12CommandList.h"
#include "D3D12GraphicsQueue.h"
#include <memory>

namespace GE::D3D12
{
    class CommandContext final : public RHI::CommandContext
    {
    public:
        CommandContext(ID3D12Device& device, GraphicsQueue& queue, ID3D12DescriptorHeap& shaderHeap,
                       ID3D12DescriptorHeap& samplerHeap);
        RHI::FrameRecording BeginFrame() override;
        RHI::SubmissionId Submit(RHI::FrameRecording&& recording) override;
        void Abort(RHI::FrameRecording&& recording) noexcept override;

        bool IsComplete(RHI::SubmissionId id) const override { return queue.IsComplete(id); }

        void Wait(RHI::SubmissionId id) override { queue.Wait(id); }

        void WaitIdle() override;
        void ExecuteImmediate(const std::function<void(RHI::CommandList&)>& record) override;

    private:
        struct Slot
        {
            ComPtr<ID3D12CommandAllocator> allocator;
            RHI::SubmissionId lastSubmission;
        };

        GraphicsQueue& queue;
        std::array<Slot, RHI::FrameSlotCount> slots;
        std::unique_ptr<CommandList> commands;
        uint32_t nextSlot = 0;
        bool active = false;
        bool failed = false;
    };
} // namespace GE::D3D12
