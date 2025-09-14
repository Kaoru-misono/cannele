#include "state_tracking.hpp"

#include <core/assert.hpp>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        auto deduce_pipeline_stage_from_state(EResourceStates states) -> EPipelineStage
        {
            auto result = EPipelineStage::none;

            if (enum_has_any_flags(states, EResourceStates::vertex_attribute_read)) {
                result |= EPipelineStage::vertex_input;
            }
            if (enum_has_any_flags(states, EResourceStates::index_read)) {
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

    auto ResourceStateTracker::set_buffer_state(RHIBuffer* buffer, EResourceStates state) -> void
    {
        if (buffer->description()->memory_type != EMemoryType::gpu_only) return;

        auto buffer_state = get_buffer_state(buffer);
        if (state != buffer_state->state || state == EResourceStates::UAV_access) {
            buffer_barriers.emplace_back(buffer, buffer_state->state, state);
            buffer_state->state = state;
        }
    }

    auto ResourceStateTracker::set_texture_state(RHITexture* texture, TextureSubresourceRange subresources, EResourceStates state) -> void
    {
        auto texture_state = get_texture_state(texture);
        auto texture_desc = texture->description();
        CNE_ASSERT(subresources.mip_count <= texture_desc->mip_count && subresources.layer_count <= texture_desc->layer_count);
        auto is_whole_texture = texture->contains_all_subresources(subresources);

        if (is_whole_texture && texture_state->subresource_states.empty()) {
            if (state != texture_state->state || state == EResourceStates::UAV_access) {
                texture_barriers.emplace_back(texture, k_all_subresources, true, texture_state->state, state);
                texture_state->state = state;
            }
        } else {
            auto common_state = EResourceStates::unknown;
            auto is_same_state = true;
            auto is_single_slice = subresources.mip_count == 1 && subresources.layer_count == 1;
            if (texture_state->subresource_states.empty()) {
                texture_state->subresource_states.resize(texture_desc->mip_count * texture_desc->layer_count);
                texture_state->state = EResourceStates::unknown;
            } else {
                common_state = texture_state->subresource_states[0];
                for (auto layer = subresources.layer; layer < texture_desc->layer_count + subresources.layer_count; layer++) {
                    for (auto mip_level = subresources.mip_level; mip_level < texture_desc->mip_count + subresources.mip_count; mip_level++) {
                        auto& subresource_state = texture_state->subresource_states[layer * texture_desc->mip_count + mip_level];
                        if (subresource_state != state || subresource_state == EResourceStates::UAV_access) {
                            texture_barriers.emplace_back(texture, subresources, false, state, state);
                            subresource_state = state;
                        }

                        if (subresource_state != common_state) {
                            is_same_state = false;
                        }
                    }
                }
            }

            if (is_same_state) {
                texture_state->state = common_state;
                texture_state->subresource_states.clear();
            }
        }
    }

    auto ResourceStateTracker::finish_tracking() -> void
    {
        for (auto& [buffer, state] : buffer_states) {
            if (state.state != buffer->description()->final_state) {
                set_buffer_state(buffer, buffer->description()->final_state);
            }
        }

        for (auto& [texture, state] : texture_states) {
            if (state.state != texture->description()->final_state) {
                set_texture_state(
                    texture,
                    texture->resolve_subresource_rage(k_all_subresources),
                    texture->description()->final_state
                );
            }
        }
    }

    auto ResourceStateTracker::clear_barriers() -> void
    {
        texture_barriers.clear();
        buffer_barriers.clear();
    }

    auto ResourceStateTracker::reset() -> void
    {
        texture_barriers.clear();
        buffer_barriers.clear();
        texture_states.clear();
        buffer_states.clear();
    }

    auto ResourceStateTracker::get_buffer_state(RHIBuffer* buffer) -> BufferState*
    {
        auto it = buffer_states.find(buffer);
        if (it != buffer_states.end()) {
            return &it->second;
        }

        it = buffer_states.emplace(buffer, BufferState{.state = buffer->description()->final_state}).first;

        return &it->second;
    }

    auto ResourceStateTracker::get_texture_state(RHITexture* texture) -> TextureState*
    {
        auto it = texture_states.find(texture);
        if (it != texture_states.end()) {
            return &it->second;
        }

        it = texture_states.emplace(texture, TextureState{.state = texture->description()->final_state}).first;

        return &it->second;
    }
}
