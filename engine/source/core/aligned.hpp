#pragma once

namespace cannele
{
    template <typename T>
    auto aligned_size(T size, T alignment) -> T
    {
        return (size + alignment - 1) & ~(alignment - 1);
    }
}
