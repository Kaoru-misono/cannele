#pragma once

#include "../definitions.hpp"

namespace cannele::inline graphics::rhi
{
    constexpr auto k_version_submitted_flag = 0x8000000000000000ull;
    constexpr auto k_version_queue_shift    = 60u;
    constexpr auto k_version_queue_mask     = 0x7u;
    constexpr auto k_version_id_mask        = 0x0fffffffffffffffull;

    constexpr auto make_version(uint64_t time_point, EQueueType type, bool submitted) -> uint64_t
    {
        auto result = (time_point & k_version_id_mask) | ((uint64_t) type) << k_version_queue_shift;

        if (submitted) result |= k_version_submitted_flag;

        return result;
    }

    constexpr auto time_point(uint64_t version) -> uint64_t
    {
        return version & k_version_id_mask;
    }

    constexpr auto queue_type(uint64_t version) -> EQueueType
    {
        return (EQueueType) ((version >> k_version_queue_shift) & k_version_queue_mask);
    }

    constexpr auto submitted(uint64_t version) -> bool
    {
        return (version & k_version_submitted_flag) != 0;
    }
}
