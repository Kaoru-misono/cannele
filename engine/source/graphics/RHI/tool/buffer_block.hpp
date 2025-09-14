#pragma once

#include "../resource.hpp"

#include <mutex>
#include <expected>

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

    // TODO: make this device child.
    struct BufferBlockPool
    {
        using PageID = int;
        using ThreadID = std::thread::id;

        struct Block final
        {
            size_t offset{};
            size_t size{};
            bool free{};
        };

        using BlockIterator = std::list<Block>::iterator;

        struct Page final
        {
            PageID id{};
            BufferHandle buffer{};
            std::list<Block> blocks{};
            size_t capacity{};
            size_t used{};
            std::byte* mapped_pointer{};
            ThreadID thread_id{};

            Page(PageID id, BufferHandle buffer);

            auto allocate_block(size_t size, ThreadID lock_to_thread) -> std::expected<BlockIterator, std::string>;

            auto free_block(BlockIterator block) -> void;

            auto map(IDevice* device) -> bool;
            auto unmap(IDevice* device) -> bool;
        };

        struct BufferBlock final
        {
            BufferBlockPool* pool{};
            Page* page{};
            BlockIterator block{};

            BufferBlock(BufferBlockPool* pool, Page* page, BlockIterator block);
            ~BufferBlock();

            inline auto offset() const -> size_t { return block->offset; }
            inline auto size() const -> size_t { return block->size; }

            inline auto buffer() -> RHIBuffer* { return page->buffer.get(); }

            template <typename T = std::byte>
            inline auto map() -> T*
            {
                return (T*) pool->map(this);
            }

            inline auto unmap() -> void { pool->unmap(this); }
        };

        IDevice* device{};
        size_t capacity{};
        size_t used{};
        PageID next_page_id{1};
        size_t alignment{1024};
        size_t page_size{16 * 1024 * 1024};
        std::unordered_map<int, std::shared_ptr<Page>> pages{};
        EMemoryType memory_type{EMemoryType::cpu_upload};
        std::mutex mutex{};

        BufferBlockPool(IDevice* device);
        BufferBlockPool(IDevice* device, size_t page_size, EMemoryType memory_type = EMemoryType::cpu_upload);
        ~BufferBlockPool();

        auto allocate_buffer_block(size_t size) -> std::shared_ptr<BufferBlock>;
        auto free_buffer_block(BufferBlock* block) -> void;

        auto map(BufferBlock* block) -> std::byte*;
        auto unmap(BufferBlock* block) -> void;

    private:

        auto create_block(size_t size) -> std::shared_ptr<BufferBlock>;
        auto allocate_page(size_t size) -> std::shared_ptr<Page>;
        auto free_page(std::shared_ptr<Page> page) -> void;
    };

    using BufferBlockHandle = std::shared_ptr<BufferBlockPool::BufferBlock>;
}
