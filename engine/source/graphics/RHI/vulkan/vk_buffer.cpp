#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <math/tool.hpp>

#include <volk.h>
#include <vk_mem_alloc.h>

#if ENABLE_POOL_TRACE
    #define TRACE_POOLED_BUFFER(CREATE_OR_RELEASE, NAME, SIZE) \
        CNE_TRACE(CREATE_OR_RELEASE " Buffer: \"{}\" with size {} KB", NAME, SIZE / 1024);
#else
    #define TRACE_POOLED_BUFFER(CREATE_OR_RELEASE, NAME, SIZE)
#endif

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::create_buffer(std::string_view name, BufferCreateInfo* info) -> BufferHandle
    {
        auto size = info->size_bytes;
        // Round up to multiple of 256 bytes to avoid pool fragmentation.
        info->size_bytes = math::divide_rounding_up(info->size_bytes, (size_t) 256) * 256u;

        auto hash = XXH64(info, sizeof(BufferCreateInfo), 0);
        auto buffer = buffer_pool->create<VulkanBuffer>(hash, this, info);

        buffer->allocated_size_bytes = buffer->info.size_bytes;
        buffer->info.size_bytes = size;

        TRACE_POOLED_BUFFER("Create", name, info->size_bytes);

        // Set deleter for pool buffer:
        buffer->deleter = [pool = buffer_pool.get()](VulkanBuffer* resource) {
            pool->resource_delete(pool, resource);
            TRACE_POOLED_BUFFER("Release", resource->name, resource->allocated_size_bytes);
        };

        set_resource_name(device, VK_OBJECT_TYPE_BUFFER, (uint64_t) buffer->buffer, name);
        buffer->name = name;

        return buffer;
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice* device, BufferCreateInfo* in_info)
        : VulkanDeviceChild<VulkanBuffer>(device)
        , info(*in_info)
    {
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
        buffer_ci.size        = info.size_bytes;
        buffer_ci.usage       = convert_to_vk_buffer_usage(info.usage);
        buffer_ci.flags       = 0;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        auto allocation_info = VmaAllocationInfo{};
        auto result = vmaCreateBuffer(
            parent->allocator,
            &buffer_ci, &allocation_create_info,
            &buffer, &allocation, &allocation_info
        );
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create buffer: {}", vk_error_to_string(result)));

        auto device_address_info = VkBufferDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
        device_address = vkGetBufferDeviceAddress(parent->device, &device_address_info);

        tracker.buffer = this;
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (allocation) {
            vmaDestroyBuffer(parent->allocator, buffer, allocation);
        }
    }

    auto VulkanBuffer::descriptor_handle(BufferRange range, EDescriptorType type) -> math::uint2
    {
        range.adapt_to_buffer(&info);

        auto hash = (uint32_t) core::hash((uint8_t) type, range.offset_bytes, range.size_bytes);

        auto it = buffer_views.find(hash);

        if (it != buffer_views.end()) {
            return {it->second.bindless_index, 0};
        }

        auto buffer_view = VulkanBufferView{};
        buffer_view.resource_type = type;
        buffer_view.range = range;
        buffer_view.bindless_index = parent->bindless_manager->register_buffer(type, this, range.offset_bytes, range.size_bytes);

        it = buffer_views.emplace(hash, buffer_view).first;

        return {it->second.bindless_index, 0};
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
