#include "buffer_block.hpp"
#include "version.hpp"
#include "../device.hpp"

#include <core/aligned.hpp>

namespace cannele::inline  graphics::rhi
{
    BufferBlockPool::BufferBlockPool(IDevice* device)
        : device(device)
    {}

    BufferBlockPool::BufferBlockPool(IDevice* device, size_t page_size, EMemoryType memory_type)
        : device(device)
        , page_size(page_size)
        , memory_type(memory_type)
    {}

    BufferBlockPool::~BufferBlockPool()
    {}

    auto BufferBlockPool::allocate_buffer_block(size_t size) -> std::shared_ptr<BufferBlock>
    {
        auto aligned_size = align_size(size, alignment);

        auto thread_id = std::this_thread::get_id();

        if (aligned_size < page_size) {
            for (auto& [id, page] : pages) {
                if (page->thread_id == thread_id) {
                    auto block = page->allocate_block(aligned_size, thread_id);

                    auto buffer_block = std::make_shared<BufferBlock>(this, page.get(), *block);
                    used += aligned_size;

                    return buffer_block;
                }
            }
        }

        auto new_page_size = aligned_size < page_size ? page_size : aligned_size;
        auto new_page = allocate_page(new_page_size);

        auto block = new_page->allocate_block(aligned_size, thread_id);

        auto buffer_block = std::make_shared<BufferBlock>(this, new_page.get(), *block);
        used += aligned_size;

        return buffer_block;
    }

    auto BufferBlockPool::allocate_page(size_t size) -> std::shared_ptr<Page>
    {
        auto buffer_info = BufferCreateInfo{
            .memory_type = memory_type,
            .usage = EBufferUsage::transfer_src | EBufferUsage::transfer_dst,
            .size_bytes = size,
        };
        auto buffer = device->create_buffer("BufferBlockPool::Page", &buffer_info);

        auto page = std::make_shared<Page>(next_page_id++, buffer);
        pages.emplace(page->id, page);
        capacity += size;

        return page;
    }

    auto BufferBlockPool::free_page(std::shared_ptr<Page> page) -> void
    {
        capacity -= page->capacity;

        pages.erase(page->id);
    }

    auto BufferBlockPool::free_buffer_block(BufferBlock* block) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);

        used -= block->size();

        auto page = pages[block->page->id];
        page->free_block(block->block);

        if (page->used == 0) {
            if (page->capacity == page_size) {
                auto empty_page_count = 0;
                for (auto& [id, page] : pages) {
                    if (page->used == 0) {
                        page->thread_id = std::thread::id{};
                        empty_page_count++;
                    }
                }

                if (empty_page_count > 1) {
                    free_page(page);
                }
            } else {
                free_page(page);
            }
        }
    }

    auto BufferBlockPool::map(BufferBlock* block) -> std::byte*
    {
        auto page = block->page;
        page->map(device);

        return page->mapped_pointer + block->offset();
    }

    auto BufferBlockPool::unmap(BufferBlock* block) -> void
    {
        block->page->unmap(device);
    }

    BufferBlockPool::Page::Page(PageID id, BufferHandle buffer)
        : id(id)
        , buffer(buffer)
    {
        capacity = buffer->description()->size_bytes;
        blocks.emplace_back(0, capacity, true);
    }

    auto BufferBlockPool::Page::allocate_block(size_t size, ThreadID lock_to_thread) -> std::expected<BlockIterator, std::string>
    {
        CNE_ASSERT(thread_id == std::thread::id{} || lock_to_thread == thread_id);

        for (auto block = blocks.begin(); block != blocks.end(); block++) {
            if (block->free && block->size >= size) {
                used += size;

                if (block->size > size) {
                    auto next = std::next(block);
                    blocks.insert(next, {block->offset + size, block->size - size, true});
                    block->size = size;
                }

                block->free = false;

                thread_id = lock_to_thread;

                return block;
            }
        }

        return std::unexpected<std::string>("Failed to allocate block");
    }

    auto BufferBlockPool::Page::free_block(BlockIterator block) -> void
    {
        used -= block->size;

        if (block != blocks.begin()) {
            auto previous = std::prev(block);
            if (previous->free) {
                previous->size += block->size;
                blocks.erase(block);
                block = previous;
            }
        }

        auto next = std::next(block);
        if (next != blocks.end()) {
            if (next->free) {
                block->size += next->size;
                blocks.erase(next);
            }
        }

        block->free = true;
    }

    auto BufferBlockPool::Page::map(IDevice* device) -> bool
    {
        if (mapped_pointer) return false;

        mapped_pointer = device->map_buffer(buffer);

        return true;
    }

    auto BufferBlockPool::Page::unmap(IDevice* device) -> bool
    {
        if (!mapped_pointer) return false;

        device->unmap_buffer(buffer);
        mapped_pointer = nullptr;

        return true;
    }

    BufferBlockPool::BufferBlock::BufferBlock(BufferBlockPool* pool, Page* page, BlockIterator block)
        : pool(pool)
        , page(page)
        , block(block)
    {}

    BufferBlockPool::BufferBlock::~BufferBlock()
    {
        pool->free_buffer_block(this);
    }
}
