#pragma once

#include "traits.hpp"

namespace cannele::inline core
{
    template<hashable_type T>
    auto hash(T const& value) -> size_t
    {
        // If We defined a hash function, use it.
        if constexpr (requires { value.hash(); }) {
            return value.hash();
        }

        return std::hash<T>{}(value);
    }

    template <hashable_type T>
    inline auto hash_combine(size_t seed, T const& value) -> size_t
    {
        auto hashed_value = hash(value);
        return hashed_value ^ (seed + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }

    template <hashable_type T, hashable_type... Args>
    auto hash(T const& first, Args&&... other) -> size_t
    {
        auto result = hash(other...);
        return hash_combine(result, first);
    }
}
