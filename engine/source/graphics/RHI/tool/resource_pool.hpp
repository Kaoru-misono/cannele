#pragma once

#include "../resource.hpp"

#include <core/traits.hpp>
#include <core/assert.hpp>
#include <core/hash.hpp>
#include <core/idiom.hpp>
#include <core/ref_count_ptr.hpp>

#include <mutex>
#include <xxhash.h>

#define ENABLE_POOL_TRACE 0

namespace cannele::inline graphics::rhi
{
    template <typename T>
    concept pooled_resource = std::is_base_of_v<IPoolableResource, T>;

    template <pooled_resource T>
    struct ResourcePool final: ResourcePoolBase
    {
        // We Pinned the resource pool here because of mutex.
        CNE_PINNED(ResourcePool);

        struct Entry final
        {
            std::shared_ptr<T> resource{};
            uint32_t frame_to_free{};
        };

        using EntryMap = std::unordered_map<size_t, std::vector<Entry>>;

        std::recursive_mutex mutex{};
        EntryMap entries_map{};
        uint32_t frame_count{};
        uint32_t max_lifetime{};

        ResourcePool() = default;
        ResourcePool(uint32_t max_lifetime);
        virtual ~ResourcePool();

        template <typename U, typename... Args> requires (std::is_convertible_v<T*, U*> && std::is_constructible_v<T, Args...>)
        [[nodiscard]] auto create(std::string_view name, size_t pool_hash, Args&&... args) -> std::shared_ptr<U>;

        auto recycle_resource(IPoolableResource* resource) -> void override
        {
            std::lock_guard<std::recursive_mutex> lock(mutex);

            auto new_resource = (T*) resource;
            auto& entry = entries_map[resource->pool_hash].emplace_back(
                std::shared_ptr<T>{new_resource, [](T* resource) { resource->delete_this(); }},
                frame_count + max_lifetime
            );

            // CNE_TRACE("Recycle resource: {} {} at frame {}", resource->name, (void*) resource, frame_count);
        }

        auto new_frame(uint32_t frame) -> void;
    };
}

namespace cannele::inline graphics::rhi
{
    template <pooled_resource T>
    ResourcePool<T>::ResourcePool(uint32_t max_lifetime)
        : max_lifetime(max_lifetime)
    {}

    template <pooled_resource T>
    ResourcePool<T>::~ResourcePool()
    {
        state = PoolState::releasing;

        entries_map.clear();
    }

    template <pooled_resource T>
    template <typename U, typename... Args> requires (std::is_convertible_v<T*, U*> && std::is_constructible_v<T, Args...>)
    auto ResourcePool<T>::create(std::string_view name, size_t pool_hash, Args&&... args) -> std::shared_ptr<U>
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);

        auto free_entries = &entries_map[pool_hash];
        auto resource = std::shared_ptr<T>{};

        if (free_entries->empty()) {
            resource =  std::shared_ptr<T>{new T{std::forward<Args>(args)...}, [](T* resource) { resource->delete_this(); }};
            // CNE_TRACE("Create new resource: {}  {} at frame {}", name, (void*) resource.get(), frame_count);
            resource->pool_hash = pool_hash;
            resource->pool = this->weak_from_this();
        } else {
            std::swap(free_entries->front(), free_entries->back()); // Always use the oldest one.

            resource = std::move(free_entries->back().resource);

            // CNE_TRACE("Reuse resource: {}  {} at frame {}", name, (void*) resource.get(), frame_count);
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

                    // TODO: frame to free is not accurate to release the resource.
                    // Think about this, free is at the beginning of the frame, and this frame maybe reuse this resource.
                    // So, temporary use ">" to release the resource.
                    if (frame_count > entry.frame_to_free) {
                        // entry.resource->mark_free = true;
                        // return true;
                    }

                    return false;
                }),
                entries.end()
            );
        }
    }
}
