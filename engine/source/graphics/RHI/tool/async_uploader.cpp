#include "async_uploader.hpp"
#include "../RHI.hpp"

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
    }
    AsyncUploader::AsyncUploader(IDevice* device)

        : device(device)
        , dispatched_submition(std::make_shared<TaskSet>())
    {
        auto command_list_info = CommandListCreateInfo{
            .queue_type = EQueueType::transfer,
        };
        async_transfer_command_list = device->create_command_list(&command_list_info);
        per_frame_transfer_list = device->create_command_list(&command_list_info);
    }

    AsyncUploader::~AsyncUploader()
    {
        flush();
    }

    auto AsyncUploader::execute_tasks() -> void
    {
        auto need_submit = false;

        async_transfer_command_list->start();

        while (true) {
            auto pending_task = RefCountPtr<AsyncUploadTask>{};
            if (task_queue.dequeue(pending_task)) {
                pending_task->task(async_transfer_command_list.get());
                need_submit = true;

                executing_task.push(pending_task);;

                continue;
            }
            break;
        }

        async_transfer_command_list->finish();

        if (need_submit) {
            last_submit_time = device->submit_command_lists({&async_transfer_command_list, 1}, EQueueType::transfer);
        }
    }
    auto AsyncUploader::dispatch_submition(bool force) -> void

    {
        auto can_dispatch = force ? true : (!dispatched_submition || dispatched_submition->GetIsComplete());
        if (!can_dispatch) return;

        auto task_scheduler = try_task_scheduler();
        task_scheduler->WaitforTask(dispatched_submition.get());
        dispatched_submition->m_Function = [this, task_scheduler](TaskSetPartition range, uint32_t threadnum) {
            device->wait_for_submission(EQueueType::transfer, last_submit_time);

            while (!executing_task.empty()) {
                auto task = executing_task.front();
                executing_task.pop();

                task->finish();
            }

            execute_tasks();

            if (!task_queue.empty() || !executing_task.empty()) {
                dispatch_triggle_tasks.enqueue([this, task_scheduler]() { dispatch_submition(true); });
            }
        };
        task_scheduler->AddTaskSetToPipe(dispatched_submition.get());
    }

    auto AsyncUploader::add_task(AsyncUploadTask::TaskFunction&& task, AsyncUploadTask::FinishFunction&& finish) -> void
    {
        auto upload_task = std::make_shared<AsyncUploadTask>(std::move(task), std::move(finish));

        task_queue.enqueue(std::move(upload_task));

        dispatch_submition(false);
    }

    auto AsyncUploader::all_tasks_finished() -> bool
    {
        auto time = async_transfer_command_list->device()->current_timeline_value(EQueueType::transfer);

        return ((time >= last_submit_time) && executing_task.empty());
    }

    auto AsyncUploader::update() -> void
    {
        while (!dispatch_triggle_tasks.empty()) {
            auto triggle_task = CallbackFunction{};
            if (dispatch_triggle_tasks.dequeue(triggle_task)) {
                triggle_task();
            } else {
                break;
            }
        }
    }

    auto AsyncUploader::flush() -> void
    {
        dispatch_submition();

        auto task_scheduler = try_task_scheduler();
        while (!dispatch_triggle_tasks.empty()) {
            auto triggle_task = CallbackFunction{};
            if (dispatch_triggle_tasks.dequeue(triggle_task)) {
                triggle_task();
            }
        }
        task_scheduler->WaitforTask(dispatched_submition.get());
        device->wait_for_submission(EQueueType::transfer, last_submit_time);
    }
}
