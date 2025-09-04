#pragma once

#include "../RHI_resource.hpp"

namespace cannele::inline graphics::rhi
{
    struct BufferStateTracker
    {
        RHIBuffer* buffer{};
        EResourceStates permanent_state{EResourceStates::unknown};
    };

    struct TextureStateTracker
    {
        RHITexture* texture{};
        EResourceStates permanent_state{EResourceStates::unknown};
        bool state_initialized{false};
    };

    struct BufferState final
    {
        EResourceStates state{EResourceStates::unknown};
        EPipelineStage pipeline_stage{EPipelineStage::none};
        bool enable_uav_barriers{true};
        bool first_uav_barrier_placed{false};
        bool permanent_transition{false};
    };

    struct TextureState final
    {
        std::vector<EResourceStates> subresource_states{};
        std::vector<EPipelineStage> subresource_stages{};
        EResourceStates state{EResourceStates::unknown};
        EPipelineStage pipeline_stage{EPipelineStage::none};
        bool enable_uav_barriers{true};
        bool first_uav_barrier_placed{false};
        bool permanent_transition{false};
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

        auto begin_tracking_buffer_state(BufferStateTracker* tracker, EResourceStates state, EPipelineStage current_stage) -> void;
        auto begin_tracking_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state, EPipelineStage current_stage) -> void;

        auto lock_buffer_state(BufferStateTracker* tracker, EResourceStates state) -> void;
        auto lock_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state) -> void;

        auto buffer_state(BufferStateTracker* tracker) -> EResourceStates;
        auto texture_subresource_state(TextureStateTracker* tracker, uint32_t mip_level, uint32_t array_layer) -> EResourceStates;


        // Internal use only.

        auto require_buffer_state(BufferStateTracker* tracker, EResourceStates state, EPipelineStage pipeline_stage = EPipelineStage::none) -> void;
        auto require_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state, EPipelineStage pipeline_stage = EPipelineStage::none) -> void;

        auto find_tracked_buffer_state(BufferStateTracker* tracker, bool create_if_missing) -> BufferState*;
        auto find_tracked_texture_state(TextureStateTracker* tracker, bool create_if_missing) -> TextureState*;

        auto keep_initial_state() -> void;

        // Clear barriers will flush all states into tracker.
        auto clear_barriers() -> void;

        auto finish_tracking() -> void;

        auto has_barrier() -> bool { return !buffer_barriers.empty() && !texture_barriers.empty(); }
    };
}
