#pragma once

#include <core/traits.hpp>
#include <core/assert.hpp>
#include <core/hash.hpp>
#include <core/idiom.hpp>
#include <core/ref_count_ptr.hpp>

#include <mutex>
#include <xxhash.h>

#define ENABLE_POOL_TRACE 1

namespace cannele::inline graphics::rhi
{
    template <typename T>
    concept pooled_resource = requires (T t) { t.pool_hash; t.mark_free; };

    template <pooled_resource T>
    struct ResourcePool final
    {
        // We Pinned the resource pool here because of mutex.
        CNE_PINNED(ResourcePool);

        enum struct PoolState: uint8_t
        {
            usable,
            releasing,
        };

        struct Entry final
        {
            RefCountPtr<T> resource{};
            uint32_t frame_to_free{};
        };

        using EntryMap = std::unordered_map<size_t, std::vector<Entry>>;

        std::recursive_mutex mutex{};
        EntryMap entries_map{};
        uint32_t frame_count{};
        uint32_t max_lifetime{};
        PoolState state{};

        ResourcePool() = default;
        ResourcePool(uint32_t max_lifetime);
        virtual ~ResourcePool();

        template <typename U, typename... Args> requires (std::is_convertible_v<T*, U*> && std::is_constructible_v<T, Args...>)
        [[nodiscard]] auto create(size_t pool_hash, Args&&... args) -> RefCountPtr<U>;

        auto new_frame(uint32_t frame) -> void;

        static auto resource_delete(ResourcePool* pool, T* resource) -> void
        {
            if (pool && pool->state == PoolState::usable) {
                if (resource->mark_free) {
                    delete resource;
                } else {
                    std::lock_guard<std::recursive_mutex> lock(pool->mutex);

                    pool->entries_map[resource->pool_hash].emplace_back(
                        RefCountPtr<T>{resource, [pool](T* resource) {
                            resource_delete(pool, resource);
                        }},
                        pool->frame_count + pool->max_lifetime
                    );
                }
            } else {
                // Pool is releasing.
                delete resource;
            }
        }
    };
}

namespace cannele::inline graphics::rhi
{
    template <pooled_resource T>
    ResourcePool<T>::ResourcePool(uint32_t max_lifetime)
        : state(PoolState::usable), max_lifetime(max_lifetime)
    {

    }

    template <pooled_resource T>
    ResourcePool<T>::~ResourcePool()
    {
        state = PoolState::releasing;

        entries_map.clear();
    }

    template <pooled_resource T>
    template <typename U, typename... Args> requires (std::is_convertible_v<T*, U*> && std::is_constructible_v<T, Args...>)
    auto ResourcePool<T>::create(size_t pool_hash, Args&&... args) -> RefCountPtr<U>
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);

        auto free_entries = &entries_map[pool_hash];
        auto resource = RefCountPtr<T>{};

        if (free_entries->empty()) {
            resource =  RefCountPtr<T>(new T{std::forward<Args>(args)...}, [this](T* resource) {
                resource_delete(this, resource);
            });
            resource->pool_hash = pool_hash;
        } else {
            std::swap(free_entries->front(), free_entries->back()); // Always use the oldest one.

            resource = std::move(free_entries->back().resource);
            free_entries->pop_back();
        }

        return resource;
    }

    template <pooled_resource T>
    auto ResourcePool<T>::new_frame(uint32_t frame) -> void
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);

        frame_count = frame;

        for (auto& [_, entries]: entries_map) {
            // Remove empty keys.
            if (entries.empty()) continue;

            entries.erase(
                std::remove_if(entries.begin(),entries.end(), [this](auto& entry) {
                    if (frame_count > entry.frame_to_free) {
                        CNE_WARN("current frame: {}, entry frame to free: {}", frame_count, entry.frame_to_free);
                    }

                    if (frame_count >= entry.frame_to_free) {
                        entry.resource->mark_free = true;
                        return true;
                    }

                    return false;
                }),
                entries.end()
            );
        }
    }
}
