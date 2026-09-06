#pragma once

#include "RHIResult.h"

#include <cstdint>

namespace GE::RHI
{
    class Fence;
    class CommandList;

    // 描述队列的命令用途，不保证不同用途对应不同物理执行引擎。
    enum class QueueType : std::uint8_t
    {
        Graphics,
        Compute,
        Copy
    };

    class Queue
    {
    public:
        // 通过基类销毁时调用派生类析构；默认定义不需要额外的源文件。
        virtual ~Queue() noexcept = default;

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;
        Queue(Queue&&) = delete;
        Queue& operator=(Queue&&) = delete;

        // 查询该队列承诺支持的命令用途；不访问 GPU，也不改变对象。
        [[nodiscard]]
        virtual QueueType GetType() const noexcept = 0;

        // 向 GPU 队列加入完成标记；成功只表示提交成功，不表示 GPU 已完成。
        [[nodiscard]]
        virtual RHIResult Signal(
            Fence& fence,
            std::uint64_t value) noexcept = 0;

        // 提交一条已经 Close 的命令列表；GPU 完成进度由 Fence 单独表达。
        [[nodiscard]] virtual RHIResult Execute(
            CommandList& command_list) noexcept = 0;
    protected:
        // 构造函数不能是 virtual；仅供派生类构造基类部分。
        Queue() noexcept = default;
    };
}
