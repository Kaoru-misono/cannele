#include "task_scheduler.hpp"

namespace cannele::inline task
{
    auto try_task_scheduler() -> TaskScheduler*
    {
        static auto task_scheduler = TaskScheduler{};
        return &task_scheduler;
    }

    auto waitfor_tasks(std::vector<TaskSet*> const& task_sets) -> void
    {
        auto task_scheduler = try_task_scheduler();
        for (auto& task_set: task_sets) {
            task_scheduler->WaitforTask(task_set);
        }
    }

    auto launch_and_wait(std::span<std::unique_ptr<TaskSet>> task_sets) -> void
    {
        auto task_scheduler = try_task_scheduler();
        for (auto& task_set: task_sets) {
            task_scheduler->AddTaskSetToPipe(task_set.get());
        }
        task_scheduler->WaitforAll();
    }
}
