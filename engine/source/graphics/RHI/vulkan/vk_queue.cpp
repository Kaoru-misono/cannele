#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <ranges>

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::submit_command_buffers(SubmitInfo* info) -> void
    {
        // TODO: Check the same queue family.
        queue(info->queue_type)->submit(info);
    }

    auto VulkanDevice::current_timeline_value(EQueueType type) -> uint64_t
    {
        auto queue = this->queue(type);

        auto time = k_invalid_time;
        CHECK_VK_RESULT(vkGetSemaphoreCounterValue(device, queue->timeline, &time));

        return time;
    }

    auto VulkanDevice::wait_for_queue(EQueueType type) -> void
    {
        queue(type)->wait();
    }

    VulkanQueue::VulkanQueue(VulkanDevice* device, EQueueType type, uint32_t family_index, VkQueue queue)
        : parent(device)
        , type(type)
        , family_index(family_index)
        , queue(queue)
    {
        auto type_ci = VkSemaphoreTypeCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        type_ci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        type_ci.initialValue  = 0;

        auto semaphore_ci = VkSemaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphore_ci.pNext = &type_ci;

        CHECK_VK_RESULT(vkCreateSemaphore(parent->device, &semaphore_ci, parent->allocation_callbacks, &timeline));
    }

    VulkanQueue::~VulkanQueue()
    {
        vkDestroySemaphore(parent->device, timeline, parent->allocation_callbacks);

        command_buffers_free.clear();
        command_buffers_in_flight.clear();
    }

    auto VulkanQueue::allocate_command_buffer() -> std::shared_ptr<VulkanCommandBuffer>
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto command_buffer = std::shared_ptr<VulkanCommandBuffer>{};
        if (command_buffers_free.empty()) {
            command_buffer = std::make_shared<VulkanCommandBuffer>(parent, this);
        } else {
            command_buffer = std::move(command_buffers_free.front());
            command_buffers_free.pop_front();
        }

        return command_buffer;
    }

    auto VulkanQueue::free_command_buffer(std::shared_ptr<VulkanCommandBuffer> command_buffer) -> void
    {
        command_buffer->reset();

        std::lock_guard<std::mutex> lock(mutex);

        command_buffers_free.push_back(std::move(command_buffer));
    }

    auto VulkanQueue::free_command_buffers() -> void
    {
        auto submissions = std::move(command_buffers_in_flight);

        auto last_completion_time = update_last_completion_time();

        for (auto& command_buffer: submissions) {
            if (command_buffer->submission_time <= last_completion_time) {
                free_command_buffer(command_buffer);
            } else {
                command_buffers_in_flight.emplace_back(std::move(command_buffer));
            }
        }

        // TODO: Flush heap.
    }

    auto VulkanQueue::create_command_encoder() -> std::shared_ptr<VulkanCommandEncoder>
    {
        auto encoder = std::make_shared<VulkanCommandEncoder>(parent, this);

        return encoder;
    }

    auto VulkanQueue::submit(SubmitInfo* info) -> uint64_t
    {
        auto submission_time = ++last_submitted_time;

        auto command_buffers_submit_info = (
            info->command_buffers |
            std::views::transform([](auto const& command_buffer) -> VkCommandBufferSubmitInfo {
                auto vulkan_command_buffer = pointer_cast<VulkanCommandBuffer>(command_buffer);
                return VkCommandBufferSubmitInfo{
                    .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = vulkan_command_buffer->command_buffer,
                };
            }) |
            std::ranges::to<std::vector>()
        );

        // TODO: External semaphores.
        auto wait_semaphores_submit_info = std::vector<VkSemaphoreSubmitInfo>{};
        auto signal_semaphores_submit_info = std::vector<VkSemaphoreSubmitInfo>{};
        if (surface_sync) {
            wait_semaphores_submit_info.emplace_back(VkSemaphoreSubmitInfo{
                .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = surface_sync.image_available_semaphore,
                .value     = 0,
                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
            });

            signal_semaphores_submit_info.emplace_back(VkSemaphoreSubmitInfo{
                .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = surface_sync.render_finished_semaphore,
                .value     = 0,
                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
            });
        }

        signal_semaphores_submit_info.emplace_back(VkSemaphoreSubmitInfo{
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = timeline,
            .value     = submission_time,
            .stageMask = VK_PIPELINE_STAGE_2_NONE
        });

        auto submit_info2 = VkSubmitInfo2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit_info2.commandBufferInfoCount   = command_buffers_submit_info.size();
        submit_info2.pCommandBufferInfos      = command_buffers_submit_info.data();
        submit_info2.waitSemaphoreInfoCount   = wait_semaphores_submit_info.size();
        submit_info2.pWaitSemaphoreInfos      = wait_semaphores_submit_info.data();
        submit_info2.signalSemaphoreInfoCount = signal_semaphores_submit_info.size();
        submit_info2.pSignalSemaphoreInfos    = signal_semaphores_submit_info.data();

        CHECK_VK_RESULT(vkQueueSubmit2(queue, 1, &submit_info2, surface_sync.fence));

        surface_sync.fence = VK_NULL_HANDLE;

        for (auto& command_buffer: info->command_buffers) {
            auto vulkan_command_buffer = pointer_cast<VulkanCommandBuffer>(command_buffer);
            vulkan_command_buffer->submission_time = submission_time;
            command_buffers_in_flight.emplace_back(std::move(vulkan_command_buffer));
        }
        free_command_buffers();

        return submission_time;
    }

    auto VulkanQueue::wait() -> bool
    {
        auto result = vkQueueWaitIdle(queue);

        return result == VK_SUCCESS;
    }

    auto VulkanQueue::update_last_completion_time() -> uint64_t
    {
        vkGetSemaphoreCounterValue(parent->device, timeline, &last_completion_time);

        return last_completion_time;
    }
}
