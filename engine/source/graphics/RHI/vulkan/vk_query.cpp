#include "vk_RHI.hpp"
#include "vk_tool.hpp"

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::create_timer_query() -> TimerQueryHandle
    {
        auto query = time_query_pool->allocate();
        CNE_ASSERT_WITH(query, "Time query pool space is not enough.");

        return query;
    }

    VulkanTimerQueryPool::VulkanTimerQueryPool(VulkanDevice* device)
        : VulkanDeviceChild<VulkanTimerQueryPool>(device)
        , allocated(device->device_info.max_time_queries)
    {
        auto query_pool_ci = VkQueryPoolCreateInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query_pool_ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query_pool_ci.queryCount = parent->device_info.max_time_queries * 2;

        auto result = vkCreateQueryPool(parent->device, &query_pool_ci, parent->allocation_callbacks, &query_pool);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create query pool: {}", vk_error_to_string(result)));
    }

    VulkanTimerQueryPool::~VulkanTimerQueryPool()
    {
        vkDestroyQueryPool(parent->device, query_pool, parent->allocation_callbacks);
    }

    auto VulkanTimerQueryPool::allocate() -> RefCountPtr<VulkanTimerQuery>
    {
        std::lock_guard lock(mutex);

        auto capacity = this->capacity();

        for (auto i = 0zu; i < capacity; i++) {
            auto index = (next_available_index + i) % capacity;
            if (!allocated[index]) {
                allocated[index] = true;
                next_available_index = (index + 1) % capacity;
                return std::make_shared<VulkanTimerQuery>(this, index * 2, index * 2 + 1);
            }
        }

        return {};
    }

    auto VulkanTimerQueryPool::release(int index) -> void
    {
        std::lock_guard lock(mutex);
        allocated[index] = false;
        next_available_index = std::min(next_available_index, index);
    }

    auto VulkanTimerQueryPool::reset_query(int begin_index, int count) -> void
    {
        vkResetQueryPool(parent->device, query_pool, begin_index, count);
    }

    VulkanTimerQuery::VulkanTimerQuery(VulkanTimerQueryPool* in_pool, int in_begin_index, int in_end_index)
        : pool(in_pool)
        , begin_index(in_begin_index)
        , end_index(in_end_index)
    {}

    VulkanTimerQuery::~VulkanTimerQuery()
    {
        pool->release(begin_index / 2);
    }
}
