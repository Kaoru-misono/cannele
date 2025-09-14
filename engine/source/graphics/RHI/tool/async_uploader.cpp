#include "async_uploader.hpp"
#include "../device.hpp"

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
    }
    AsyncUploader::AsyncUploader(IDevice* device)
        : device(device)
        , dispatched_submition(std::make_unique<TaskSet>())
    {
        task_scheduler = try_task_scheduler();
    }

    AsyncUploader::~AsyncUploader()
    {
        flush();
    }

    auto AsyncUploader::execute_tasks() -> void
    {
        auto need_submit = false;

        auto transfer_encoder = device->create_command_encoder(EQueueType::transfer);

        while (true) {
            auto pending_task = std::shared_ptr<AsyncUploadTask>{};
            if (task_queue.dequeue(pending_task)) {
                pending_task->task(transfer_encoder.get());
                need_submit = true;

                executing_task.push(pending_task);;

                continue;
            }
            break;
        }

        last_used_command_buffer = transfer_encoder->finish();
        device->submit_command_buffer(EQueueType::transfer, last_used_command_buffer);
    }

    auto AsyncUploader::dispatch_submition(bool force) -> void
    {
        auto can_dispatch = force ? true : (!dispatched_submition || dispatched_submition->GetIsComplete());
        if (!can_dispatch) return;

        task_scheduler->WaitforTask(dispatched_submition.get());
        dispatched_submition->m_Function = [this](TaskSetPartition range, uint32_t threadnum) {
            device->wait_for_queue(EQueueType::transfer);

            while (!executing_task.empty()) {
                auto task = executing_task.front();
                executing_task.pop();

                task->finish();
            }

            execute_tasks();

            if (!task_queue.empty() || !executing_task.empty()) {
                dispatch_triggle_tasks.enqueue([this]() { dispatch_submition(true); });
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

        while (!dispatch_triggle_tasks.empty()) {
            auto triggle_task = CallbackFunction{};
            if (dispatch_triggle_tasks.dequeue(triggle_task)) {
                triggle_task();
            }
        }
        task_scheduler->WaitforTask(dispatched_submition.get());
        device->wait_for_queue(EQueueType::transfer);
    }

    auto AsyncUploader::wait_task_complete() -> void
    {
        // TODO: If wait, not permition new task execute until wait finish.
        task_scheduler->WaitforTask(dispatched_submition.get());
        device->wait_for_queue(EQueueType::transfer);

        while (!executing_task.empty()) {
            auto task = executing_task.front();
            executing_task.pop();

            task->finish();
        }
    }
}
