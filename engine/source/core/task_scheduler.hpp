#pragma once

#include <enkiTS/TaskScheduler.h>

#include <span>

namespace cannele::inline task
{
    // TODO: Warp them.
    using TaskScheduler = enki::TaskScheduler;
    using ITaskSet = enki::ITaskSet;
    using TaskSet = enki::TaskSet;
    using IPinnedTask = enki::IPinnedTask;
    using TaskSetPartition = enki::TaskSetPartition;

    auto try_task_scheduler() -> TaskScheduler*;
    auto waitfor_tasks(std::vector<TaskSet*> const& task_sets) -> void;
    auto launch_and_wait(std::span<std::unique_ptr<TaskSet>> task_sets) -> void;
}
