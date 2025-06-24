#pragma once

#include "../RHI_resource.hpp"

namespace cannele::inline graphics::rhi
{
    struct BufferStateTracker
    {
        RHIBuffer* buffer{};
        EResourceStates permanent_state = EResourceStates::unknown;
    };

    struct TextureStateTracker
    {
        RHITexture* texture{};
        EResourceStates permanent_state = EResourceStates::unknown;
        bool state_initialized{false};
    };

    struct BufferState final
    {
        EResourceStates state = EResourceStates::unknown;
        bool enable_uav_barriers{true};
        bool first_uav_barrier_placed{false};
        bool permanent_transition{false};
    };

    struct TextureState final
    {
        std::vector<EResourceStates> subresource_states{};
        EResourceStates state = EResourceStates::unknown;
        bool enable_uav_barriers{true};
        bool first_uav_barrier_placed{false};
        bool permanent_transition{false};
    };

    struct BufferBarrier final
    {
        RHIBuffer* buffer{};

        EResourceStates src_state = EResourceStates::unknown;
        EResourceStates dst_state = EResourceStates::unknown;
    };

    struct TextureBarrier final
    {
        RHITexture* texture{};

        uint32_t mip_level{0};
        uint32_t array_layer{0};
        bool contain_all_resource{false};

        EResourceStates src_state = EResourceStates::unknown;
        EResourceStates dst_state = EResourceStates::unknown;
    };

    struct ResourceStateTracker final
    {
        std::vector<TextureBarrier> texture_barriers{};
        std::vector<BufferBarrier> buffer_barriers{};

        std::unordered_map<TextureStateTracker*, TextureState> texture_states{};
        std::unordered_map<BufferStateTracker*, BufferState> buffer_states{};

        std::vector<std::pair<TextureStateTracker*, EResourceStates>> locked_texture_states{};
        std::vector<std::pair<BufferStateTracker*, EResourceStates>> locked_buffer_states{};

        auto enable_uav_barriers(TextureStateTracker* tracker, bool enable) -> void;
        auto enable_uav_barriers(BufferStateTracker* tracker, bool enable) -> void;

        auto begin_tracking_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state) -> void;
        auto begin_tracking_buffer_state(BufferStateTracker* tracker, EResourceStates state) -> void;

        auto lock_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state) -> void;
        auto lock_buffer_state(BufferStateTracker* tracker, EResourceStates state) -> void;

        auto texture_subresource_state(TextureStateTracker* tracker, uint32_t mip_level, uint32_t array_layer) -> EResourceStates;
        auto buffer_state(BufferStateTracker* tracker) -> EResourceStates;


        // Internal use only.

        auto require_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state) -> void;
        auto require_buffer_state(BufferStateTracker* tracker, EResourceStates state) -> void;

        auto find_tracked_texture_state(TextureStateTracker* tracker, bool create_if_missing) -> TextureState*;
        auto find_tracked_buffer_state(BufferStateTracker* tracker, bool create_if_missing) -> BufferState*;

        auto keep_initial_state() -> void;

        auto clear_barriers() -> void;

        auto finish_tracking() -> void;

        auto has_barrier() -> bool { return !buffer_barriers.empty() && !texture_barriers.empty(); }
    };
}
