#include "buffer_block.hpp"
#include "version.hpp"
#include "../RHI.hpp"

#include <core/aligned.hpp>

namespace cannele::inline  graphics::rhi
{
    BufferBlockPool::BufferBlockPool(IDevice* device, size_t size_per_block, size_t capacity)
        : device(device)
        , block_size(size_per_block)
        , capacity_size(capacity)
    {}

    BufferBlockPool::~BufferBlockPool()
    {

    }

    auto BufferBlockPool::create_block(size_t size) -> RefCountPtr<BufferBlock>
    {
        auto block = std::make_shared<BufferBlock>();

        auto info = BufferCreateInfo{
            .size_bytes = size,
            .type       = EBufferType::cpu_write,
            .usage      = EBufferUsage::transfer_src | EBufferUsage::transfer_dst,
        };
        block->buffer = device->create_buffer("Upload BufferBlock", &info);
        block->size_bytes = info.size_bytes;

        return block;
    }

    auto BufferBlockPool::suballocate_buffer(size_t size, uint64_t version, uint32_t alignment) -> BufferSubBlock
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto pending_to_release = RefCountPtr<BufferBlock>{};

        if (working_block) {
            auto aligned_offset = aligned_size(working_block->used_bytes, (size_t) alignment);
            auto end_of_data = aligned_offset + size;

            if (end_of_data <= working_block->size_bytes) {
                working_block->used_bytes = end_of_data;

                return {
                    .buffer = working_block->buffer,
                    .range = {aligned_offset, size},
                };
            }

            // Should create a new block;
            blocks.emplace(std::move(working_block));
        }

        auto queue = queue_type(version);
        auto current_time = device->current_timeline_value(queue);

        for (auto it = blocks.begin(); it != blocks.end(); it++) {
            auto block = *it;

            if (submitted(block->version) && time_point(block->version) <= current_time) {
                block->version = 0;
            }

            if (block->version == 0 && block->size_bytes >= size) {
                working_block = block;
                blocks.erase(it);
                break;
            }
        }

        if (!working_block) {
            auto allocate_size = aligned_size(std::max(size, block_size), k_page_size);

            if (capacity_size > 0 && (allocated_size + allocate_size) > capacity_size) {
                return {};
            }

            working_block = create_block(allocate_size);
            allocate_size = working_block->size_bytes;
        }

        working_block->version = version;
        working_block->used_bytes = size;

        return {
            .buffer = working_block->buffer,
            .range = {0, size},
        };
    }

    auto BufferBlockPool::update_block_version(uint64_t current_version, uint64_t new_version) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (working_block) {
            blocks.emplace(std::move(working_block));
        }

        for (auto& block: blocks) {
            if (block->version == current_version) {
                block->version = new_version;
            }
        }
    }
}
