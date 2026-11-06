#include "D3D12GraphicsQueue.h"

namespace GE::D3D12
{
    GraphicsQueue::GraphicsQueue(ID3D12Device& device)
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device.CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");
        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event)
            Check(HRESULT_FROM_WIN32(GetLastError()), "CreateEvent");
    }

    GraphicsQueue::~GraphicsQueue()
    {
        CloseHandle(event);
    }

    RHI::SubmissionId GraphicsQueue::Signal()
    {
        const RHI::SubmissionId id{++nextSubmission};
        Check(queue->Signal(fence.Get(), id.value), "Queue Signal");
        return id;
    }

    RHI::SubmissionId GraphicsQueue::Submit(ID3D12CommandList& commands)
    {
        ID3D12CommandList* lists[] = {&commands};
        queue->ExecuteCommandLists(1, lists);
        return Signal(); // Establishes completion marker, does not wait for GPU
                         // completion.
    }

    bool GraphicsQueue::IsComplete(RHI::SubmissionId id) const
    {
        const uint64_t completed = fence->GetCompletedValue();
        if (completed == UINT64_MAX)
            throw std::runtime_error("Device removed while reading Fence");
        return completed >= id.value;
    }

    void GraphicsQueue::Wait(RHI::SubmissionId id)
    {
        if (IsComplete(id))
            return;
        Check(fence->SetEventOnCompletion(id.value, event), "Fence SetEventOnCompletion");
        if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
            throw std::runtime_error("Fence wait failed");
        IsComplete(id); // Detect device removal that woke the event.
    }

    void GraphicsQueue::WaitIdle()
    {
        Wait(Signal());
    }
} // namespace GE::D3D12
