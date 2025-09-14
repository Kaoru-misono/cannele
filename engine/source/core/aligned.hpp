#pragma once

#include <bit>

namespace cannele
{
    template <typename T>
    auto align_size(T size, T alignment) -> T
    {
        if (std::has_single_bit(alignment)) {
            return (size + alignment - 1) & ~(alignment - 1);
        } else {
            return (size + alignment - 1) / alignment * alignment;
        }
    }
}
