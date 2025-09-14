#include "vk_RHI.hpp"
#include "vk_tool.hpp"

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::submit_command_lists(std::span<CommandListHandle> lists, EQueueType type) -> uint64_t
    {
        auto queue = this->queue(type);
        auto vulkan_list = std::vector<VulkanCommandList*>{};
        vulkan_list.reserve(lists.size());
        std::ranges::transform(
            lists,
            std::back_inserter(vulkan_list),
            [](auto const& list) -> VulkanCommandList* {
                return assert_ref_count_cast<VulkanCommandList>(list).get();
            }
        );
        return queue->submit(vulkan_list);
    }

    auto VulkanDevice::current_timeline_value(EQueueType type) -> uint64_t
    {
        auto queue = this->queue(type);

        auto time = k_invalid_time;
        auto result = vkGetSemaphoreCounterValue(device, queue->timeline, &time);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to get timeline value: {}", vk_error_to_string(result)));

        return time;
    }

    auto VulkanDevice::wait_for_submission(EQueueType type, uint64_t submission_time) -> void
    {
        auto queue = this->queue(type);

        queue->wait_command_list(submission_time, std::numeric_limits<uint64_t>::max());
    }

    VulkanQueue::VulkanQueue(VulkanDevice* device, EQueueType type, uint32_t family_index, VkQueue queue)
        : parent(device)
        , type(type)
        , family_index(family_index)
        , queue(queue)
        , buffer_block(std::make_unique<BufferBlockPool>(device, device->device_info.upload_block_size, 0))
    {
        auto type_ci = VkSemaphoreTypeCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type_ci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        type_ci.initialValue  = 0;

        auto semaphore_ci = VkSemaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphore_ci.pNext = &type_ci;

        auto result = vkCreateSemaphore(parent->device, &semaphore_ci, parent->allocation_callbacks, &timeline);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create semaphore: {}", vk_error_to_string(result)));
    }

    VulkanQueue::~VulkanQueue()
    {
        vkDestroySemaphore(parent->device, timeline, parent->allocation_callbacks);

        command_buffers_free.clear();
        command_buffers_in_flight.clear();
    }

    auto VulkanQueue::allocate_command_buffer() -> VulkanCommandBufferPtr
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto command_buffer = VulkanCommandBufferPtr{};
        if (command_buffers_free.empty()) {
            command_buffer = std::make_shared<VulkanCommandBuffer>(parent, family_index);
        } else {
            command_buffer = std::move(command_buffers_free.front());
            command_buffers_free.pop_front();
        }

        command_buffer->recording_time = ++last_recording_time;

        return command_buffer;
    }

    auto VulkanQueue::add_wait_semaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 opt_stage) -> void
    {
        if (semaphore == VK_NULL_HANDLE) return;

        wait_semaphores.emplace_back(semaphore, value, opt_stage);
    }

    auto VulkanQueue::add_signal_semaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 opt_stage) -> void
    {
        if (semaphore == VK_NULL_HANDLE) return;

        signal_semaphores.emplace_back(semaphore, value, opt_stage);
    }

    auto VulkanQueue::submit(std::span<VulkanCommandList*> command_lists) -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto command_buffers_submit_info = std::vector<VkCommandBufferSubmitInfo>{};
        auto wait_semaphores_submit_info = std::vector<VkSemaphoreSubmitInfo>{};
        auto signal_semaphores_submit_info = std::vector<VkSemaphoreSubmitInfo>{};

        auto submission_time = ++last_submitted_time;

        command_buffers_submit_info.reserve(command_lists.size());
        std::ranges::transform(
            command_lists,
            std::back_inserter(command_buffers_submit_info),
            [&](VulkanCommandList* vulkan_command_list) -> VkCommandBufferSubmitInfo {
                auto vulkan_command_buffer = vulkan_command_list->active_command_buffer;

                return VkCommandBufferSubmitInfo{
                    .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = vulkan_command_buffer->command_buffer
                };
            }
        );

        wait_semaphores_submit_info.reserve(wait_semaphores.size());
        std::ranges::transform(
            wait_semaphores,
            std::back_inserter(wait_semaphores_submit_info),
            [](auto const& info) -> VkSemaphoreSubmitInfo {
                return VkSemaphoreSubmitInfo{
                    .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = info.semaphore,
                    .value     = info.value,
                    .stageMask = info.stage_mask
                };
            }
        );

        signal_semaphores_submit_info.reserve(signal_semaphores.size() + 1);
        std::ranges::transform(
            signal_semaphores,
            std::back_inserter(signal_semaphores_submit_info),
            [](auto const& info) -> VkSemaphoreSubmitInfo {
                return VkSemaphoreSubmitInfo{
                    .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = info.semaphore,
                    .value     = info.value,
                    .stageMask = info.stage_mask
                };
            }
        );
        signal_semaphores_submit_info.emplace_back(VkSemaphoreSubmitInfo{
            VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            nullptr,
            timeline,
            submission_time,
        });

        auto submit_info2 = VkSubmitInfo2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit_info2.commandBufferInfoCount   = (uint32_t) command_buffers_submit_info.size();
        submit_info2.pCommandBufferInfos      = command_buffers_submit_info.data();
        submit_info2.waitSemaphoreInfoCount   = (uint32_t) wait_semaphores_submit_info.size();
        submit_info2.pWaitSemaphoreInfos      = wait_semaphores_submit_info.data();
        submit_info2.signalSemaphoreInfoCount = (uint32_t) signal_semaphores_submit_info.size();
        submit_info2.pSignalSemaphoreInfos    = signal_semaphores_submit_info.data();

        auto result = vkQueueSubmit2(queue, 1, &submit_info2, VK_NULL_HANDLE);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to submit command buffers: {}", vk_error_to_string(result)));

        wait_semaphores.clear();
        signal_semaphores.clear();

        // Tell the command lists that they have been submitted.
        for (auto& vulkan_command_list: command_lists) {
            vulkan_command_list->active_command_buffer->submission_time = submission_time;
            command_buffers_in_flight.emplace_back(vulkan_command_list->active_command_buffer);
            vulkan_command_list->finish_submission(this, submission_time);
        }

        return submission_time;
    }

    auto VulkanQueue::refresh_command_buffers() -> void
    {
        auto submissions = std::move(command_buffers_in_flight);

        auto last_completion_time = update_last_completion_time();

        for (auto& cmd: submissions) {
            if (cmd->submission_time <= last_completion_time) {
                cmd->clear_references();
                cmd->submission_time = 0;
                command_buffers_free.emplace_back(std::move(cmd));
            } else {
                command_buffers_in_flight.emplace_back(std::move(cmd));
            }
        }
    }

    auto VulkanQueue::command_buffer_in_flight(uint64_t submission_time) -> VulkanCommandBufferPtr
    {
        for (auto& command_buffer: command_buffers_in_flight) {
            if (command_buffer->submission_time == submission_time) {
                return command_buffer;
            }
        }

        return nullptr;
    }

    auto VulkanQueue::update_last_completion_time() -> uint64_t
    {
        vkGetSemaphoreCounterValue(parent->device, timeline, &last_completion_time);

        return last_completion_time;
    }

    auto VulkanQueue::poll_command_list(uint64_t submission_time) -> bool
    {
        if (submission_time > last_submitted_time || submission_time == 0) return false;

        if (last_completion_time >= submission_time) return true;

        return update_last_completion_time() >= submission_time;
    }

    auto VulkanQueue::wait_command_list(uint64_t submission_time, uint64_t timeout) -> bool
    {
        if (submission_time > last_submitted_time || submission_time == 0) return false;
        if (poll_command_list(submission_time)) return true;

        auto semaphore_wait_info = VkSemaphoreWaitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        semaphore_wait_info.semaphoreCount = 1;
        semaphore_wait_info.pSemaphores    = &timeline;
        semaphore_wait_info.pValues        = &submission_time;

        auto result = vkWaitSemaphores(parent->device, &semaphore_wait_info, timeout);
        if (result == VK_TIMEOUT) {
            CNE_ERROR("Failed to wait timeline: {}", vk_error_to_string(result));
        }

        return result == VK_SUCCESS;
    }
}
