#include "state_tracking.hpp"

#include <core/assert.hpp>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        auto deduce_pipeline_stage_from_state(EResourceStates states) -> EPipelineStage
        {
            auto result = EPipelineStage::none;

            if (enum_has_any_flags(states, EResourceStates::vertex_buffer)) {
                result |= EPipelineStage::vertex_input;
            }
            if (enum_has_any_flags(states, EResourceStates::index_buffer)) {
                result |= EPipelineStage::index_input;
            }
            if (enum_has_any_flags(states, EResourceStates::indirect_command_read)) {
                result |= EPipelineStage::draw_indirect;
            }
            if (enum_has_any_flags(states, EResourceStates::SRV_access | EResourceStates::UAV_access)) {
                result |= EPipelineStage::all_graphics | EPipelineStage::compute_shader | EPipelineStage::mesh_shader;
            }
            if (enum_has_any_flags(states, EResourceStates::transfer_src | EResourceStates::transfer_dst)) {
                result |= EPipelineStage::transfer;
            }
            if (enum_has_any_flags(states, EResourceStates::depth_stencil_read | EResourceStates::depth_stencil_attachment)) {
                result |= EPipelineStage::early_fragment_tests | EPipelineStage::late_fragment_tests;
            }
            if (enum_has_any_flags(states, EResourceStates::color_attachment)) {
                result |= EPipelineStage::color_attachment_output;
            }
            if (enum_has_any_flags(states, EResourceStates::present)) {
                result |= EPipelineStage::bottom_of_pipe;
            }

            return result;
        }
    }

    auto ResourceStateTracker::enable_uav_barriers(TextureStateTracker* tracker, bool enable) -> void
    {
        auto state = find_tracked_texture_state(tracker, true);

        state->enable_uav_barriers = enable;
    }

    auto ResourceStateTracker::enable_uav_barriers(BufferStateTracker* tracker, bool enable) -> void
    {
        auto state = find_tracked_buffer_state(tracker, true);

        state->enable_uav_barriers = enable;
    }

    auto ResourceStateTracker::begin_tracking_buffer_state(BufferStateTracker* tracker, EResourceStates state, EPipelineStage current_stage) -> void
    {
        auto tracked_state = find_tracked_buffer_state(tracker, true);

        tracked_state->state = state;
        tracked_state->pipeline_stage = current_stage == EPipelineStage::none ? deduce_pipeline_stage_from_state(state) : current_stage;
    }

    auto ResourceStateTracker::begin_tracking_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state, EPipelineStage current_stage) -> void
    {
        auto texture_desc = tracker->texture->description();

        auto tracked_state = find_tracked_texture_state(tracker, true);

        subresources.adapt_to_texture(texture_desc, false);

        if (subresources.contain_all_resources(texture_desc)) {
            tracked_state->state = state;
            tracked_state->pipeline_stage = deduce_pipeline_stage_from_state(state);
            tracked_state->subresource_states.clear();
        } else {
            tracked_state->state = EResourceStates::unknown;
            tracked_state->subresource_states.resize(texture_desc->num_layers * texture_desc->num_mips, tracked_state->state);

            auto pipeline_stage = current_stage == EPipelineStage::none ? deduce_pipeline_stage_from_state(state) : deduce_pipeline_stage_from_state(state);
            for (auto level = subresources.base_mip_level; level < subresources.base_mip_level + subresources.num_mip_levels; level++) {
                for (auto layer = subresources.base_array_layer; layer < subresources.base_array_layer + subresources.num_array_layers; layer++) {
                    tracked_state->subresource_states[layer * texture_desc->num_mips + level] = state;
                    tracked_state->pipeline_stage = pipeline_stage;
                }
            }
        }
    }

    auto ResourceStateTracker::lock_buffer_state(BufferStateTracker* tracker, EResourceStates state) -> void
    {
        if (buffer_states.at(tracker).state != state) {
            require_buffer_state(tracker, state, EPipelineStage::none);
        }

        locked_buffer_states.emplace_back(std::make_pair(tracker, state));
    }

    auto ResourceStateTracker::lock_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state) -> void
    {
        auto texture_desc = tracker->texture->description();

        subresources.adapt_to_texture(texture_desc, false);

        if (!subresources.contain_all_resources(texture_desc)) {
            CNE_ERROR("Attamp to lock subresources of texture: {} that are not contained in the subresource set", tracker->texture->name);
        } else {
            if (texture_states.at(tracker).state != state) {
                require_texture_state(tracker, subresources, state, EPipelineStage::none);
            }

            locked_texture_states.emplace_back(std::make_pair(tracker, state));
            find_tracked_texture_state(tracker, true)->permanent_transition = true;
        }
    }

    auto ResourceStateTracker::buffer_state(BufferStateTracker* tracker) -> EResourceStates
    {
        auto tracked_state = find_tracked_buffer_state(tracker, false);

        if (!tracked_state) return EResourceStates::unknown;

        return tracked_state->state;
    }

    auto ResourceStateTracker::texture_subresource_state(TextureStateTracker* tracker, uint32_t mip_level, uint32_t array_layer) -> EResourceStates
    {
        auto texture_desc = tracker->texture->description();
        auto tracked_state = find_tracked_texture_state(tracker, false);

        if (!tracked_state) return EResourceStates::unknown;

        if (tracked_state->subresource_states.empty()) return tracked_state->state;

        return tracked_state->subresource_states[array_layer * texture_desc->num_mips + mip_level];
    }

    auto ResourceStateTracker::require_buffer_state(BufferStateTracker* tracker, EResourceStates state, EPipelineStage pipeline_stage) -> void
    {
        if (tracker->permanent_state != EResourceStates::unknown) {
            // TODO: Varify
            return;
        }

        pipeline_stage = pipeline_stage == EPipelineStage::none ? deduce_pipeline_stage_from_state(state) : pipeline_stage;

        auto buffer_desc = tracker->buffer->description();

        if (buffer_desc->type != EBufferType::gpu_only) {
            return;
        }

        auto tracked_state = find_tracked_buffer_state(tracker, true);

        auto need_transition = tracked_state->state != state && state != EResourceStates::unused;
        auto need_uav_barrier = (
            true
            && (enum_has_any_flags(state, EResourceStates::UAV_access))
            && (tracked_state->enable_uav_barriers || !tracked_state->first_uav_barrier_placed)
        );

        if (need_transition) {
            // See if this buffer is already used for a different purpose in this batch.
            // If it is, combine the state bits.
            // Example: same buffer used as index and vertex buffer, or as SRV and indirect arguments.
            for (auto& buffer_barrier: buffer_barriers) {
                if (buffer_barrier.buffer == tracker->buffer) {
                    buffer_barrier.dst_state |= state;
                    buffer_barrier.dst_stage |= pipeline_stage;
                    tracked_state->state = buffer_barrier.dst_state;
                    tracked_state->pipeline_stage = buffer_barrier.dst_stage;
                    return;
                }
            }
        }

        if (need_transition || need_uav_barrier) {
            buffer_barriers.emplace_back(BufferBarrier{
                .buffer = tracker->buffer,
                .src_state = tracked_state->state,
                .dst_state = state,
                .src_stage = tracked_state->pipeline_stage,
                .dst_stage = pipeline_stage
            });
        }

        if (need_uav_barrier && !need_transition) {
            tracked_state->first_uav_barrier_placed = true;
        }

        tracked_state->state = state;
        tracked_state->pipeline_stage = pipeline_stage;
    }

    auto ResourceStateTracker::require_texture_state(TextureStateTracker* tracker, TextureSubresourceSet subresources, EResourceStates state, EPipelineStage pipeline_stage) -> void
    {
        if (tracker->permanent_state != EResourceStates::unknown) {
            // TODO: Varify
            return;
        }

        pipeline_stage = pipeline_stage == EPipelineStage::none ? deduce_pipeline_stage_from_state(state) : pipeline_stage;

        auto texture_desc = tracker->texture->description();

        auto tracked_state = find_tracked_texture_state(tracker, true);

        subresources.adapt_to_texture(texture_desc, false);

        if (subresources.contain_all_resources(texture_desc) && tracked_state->subresource_states.empty()) {

            auto need_transition = tracked_state->state != state && state != EResourceStates::unused;
            auto need_uav_barrier = (
                true
                && (enum_has_any_flags(state, EResourceStates::UAV_access))
                && (tracked_state->enable_uav_barriers || !tracked_state->first_uav_barrier_placed)
            );

            if (need_transition || need_uav_barrier) {
                texture_barriers.emplace_back(TextureBarrier{
                    .texture = tracker->texture,
                    .contain_all_resource = true,
                    .src_state = tracked_state->state,
                    .dst_state = state,
                    .src_stage = tracked_state->pipeline_stage,
                    .dst_stage = pipeline_stage
                });
            }

            if (need_uav_barrier && !need_transition) {
                tracked_state->first_uav_barrier_placed = true;
            }

            tracked_state->state = state;
            tracked_state->pipeline_stage = pipeline_stage;
        } else {
            auto state_expanded = false;
            if (tracked_state->subresource_states.empty()) {
                if (tracked_state->state == EResourceStates::unknown) {
                    CNE_ERROR("Unknown prior state of texture: {}.", tracker->texture->name);
                    CNE_ERROR("Call ` begin_tracking_texture_state() ` before using the texture.");
                }

                tracked_state->subresource_states.resize(texture_desc->num_mips * texture_desc->num_layers, tracked_state->state);
                tracked_state->state = EResourceStates::unknown;
                tracked_state->pipeline_stage = EPipelineStage::none;
                state_expanded = true;
            }

            auto any_uav_barrier = false;
            for (auto array_layer = subresources.base_array_layer; array_layer < subresources.base_array_layer + subresources.num_array_layers; array_layer++) {
                for (auto mip_level = subresources.base_mip_level; mip_level < subresources.base_mip_level + subresources.num_mip_levels; mip_level++) {
                    auto subresource_index = array_layer * texture_desc->num_mips + mip_level;

                    auto& prior_state = tracked_state->subresource_states[subresource_index];
                    if (prior_state == EResourceStates::unknown && !state_expanded) {
                        CNE_ERROR("Unknown prior state of texture: {}.", tracker->texture->name);
                        CNE_ERROR("subresource (Miplevel: {}, ArrayLayer: {})", mip_level, array_layer);
                        CNE_ERROR("Call ` begin_tracking_texture_state() ` before using the texture.");
                    }

                    auto need_transition = prior_state != state && state != EResourceStates::unused;
                    auto need_uav_barrier = (
                        true
                        && (enum_has_any_flags(state, EResourceStates::UAV_access))
                        && !any_uav_barrier
                        && (tracked_state->enable_uav_barriers || !tracked_state->first_uav_barrier_placed)
                    );

                    if (need_transition || need_uav_barrier) {
                        texture_barriers.emplace_back(TextureBarrier{
                            .texture = tracker->texture,
                            .mip_level = mip_level,
                            .array_layer = array_layer,
                            .contain_all_resource = false,
                            .src_state = prior_state,
                            .dst_state = state,
                            .src_stage = tracked_state->pipeline_stage,
                            .dst_stage = pipeline_stage
                        });
                    }

                    if (need_uav_barrier && !need_transition) {
                        any_uav_barrier = true;
                        tracked_state->first_uav_barrier_placed = true;
                    }

                    tracked_state->subresource_states[subresource_index] = state;
                    tracked_state->subresource_stages[subresource_index] = pipeline_stage;
                }
            }
        }
    }

    auto ResourceStateTracker::find_tracked_texture_state(TextureStateTracker* tracker, bool create_if_missing) -> TextureState*
    {
        auto it = texture_states.find(tracker);

        if (it != texture_states.end()) return &it->second;

        if (!create_if_missing) return nullptr;

        it = texture_states.emplace(tracker, TextureState{}).first;

        return &it->second;
    }

    auto ResourceStateTracker::find_tracked_buffer_state(BufferStateTracker* tracker, bool create_if_missing) -> BufferState*
    {
        auto it = buffer_states.find(tracker);

        if (it != buffer_states.end()) return &it->second;

        if (!create_if_missing) return nullptr;

        it = buffer_states.emplace(tracker, BufferState{}).first;

        return &it->second;
    }

    auto ResourceStateTracker::keep_initial_state() -> void
    {
        // TODO:
        // for (auto& [buffer, tracked_state]: buffer_states) {
        // }
        for (auto& [tracker, tracked_state]: texture_states) {
            auto texture_desc = tracker->texture->description();
            if (
                true
                && texture_desc->keep_initial_state
                && tracker->permanent_state == EResourceStates::unknown
                && !tracked_state.permanent_transition
            ) {
                require_texture_state(tracker, TextureSubresourceSet{}, texture_desc->initial_state, EPipelineStage::none);
            }
        }
    }

    auto ResourceStateTracker::clear_barriers() -> void
    {
        texture_barriers.clear();
        buffer_barriers.clear();
    }

    auto ResourceStateTracker::finish_tracking() -> void
    {
        for (auto& [tracker, state]: locked_texture_states) {
            if (tracker->permanent_state != EResourceStates::unknown && tracker->permanent_state != state) {
                CNE_ERROR("Attamp to switch state of locked texture: {}. from {:x} to {:x}", tracker->texture->name, (int) tracker->permanent_state, (int) state);
                continue;
            }

            tracker->permanent_state = state;
        }
        locked_texture_states.clear();

        for (auto& [tracker, state]: locked_buffer_states) {
            if (tracker->permanent_state != EResourceStates::unknown && tracker->permanent_state != state) {
                CNE_ERROR("Attamp to switch state of locked buffer: {}. from {:x} to {:x}", tracker->buffer->name, (int) tracker->permanent_state, (int) state);
                continue;
            }

            tracker->permanent_state = state;
        }
        locked_buffer_states.clear();

        for (auto& [tracker, state]: texture_states) {
            if (tracker->texture->description()->keep_initial_state && !tracker->state_initialized) {
                tracker->state_initialized = true;
            }
        }

        texture_states.clear();
        buffer_states.clear();
    }
}
