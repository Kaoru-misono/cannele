#pragma once

#include "../RHI_resource.hpp"

#include <core/mpsc_queue.hpp>
#include <core/task_scheduler.hpp>
#include <core/log_system.hpp>

#include <functional>
#include <queue>

namespace cannele::inline graphics::rhi
{
    struct IDevice;

    struct AsyncUploadTask
    {
        using TaskFunction = std::function<auto (RHICommandList* command_list) -> void>;
        using FinishFunction = std::function<auto () -> void>;

        TaskFunction task{};
        FinishFunction finish{};
    };

    struct AsyncUploader final
    {
        uint64_t last_submit_time{0};
        IDevice* device{};
        TaskScheduler* task_scheduler{};
        CommandListHandle async_transfer_command_list{};
        CommandListHandle per_frame_transfer_list{};

        using UploadTaskQueue = MPSCQueue<RefCountPtr<AsyncUploadTask>, MPSCQueueHeapAllocator<RefCountPtr<AsyncUploadTask>>>;
        UploadTaskQueue task_queue{};

        std::queue<RefCountPtr<AsyncUploadTask>> executing_task{};

        RefCountPtr<TaskSet> dispatched_submition{};

        using CallbackFunction = std::function<auto () -> void>;
        MPSCQueue<CallbackFunction, MPSCQueueHeapAllocator<CallbackFunction>> dispatch_triggle_tasks{};

        AsyncUploader(IDevice* device);
        ~AsyncUploader();

        auto execute_tasks() -> void;

        auto dispatch_submition(bool force = false) -> void;

        auto add_task(AsyncUploadTask::TaskFunction&& task, AsyncUploadTask::FinishFunction&& finish) -> void;

        auto all_tasks_finished() -> bool;

        auto update() -> void;

        auto flush() -> void;

        auto busy() -> bool { return !all_tasks_finished(); }

        auto wait_task_complete() -> void;
    };

    struct IUploadResource
    {
        CNE_INTERFACE(IUploadResource);

        std::atomic<bool> uploading{true};

        auto ready() -> bool { return !uploading; }
    };

    struct UploadTexture final: IUploadResource
    {
        TextureHandle texture_handle{};
        RHITexture* fallback_texture{};
    };
}

