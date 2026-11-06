#include "D3D12CommandContext.h"

namespace GE::D3D12
{
    CommandContext::CommandContext(ID3D12Device& device, GraphicsQueue& graphicsQueue, ID3D12DescriptorHeap& shaderHeap,
                                   ID3D12DescriptorHeap& samplerHeap)
        : queue(graphicsQueue)
    {
        for (auto& slot : slots)
            Check(device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slot.allocator)),
                  "CreateCommandAllocator");
        commands = std::make_unique<CommandList>(device, *slots[0].allocator.Get(), shaderHeap, samplerHeap);
    }

    RHI::FrameRecording CommandContext::BeginFrame()
    {
        assert(!active);
        if (failed)
            throw std::runtime_error("Context cannot continue after a submission failure");
        const uint32_t index = nextSlot;
        nextSlot = (nextSlot + 1) % RHI::FrameSlotCount;
        auto& slot = slots[index];
        queue.Wait(slot.lastSubmission);
        Check(slot.allocator->Reset(), "CommandAllocator Reset");
        commands->Begin(*slot.allocator.Get());
        active = true;
        return RHI::FrameRecording(*this, *commands, index);
    }

    RHI::SubmissionId CommandContext::Submit(RHI::FrameRecording&& recording)
    {
        assert(active && recording.BelongsTo(*this));
        try
        {
            commands->Close();
            auto id = queue.Submit(*commands->Native());
            commands->Commit(); // Accepted submission + completion marker established.
            slots[recording.SlotIndex()].lastSubmission = id;
            active = false;
            Finish(recording);
            return id;
        }
        catch (...)
        {
            // ExecuteCommandLists may already have run. Never guess, retry or reuse
            // this context.
            failed = true;
            active = false;
            Finish(recording);
            throw;
        }
    }

    void CommandContext::Abort(RHI::FrameRecording&& recording) noexcept
    {
        assert(active && recording.BelongsTo(*this));
        commands->Abort(); // Discard local states; queue-known states remain untouched.
        active = false;
        Finish(recording);
    }

    void CommandContext::WaitIdle()
    {
        assert(!active);
        queue.WaitIdle();
    }

    void CommandContext::ExecuteImmediate(const std::function<void(RHI::CommandList&)>& record)
    {
        auto recording = BeginFrame();
        record(recording.Cmd()); // An exception automatically aborts the recording.
        Wait(Submit(std::move(recording)));
    }
} // namespace GE::D3D12
