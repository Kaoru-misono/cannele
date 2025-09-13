#pragma once

#include <cstdint>

namespace cannele::inline graphics::renderer
{
    static constexpr auto k_max_shading_types = 0x7f;

    enum class EShadingType: uint8_t
    {
        none,
        pbr,
        last,
    };
}
