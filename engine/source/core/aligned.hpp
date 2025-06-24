#pragma once

#include <concepts>

namespace cannele
{
    template <std::unsigned_integral I>
    auto aligned_size(size_t size, I alignment) -> I
    {
        assert((alignment != 0) && ((alignment & (alignment - 1)) == 0));
        auto const alignment_mask = alignment - 1;
        return ((I) size + alignment_mask) & ~alignment_mask;
    }
}
