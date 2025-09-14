#include "arena.hpp"
#include "assert.hpp"

namespace cannele::inline core
{
    auto Arena::allocate_page(size_t size) -> Arena::Page*
    {
        std::byte* data = reinterpret_cast<std::byte*>(std::malloc(size));
        Page* page = reinterpret_cast<Page*>(data);
        page->next = nullptr;
        page->size = size - sizeof(Page);
        page->begin = reinterpret_cast<uintptr_t>(data) + sizeof(Page);
        page->end = page->begin + page->size;
        return page;
    }

    auto Arena::free_pages() -> void
    {
        Page* page = pages;
        while (page) {
            Page* next = page->next;
            std::free(page);
            page = next;
        }
        pages = nullptr;
    }

    Arena::Arena(size_t page_size)
            : page_size(page_size)
        {}

    Arena::~Arena() { free_pages(); }

    auto Arena::allocate(size_t size, size_t alignment) -> void*
    {
        pos = (pos + alignment - 1) & ~(alignment - 1);
        if (!page || (pos + size > page->end)) {
            if (!page) {
                pages = page = allocate_page(page_size);
            } else {
                if (!page->next) {
                    page->next = allocate_page(std::max(size + alignment, page_size));
                }
                page = page->next;
            }
            pos = (page->begin + alignment - 1) & ~(alignment - 1);
        }
        void* result = (void*)pos;
        pos += size;
        CNE_ASSERT(result != nullptr);
        CNE_ASSERT(((uintptr_t)result & (alignment - 1)) == 0);

        return result;
    }

    auto Arena::reset() -> void
    {
        page = pages;
        pos = page ? page->begin : 0;
    }
}
