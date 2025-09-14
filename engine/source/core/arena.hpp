#pragma once

#include <cstdint>
#include <type_traits>
#include <span>

namespace cannele::inline core
{
    struct Arena
    {
    private:

        struct Page
        {
            Page* next;
            size_t size;
            uintptr_t begin;
            uintptr_t end;
        };
        static_assert(sizeof(Page) == 32);

        size_t page_size{};
        Page* pages{};
        Page* page{};
        uintptr_t pos{};

        auto allocate_page(size_t size) -> Page*;
        auto free_pages() -> void;

    public:

        // Default page size is 1MB.
        static constexpr size_t k_default_page_size = 1024 * 1024;

        Arena(size_t page_size = k_default_page_size);

        ~Arena();

        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;

        Arena(Arena&&) = delete;
        Arena& operator=(Arena&&) = delete;

        // Allocate memory of the given size with the given alignment.
        // Alignment must be a power of 2.
        auto allocate(size_t size, size_t alignment = 16) -> void*;

        // Allocate memory for the given number of elements of type T.
        template<typename T>
        auto allocate() -> T*;

        template <typename T>
        auto allocate_array(size_t count) -> std::span<T>;

        // Reset the allocator.
        auto reset() -> void;
    };
}

namespace cannele::inline core
{
    template<typename T>
    auto Arena::allocate() -> T*
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard_layout");

        return reinterpret_cast<T*>(allocate(1 * sizeof(T), alignof(T)));
    }

    template <typename T>
    auto Arena::allocate_array(size_t count) -> std::span<T>
    {
        static_assert(std::is_standard_layout_v<T>, "T must be standard_layout");

        return std::span{reinterpret_cast<T*>(allocate(count * sizeof(T), alignof(T))), count};
    }
}
