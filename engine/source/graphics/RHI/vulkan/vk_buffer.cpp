#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <math/tool.hpp>

#include <volk.h>
#include <vk_mem_alloc.h>

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::create_buffer(std::string_view name, BufferCreateInfo* info) -> BufferHandle
    {
        auto hash = XXH64(info, sizeof(BufferCreateInfo), 0);
        auto buffer = buffer_pool->create<VulkanBuffer>(hash, this, info);

        // Set deleter for pool buffer:
        buffer->deleter = [pool = buffer_pool.get()] (VulkanBuffer* resource) {
            pool->resource_delete(pool, resource);
        };

        set_resource_name(device, VK_OBJECT_TYPE_BUFFER, (uint64_t) buffer->buffer, buffer->name);
        buffer->name = name;

        return buffer;
    }

    auto VulkanDevice::create_staging_buffer(size_t size) -> RefCountPtr<VulkanBuffer>
    {
        auto info = BufferCreateInfo{
            .size_bytes = math::divide_rounding_up(size, (size_t) 256) * 256,
            .type = EBufferType::cpu_write, // Staging buffer is always cpu write.
            .usage = EBufferUsage::transfer_src | EBufferUsage::transfer_dst,
        };

        auto hash = XXH64(&info, sizeof(BufferCreateInfo), 0);
        auto buffer = buffer_pool->create<VulkanBuffer>(hash, this, &info);

        // Set deleter for pool buffer:
        buffer->deleter = [pool = buffer_pool.get()] (VulkanBuffer* resource) {
            CNE_TRACE("Place buffer {} into pool", resource->name);
            pool->resource_delete(pool, resource);
        };

        set_resource_name(device, VK_OBJECT_TYPE_BUFFER, (uint64_t) buffer->buffer, buffer->name);
        buffer->name = "Staging Buffer";

        return buffer;
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice* device, BufferCreateInfo* in_info)
        : VulkanDeviceChild<VulkanBuffer>(device)
        , info(std::move(*in_info))
    {
        auto vk_device = device->device;
        auto allocator = device->allocator;

        auto allocation_create_info = VmaAllocationCreateInfo{};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
        switch (info.type) {
            case EBufferType::gpu_only: break;
            case EBufferType::cpu_read: allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT; break;
            case EBufferType::cpu_write: allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT; break;
            default: CNE_UNREACHABLE();
        }

        // TODO: Pass flags.
        auto buffer_ci = VkBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_ci.size        = math::divide_rounding_up(info.size_bytes, (size_t) 256) * 256u;
        buffer_ci.usage       = convert_to_vk_buffer_usage(info.usage);
        buffer_ci.flags       = 0;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        auto allocation_info = VmaAllocationInfo{};
        auto result = vmaCreateBuffer(
            allocator,
            &buffer_ci, &allocation_create_info,
            &buffer, &allocation, &allocation_info
        );
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create buffer: {}", vk_error_to_string(result)));

        auto device_address_info = VkBufferDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
        auto device_address = vkGetBufferDeviceAddress(vk_device, &device_address_info);

        tracker.buffer = this;
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (allocation) {
            vmaDestroyBuffer(parent->allocator, buffer, allocation);
        }
    }

    auto VulkanBuffer::map() -> void*
    {
        auto ptr = (void*) nullptr;
        auto result_map = vmaMapMemory(parent->allocator, allocation, &ptr);
        CNE_ASSERT_WITH(result_map == VK_SUCCESS, std::format("Failed to map memory: {}", vk_error_to_string(result_map)));

        return ptr;
    }

    auto VulkanBuffer::unmap() -> void
    {
        vmaUnmapMemory(parent->allocator, allocation);
    }
}
