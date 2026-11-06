#pragma once
#include "RHICommandList.h"
#include <functional>
#include <utility>

namespace GE::RHI
{
    class CommandContext;

    class FrameRecording
    {
    public:
        FrameRecording(CommandContext& owner, CommandList& commands, uint32_t slot);
        ~FrameRecording();
        FrameRecording(const FrameRecording&) = delete;
        FrameRecording& operator=(const FrameRecording&) = delete;
        FrameRecording(FrameRecording&& other) noexcept;
        FrameRecording& operator=(FrameRecording&&) = delete;

        CommandList& Cmd() const { return *commands; }

        uint32_t SlotIndex() const { return slot; }

        bool BelongsTo(const CommandContext& context) const { return owner == &context; }

    private:
        friend class CommandContext;
        CommandContext* owner;
        CommandList* commands;
        uint32_t slot;
    };

    class CommandContext
    {
    public:
        virtual ~CommandContext() = default;
        virtual FrameRecording BeginFrame() = 0;
        virtual SubmissionId Submit(FrameRecording&& recording) = 0;
        virtual void Abort(FrameRecording&& recording) noexcept = 0;
        virtual bool IsComplete(SubmissionId id) const = 0;
        virtual void Wait(SubmissionId id) = 0;
        virtual void WaitIdle() = 0;
        virtual void ExecuteImmediate(const std::function<void(CommandList&)>& record) = 0;

    protected:
        static void Finish(FrameRecording& recording) { recording.owner = nullptr; }
    };

    inline FrameRecording::FrameRecording(CommandContext& context, CommandList& cmd, uint32_t index)
        : owner(&context), commands(&cmd), slot(index)
    {
    }

    inline FrameRecording::~FrameRecording()
    {
        if (owner)
            owner->Abort(std::move(*this));
    }

    inline FrameRecording::FrameRecording(FrameRecording&& other) noexcept
        : owner(std::exchange(other.owner, nullptr)), commands(other.commands), slot(other.slot)
    {
    }
} // namespace GE::RHI
