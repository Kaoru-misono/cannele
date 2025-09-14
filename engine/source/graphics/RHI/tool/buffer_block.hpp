#pragma once

#include "../resource.hpp"

#include <mutex>

namespace cannele::inline graphics::rhi
{
    struct BufferBlock final
    {
        BufferHandle buffer{};
        uint64_t version{};
        size_t size_bytes{};
        size_t used_bytes{};

        void* mapped_memory{};
    };

    struct BufferSubBlock final
    {
        BufferHandle buffer{};
        BufferRange range{};

        void* cpu_data{};
    };

    struct BufferBlockPool
    {
        static constexpr auto k_page_size = 4096zu;

        IDevice* device{};
        size_t block_size{};
        size_t capacity_size{};
        size_t allocated_size{};

        // Don't need to care about hash conflicts because block's lifetime is bounded to the pool.
        std::unordered_set<std::shared_ptr<BufferBlock>> blocks{};
        std::shared_ptr<BufferBlock> working_block{};
        std::mutex mutex{};

        BufferBlockPool(IDevice* device, size_t size_per_block, size_t capacity);
        ~BufferBlockPool();

        auto suballocate_buffer(size_t size, uint64_t version, uint32_t alignment = 256) -> BufferSubBlock;

        auto update_block_version(uint64_t current_version, uint64_t new_version) -> void;

    private:

        auto create_block(size_t size) -> std::shared_ptr<BufferBlock>;
    };
}
