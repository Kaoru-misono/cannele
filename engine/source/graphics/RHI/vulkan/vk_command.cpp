#include "vk_command.hpp"
#include "vk_RHI.hpp"
#include "vk_tool.hpp"
#include "vk_shader.hpp"

#include "../tool/state_tracking.hpp"

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        inline auto range_equal(auto& a, auto& b) -> bool
        {
            return std::equal(a.begin(), a.end(), b.begin(), b.end());
        }

        auto get_mip_size(uint32_t size, uint32_t mip_level) -> uint32_t
        {
            size = size >> mip_level;
            return size > 0 ? size : 1;
        }

        auto get_mip_size(Extent3D extent, uint32_t mip_level) -> Extent3D
        {
            extent.width  = get_mip_size(extent.width, mip_level);
            extent.height = get_mip_size(extent.height, mip_level);
            extent.depth  = get_mip_size(extent.depth, mip_level);
            return extent;
        }

        struct CommandRecorder
        {
            VulkanDevice* device{};

            VkCommandBuffer command_buffer{VK_NULL_HANDLE};

            ResourceStateTracker resource_state_tracker{};

            std::vector<VulkanTextureView*> render_targets{};
            std::vector<VulkanTextureView*> resolve_targets{};
            VulkanTextureView* depth_stencil_target{};

            bool graphics_pass_active{false};
            bool graphics_state_valid{false};
            GraphicsState graphics_state{};
            VulkanBindingData* binding_data{};
            VulkanGraphicsPipeline* graphics_pipeline{};

            bool compute_pass_active{false};
            bool compute_state_valid{false};
            VulkanComputePipeline* compute_pipeline{};

            CommandRecorder(VulkanDevice* device)
                : device(device)
            {}

            auto record(VulkanCommandBuffer* command_buffer) -> void;

            auto record_copy_buffer(commands::copy_buffer const* cmd) -> void;
            auto record_copy_texture(commands::copy_texture const* cmd) -> void;
            auto record_copy_texture_to_buffer(commands::copy_texture_to_buffer const* cmd) -> void;
            auto record_clear_buffer_uint(commands::clear_buffer_uint const* cmd) -> void;
            auto record_clear_texture_float(commands::clear_texture_float const* cmd) -> void;
            auto record_clear_texture_uint(commands::clear_texture_uint const* cmd) -> void;
            auto record_clear_texture_depth_stencil(commands::clear_texture_depth_stencil const* cmd) -> void;
            auto record_upload_texture_data(commands::upload_texture_data const* cmd) -> void;
            auto record_resolve_query(commands::resolve_query const* cmd) -> void;
            auto record_begin_graphics_pass(commands::begin_graphics_pass const* cmd) -> void;
            auto record_end_graphics_pass(commands::end_graphics_pass const* cmd) -> void;
            auto record_set_graphics_state(commands::set_graphics_state const* cmd) -> void;
            auto record_draw(commands::draw const* cmd) -> void;
            auto record_draw_indexed(commands::draw_indexed const* cmd) -> void;
            auto record_draw_indirect(commands::draw_indirect const* cmd) -> void;
            auto record_draw_indexed_indirect(commands::draw_indexed_indirect const* cmd) -> void;
            auto record_dispatch_mesh(commands::dispatch_mesh const* cmd) -> void;
            auto record_dispatch_mesh_indirect(commands::dispatch_mesh_indirect const* cmd) -> void;
            auto record_begin_compute_pass(commands::begin_compute_pass const* cmd) -> void;
            auto record_end_compute_pass(commands::end_compute_pass const* cmd) -> void;
            auto record_set_compute_state(commands::set_compute_state const* cmd) -> void;
            auto record_dispatch_compute(commands::dispatch_compute const* cmd) -> void;
            auto record_dispatch_compute_indirect(commands::dispatch_compute_indirect const* cmd) -> void;
            auto record_set_buffer_state(commands::set_buffer_state const* cmd) -> void;
            auto record_set_texture_state(commands::set_texture_state const* cmd) -> void;
            auto record_push_command_label(commands::push_command_label const* cmd) -> void;
            auto record_pop_command_label(commands::pop_command_label const* cmd) -> void;
            auto record_insert_debug_marker(commands::insert_debug_marker const* cmd) -> void;
            auto record_write_timestamp(commands::write_timestamp const* cmd) -> void;
            auto record_insert_global_barrier(commands::insert_global_barrier const*) -> void;

            auto insert_barriers_for_graphics_state(CommandList* command_list, CommandList::CommandSlot const* begin_graphics_pass_slot) -> void;
            auto require_binding_states(VulkanBindingData* binding_data) -> void;
            auto set_bindings(VulkanBindingData* binding_data, VkPipelineBindPoint bind_point) -> void;
            auto commit_barriers() -> void;

            auto require_buffer_state(VulkanBuffer* buffer, EResourceStates state) -> void;
            auto require_texture_state(VulkanTexture* texture, TextureSubresourceRange const& subresources, EResourceStates state) -> void;
        };
    }

    auto CommandRecorder::record(VulkanCommandBuffer* in_command_buffer) -> void
    {
        command_buffer = in_command_buffer->command_buffer;

        auto begin_info = VkCommandBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK_VK_RESULT(vkBeginCommandBuffer(command_buffer, &begin_info));

        auto& command_list = in_command_buffer->command_list;
        for (auto slot = command_list->get_commands(); slot; slot = slot->next) {
            if (slot->id == CommandID::begin_graphics_pass) {
                insert_barriers_for_graphics_state(command_list.get(), slot);
            }

        #define RECORD_COMMAND(COMMAND) \
            case CommandID::COMMAND: \
            record_##COMMAND(command_list->get_command<commands::COMMAND>(slot)); \
            break;

            switch (slot->id) {
                RHI_COMMANDS(RECORD_COMMAND);
            }
        #undef RECORD_COMMAND
        }

        resource_state_tracker.finish_tracking();
        commit_barriers();
        resource_state_tracker.reset();

        CHECK_VK_RESULT(vkEndCommandBuffer(command_buffer));
    }

    auto CommandRecorder::record_copy_buffer(commands::copy_buffer const* cmd) -> void
    {
        auto src_buffer = cast<VulkanBuffer*>(cmd->src_buffer);
        auto dst_buffer = cast<VulkanBuffer*>(cmd->dst_buffer);

        require_buffer_state(src_buffer, EResourceStates::transfer_src);
        require_buffer_state(dst_buffer, EResourceStates::transfer_dst);
        commit_barriers();

        auto copy_region = VkBufferCopy2{
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .srcOffset = cmd->src_offset,
            .dstOffset = cmd->dst_offset,
            .size      = cmd->size,
        };

        auto copy_info = VkCopyBufferInfo2{
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .srcBuffer   = src_buffer->buffer,
            .dstBuffer   = dst_buffer->buffer,
            .regionCount = 1,
            .pRegions    = &copy_region,
        };

        vkCmdCopyBuffer2(command_buffer, &copy_info);
    }

    auto CommandRecorder::record_copy_texture(commands::copy_texture const* cmd) -> void
    {
        auto src_texture = cast<VulkanTexture*>(cmd->src_texture);
        auto dst_texture = cast<VulkanTexture*>(cmd->dst_texture);
        auto& src_subresources = cmd->src_subresources;
        auto& dst_subresources = cmd->dst_subresources;
        auto src_offset = cmd->src_offset;
        auto dst_offset = cmd->dst_offset;
        auto extent = cmd->extent;

        require_texture_state(src_texture, src_subresources, EResourceStates::transfer_src);
        require_texture_state(dst_texture, dst_subresources, EResourceStates::transfer_dst);
        commit_barriers();

        auto copy_regions = std::vector<VkImageCopy2>{};
        for (auto layer = 0u; layer < dst_subresources.layer_count; layer++) {
            for (auto mip = 0u; mip < dst_subresources.mip_count; mip++) {
                auto src_mip = src_subresources.mip_level + mip;
                auto dst_mip = dst_subresources.mip_level + mip;

                auto src_mip_size = get_mip_size(src_texture->info.extent, src_mip);
                auto fixed_extent = extent;
                if (fixed_extent.width == k_remaining_texture_size) {
                    fixed_extent.width = src_mip_size.width - src_offset.x;
                }
                if (fixed_extent.height == k_remaining_texture_size) {
                    fixed_extent.height = src_mip_size.height - src_offset.y;
                }
                if (fixed_extent.depth == k_remaining_texture_size) {
                    fixed_extent.depth = src_mip_size.depth - src_offset.z;
                }

                auto& copy_region = copy_regions.emplace_back();
                copy_region.sType          = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
                copy_region.srcSubresource = VkImageSubresourceLayers{
                    .aspectMask     = aspect_flag_from_format(src_texture->format),
                    .mipLevel       = src_mip,
                    .baseArrayLayer = src_subresources.layer_count + layer,
                    .layerCount     = 1,
                };
                copy_region.dstSubresource = VkImageSubresourceLayers{
                    .aspectMask     = aspect_flag_from_format(dst_texture->format),
                    .mipLevel       = dst_mip,
                    .baseArrayLayer = dst_subresources.layer_count + layer,
                    .layerCount     = 1,
                };
                copy_region.srcOffset      = VkOffset3D{(int32_t) src_offset.x, (int32_t) src_offset.y, (int32_t) src_offset.z};
                copy_region.dstOffset      = VkOffset3D{(int32_t) dst_offset.x, (int32_t) dst_offset.y, (int32_t) dst_offset.z};
                copy_region.extent         = VkExtent3D{fixed_extent.width, fixed_extent.height, fixed_extent.depth};
            }
        }

        auto copy_info = VkCopyImageInfo2{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};
        copy_info.srcImage       = src_texture->image;
        copy_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy_info.dstImage       = dst_texture->image;
        copy_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_info.regionCount    = copy_regions.size();
        copy_info.pRegions       = copy_regions.data();

        vkCmdCopyImage2(command_buffer, &copy_info);
    }

    auto CommandRecorder::record_copy_texture_to_buffer(commands::copy_texture_to_buffer const* cmd) -> void
    {}

    auto CommandRecorder::record_clear_buffer_uint(commands::clear_buffer_uint const* cmd) -> void
    {
        auto buffer = cast<VulkanBuffer*>(cmd->buffer);

        require_buffer_state(buffer, EResourceStates::transfer_dst);
        commit_barriers();

        auto offset = cmd->range.offset;
        auto size   = cmd->range.size;

        if (offset == 0 && size == buffer->info.size_bytes) {
            size = VK_WHOLE_SIZE;
        }

        vkCmdFillBuffer(command_buffer, buffer->buffer, offset, size, cmd->clear_value);
    }

    auto CommandRecorder::record_clear_texture_float(commands::clear_texture_float const* cmd) -> void
    {
        auto texture = cast<VulkanTexture*>(cmd->texture);

        require_texture_state(texture, cmd->subresources, EResourceStates::transfer_dst);
        commit_barriers();

        auto subresource_range = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = cmd->subresources.mip_level,
            .levelCount     = cmd->subresources.mip_count,
            .baseArrayLayer = cmd->subresources.layer,
            .layerCount     = cmd->subresources.layer_count,
        };
        subresource_range.levelCount = std::clamp(subresource_range.levelCount, 1u, texture->info.mip_count);
        subresource_range.layerCount = std::clamp(subresource_range.layerCount, 1u, texture->info.layer_count);

        auto clear_color = VkClearColorValue{};
        std::memcpy(clear_color.float32, &cmd->clear_color, sizeof(cmd->clear_color));

        vkCmdClearColorImage(
            command_buffer,
            texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear_color,
            1,
            &subresource_range
        );
    }

    auto CommandRecorder::record_clear_texture_uint(commands::clear_texture_uint const* cmd) -> void
    {
        auto texture = cast<VulkanTexture*>(cmd->texture);

        require_texture_state(texture, cmd->subresources, EResourceStates::transfer_dst);
        commit_barriers();

        auto subresource_range = VkImageSubresourceRange{
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = cmd->subresources.mip_level,
            .levelCount     = cmd->subresources.mip_count,
            .baseArrayLayer = cmd->subresources.layer,
            .layerCount     = cmd->subresources.layer_count,
        };

        auto clear_color = VkClearColorValue{};
        std::memcpy(clear_color.uint32, &cmd->clear_color, sizeof(cmd->clear_color));

        vkCmdClearColorImage(
            command_buffer,
            texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear_color,
            1,
            &subresource_range
        );
    }

    auto CommandRecorder::record_clear_texture_depth_stencil(commands::clear_texture_depth_stencil const* cmd) -> void
    {
        auto texture = cast<VulkanTexture*>(cmd->texture);
        auto& subresources = cmd->subresources;

        require_texture_state(texture, subresources, EResourceStates::transfer_dst);
        commit_barriers();

        auto subresource_range = VkImageSubresourceRange{
            .aspectMask     = 0,
            .baseMipLevel   = subresources.mip_level,
            .levelCount     = subresources.mip_count,
            .baseArrayLayer = subresources.layer,
            .layerCount     = subresources.layer_count,
        };

        auto clear_depth_stencil = VkClearDepthStencilValue{};
        if (cmd->clear_depth) {
            clear_depth_stencil.depth = cmd->clear_depth.value();
            subresource_range.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        if (cmd->clear_stencil) {
            clear_depth_stencil.stencil = cmd->clear_stencil.value();
            subresource_range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        vkCmdClearDepthStencilImage(
            command_buffer,
            texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear_depth_stencil,
            1,
            &subresource_range
        );
    }

    auto CommandRecorder::record_upload_texture_data(commands::upload_texture_data const* cmd) -> void
    {
        auto buffer = cast<VulkanBuffer*>(cmd->src_buffer);
        auto texture = cast<VulkanTexture*>(cmd->dst_texture);
        auto& subresources = cmd->dst_subresources;

        require_buffer_state(buffer, EResourceStates::transfer_src);
        require_texture_state(texture, subresources, EResourceStates::transfer_dst);
        commit_barriers();

        auto offset = cmd->src_offset;

        auto copy_regions = std::vector<VkBufferImageCopy2>{};
        for (auto layer_offset = 0u; layer_offset < subresources.layer_count; layer_offset++) {
            auto layer = subresources.layer + layer_offset;
            for (auto mip_offset = 0u; mip_offset < subresources.mip_count; mip_offset++) {
                auto mip_level = subresources.mip_level + mip_offset;

                // TODO:
                auto row_length_in_blocks = 0u;
                auto row_length_in_texels = 0u;

                auto& region = copy_regions.emplace_back(VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2);
                region.bufferOffset = offset;
                region.bufferRowLength = row_length_in_texels;
                region.bufferImageHeight = 0;
                region.imageSubresource = VkImageSubresourceLayers{
                    .aspectMask     = aspect_flag_from_format(texture->format),
                    .mipLevel       = mip_level,
                    .baseArrayLayer = layer,
                    .layerCount     = 1,
                };
                region.imageOffset = VkOffset3D{
                    .x = (int32_t) cmd->dst_offset.x,
                    .y = (int32_t) cmd->dst_offset.y,
                    .z = (int32_t) cmd->dst_offset.z
                };

                region.imageExtent = VkExtent3D{
                    .width  = std::min(cmd->extent.width,  get_mip_size(texture->info.extent.width, mip_level)),
                    .height = std::min(cmd->extent.height, get_mip_size(texture->info.extent.height, mip_level)),
                    .depth  = std::min(cmd->extent.depth,  get_mip_size(texture->info.extent.depth, mip_level)),
                };

                // TODO: ready when layout supported.
                // buffer_offset +=
                // subresource_layout++;
            }
        }

        auto buffer_to_image_info = VkCopyBufferToImageInfo2{VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
        buffer_to_image_info.srcBuffer      = buffer->buffer;
        buffer_to_image_info.dstImage       = texture->image;
        buffer_to_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        buffer_to_image_info.regionCount    = copy_regions.size();
        buffer_to_image_info.pRegions       = copy_regions.data();

        vkCmdCopyBufferToImage2(command_buffer, &buffer_to_image_info);
    }

    auto CommandRecorder::record_resolve_query(commands::resolve_query const* cmd) -> void
    {
        auto query = cast<VulkanTimerQuery*>(cmd->query);
        auto buffer = cast<VulkanBuffer*>(cmd->buffer);

        require_buffer_state(buffer, EResourceStates::transfer_dst);
        commit_barriers();

        vkCmdCopyQueryPoolResults(
            command_buffer,
            query->pool->query_pool,
            cmd->query_index,
            cmd->query_count,
            buffer->buffer,
            cmd->offset,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );
    }

    auto CommandRecorder::record_begin_graphics_pass(commands::begin_graphics_pass const* cmd) -> void
    {
        // TODO: Use TextureView to do everything.
        auto render_area = VkRect2D{
            .offset = {0, 0},
            .extent = { // TODO: Store properties in context.
                device->physical_device_properties.properties2.properties.limits.maxFramebufferWidth,
                device->physical_device_properties.properties2.properties.limits.maxFramebufferHeight
            }
        };

        auto color_attachment_infos = std::vector<VkRenderingAttachmentInfo>{};
        color_attachment_infos.reserve(cmd->color_attachments.size());
        std::ranges::transform(
            cmd->color_attachments,
            std::back_inserter(color_attachment_infos),
            [&](auto const& attachment) -> VkRenderingAttachmentInfo {
                auto view = cast<VulkanTextureView*>(attachment.view);
                auto texture = view->texture_;

                require_texture_state(texture, view->range_, EResourceStates::color_attachment);

                // TODO: Clip render area by subresource width and height.
                auto subresource = TextureSubresourceRange{};
                auto attachment_layer_count = (
                    texture->info.dimension == ETextureDimension::tex_3d
                    ? texture->info.extent.depth
                    : subresource.layer_count
                );
                render_area.extent.width  = std::min(render_area.extent.width, get_mip_size(texture->info.extent.width, subresource.mip_level));
                render_area.extent.height = std::min(render_area.extent.height, get_mip_size(texture->info.extent.height, subresource.mip_level));

                auto clear_color = &attachment.clear_color;
                return VkRenderingAttachmentInfo{
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView          = view->image_view,
                    .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = to_vk_load_op(attachment.load),
                    .storeOp            = to_vk_store_op(attachment.store),
                    .clearValue         = {.color = {clear_color->x, clear_color->y, clear_color->z, clear_color->w}},
                };

                render_targets.emplace_back(view);
            }
        );

        auto has_depth = false;
        auto has_stencil = false;
        auto depth_attachment = VkRenderingAttachmentInfo{};
        auto stencil_attachment = VkRenderingAttachmentInfo{};
        if (auto attachment = cmd->depth_stencil_attachment) {
            auto view = cast<VulkanTextureView*>(attachment->view);
            auto texture = view->texture_;

            require_texture_state(texture, view->range_, EResourceStates::depth_stencil_attachment);

            // TODO: Clip render area by subresource width and height.
            auto subresource = TextureSubresourceRange{};
            render_area.extent.width = std::min(render_area.extent.width, texture->info.extent.width);
            render_area.extent.height = std::min(render_area.extent.height, texture->info.extent.height);

            if (is_depth_format(texture->format)) {
                has_depth = true;
                depth_attachment = VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                depth_attachment.imageView          = view->image_view;
                depth_attachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depth_attachment.resolveMode        = VK_RESOLVE_MODE_NONE;
                depth_attachment.resolveImageView   = VK_NULL_HANDLE;
                depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depth_attachment.loadOp             = to_vk_load_op(attachment->depth_load);
                depth_attachment.storeOp            = to_vk_store_op(attachment->depth_store);
                depth_attachment.clearValue         = VkClearValue{
                    .depthStencil = VkClearDepthStencilValue{
                        attachment->clear_depth,
                        attachment->clear_stencil,
                    }
                };
            }

            if (is_stencil_format(texture->format)) {
                has_stencil = true;
                stencil_attachment = VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                stencil_attachment.imageView          = view->image_view;
                stencil_attachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                stencil_attachment.resolveMode        = VK_RESOLVE_MODE_NONE;
                stencil_attachment.resolveImageView   = VK_NULL_HANDLE;
                stencil_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                stencil_attachment.loadOp             = to_vk_load_op(attachment->stencil_load);
                stencil_attachment.storeOp            = to_vk_store_op(attachment->stencil_store);
                stencil_attachment.clearValue         = VkClearValue{
                    .depthStencil = VkClearDepthStencilValue{
                        attachment->clear_depth,
                        attachment->clear_stencil,
                    }
                };
            }

            depth_stencil_target = view;
        }

        commit_barriers();

        auto rendering_info = VkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering_info.flags                = 0;
        rendering_info.renderArea           = render_area;
        rendering_info.layerCount           = 1;
        rendering_info.viewMask             = 0;
        rendering_info.colorAttachmentCount = color_attachment_infos.size();
        rendering_info.pColorAttachments    = color_attachment_infos.data();
        rendering_info.pDepthAttachment     = has_depth ? &depth_attachment : nullptr;
        rendering_info.pStencilAttachment   = has_stencil ? &stencil_attachment : nullptr;

        vkCmdBeginRendering(command_buffer, &rendering_info);

        graphics_pass_active = true;
    }

    auto CommandRecorder::record_end_graphics_pass(commands::end_graphics_pass const* cmd) -> void
    {
        vkCmdEndRendering(command_buffer);

        render_targets.clear();
        resolve_targets.clear();
        depth_stencil_target = nullptr;

        graphics_pass_active = false;
    }

    auto CommandRecorder::record_set_graphics_state(commands::set_graphics_state const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        auto state = &cmd->state;

        auto vertex_buffers_dirty = !graphics_state_valid || !range_equal(graphics_state.vertex_buffers, state->vertex_buffers);
        auto index_buffer_dirty = !graphics_state_valid || graphics_state.index_buffer != state->index_buffer || graphics_state.index_type != state->index_type;
        auto viewports_dirty = !graphics_state_valid || !range_equal(graphics_state.viewports, state->viewports);
        auto scissors_dirty = !graphics_state_valid || !range_equal(graphics_state.scissors, state->scissors);
        auto blend_states_dirty = !graphics_state_valid || !range_equal(graphics_state.blend_states, state->blend_states);
        auto depth_state_dirty = !graphics_state_valid || graphics_state.depth_stencil_state != state->depth_stencil_state;

        if (auto pipeline = cast<VulkanGraphicsPipeline*>(cmd->pipeline); graphics_pipeline != pipeline) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

            graphics_pipeline = pipeline;

            // Bind bindless descriptor sets here.
            auto binding_infos = std::vector<VkDescriptorBufferBindingInfoEXT>{};
            binding_infos.emplace_back(
                VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                nullptr,
                device->bindless_manager->resource_heap->descriptor_buffer_address,
                VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            );
            binding_infos.emplace_back(
                VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                nullptr,
                device->bindless_manager->sampler_heap->descriptor_buffer_address,
                VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
            );
            vkCmdBindDescriptorBuffersEXT(
                command_buffer,
                2,
                binding_infos.data()
            );

            auto buffer_index = std::vector<uint32_t>{0u, 1u};
            auto buffer_offset = std::vector<VkDeviceSize>{0, 0};
            vkCmdSetDescriptorBufferOffsetsEXT(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline->pipeline_layout,
                0,
                2,
                buffer_index.data(),
                buffer_offset.data()
            );
        }

        // if (!cmd->push_constants.empty()) {
        //     vkCmdPushConstants(
        //         command_buffer,
        //         graphics_pipeline->pipeline_layout,
        //         VK_SHADER_STAGE_ALL,
        //         0,
        //         cmd->push_constants.size(),
        //         cmd->push_constants.data()
        //     );
        // }

        auto binding_data_dirty = !graphics_state_valid || cmd->binding_data != binding_data;
        if (binding_data_dirty) {
            binding_data = static_cast<VulkanBindingData*>(cmd->binding_data);
            set_bindings(binding_data, VK_PIPELINE_BIND_POINT_GRAPHICS);
        }

        if (vertex_buffers_dirty) {
            if (!state->vertex_buffers.empty()) {
                auto vertex_buffers = std::vector<VkBuffer>{};
                auto vertex_offsets = std::vector<VkDeviceSize>{};

                for (auto& vertex_buffer: state->vertex_buffers) {
                    vertex_buffers.emplace_back(cast<VulkanBuffer*>(vertex_buffer.buffer)->buffer);
                    vertex_offsets.emplace_back(vertex_buffer.offset);
                }

                vkCmdBindVertexBuffers(
                    command_buffer,
                    0,
                    vertex_buffers.size(),
                    vertex_buffers.data(),
                    vertex_offsets.data()
                );
            }
        }

        if (index_buffer_dirty) {
            if (state->index_buffer.buffer) {
                auto index_buffer = cast<VulkanBuffer*>(state->index_buffer.buffer)->buffer;
                vkCmdBindIndexBuffer(
                    command_buffer,
                    index_buffer,
                    state->index_buffer.offset,
                    state->index_type == EIndexType::uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32
                );
            }
        }

        if (graphics_pipeline->info.dynamic_input_state) {
            if (auto input_state = state->vertex_input_state) {
                auto vk_bindings = std::vector<VkVertexInputBindingDescription2EXT>{};
                auto vk_attributes = std::vector<VkVertexInputAttributeDescription2EXT>{};
                for (auto& stream: input_state->streams) {
                    auto binding = &vk_bindings.emplace_back(VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT);
                    binding->binding   = stream.binding;
                    binding->stride    = stream.stride;
                    binding->inputRate = stream.input_rate == EVertexInputRate::vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
                    binding->divisor   = 1;

                    for (auto& attribute: stream.attributes) {
                        auto vk_attribute = &vk_attributes.emplace_back(VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT);
                        vk_attribute->location = attribute.location;
                        vk_attribute->binding  = stream.binding;
                        vk_attribute->format   = to_vk_format(attribute.format);
                        vk_attribute->offset   = attribute.offset_bytes;
                    }
                }
                vkCmdSetVertexInputEXT(
                    command_buffer,
                    vk_bindings.size(),
                    vk_bindings.data(),
                    vk_attributes.size(),
                    vk_attributes.data()
                );
            } else {
                vkCmdSetVertexInputEXT(
                    command_buffer,
                    0,
                    nullptr,
                    0,
                    nullptr
                );
            }
        }

        if (viewports_dirty) {
            auto vk_viewports = std::vector<VkViewport>{};
            vk_viewports.reserve(state->viewports.size());
            std::ranges::transform(
                state->viewports,
                std::back_inserter(vk_viewports),
                [](auto& viewport) -> VkViewport {
                    return VkViewport{
                        .x        = viewport.x,
                        .y        = viewport.height - viewport.y,
                        .width    = viewport.width,
                        .height   = -viewport.height,
                        .minDepth = viewport.min_depth,
                        .maxDepth = viewport.max_depth
                    };
                }
            );

            vkCmdSetViewportWithCount(command_buffer, vk_viewports.size(), vk_viewports.data());
        }

        if (scissors_dirty) {
            auto vk_scissors = std::vector<VkRect2D>{};
            CNE_ASSERT(!state->scissors.empty());
            vk_scissors.reserve(state->scissors.size());
            std::ranges::transform(
                state->scissors,
                std::back_inserter(vk_scissors),
                [](auto& scissor) -> VkRect2D {
                    return VkRect2D{
                        .offset = VkOffset2D{
                            .x = scissor.x,
                            .y = scissor.y
                        },
                        .extent = VkExtent2D{
                            .width  = scissor.width,
                            .height = scissor.height
                        }
                    };
                }
            );

            vkCmdSetScissorWithCount(command_buffer, vk_scissors.size(), vk_scissors.data());
        }


        if (graphics_pipeline->info.dynamic_blend_states && blend_states_dirty) {
            auto blend_states = state->blend_states;
            // CNE_ASSERT_WITH(blend_states.size() == render_target->color_attachments.size(), "Blend states must match number of color attachments.");

            std::vector<VkBool32> blend_enable{};
            std::vector<VkColorBlendEquationEXT> color_blend_equation{};
            std::vector<VkColorComponentFlags> color_component_flags{};
            blend_enable.reserve(blend_states.size());
            color_blend_equation.reserve(blend_states.size());
            color_component_flags.reserve(blend_states.size());
            for (auto& blend_state: blend_states) {
                blend_enable.emplace_back(blend_state.enable_blend ? VK_TRUE : VK_FALSE);
                color_blend_equation.emplace_back(VkColorBlendEquationEXT{
                    .srcColorBlendFactor = to_vk_blend_factor(blend_state.color_blend.src_factor),
                    .dstColorBlendFactor = to_vk_blend_factor(blend_state.color_blend.dst_factor),
                    .colorBlendOp        = to_vk_blend_op(blend_state.color_blend.blend_op),
                    .srcAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_blend.src_factor),
                    .dstAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_blend.dst_factor),
                    .alphaBlendOp        = to_vk_blend_op(blend_state.alpha_blend.blend_op)
                });
                color_component_flags.emplace_back(
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::r) ? VK_COLOR_COMPONENT_R_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::g) ? VK_COLOR_COMPONENT_G_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::b) ? VK_COLOR_COMPONENT_B_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::a) ? VK_COLOR_COMPONENT_A_BIT : 0)
                );
            }
            vkCmdSetColorBlendEnableEXT(command_buffer, 0, blend_states.size(), blend_enable.data());
            vkCmdSetColorBlendEquationEXT(command_buffer, 0, blend_states.size(), color_blend_equation.data());
            vkCmdSetColorWriteMaskEXT(command_buffer, 0, blend_states.size(), color_component_flags.data());
        }

        // TODO:
        vkCmdSetCullMode(command_buffer, VK_CULL_MODE_NONE);
        vkCmdSetPolygonModeEXT(command_buffer, VK_POLYGON_MODE_FILL);
        vkCmdSetRasterizationSamplesEXT(command_buffer, VK_SAMPLE_COUNT_1_BIT);
        vkCmdSetFrontFace(command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        if (graphics_pipeline->info.dynamic_depth_state && depth_state_dirty) {
            graphics_state.depth_stencil_state = state->depth_stencil_state;

            auto has_depth_stencil_attachment = (bool) depth_stencil_target;
            auto enable_depth_test  = !has_depth_stencil_attachment ? VK_FALSE : (state->depth_stencil_state.enable_depth_test ? VK_TRUE : VK_FALSE);
            auto enable_depth_write = !has_depth_stencil_attachment ? VK_FALSE : (state->depth_stencil_state.enable_depth_write ? VK_TRUE : VK_FALSE);
            auto depth_compare_op   = !has_depth_stencil_attachment ? VK_COMPARE_OP_ALWAYS : to_vk_compare_op(state->depth_stencil_state.depth_compare);
            vkCmdSetDepthTestEnable(command_buffer, enable_depth_test);
            vkCmdSetDepthWriteEnable(command_buffer, enable_depth_write);
            vkCmdSetDepthBoundsTestEnable(command_buffer, VK_FALSE);
            vkCmdSetDepthBiasEnable(command_buffer, VK_FALSE);
            vkCmdSetDepthClampEnableEXT(command_buffer, VK_FALSE);
            vkCmdSetDepthCompareOp(command_buffer, depth_compare_op);
        }

        // TODO:
        vkCmdSetStencilTestEnable(command_buffer, VK_FALSE);
        vkCmdSetStencilOp(command_buffer,
            VK_STENCIL_FACE_FRONT_AND_BACK,
            VK_STENCIL_OP_KEEP,
            VK_STENCIL_OP_KEEP,
            VK_STENCIL_OP_KEEP,
            VK_COMPARE_OP_ALWAYS
        );

        vkCmdSetLogicOpEnableEXT(command_buffer, VK_FALSE);
        vkCmdSetLogicOpEXT(command_buffer, VK_LOGIC_OP_NO_OP);

        graphics_state = *state;
        graphics_state_valid = true;

        compute_state_valid = false;
        compute_pipeline = {};
    }

    auto CommandRecorder::record_draw(commands::draw const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        vkCmdDraw(
            command_buffer,
            cmd->args.vertex_count,
            cmd->args.instance_count,
            cmd->args.first_vertex,
            cmd->args.first_instance
        );
    }

    auto CommandRecorder::record_draw_indexed(commands::draw_indexed const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        vkCmdDrawIndexed(
            command_buffer,
            cmd->args.vertex_count,
            cmd->args.instance_count,
            cmd->args.first_index,
            cmd->args.first_vertex,
            cmd->args.first_instance
        );
    }

    auto CommandRecorder::record_draw_indirect(commands::draw_indirect const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        auto args_buffer = cast<VulkanBuffer*>(cmd->args_buffer.buffer);
        auto count_buffer = cast<VulkanBuffer*>(cmd->count_buffer.buffer);

        require_buffer_state(args_buffer, EResourceStates::indirect_command_read);
        if (count_buffer) {
            require_buffer_state(count_buffer, EResourceStates::indirect_command_read);
        }
        commit_barriers();

        if (count_buffer) {
            vkCmdDrawIndirectCount(
                command_buffer,
                args_buffer->buffer,
                cmd->args_buffer.offset,
                count_buffer->buffer,
                cmd->count_buffer.offset,
                cmd->draw_count,
                sizeof(VkDrawIndirectCommand)
            );
        } else {
            vkCmdDrawIndirect(
                command_buffer,
                args_buffer->buffer,
                cmd->args_buffer.offset,
                cmd->draw_count,
                sizeof(VkDrawIndirectCommand)
            );
        }
    }

    auto CommandRecorder::record_draw_indexed_indirect(commands::draw_indexed_indirect const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        auto args_buffer = cast<VulkanBuffer*>(cmd->args_buffer.buffer);
        auto count_buffer = cast<VulkanBuffer*>(cmd->count_buffer.buffer);

        require_buffer_state(args_buffer, EResourceStates::indirect_command_read);
        if (count_buffer) {
            require_buffer_state(count_buffer, EResourceStates::indirect_command_read);
        }
        commit_barriers();

        if (count_buffer) {
            vkCmdDrawIndexedIndirectCount(
                command_buffer,
                args_buffer->buffer,
                cmd->args_buffer.offset,
                count_buffer->buffer,
                cmd->count_buffer.offset,
                cmd->draw_count,
                sizeof(VkDrawIndexedIndirectCommand)
            );
        } else {
            vkCmdDrawIndexedIndirect(
                command_buffer,
                args_buffer->buffer,
                cmd->args_buffer.offset,
                cmd->draw_count,
                sizeof(VkDrawIndexedIndirectCommand)
            );
        }
    }

    auto CommandRecorder::record_dispatch_mesh(commands::dispatch_mesh const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        vkCmdDrawMeshTasksEXT(command_buffer, cmd->group_count_x, cmd->group_count_y, cmd->group_count_z);
    }

    auto CommandRecorder::record_dispatch_mesh_indirect(commands::dispatch_mesh_indirect const* cmd) -> void
    {
        if (!graphics_pass_active) return;

        auto args_buffer = cast<VulkanBuffer*>(cmd->args_buffer.buffer);

        vkCmdDrawMeshTasksIndirectEXT(
            command_buffer,
            args_buffer->buffer,
            cmd->args_buffer.offset,
            1,
            sizeof(VkDrawMeshTasksIndirectCommandEXT)
        );
    }

    auto CommandRecorder::record_begin_compute_pass(commands::begin_compute_pass const* cmd) -> void
    {
        compute_pass_active = true;
    }

    auto CommandRecorder::record_end_compute_pass(commands::end_compute_pass const* cmd) -> void
    {
        compute_pass_active = false;
    }

    auto CommandRecorder::record_set_compute_state(commands::set_compute_state const* cmd) -> void
    {
        if (!compute_pass_active) return;

        if (auto pipeline = cast<VulkanComputePipeline*>(cmd->pipeline); compute_pipeline != pipeline) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->pipeline);

            compute_pipeline = pipeline;

            // Bind bindless descriptor sets here.
            auto binding_infos = std::vector<VkDescriptorBufferBindingInfoEXT>{};
            binding_infos.emplace_back(
                VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                nullptr,
                device->bindless_manager->resource_heap->descriptor_buffer_address,
                VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
            );
            binding_infos.emplace_back(
                VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                nullptr,
                device->bindless_manager->sampler_heap->descriptor_buffer_address,
                VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
            );
            vkCmdBindDescriptorBuffersEXT(
                command_buffer,
                2,
                binding_infos.data()
            );

            auto buffer_index = std::vector<uint32_t>{0u, 1u};
            auto buffer_offset = std::vector<VkDeviceSize>{0, 0};
            vkCmdSetDescriptorBufferOffsetsEXT(
                command_buffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline->pipeline_layout,
                0,
                2,
                buffer_index.data(),
                buffer_offset.data()
            );
        }

        // if (!cmd->push_constants.empty()) {
        //     vkCmdPushConstants(
        //         command_buffer,
        //         compute_pipeline->pipeline_layout,
        //         VK_SHADER_STAGE_ALL,
        //         0,
        //         cmd->push_constants.size(),
        //         cmd->push_constants.data()
        //     );
        // }

        auto binding_data_dirty = !compute_state_valid || cmd->binding_data != binding_data;
        if (binding_data_dirty) {
            binding_data = static_cast<VulkanBindingData*>(cmd->binding_data);
            require_binding_states(binding_data);
            commit_barriers();
            set_bindings(binding_data, VK_PIPELINE_BIND_POINT_COMPUTE);
        }

        compute_state_valid = true;

        graphics_state_valid = false;
        graphics_pipeline = {};
    }

    auto CommandRecorder::record_dispatch_compute(commands::dispatch_compute const* cmd) -> void
    {
        if (!compute_pass_active) return;

        vkCmdDispatch(command_buffer, cmd->group_count_x, cmd->group_count_y, cmd->group_count_z);
    }

    auto CommandRecorder::record_dispatch_compute_indirect(commands::dispatch_compute_indirect const* cmd) -> void
    {
        if (!compute_pass_active) return;

        auto args_buffer = cast<VulkanBuffer*>(cmd->args_buffer.buffer);

        require_buffer_state(args_buffer, EResourceStates::indirect_command_read);
        commit_barriers();

        vkCmdDispatchIndirect(
            command_buffer,
            args_buffer->buffer,
            cmd->args_buffer.offset
        );
    }

    auto CommandRecorder::record_set_buffer_state(commands::set_buffer_state const* cmd) -> void
    {
        resource_state_tracker.set_buffer_state(cmd->buffer, cmd->state);
    }

    auto CommandRecorder::record_set_texture_state(commands::set_texture_state const* cmd) -> void
    {
        resource_state_tracker.set_texture_state(cmd->texture, cmd->subresources, cmd->state);
    }

    auto CommandRecorder::record_push_command_label(commands::push_command_label const* cmd) -> void
    {
        auto debug_label = VkDebugUtilsLabelEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        debug_label.pLabelName = cmd->name.data();
        debug_label.color[0] = cmd->color.x;
        debug_label.color[1] = cmd->color.y;
        debug_label.color[2] = cmd->color.z;
        debug_label.color[3] = cmd->color.w;

        vkCmdBeginDebugUtilsLabelEXT(command_buffer, &debug_label);
    }

    auto CommandRecorder::record_pop_command_label(commands::pop_command_label const* cmd) -> void
    {
        vkCmdEndDebugUtilsLabelEXT(command_buffer);
    }

    auto CommandRecorder::record_insert_debug_marker(commands::insert_debug_marker const* cmd) -> void
    {
        auto debug_label = VkDebugUtilsLabelEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        debug_label.pLabelName = cmd->name.data();
        debug_label.color[0] = cmd->color.x;
        debug_label.color[1] = cmd->color.y;
        debug_label.color[2] = cmd->color.z;
        debug_label.color[3] = cmd->color.w;

        vkCmdInsertDebugUtilsLabelEXT(command_buffer, &debug_label);
    }

    auto CommandRecorder::record_write_timestamp(commands::write_timestamp const* cmd) -> void
    {
        auto query = cast<VulkanTimerQuery*>(cmd->query);

        vkCmdResetQueryPool(command_buffer, query->pool->query_pool, cmd->query_index, 1);
        vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query->pool->query_pool, cmd->query_index);
    }

    auto CommandRecorder::record_insert_global_barrier(commands::insert_global_barrier const*) -> void
    {
        auto memoryBarrier = VkMemoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VkDependencyFlags{0},
            1,
            &memoryBarrier,
            0,
            nullptr,
            0,
            nullptr
        );
    }

    auto CommandRecorder::insert_barriers_for_graphics_state(CommandList* command_list, CommandList::CommandSlot const* begin_graphics_pass_slot) -> void
    {
        auto graphics_state_valid = false;
        auto graphics_state = GraphicsState{};
        auto binding_data = (VulkanBindingData*) nullptr;
        for (auto slot = begin_graphics_pass_slot; slot; slot = slot->next) {
            if (slot->id == CommandID::set_graphics_state) {
                auto const* cmd = command_list->get_command<commands::set_graphics_state>(slot);
                auto state = &cmd->state;

                auto binding_data_dirty = !graphics_state_valid || cmd->binding_data != binding_data;
                auto vertex_buffers_dirty = !graphics_state_valid || !range_equal(graphics_state.vertex_buffers, state->vertex_buffers);
                auto index_buffer_dirty = !graphics_state_valid || graphics_state.index_buffer != state->index_buffer || graphics_state.index_type != state->index_type;

                // TODO:
                if (binding_data_dirty) {
                    // binding_data = static_cast<VulkanBindingData*>(cmd->binding_data);
                    // require_binding_states(binding_data);
                }

                if (vertex_buffers_dirty) {
                    for (auto& vertex_buffer: state->vertex_buffers) {
                        auto buffer = cast<VulkanBuffer*>(vertex_buffer.buffer);
                        require_buffer_state(buffer, EResourceStates::vertex_attribute_read);
                    }
                }

                if (index_buffer_dirty) {
                    if (state->index_buffer.buffer) {
                        auto buffer = cast<VulkanBuffer*>(state->index_buffer.buffer);
                        require_buffer_state(buffer, EResourceStates::index_read);
                    }
                }

                graphics_state_valid = true;
                graphics_state = cmd->state;
            }
        }
        // Barrier will be commit before begin rendering, so we don't need to commit here.
    }

    auto CommandRecorder::require_binding_states(VulkanBindingData* binding_data) -> void
    {
        for (auto& buffer_state: binding_data->buffer_states) {
            require_buffer_state(buffer_state.buffer, buffer_state.state);
        }
        for (auto& texture_state: binding_data->texture_states) {
            require_texture_state(
                texture_state.texture_view->texture_,
                texture_state.texture_view->range_,
                texture_state.state
            );
        }
    }

    auto CommandRecorder::set_bindings(VulkanBindingData* binding_data, VkPipelineBindPoint bind_point) -> void
    {
        for (auto i = 0u; i < binding_data->ranges.size(); i++) {
            vkCmdPushConstants(
                command_buffer,
                binding_data->pipeline_layout,
                binding_data->ranges[i].stageFlags,
                binding_data->ranges[i].offset,
                binding_data->ranges[i].size,
                binding_data->push_constant_datas[i].data()
            );
        }
    }

    auto CommandRecorder::commit_barriers() -> void
    {
        CNE_ASSERT(!graphics_pass_active);

        auto buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
        auto image_barriers = std::vector<VkImageMemoryBarrier2>{};

        buffer_barriers.reserve(resource_state_tracker.buffer_barriers.size());
        image_barriers.reserve(resource_state_tracker.texture_barriers.size());

        std::ranges::transform(
            resource_state_tracker.buffer_barriers,
            std::back_inserter(buffer_barriers),
            [&](auto& barrier) -> VkBufferMemoryBarrier2 {
                auto buffer = cast<VulkanBuffer*>(barrier.buffer);

                return VkBufferMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask        = to_vk_pipeline_stage(barrier.src_state), // TODO: improve.
                    .srcAccessMask       = to_vk_access_type(barrier.src_state),
                    .dstStageMask        = to_vk_pipeline_stage(barrier.dst_state),
                    .dstAccessMask       = to_vk_access_type(barrier.dst_state),
                    .buffer              = buffer->buffer,
                    .offset              = 0,
                    .size                = buffer->info.size_bytes,
                };
            }
        );

        std::ranges::transform(
            resource_state_tracker.texture_barriers,
            std::back_inserter(image_barriers),
            [&](auto& barrier) -> VkImageMemoryBarrier2 {
                auto texture = cast<VulkanTexture*>(barrier.texture);
                auto view = texture->subresource_view(barrier.subresources);

                // CNE_TRACE("Texture Barrier: {} from {} to {}", (void*) texture->image, to_string(barrier.src_state), to_string(barrier.dst_state));

                auto src_image_layout = view->image_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED : image_layout_from_access(barrier.src_state, is_depth_stencil_format(texture->format));
                auto dst_image_layout = image_layout_from_access(barrier.dst_state, is_depth_stencil_format(texture->format));
                view->image_layout = dst_image_layout;
                CNE_ASSERT(dst_image_layout != VK_IMAGE_LAYOUT_UNDEFINED && !(src_image_layout == VK_IMAGE_LAYOUT_UNDEFINED && dst_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

                auto image_barrier = VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask        = to_vk_pipeline_stage(barrier.src_state), // TODO: improve.
                    .srcAccessMask       = to_vk_access_type(barrier.src_state),
                    .dstStageMask        = to_vk_pipeline_stage(barrier.dst_state),
                    .dstAccessMask       = to_vk_access_type(barrier.dst_state),
                    .oldLayout           = src_image_layout,
                    .newLayout           = dst_image_layout,
                    .image               = texture->image,
                    .subresourceRange    = {
                        .aspectMask     = aspect_flag_from_format(texture->format),
                        .baseMipLevel   = barrier.whole_texture ? 0 : barrier.subresources.mip_level,
                        .levelCount     = barrier.whole_texture ? texture->info.mip_count : barrier.subresources.mip_count,
                        .baseArrayLayer = barrier.whole_texture ? 0 : barrier.subresources.layer,
                        .layerCount     = barrier.whole_texture ? texture->info.layer_count : barrier.subresources.layer_count,
                    },
                };

                return image_barrier;
            }
        );

        auto dependency_info = VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency_info.bufferMemoryBarrierCount = buffer_barriers.size();
        dependency_info.pBufferMemoryBarriers    = buffer_barriers.data();
        dependency_info.imageMemoryBarrierCount  = image_barriers.size();
        dependency_info.pImageMemoryBarriers     = image_barriers.data();

        // for (auto i = 0u; i < image_barriers.size(); i++) {
        //     CNE_TRACE("Image Barrier: {}({}) from {} to {}",resource_state_tracker.texture_barriers[i].texture->name, (void*) image_barriers[i].image, to_string(image_barriers[i].srcStageMask), to_string(image_barriers[i].dstStageMask));
        // }

        vkCmdPipelineBarrier2(command_buffer, &dependency_info);

        resource_state_tracker.clear_barriers();
    }

    auto CommandRecorder::require_buffer_state(VulkanBuffer* buffer, EResourceStates state) -> void
    {
        resource_state_tracker.set_buffer_state(buffer, state);
    }

    auto CommandRecorder::require_texture_state(VulkanTexture* texture, TextureSubresourceRange const& subresources, EResourceStates state) -> void
    {
        if (texture->info.final_state != EResourceStates::present && texture->subresource_view(subresources)->image_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
            // TODO: transition layout when create it.
            // because of initial layout is undefined, we need to insert a barrier to make it layout correct.
            auto initial_state = texture->info.final_state;
            texture->info.final_state = EResourceStates::unknown;
            resource_state_tracker.set_texture_state(texture, subresources, state);
            texture->info.final_state = initial_state;
        } else {
            resource_state_tracker.set_texture_state(texture, subresources, state);
        }
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device, VulkanQueue* queue)
        : RHICommandBuffer(device)
        , queue(queue)
    {
        auto create_info = VkCommandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        create_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        create_info.queueFamilyIndex = queue->family_index;
        CHECK_VK_RESULT(vkCreateCommandPool(device->device, &create_info, device->allocation_callbacks, &command_pool));

        auto allocate_info = VkCommandBufferAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate_info.commandPool        = command_pool;
        allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        auto parent = get_device<VulkanDevice>();
        CHECK_VK_RESULT(vkAllocateCommandBuffers(parent->device, &allocate_info, &command_buffer));
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        auto parent = get_device<VulkanDevice>();
        vkDestroyCommandPool(parent->device, command_pool, parent->allocation_callbacks);
    }

    auto VulkanCommandBuffer::reset() -> void
    {
        // NOTE: Must reset command buffer or it will cause memory leak, implicit reset by begin seems some problems.
        // vkResetCommandBuffer(command_buffer, 0);
        auto parent = get_device<VulkanDevice>();
        CHECK_VK_RESULT(vkResetCommandPool(parent->device, command_pool, 0));
        RHICommandBuffer::reset();

        binding_cache.binding_data.clear();
    }

    auto VulkanDevice::create_command_encoder(EQueueType type) -> std::shared_ptr<CommandEncoder>
    {
        return queue(type)->create_command_encoder();
    }

    VulkanCommandEncoder::VulkanCommandEncoder(VulkanDevice* device, VulkanQueue* queue)
        : CommandEncoder(device)
        , queue(queue)
        , command_buffer(queue->allocate_command_buffer())
    {
        command_list = command_buffer->command_list.get();
    }

    VulkanCommandEncoder::~VulkanCommandEncoder()
    {
        if (command_buffer) {
            queue->free_command_buffer(command_buffer);
        }
    }

    auto VulkanCommandEncoder::finish() -> std::shared_ptr<RHICommandBuffer>
    {
        // Actually record the command to vkCommandBuffer.
        auto recorder = CommandRecorder{get_device<VulkanDevice>()};
        recorder.record(command_buffer.get());
        command_list = nullptr;

        return std::move(command_buffer);
    }

    auto VulkanCommandEncoder::binding_data(RootShaderObject* root_object) -> BindingData*
    {
        root_object->track_resources(&command_buffer->tracked_resources);

        auto builder = BindingDataBuilder{};
        builder.device = get_device<VulkanDevice>();
        builder.allocator = command_buffer->arena.get();
        builder.binding_cache = &command_buffer->binding_cache;

        auto specialized_layout = root_object->specialized_layout();
        return builder.bind_as_root(root_object, cast<VulkanRootShaderObjectLayout*>(specialized_layout));
    }

//     VulkanCommandBufferDeprecated::VulkanCommandBufferDeprecated(VulkanDevice* device, uint32_t queue_family_index)
//         : parent(device)
//     {
//         auto pool_ci = VkCommandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
//         pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
//         pool_ci.queueFamilyIndex = queue_family_index;
//
//         auto result_pool_create = vkCreateCommandPool(parent->device, &pool_ci, nullptr, &command_pool);
//         CNE_ASSERT_WITH(result_pool_create == VK_SUCCESS, std::format("Failed to create command pool: {}", vk_error_to_string(result_pool_create)));
//
//         auto cmd_buffer_alloi = VkCommandBufferAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
//         cmd_buffer_alloi.commandPool        = command_pool;
//         cmd_buffer_alloi.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
//         cmd_buffer_alloi.commandBufferCount = 1;
//
//         auto result_cmd_create = vkAllocateCommandBuffers(parent->device, &cmd_buffer_alloi, &command_buffer);
//         CNE_ASSERT_WITH(result_cmd_create == VK_SUCCESS, std::format("Failed to allocate command buffer: {}", vk_error_to_string(result_cmd_create)));
//     }
//
//     VulkanCommandBufferDeprecated::~VulkanCommandBufferDeprecated()
//     {
//         vkDestroyCommandPool(parent->device, command_pool, parent->allocation_callbacks);
//
//         referenced_resources.clear();
//         referenced_sataging_buffers.clear();
//     }
//
//     auto VulkanCommandBufferDeprecated::add_reference(std::shared_ptr<IResource> resource) -> void
//     {
//         referenced_resources.emplace_back(resource);
//     }
//
//     auto VulkanCommandBufferDeprecated::add_reference_sataging_buffer(std::shared_ptr<VulkanBuffer> buffer) -> void
//     {
//         referenced_sataging_buffers.emplace_back(buffer);
//     }
//
//     auto VulkanCommandBufferDeprecated::reset() -> void
//     {
//         // NOTE: Must reset command buffer or it will cause memory leak, implicit reset by begin seems some problems.
//         // vkResetCommandBuffer(command_buffer, 0);
//         vkResetCommandPool(parent->device, command_pool, 0);
//     }
//
//     auto VulkanCommandBufferDeprecated::clear_references() -> void
//     {
//         referenced_resources.clear();
//         referenced_sataging_buffers.clear();
//     }
//
//     VulkanCommandList::VulkanCommandList(VulkanDevice* device, CommandListCreateInfo const* info)
//         : RHICommandList(device)
//         , info(*info)
//         , block_pool(device->queue(info->queue_type)->buffer_block.get())
//     {
//         resource_state_tracker.buffer_barriers.reserve(16);
//         resource_state_tracker.texture_barriers.reserve(16);
//     }
//
//     VulkanCommandList::~VulkanCommandList()
//     {
//
//     }
//
//     auto VulkanCommandList::start() -> void
//     {
//         auto parent = get_device<VulkanDevice>();
//         active_command_buffer = parent->queue(info.queue_type)->allocate_command_buffer();
//         active_command_buffer->reset();
//
//         auto begin_info = VkCommandBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
//         begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
//
//         vkBeginCommandBuffer(active_command_buffer->command_buffer, &begin_info);
//
//         reset();
//     }
//
//     auto VulkanCommandList::finish() -> void
//     {
//         end_rendering();
//
//         resource_state_tracker.keep_initial_state();
//         commit_barriers();
//
//         vkEndCommandBuffer(active_command_buffer->command_buffer);
//
//         reset();
//     }
//
//     auto VulkanCommandList::reset() -> void
//     {
//         end_rendering();
//
//         current_pipeline_layout = VK_NULL_HANDLE;
//         current_push_constant_visibility = {};
//
//         current_graphics_state = {};
//         current_mesh_state = {};
//         current_compute_state = {};
//     }
//
//     // Clear Operations
//     auto VulkanCommandList::clear_buffer_uint(BufferHandle buffer, uint32_t clear_value) -> void
//     {
//         end_rendering();
//
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, EResourceStates::transfer_dst, EPipelineStage::clear);
//         }
//         commit_barriers();
//
//         vkCmdFillBuffer(active_command_buffer->command_buffer, vulkan_buffer->buffer, 0, vulkan_buffer->info.size_bytes, clear_value);
//     }
//
//     auto VulkanCommandList::clear_texture_float(TextureHandle texture, TextureSubresourceRange subresources, math::float4 clear_color) -> void
//     {
//         auto clear_color_value = VkClearColorValue{.float32 = {clear_color.x, clear_color.y, clear_color.z, clear_color.w}};
//
//         clear_texture(texture, subresources, &clear_color_value);
//     }
//
//     auto VulkanCommandList::clear_texture_uint(TextureHandle texture, TextureSubresourceRange subresources, uint32_t clear_color) -> void
//     {
//         auto clear_color_value = VkClearColorValue{.uint32 = {clear_color, clear_color, clear_color, clear_color}};
//
//         clear_texture(texture, subresources, &clear_color_value);
//     }
//
//     auto VulkanCommandList::clear_texture(TextureHandle texture, TextureSubresourceRange subresources, VkClearColorValue* clear_color) -> void
//     {
//         end_rendering();
//
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         adapt_to_texture(&subresources, &vulkan_texture->info, false);
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_texture_state(
//                 &vulkan_texture->tracker,
//                 subresources,
//                 EResourceStates::transfer_dst,
//                 EPipelineStage::clear
//             );
//         }
//         commit_barriers();
//
//         auto image_subresource_range = VkImageSubresourceRange{
//             .aspectMask     = aspect_flag_from_format(to_vk_format(vulkan_texture->info.format)),
//             .baseMipLevel   = subresources.mip_level,
//             .levelCount     = subresources.mip_count,
//             .baseArrayLayer = subresources.layer,
//             .layerCount     = subresources.layer_count
//         };
//
//         vkCmdClearColorImage(
//             active_command_buffer->command_buffer,
//             vulkan_texture->image,
//             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             clear_color,
//             1,
//             &image_subresource_range
//         );
//     }
//
//     auto VulkanCommandList::clear_depth_stencil(TextureHandle texture, TextureSubresourceRange subresources, std::optional<float> clear_depth, std::optional<uint8_t> clear_stencil) -> void
//     {
//         end_rendering();
//
//         if (!clear_depth && !clear_stencil) return;
//
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         adapt_to_texture(&subresources, &vulkan_texture->info, false);
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_texture_state(
//                 &vulkan_texture->tracker,
//                 subresources,
//                 EResourceStates::transfer_dst,
//                 EPipelineStage::clear
//             );
//         }
//         commit_barriers();
//
//         auto aspect_flags = VkImageAspectFlags{};
//
//         if (clear_depth) {
//             aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
//         }
//
//         if (clear_stencil) {
//             aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
//         }
//
//         auto subresource_range = VkImageSubresourceRange{
//             .aspectMask     = aspect_flags,
//             .baseMipLevel   = subresources.mip_level,
//             .levelCount     = subresources.mip_count,
//             .baseArrayLayer = subresources.layer,
//             .layerCount     = subresources.layer_count,
//         };
//
//         auto clear_value = VkClearDepthStencilValue{
//             .depth   = clear_depth.value_or(0.0f),
//             .stencil = clear_stencil.value_or(0)
//         };
//
//         vkCmdClearDepthStencilImage(
//             active_command_buffer->command_buffer,
//             vulkan_texture->image,
//             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             &clear_value,
//             1,
//             &subresource_range
//         );
//     }
//
//     // Resource Transfers
//     auto VulkanCommandList::copy_buffer(BufferHandle src_buffer, size_t src_offset_bytes, BufferHandle dst_buffer, size_t dst_offset_bytes, size_t data_size_bytes) -> void
//     {
//         auto src_vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(src_buffer);
//         auto dst_vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(dst_buffer);
//
//         CNE_ASSERT(src_offset_bytes + data_size_bytes <= src_vulkan_buffer->info.size_bytes);
//         CNE_ASSERT(dst_offset_bytes + data_size_bytes <= dst_vulkan_buffer->info.size_bytes);
//
//         if (src_vulkan_buffer->info.memory_type == MemoryType::gpu_only) {
//             active_command_buffer->add_reference(src_buffer);
//         } else {
//             active_command_buffer->add_reference_sataging_buffer(src_vulkan_buffer);
//         }
//
//         if (dst_vulkan_buffer->info.memory_type == MemoryType::gpu_only) {
//             active_command_buffer->add_reference(dst_buffer);
//         } else {
//             active_command_buffer->add_reference_sataging_buffer(dst_vulkan_buffer);
//         }
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_buffer_state(&src_vulkan_buffer->tracker, EResourceStates::transfer_src, EPipelineStage::copy);
//             resource_state_tracker.require_buffer_state(&dst_vulkan_buffer->tracker, EResourceStates::transfer_dst, EPipelineStage::copy);
//         }
//         commit_barriers();
//
//         auto copy_region = VkBufferCopy2{VK_STRUCTURE_TYPE_BUFFER_COPY_2};
//         copy_region.size      = data_size_bytes;
//         copy_region.srcOffset = src_offset_bytes;
//         copy_region.dstOffset = dst_offset_bytes;
//
//         auto copy_info = VkCopyBufferInfo2{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
//         copy_info.srcBuffer   = src_vulkan_buffer->buffer;
//         copy_info.dstBuffer   = dst_vulkan_buffer->buffer;
//         copy_info.regionCount = 1;
//         copy_info.pRegions    = &copy_region;
//
//         vkCmdCopyBuffer2(active_command_buffer->command_buffer, &copy_info);
//     }
//
//     auto VulkanCommandList::copy_texture(TextureHandle src_texture, TextureSlice src_slice, TextureHandle dst_texture, TextureSlice dst_slice) -> void
//     {
//         auto src_vulkan_texture = assert_ref_count_cast<VulkanTexture>(src_texture);
//         auto dst_vulkan_texture = assert_ref_count_cast<VulkanTexture>(dst_texture);
//
//         active_command_buffer->add_reference(src_texture);
//         active_command_buffer->add_reference(dst_texture);
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_texture_state(
//                 &src_vulkan_texture->tracker,
//                 TextureSubresourceRange{src_slice.mip_level, 1, src_slice.layer, 1},
//                 EResourceStates::transfer_src,
//                 EPipelineStage::copy
//             );
//             resource_state_tracker.require_texture_state(
//                 &dst_vulkan_texture->tracker,
//                 TextureSubresourceRange{dst_slice.mip_level, 1, dst_slice.layer, 1},
//                 EResourceStates::transfer_dst,
//                 EPipelineStage::copy
//             );
//         }
//         commit_barriers();
//
//         auto src_subresource = VkImageSubresourceLayers{
//             .aspectMask     = aspect_flag_from_format(to_vk_format(src_vulkan_texture->info.format)),
//             .mipLevel       = src_slice.mip_level,
//             .baseArrayLayer = src_slice.layer,
//             .layerCount     = 1
//         };
//
//         auto dst_subresource = VkImageSubresourceLayers{
//             .aspectMask     = aspect_flag_from_format(to_vk_format(dst_vulkan_texture->info.format)),
//             .mipLevel       = dst_slice.mip_level,
//             .baseArrayLayer = dst_slice.layer,
//             .layerCount     = 1
//         };
//
//         auto extent = VkExtent3D{
//             std::min(src_slice.extent.x, dst_slice.extent.x),
//             std::min(src_slice.extent.y, dst_slice.extent.y),
//             std::min(src_slice.extent.z, dst_slice.extent.z)
//         };
//
//         auto copy_region = VkImageCopy2{VK_STRUCTURE_TYPE_IMAGE_COPY_2};
//         copy_region.srcSubresource = src_subresource;
//         copy_region.srcOffset      = VkOffset3D{(int) src_slice.origin.x, (int) src_slice.origin.y, (int) src_slice.origin.z};
//         copy_region.dstSubresource = dst_subresource;
//         copy_region.dstOffset      = VkOffset3D{(int) dst_slice.origin.x, (int) dst_slice.origin.y, (int) dst_slice.origin.z};
//         copy_region.extent         = extent;
//
//         auto copy_image_info = VkCopyImageInfo2{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};
//         copy_image_info.srcImage       = src_vulkan_texture->image;
//         copy_image_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//         copy_image_info.dstImage       = dst_vulkan_texture->image;
//         copy_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         copy_image_info.regionCount    = 1;
//         copy_image_info.pRegions       = &copy_region;
//
//         vkCmdCopyImage2(active_command_buffer->command_buffer, &copy_image_info);
//     }
//
//     auto VulkanCommandList::write_buffer(BufferHandle buffer, std::span<std::byte> data, size_t offset_bytes) -> void
//     {
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         CNE_ASSERT(offset_bytes + data.size() <= vulkan_buffer->info.size_bytes);
//
//         if (vulkan_buffer->info.type == MemoryType::cpu_write) {
//             std::memcpy(vulkan_buffer->map<std::byte>() + offset_bytes, data.data(), data.size());
//             vulkan_buffer->unmap();
//
//             return;
//         }
//
//         end_rendering();
//
//         constexpr auto upload_buffer_limit = 65536;
//
//         if (data.size() <= upload_buffer_limit && (offset_bytes & 3) == 0) {
//             active_command_buffer->add_reference(buffer);
//
//             if (automatic_barriers) {
//                 resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, EResourceStates::transfer_dst);
//             }
//             commit_barriers();
//
//             auto size_to_write = (data.size() + 3) & ~3ull;
//
//             vkCmdUpdateBuffer(active_command_buffer->command_buffer, vulkan_buffer->buffer, offset_bytes, size_to_write, data.data());
//         } else {
//             auto sub_buffer_block = block_pool->suballocate_buffer(data.size(), make_version(active_command_buffer->recording_time, this->info.queue_type, false));
//             auto staging_buffer = assert_ref_count_cast<VulkanBuffer>(sub_buffer_block.buffer);
//             auto mapped_ptr = staging_buffer->map<std::byte>() + sub_buffer_block.range.offset;
//             std::memcpy(mapped_ptr, data.data(), data.size());
//             staging_buffer->unmap();
//
//             if (!automatic_barriers) {
//                 // If not set automatically, we need to insert barrier for staging buffer here.
//                 resource_state_tracker.require_buffer_state(&staging_buffer->tracker, EResourceStates::transfer_src);
//             }
//
//             copy_buffer(
//                 sub_buffer_block.buffer,
//                 sub_buffer_block.range.offset,
//                 buffer,
//                 offset_bytes,
//                 data.size()
//             );
//         }
//     }
//
//     auto VulkanCommandList::write_texture(TextureHandle texture, uint32_t level, uint32_t layer, TextureSliceDataView data) -> void
//     {
//         end_rendering();
//
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//         auto info = vulkan_texture->info;
//
//         active_command_buffer->add_reference(texture);
//
//         if (automatic_barriers) {
//             resource_state_tracker.require_texture_state(
//                 &vulkan_texture->tracker,
//                 TextureSubresourceRange{level, 1, layer, 1},
//                 EResourceStates::transfer_dst
//             );
//         }
//         commit_barriers();
//
//         auto mip_width = std::max(info.extent.width >> level, 1u);
//         auto mip_height = std::max(info.extent.height >> level, 1u);
//         auto mip_depth = std::max(info.extent.depth >> level, 1u);
//
//         auto format_info = get_format_info(info.format);
//         auto num_cols    = (mip_width + format_info->blocks - 1) / format_info->blocks;
//         auto num_rows    = (mip_height + format_info->blocks - 1) / format_info->blocks;
//         auto row_bytes   = (size_t) num_cols * format_info->bytes_per_block;
//         auto memory_size = (size_t) row_bytes * num_rows * mip_depth;
//
//         auto sub_buffer_block = block_pool->suballocate_buffer(
//             memory_size,
//             make_version(active_command_buffer->recording_time, this->info.queue_type, false)
//         );
//         auto staging_buffer = assert_ref_count_cast<VulkanBuffer>(sub_buffer_block.buffer);
//         active_command_buffer->add_reference(staging_buffer);
//
//         auto min_row_bytes = std::min(row_bytes, (size_t) data.extent(0));
//         auto mapped_ptr = staging_buffer->map<std::byte>() + sub_buffer_block.range.offset;
//         for (auto layer = 0; layer < data.extent(2); layer++) {
//             auto source_ptr = data.data_handle() + layer * data.extent(0) * data.extent(1);
//             for (auto row = 0; row < data.extent(1); row++) {
//                 std::memcpy(
//                     mapped_ptr + row * min_row_bytes,
//                     source_ptr + row * data.extent(0),
//                     min_row_bytes
//                 );
//             }
//         }
//         staging_buffer->unmap();
//
//         auto buffer_image_copy = VkBufferImageCopy2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
//         buffer_image_copy.bufferOffset                    = 0;
//         buffer_image_copy.bufferRowLength                 = num_cols * format_info->blocks;
//         buffer_image_copy.bufferImageHeight               = num_rows * format_info->blocks;
//         buffer_image_copy.imageSubresource.aspectMask     = aspect_flag_from_format(to_vk_format(info.format));
//         buffer_image_copy.imageSubresource.mipLevel       = level;
//         buffer_image_copy.imageSubresource.baseArrayLayer = layer;
//         buffer_image_copy.imageSubresource.layerCount     = 1;
//         buffer_image_copy.imageOffset                     = {0, 0, 0};
//         buffer_image_copy.imageExtent                     = {mip_width, mip_height, mip_depth};
//
//         auto buffer_image_info = VkCopyBufferToImageInfo2{VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
//         buffer_image_info.srcBuffer      = staging_buffer->buffer;
//         buffer_image_info.dstImage       = vulkan_texture->image;
//         buffer_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         buffer_image_info.regionCount    = 1;
//         buffer_image_info.pRegions       = &buffer_image_copy;
//
//         vkCmdCopyBufferToImage2(active_command_buffer->command_buffer, &buffer_image_info);
//     }
//
//     // Compute Operations
//     auto VulkanCommandList::push_constants(void const* data, size_t size_bytes) -> void
//     {
//         vkCmdPushConstants(
//             active_command_buffer->command_buffer,
//             current_pipeline_layout,
//             current_push_constant_visibility,
//             0,
//             size_bytes,
//             data
//         );
//     }
//
//     auto VulkanCommandList::set_compute_state(ComputeState* state) -> void
//     {
//         end_rendering();
//
//         auto parent = get_device<VulkanDevice>();
//         if (current_compute_state.pipeline != state->pipeline) {
//             auto vulkan_pipeline = assert_ref_count_cast<VulkanComputePipeline>(state->pipeline);
//             vkCmdBindPipeline(active_command_buffer->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vulkan_pipeline->pipeline);
//
//             // Bind bindless descriptor sets here.
//             auto binding_infos = std::vector<VkDescriptorBufferBindingInfoEXT>{};
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->resource_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->sampler_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             vkCmdBindDescriptorBuffersEXT(
//                 active_command_buffer->command_buffer,
//                 2,
//                 binding_infos.data()
//             );
//
//             auto buffer_index = std::vector<uint32_t>{0u, 1u};
//             auto buffer_offset = std::vector<VkDeviceSize>{0, 0};
//             vkCmdSetDescriptorBufferOffsetsEXT(
//                 active_command_buffer->command_buffer,
//                 VK_PIPELINE_BIND_POINT_COMPUTE,
//                 vulkan_pipeline->pipeline_layout,
//                 0,
//                 2,
//                 buffer_index.data(),
//                 buffer_offset.data()
//             );
//
//             active_command_buffer->add_reference(state->pipeline);
//             current_pipeline_layout = vulkan_pipeline->pipeline_layout;
//             current_push_constant_visibility = VK_SHADER_STAGE_COMPUTE_BIT;
//         }
//
//         if (auto indirect_buffer = state->indirect_buffer) {
//             active_command_buffer->add_reference(indirect_buffer);
//
//             auto vk_indirect_buffer = assert_ref_count_cast<VulkanBuffer>(state->indirect_buffer);
//             if (automatic_barriers) {
//                 resource_state_tracker.require_buffer_state(
//                     &vk_indirect_buffer->tracker,
//                     EResourceStates::indirect_command_read,
//                     EPipelineStage::draw_indirect
//                 );
//             }
//         }
//         commit_barriers();
//
//         current_compute_state = *state;
//         current_graphics_state = {};
//         current_mesh_state = {};
//     }
//
//     auto VulkanCommandList::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
//     {
//         vkCmdDispatch(active_command_buffer->command_buffer, group_count_x, group_count_y, group_count_z);
//     }
//
//     auto VulkanCommandList::dispatch_indirect(uint32_t offset) -> void
//     {
//         auto vk_indirect_buffer = assert_ref_count_cast<VulkanBuffer>(current_compute_state.indirect_buffer);
//
//         vkCmdDispatchIndirect(active_command_buffer->command_buffer, vk_indirect_buffer->buffer, offset);
//     }
//
//     // Graphics Operations
//     auto VulkanCommandList::set_graphics_state(GraphicsState* state) -> void
//     {
//         auto parent = get_device<VulkanDevice>();
//         auto pipeline_need_update = false;
//         if (current_graphics_state.pipeline != state->pipeline) {
//             auto vulkan_pipeline = assert_ref_count_cast<VulkanGraphicsPipeline>(state->pipeline);
//             vkCmdBindPipeline(active_command_buffer->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_pipeline->pipeline);
//
//             // Bind bindless descriptor sets here.
//             auto binding_infos = std::vector<VkDescriptorBufferBindingInfoEXT>{};
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->resource_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->sampler_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             vkCmdBindDescriptorBuffersEXT(
//                 active_command_buffer->command_buffer,
//                 2,
//                 binding_infos.data()
//             );
//
//             auto buffer_index = std::vector<uint32_t>{0u, 1u};
//             auto buffer_offset = std::vector<VkDeviceSize>{0, 0};
//             vkCmdSetDescriptorBufferOffsetsEXT(
//                 active_command_buffer->command_buffer,
//                 VK_PIPELINE_BIND_POINT_GRAPHICS,
//                 vulkan_pipeline->pipeline_layout,
//                 0,
//                 2,
//                 buffer_index.data(),
//                 buffer_offset.data()
//             );
//
//             active_command_buffer->add_reference(state->pipeline);
//             current_pipeline_layout = vulkan_pipeline->pipeline_layout;
//             current_push_constant_visibility = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
//             pipeline_need_update = true;
//         }
//
//         if (resource_state_tracker.has_barrier() || current_graphics_state.render_target != state->render_target) {
//             end_rendering();
//         }
//
//         if(auto render_target = state->render_target) {
//             std::vector<VkRenderingAttachmentInfo> color_attachments{};
//             color_attachments.reserve(render_target->color_attachments.size());
//             std::ranges::transform(
//                 render_target->color_attachments,
//                 std::back_inserter(color_attachments),
//                 [&](auto const& attachment) -> VkRenderingAttachmentInfo {
//                     active_command_buffer->add_reference(attachment.texture);
//
//                     auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(attachment.texture);
//
//                     if (automatic_barriers) {
//                         resource_state_tracker.require_texture_state(
//                             &vulkan_texture->tracker,
//                             TextureSubresourceRange{},
//                             EResourceStates::color_attachment
//                         );
//                     }
//
//                     auto clear_color = &attachment.clear_color;
//                     return VkRenderingAttachmentInfo{
//                         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//                         .imageView          = vulkan_texture->image_view(TextureSubresourceRange{}),
//                         .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                         .resolveMode        = VK_RESOLVE_MODE_NONE,
//                         .resolveImageView   = VK_NULL_HANDLE,
//                         .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//                         .loadOp             = to_vk_load_op(attachment.load),
//                         .storeOp            = to_vk_store_op(attachment.store),
//                         .clearValue         = {.color = {clear_color->x, clear_color->y, clear_color->z, clear_color->w}},
//                     };
//                 }
//             );
//
//             auto has_stencil = false;
//             auto vk_depth_stencil_attachment = VkRenderingAttachmentInfo{};
//             if (auto attachment = render_target->depth_stencil_attachment) {
//                 active_command_buffer->add_reference(attachment.texture);
//
//                 auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(attachment.texture);
//
//                 if (automatic_barriers) {
//                     resource_state_tracker.require_texture_state(
//                         &vulkan_texture->tracker,
//                         TextureSubresourceRange{},
//                         EResourceStates::depth_stencil_attachment // TODO: read only.
//                     );
//                 }
//
//                 has_stencil = !is_depth_only_format(to_vk_format(vulkan_texture->info.format));
//                 vk_depth_stencil_attachment = VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
//                 vk_depth_stencil_attachment.imageView          = vulkan_texture->image_view(TextureSubresourceSet{});
//                 vk_depth_stencil_attachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//                 vk_depth_stencil_attachment.resolveMode        = VK_RESOLVE_MODE_NONE;
//                 vk_depth_stencil_attachment.resolveImageView   = VK_NULL_HANDLE;
//                 vk_depth_stencil_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//                 vk_depth_stencil_attachment.loadOp             = to_vk_load_op(attachment.load);
//                 vk_depth_stencil_attachment.storeOp            = to_vk_store_op(attachment.store);
//                 vk_depth_stencil_attachment.clearValue         = VkClearValue{
//                     .depthStencil = VkClearDepthStencilValue{
//                         render_target->depth_stencil_attachment.clear_depth,
//                         render_target->depth_stencil_attachment.clear_stencil,
//                     }
//                 };
//             }
//
//             commit_barriers();
//
//             auto offset = VkOffset2D{render_target->info.offset.x, render_target->info.offset.y};
//             auto extent = VkExtent2D{render_target->info.extent.x, render_target->info.extent.y};
//             auto rendering_info = VkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
//             rendering_info.flags                = 0;
//             rendering_info.renderArea           = {offset, extent};
//             rendering_info.layerCount           = 1;
//             rendering_info.viewMask             = 0;
//             rendering_info.colorAttachmentCount = (uint32_t) color_attachments.size();
//             rendering_info.pColorAttachments    = color_attachments.data();
//             rendering_info.pDepthAttachment     = render_target->depth_stencil_attachment ? &vk_depth_stencil_attachment : nullptr;
//             rendering_info.pStencilAttachment   = has_stencil ? &vk_depth_stencil_attachment : nullptr;
//
//             CNE_ASSERT_WITH(!color_attachments.empty(), "Render target must have at least one color attachment.");
//
//             vkCmdBeginRendering(active_command_buffer->command_buffer, &rendering_info);
//
//             auto blend_states = &render_target->info.blend_states;
//             CNE_ASSERT_WITH(blend_states->size() == render_target->color_attachments.size(), "Blend states must match number of color attachments.");
//
//             std::vector<VkBool32> blend_enable{};
//             std::vector<VkColorBlendEquationEXT> color_blend_equation{};
//             std::vector<VkColorComponentFlags> color_component_flags{};
//             blend_enable.reserve(blend_states->size());
//             color_blend_equation.reserve(blend_states->size());
//             color_component_flags.reserve(blend_states->size());
//             for (auto& blend_state: *blend_states) {
//
//                 blend_enable.emplace_back(blend_state.enable_blend ? VK_TRUE : VK_FALSE);
//                 color_blend_equation.emplace_back(VkColorBlendEquationEXT{
//                     .srcColorBlendFactor = to_vk_blend_factor(blend_state.color_src_blend),
//                     .dstColorBlendFactor = to_vk_blend_factor(blend_state.color_dst_blend),
//                     .colorBlendOp        = to_vk_blend_op(blend_state.color_blend_op),
//                     .srcAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_src_blend),
//                     .dstAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_dst_blend),
//                     .alphaBlendOp        = to_vk_blend_op(blend_state.alpha_blend_op)
//                 });
//                 color_component_flags.emplace_back(
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::r) ? VK_COLOR_COMPONENT_R_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::g) ? VK_COLOR_COMPONENT_G_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::b) ? VK_COLOR_COMPONENT_B_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::a) ? VK_COLOR_COMPONENT_A_BIT : 0)
//                 );
//             }
//             vkCmdSetColorBlendEnableEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), blend_enable.data());
//             vkCmdSetColorBlendEquationEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_blend_equation.data());
//             vkCmdSetColorWriteMaskEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_component_flags.data());
//
//             // Set or clear dynamic settings:
//             vkCmdSetCullMode(active_command_buffer->command_buffer, VK_CULL_MODE_NONE);
//             vkCmdSetPolygonModeEXT(active_command_buffer->command_buffer, VK_POLYGON_MODE_FILL);
//             vkCmdSetRasterizationSamplesEXT(active_command_buffer->command_buffer, VK_SAMPLE_COUNT_1_BIT);
//             vkCmdSetFrontFace(active_command_buffer->command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
//
//             auto has_depth_stencil_attachment = (bool) render_target->depth_stencil_attachment;
//             auto enable_depth_test  = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_test ? VK_TRUE : VK_FALSE);
//             auto enable_depth_write = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_write ? VK_TRUE : VK_FALSE);
//             auto depth_compare_op   = !has_depth_stencil_attachment ? VK_COMPARE_OP_ALWAYS : to_vk_compare_op(render_target->info.depth_state.depth_compare);
//             vkCmdSetDepthTestEnable(active_command_buffer->command_buffer, enable_depth_test);
//             vkCmdSetDepthWriteEnable(active_command_buffer->command_buffer, enable_depth_write);
//             vkCmdSetDepthBoundsTestEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthBiasEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthClampEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthCompareOp(active_command_buffer->command_buffer, depth_compare_op);
//
//             vkCmdSetStencilTestEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetStencilOp(active_command_buffer->command_buffer,
//                 VK_STENCIL_FACE_FRONT_AND_BACK,
//                 VK_STENCIL_OP_KEEP,
//                 VK_STENCIL_OP_KEEP,
//                 VK_STENCIL_OP_KEEP,
//                 VK_COMPARE_OP_ALWAYS
//             );
//
//             vkCmdSetLogicOpEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetLogicOpEXT(active_command_buffer->command_buffer, VK_LOGIC_OP_NO_OP);
//         }
//
//         if (state->viewport_state) {
//             set_viewport_state(&state->viewport_state);
//         }
//
//         if (auto input_state = state->vertex_input_state) {
//             auto vk_bindings = std::vector<VkVertexInputBindingDescription2EXT>{};
//             auto vk_attributes = std::vector<VkVertexInputAttributeDescription2EXT>{};
//             for (auto& stream: input_state->streams) {
//                 auto binding = &vk_bindings.emplace_back(VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT);
//                 binding->binding   = stream.binding;
//                 binding->stride    = stream.stride;
//                 binding->inputRate = stream.input_rate == EVertexInputRate::vertex ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
//                 binding->divisor   = 1;
//
//                 for (auto& attribute: stream.attributes) {
//                     auto vk_attribute = &vk_attributes.emplace_back(VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT);
//                     vk_attribute->location = attribute.location;
//                     vk_attribute->binding  = stream.binding;
//                     vk_attribute->format   = to_vk_format(attribute.format);
//                     vk_attribute->offset   = attribute.offset_bytes;
//                 }
//             }
//
//             vkCmdSetVertexInputEXT(
//                 active_command_buffer->command_buffer,
//                 vk_bindings.size(),
//                 vk_bindings.data(),
//                 vk_attributes.size(),
//                 vk_attributes.data()
//             );
//         } else {
//             vkCmdSetVertexInputEXT(
//                 active_command_buffer->command_buffer,
//                 0,
//                 nullptr,
//                 0,
//                 nullptr
//             );
//         }
//
//         // TODO: other dynamic state.
//
//         if (auto& index_buffer_binding = state->index_buffer_binding; index_buffer_binding && index_buffer_binding != current_graphics_state.index_buffer_binding) {
//             active_command_buffer->add_reference(index_buffer_binding.buffer);
//
//             vkCmdBindIndexBuffer(
//                 active_command_buffer->command_buffer,
//                 assert_ref_count_cast<VulkanBuffer>(state->index_buffer_binding.buffer)->buffer,
//                 state->index_buffer_binding.offset_bytes,
//                 state->index_buffer_binding.format == EFormat::r16_uint ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32
//             );
//         }
//
//         if (!state->vertex_buffer_bindings.empty() && state->vertex_buffer_bindings != current_graphics_state.vertex_buffer_bindings) {
//             auto vk_vertex_buffers = std::vector<VkBuffer>{};
//             auto vk_vertex_offsets = std::vector<VkDeviceSize>{};
//
//             for (auto& binding: state->vertex_buffer_bindings) {
//                 vk_vertex_buffers.emplace_back(assert_ref_count_cast<VulkanBuffer>(binding.buffer)->buffer);
//                 vk_vertex_offsets.emplace_back(binding.offset_bytes);
//
//                 active_command_buffer->add_reference(binding.buffer);
//             }
//
//             vkCmdBindVertexBuffers(
//                 active_command_buffer->command_buffer,
//                 0,
//                 vk_vertex_buffers.size(),
//                 vk_vertex_buffers.data(),
//                 vk_vertex_offsets.data()
//             );
//         }
//
//         if (state->indirect_buffer) {
//             active_command_buffer->add_reference(state->indirect_buffer);
//         }
//
//         // TODO: Shading rate.
//
//         current_graphics_state = *state;
//         current_compute_state = {};
//         current_mesh_state = {};
//     }
//
//     auto VulkanCommandList::set_viewport_state(ViewportState* state) -> void
//     {
//         if (!state) return;
//
//         auto current_viewport_state = &current_graphics_state.viewport_state;
//         if (current_viewport_state->viewports != state->viewports && !state->viewports.empty()) {
//             auto vk_viewports = std::vector<VkViewport>{};
//             vk_viewports.reserve(state->viewports.size());
//             std::ranges::transform(
//                 state->viewports,
//                 std::back_inserter(vk_viewports),
//                 [](auto& viewport) -> VkViewport {
//                     return VkViewport{
//                         .x        = viewport.x,
//                         .y        = viewport.height - viewport.y,
//                         .width    = viewport.width,
//                         .height   = -viewport.height,
//                         .minDepth = viewport.min_depth,
//                         .maxDepth = viewport.max_depth
//                     };
//                 }
//             );
//
//             vkCmdSetViewportWithCount(active_command_buffer->command_buffer, vk_viewports.size(), vk_viewports.data());
//             current_viewport_state->viewports = state->viewports;
//         }
//         if (current_viewport_state->scissors != state->scissors && !state->scissors.empty()) {
//             auto vk_scissors = std::vector<VkRect2D>{};
//             vk_scissors.reserve(state->scissors.size());
//             std::ranges::transform(
//                 state->scissors,
//                 std::back_inserter(vk_scissors),
//                 [](auto& scissor) -> VkRect2D {
//                     return VkRect2D{
//                         .offset = VkOffset2D{
//                             .x = scissor.x,
//                             .y = scissor.y
//                         },
//                         .extent = VkExtent2D{
//                             .width  = scissor.width,
//                             .height = scissor.height
//                         }
//                     };
//                 }
//             );
//
//             vkCmdSetScissorWithCount(active_command_buffer->command_buffer, vk_scissors.size(), vk_scissors.data());
//             current_viewport_state->scissors = state->scissors;
//         }
//     }
//
//     auto VulkanCommandList::draw(DrawArguments* args) -> void
//     {
//         vkCmdDraw(
//             active_command_buffer->command_buffer,
//             args->vertex_count,
//             args->instance_count,
//             args->first_vertex,
//             args->first_instance
//         );
//     }
//
//     auto VulkanCommandList::draw_indexed(DrawArguments* args) -> void
//     {
//         vkCmdDrawIndexed(
//             active_command_buffer->command_buffer,
//             args->vertex_count,
//             args->instance_count,
//             args->first_index,
//             args->first_vertex,
//             args->first_instance
//         );
//     }
//
//     auto VulkanCommandList::draw_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void
//     {
//         static_assert(sizeof(VkDrawIndirectCommand) == sizeof(DrawIndirectCommand));
//
//         vkCmdDrawIndirect(
//             active_command_buffer->command_buffer,
//             assert_ref_count_cast<VulkanBuffer>(current_graphics_state.indirect_buffer)->buffer,
//             offset_bytes,
//             draw_count,
//             sizeof(DrawIndirectCommand)
//         );
//     }
//
//     auto VulkanCommandList::draw_indexed_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void
//     {
//         static_assert(sizeof(VkDrawIndexedIndirectCommand) == sizeof(DrawIndexedIndirectCommand));
//
//         vkCmdDrawIndexedIndirect(
//             active_command_buffer->command_buffer,
//             assert_ref_count_cast<VulkanBuffer>(current_graphics_state.indirect_buffer)->buffer,
//             offset_bytes,
//             draw_count,
//             sizeof(DrawIndexedIndirectCommand)
//         );
//     }
//
//     auto VulkanCommandList::set_mesh_state(MeshState* state) -> void
//     {
//         auto parent = get_device<VulkanDevice>();
//         auto pipeline_need_update = false;
//         if (current_mesh_state.pipeline != state->pipeline) {
//             auto vulkan_pipeline = assert_ref_count_cast<VulkanMeshPipeline>(state->pipeline);
//             vkCmdBindPipeline(active_command_buffer->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_pipeline->pipeline);
//
//             // Bind bindless descriptor sets here.
//             auto binding_infos = std::vector<VkDescriptorBufferBindingInfoEXT>{};
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->resource_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             binding_infos.emplace_back(
//                 VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
//                 nullptr,
//                 parent->bindless_manager->sampler_heap->descriptor_buffer_address,
//                 VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
//             );
//             vkCmdBindDescriptorBuffersEXT(
//                 active_command_buffer->command_buffer,
//                 2,
//                 binding_infos.data()
//             );
//
//             auto buffer_index = std::vector<uint32_t>{0u, 1u};
//             auto buffer_offset = std::vector<VkDeviceSize>{0, 0};
//             vkCmdSetDescriptorBufferOffsetsEXT(
//                 active_command_buffer->command_buffer,
//                 VK_PIPELINE_BIND_POINT_GRAPHICS,
//                 vulkan_pipeline->pipeline_layout,
//                 0,
//                 2,
//                 buffer_index.data(),
//                 buffer_offset.data()
//             );
//
//             active_command_buffer->add_reference(state->pipeline);
//             current_pipeline_layout = vulkan_pipeline->pipeline_layout;
//             current_push_constant_visibility = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
//             pipeline_need_update = true;
//         }
//
//         if (resource_state_tracker.has_barrier() || current_graphics_state.render_target != state->render_target) {
//             end_rendering();
//         }
//
//         if (state->indirect_buffer) {
//             active_command_buffer->add_reference(state->indirect_buffer);
//
//             auto vk_indirect_buffer = assert_ref_count_cast<VulkanBuffer>(state->indirect_buffer);
//             if (automatic_barriers) {
//                 resource_state_tracker.require_buffer_state(
//                     &vk_indirect_buffer->tracker,
//                     EResourceStates::indirect_command_read,
//                     EPipelineStage::draw_indirect
//                 );
//             }
//         }
//
//         if(auto render_target = state->render_target) {
//             std::vector<VkRenderingAttachmentInfo> color_attachments{};
//             color_attachments.reserve(render_target->color_attachments.size());
//             std::ranges::transform(
//                 render_target->color_attachments,
//                 std::back_inserter(color_attachments),
//                 [&](auto const& attachment) -> VkRenderingAttachmentInfo {
//                     auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(attachment.texture);
//
//                     if (automatic_barriers) {
//                         resource_state_tracker.require_texture_state(
//                             &vulkan_texture->tracker,
//                             TextureSubresourceRange{},
//                             EResourceStates::color_attachment
//                         );
//                     }
//
//                     auto clear_color = &attachment.clear_color;
//                     return VkRenderingAttachmentInfo{
//                         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//                         .imageView          = vulkan_texture->image_view(TextureSubresourceRange{}),
//                         .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//                         .resolveMode        = VK_RESOLVE_MODE_NONE,
//                         .resolveImageView   = VK_NULL_HANDLE,
//                         .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//                         .loadOp             = to_vk_load_op(attachment.load),
//                         .storeOp            = to_vk_store_op(attachment.store),
//                         .clearValue         = {.color = {clear_color->x, clear_color->y, clear_color->z, clear_color->w}},
//                     };
//                 }
//             );
//
//             auto has_stencil = false;
//             auto vk_depth_stencil_attachment = VkRenderingAttachmentInfo{};
//             if (auto attachment = render_target->depth_stencil_attachment) {
//                 auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(attachment.texture);
//
//                 if (automatic_barriers) {
//                     resource_state_tracker.require_texture_state(
//                         &vulkan_texture->tracker,
//                         TextureSubresourceRange{},
//                         EResourceStates::depth_stencil_attachment // TODO: read only.
//                     );
//                 }
//
//                 has_stencil = !is_depth_only_format(to_vk_format(vulkan_texture->info.format));
//                 vk_depth_stencil_attachment = VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
//                 vk_depth_stencil_attachment.imageView          = vulkan_texture->image_view(TextureSubresourceRange{});
//                 vk_depth_stencil_attachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//                 vk_depth_stencil_attachment.resolveMode        = VK_RESOLVE_MODE_NONE;
//                 vk_depth_stencil_attachment.resolveImageView   = VK_NULL_HANDLE;
//                 vk_depth_stencil_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//                 vk_depth_stencil_attachment.loadOp             = to_vk_load_op(attachment.load);
//                 vk_depth_stencil_attachment.storeOp            = to_vk_store_op(attachment.store);
//                 vk_depth_stencil_attachment.clearValue         = VkClearValue{
//                     .depthStencil = VkClearDepthStencilValue{
//                         render_target->depth_stencil_attachment.clear_depth,
//                         render_target->depth_stencil_attachment.clear_stencil,
//                     }
//                 };
//             }
//
//             commit_barriers();
//
//             auto offset = VkOffset2D{render_target->info.offset.x, render_target->info.offset.y};
//             auto extent = VkExtent2D{render_target->info.extent.x, render_target->info.extent.y};
//             auto rendering_info = VkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
//             rendering_info.flags                = 0;
//             rendering_info.renderArea           = {offset, extent};
//             rendering_info.layerCount           = 1;
//             rendering_info.viewMask             = 0;
//             rendering_info.colorAttachmentCount = (uint32_t) color_attachments.size();
//             rendering_info.pColorAttachments    = color_attachments.data();
//             rendering_info.pDepthAttachment     = render_target->depth_stencil_attachment ? &vk_depth_stencil_attachment : nullptr;
//             rendering_info.pStencilAttachment   = has_stencil ? &vk_depth_stencil_attachment : nullptr;
//
//             CNE_ASSERT_WITH(!color_attachments.empty(), "Render target must have at least one color attachment.");
//
//             vkCmdBeginRendering(active_command_buffer->command_buffer, &rendering_info);
//
//             auto blend_states = &render_target->info.blend_states;
//             CNE_ASSERT_WITH(blend_states->size() == render_target->color_attachments.size(), "Blend states must match number of color attachments.");
//
//             std::vector<VkBool32> blend_enable{};
//             std::vector<VkColorBlendEquationEXT> color_blend_equation{};
//             std::vector<VkColorComponentFlags> color_component_flags{};
//             blend_enable.reserve(blend_states->size());
//             color_blend_equation.reserve(blend_states->size());
//             color_component_flags.reserve(blend_states->size());
//             for (auto& blend_state: *blend_states) {
//
//                 blend_enable.emplace_back(blend_state.enable_blend ? VK_TRUE : VK_FALSE);
//                 color_blend_equation.emplace_back(VkColorBlendEquationEXT{
//                     .srcColorBlendFactor = to_vk_blend_factor(blend_state.color_src_blend),
//                     .dstColorBlendFactor = to_vk_blend_factor(blend_state.color_dst_blend),
//                     .colorBlendOp        = to_vk_blend_op(blend_state.color_blend_op),
//                     .srcAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_src_blend),
//                     .dstAlphaBlendFactor = to_vk_blend_factor(blend_state.alpha_dst_blend),
//                     .alphaBlendOp        = to_vk_blend_op(blend_state.alpha_blend_op)
//                 });
//                 color_component_flags.emplace_back(
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::r) ? VK_COLOR_COMPONENT_R_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::g) ? VK_COLOR_COMPONENT_G_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::b) ? VK_COLOR_COMPONENT_B_BIT : 0) |
//                     (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::a) ? VK_COLOR_COMPONENT_A_BIT : 0)
//                 );
//             }
//             vkCmdSetColorBlendEnableEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), blend_enable.data());
//             vkCmdSetColorBlendEquationEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_blend_equation.data());
//             vkCmdSetColorWriteMaskEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_component_flags.data());
//
//             // Set or clear dynamic settings:
//             vkCmdSetCullMode(active_command_buffer->command_buffer, VK_CULL_MODE_NONE);
//             vkCmdSetPolygonModeEXT(active_command_buffer->command_buffer, VK_POLYGON_MODE_FILL);
//             vkCmdSetRasterizationSamplesEXT(active_command_buffer->command_buffer, VK_SAMPLE_COUNT_1_BIT);
//             vkCmdSetFrontFace(active_command_buffer->command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
//
//             auto has_depth_stencil_attachment = (bool) render_target->depth_stencil_attachment;
//             auto enable_depth_test  = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_test ? VK_TRUE : VK_FALSE);
//             auto enable_depth_write = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_write ? VK_TRUE : VK_FALSE);
//             auto depth_compare_op   = !has_depth_stencil_attachment ? VK_COMPARE_OP_ALWAYS : to_vk_compare_op(render_target->info.depth_state.depth_compare);
//             vkCmdSetDepthTestEnable(active_command_buffer->command_buffer, enable_depth_test);
//             vkCmdSetDepthWriteEnable(active_command_buffer->command_buffer, enable_depth_write);
//             vkCmdSetDepthBoundsTestEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthBiasEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthClampEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetDepthCompareOp(active_command_buffer->command_buffer, depth_compare_op);
//
//             vkCmdSetStencilTestEnable(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetStencilOp(active_command_buffer->command_buffer,
//                 VK_STENCIL_FACE_FRONT_AND_BACK,
//                 VK_STENCIL_OP_KEEP,
//                 VK_STENCIL_OP_KEEP,
//                 VK_STENCIL_OP_KEEP,
//                 VK_COMPARE_OP_ALWAYS
//             );
//
//             vkCmdSetLogicOpEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
//             vkCmdSetLogicOpEXT(active_command_buffer->command_buffer, VK_LOGIC_OP_NO_OP);
//         }
//
//         if (state->viewport_state) {
//             set_viewport_state(&state->viewport_state);
//         }
//
//         // TODO: other dynamic state.
//
//         // TODO: Shading rate.
//
//         current_mesh_state = *state;
//         current_graphics_state = {};
//         current_compute_state = {};
//     }
//
//     auto VulkanCommandList::dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
//     {
//         vkCmdDrawMeshTasksEXT(active_command_buffer->command_buffer, group_count_x, group_count_y, group_count_z);
//     }
//
//     auto VulkanCommandList::dispatch_mesh_indirect(uint32_t offset, uint32_t count) -> void
//     {
//         auto indirect_buffer = assert_ref_count_cast<VulkanBuffer>(current_mesh_state.indirect_buffer)->buffer;
//         vkCmdDrawMeshTasksIndirectEXT(
//             active_command_buffer->command_buffer,
//             indirect_buffer,
//             offset,
//             count,
//             sizeof(VkDrawMeshTasksIndirectCommandEXT)
//         );
//         end_rendering();
//         VkMemoryBarrier2 barrier {
//             .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
//             .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
//             .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
//             .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
//             .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
//         };
//         auto dependency_info = VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
//         dependency_info.memoryBarrierCount = 1;
//         dependency_info.pMemoryBarriers = &barrier;
//         vkCmdPipelineBarrier2(active_command_buffer->command_buffer, &dependency_info);
//     }
//
//     // Debug Markers
//     auto VulkanCommandList::push_command_label(std::string_view name, math::float4 color) -> void
//     {
//         auto debug_label = VkDebugUtilsLabelEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
//         debug_label.pLabelName = name.data();
//         debug_label.color[0] = color.x;
//         debug_label.color[1] = color.y;
//         debug_label.color[2] = color.z;
//         debug_label.color[3] = color.w;
//         vkCmdBeginDebugUtilsLabelEXT(active_command_buffer->command_buffer, &debug_label);
//     }
//
//     auto VulkanCommandList::pop_command_label() -> void
//     {
//         vkCmdEndDebugUtilsLabelEXT(active_command_buffer->command_buffer);
//     }
//
//     // Profiling
//     auto VulkanCommandList::begin_timestep(RHITimerQuery* query) -> void
//     {
//         end_rendering();
//
//         auto vulkan_query = assert_cast<VulkanTimerQuery*>(query);
//
//         CNE_ASSERT(vulkan_query->begin_index >= 0);
//         CNE_ASSERT(!vulkan_query->started);
//
//         vulkan_query->resolved = false;
//
//         auto parent = get_device<VulkanDevice>();
//         parent->time_query_pool->reset_query(vulkan_query->begin_index, 2);
//
//         vkCmdWriteTimestamp(
//             active_command_buffer->command_buffer,
//             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//             parent->time_query_pool->query_pool,
//             vulkan_query->begin_index
//         );
//     }
//
//     auto VulkanCommandList::end_timestep(RHITimerQuery* query) -> void
//     {
//         end_rendering();
//
//         auto vulkan_query = assert_cast<VulkanTimerQuery*>(query);
//
//         CNE_ASSERT(vulkan_query->end_index >= 0);
//         CNE_ASSERT(!vulkan_query->started);
//         CNE_ASSERT(!vulkan_query->resolved);
//
//         auto parent = get_device<VulkanDevice>();
//         vkCmdWriteTimestamp(
//             active_command_buffer->command_buffer,
//             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//             parent->time_query_pool->query_pool,
//             vulkan_query->end_index
//         );
//
//         vulkan_query->started = true;
//     }
//
//     // Resource State Management
//     auto VulkanCommandList::enbale_automatic_barriers(bool enable) -> void
//     {
//         automatic_barriers = enable;
//     }
//
//     auto VulkanCommandList::begin_tracking_buffer(BufferHandle buffer, EResourceStates current_state, EPipelineStage current_stage) -> void
//     {
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         resource_state_tracker.begin_tracking_buffer_state(&vulkan_buffer->tracker, current_state, current_stage);
//     }
//
//     auto VulkanCommandList::begin_tracking_texture(TextureHandle texture, TextureSubresourceRange subresources, EResourceStates current_state, EPipelineStage current_stage) -> void
//     {
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         resource_state_tracker.begin_tracking_texture_state(&vulkan_texture->tracker, subresources, current_state, current_stage);
//     }
//
//     auto VulkanCommandList::set_buffer_state(BufferHandle buffer, EResourceStates dst_state, EPipelineStage dst_stage) -> void
//     {
//         active_command_buffer->add_reference(buffer);
//
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, dst_state, dst_stage);
//     }
//
//     auto VulkanCommandList::set_texture_state(TextureHandle texture, TextureSubresourceRange subresources, EResourceStates dst_state, EPipelineStage dst_stage) -> void
//     {
//         active_command_buffer->add_reference(texture);
//
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         resource_state_tracker.require_texture_state(&vulkan_texture->tracker, subresources, dst_state, dst_stage);
//     }
//
//     auto VulkanCommandList::lock_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void
//     {
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         resource_state_tracker.lock_buffer_state(&vulkan_buffer->tracker, dst_state);
//     }
//
//     auto VulkanCommandList::lock_texture_state(TextureHandle texture, EResourceStates dst_state) -> void
//     {
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         resource_state_tracker.lock_texture_state(&vulkan_texture->tracker, TextureSubresourceRange{}, dst_state);
//     }
//
//     // Execution Control
//     auto VulkanCommandList::flush() -> void
//     {
//         if (!info.enable_immediate_submit) {
//             CNE_ERROR("Flush only supported in immediate mode! Please submit the command list by device.");
//             return;
//         }
//
//         auto parent = get_device<VulkanDevice>();
//         auto queue = parent->queue(info.queue_type);
//
//         auto cmd_list = this;
//         auto wait_time = queue->submit({&cmd_list, 1});
//         queue->wait_command_list(wait_time, UINT64_MAX);
//     }
//
//     // State Queries
//     auto VulkanCommandList::buffer_state(BufferHandle buffer) -> EResourceStates
//     {
//         auto vulkan_buffer = assert_ref_count_cast<VulkanBuffer>(buffer);
//
//         return resource_state_tracker.buffer_state(&vulkan_buffer->tracker);
//     }
//
//     auto VulkanCommandList::texture_state(TextureHandle texture, uint32_t level, uint32_t layer) -> EResourceStates
//     {
//         auto vulkan_texture = assert_ref_count_cast<VulkanTexture>(texture);
//
//         return resource_state_tracker.texture_subresource_state(&vulkan_texture->tracker, level, layer);
//     }
//
//     // Synchronization
//     auto VulkanCommandList::commit_barriers(EQueueType src_queue, EQueueType dst_queue) -> void
//     {
//         if (resource_state_tracker.texture_barriers.empty() && resource_state_tracker.buffer_barriers.empty()) return;
//
//         end_rendering();
//
//         auto vk_buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
//         auto vk_image_barriers = std::vector<VkImageMemoryBarrier2>{};
//
//         vk_buffer_barriers.reserve(resource_state_tracker.buffer_barriers.size());
//         vk_image_barriers.reserve(resource_state_tracker.texture_barriers.size());
//
//         auto parent = get_device<VulkanDevice>();
//         std::ranges::transform(
//             resource_state_tracker.buffer_barriers,
//             std::back_inserter(vk_buffer_barriers),
//             [&](auto& barrier) -> VkBufferMemoryBarrier2 {
//                 auto vulkan_buffer = (VulkanBuffer*) barrier.buffer;
//                 // Directly update the state of the tracked buffer.
//                 // CNE_TRACE("Buffer Barrier: {} from {} to {}", vulkan_buffer->name, to_string(barrier.src_state), to_string(barrier.dst_state));
//                 auto buffer_state = resource_state_tracker.find_tracked_buffer_state(&vulkan_buffer->tracker, true);
//                 buffer_state->state = barrier.dst_state;
//                 buffer_state->pipeline_stage = barrier.dst_stage;
//                 return VkBufferMemoryBarrier2{
//                     .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
//                     .srcStageMask        = to_vk_pipeline_stage(barrier.src_stage), // TODO: improve.
//                     .srcAccessMask       = to_vk_access_type(barrier.src_state),
//                     .dstStageMask        = to_vk_pipeline_stage(barrier.dst_stage),
//                     .dstAccessMask       = to_vk_access_type(barrier.dst_state),
//                     .srcQueueFamilyIndex = parent->queue_family(src_queue),
//                     .dstQueueFamilyIndex = parent->queue_family(dst_queue),
//                     .buffer              = vulkan_buffer->buffer,
//                     .offset              = 0,
//                     .size                = vulkan_buffer->info.size_bytes,
//                 };
//             }
//         );
//
//         std::ranges::transform(
//             resource_state_tracker.texture_barriers,
//             std::back_inserter(vk_image_barriers),
//             [&](auto& barrier) -> VkImageMemoryBarrier2 {
//                 auto vulkan_texture = (VulkanTexture*) barrier.texture;
//                 // Directly update the state of the tracked texture. Maybe needn't to process subresource?
//                 auto texture_state = resource_state_tracker.find_tracked_texture_state(&vulkan_texture->tracker, true);
//                 texture_state->state = barrier.dst_state;
//                 texture_state->pipeline_stage = barrier.dst_stage;
//
//                 // CNE_TRACE("Texture Barrier: {} from {} to {}", (void*) vulkan_texture->image, to_string(barrier.src_state), to_string(barrier.dst_state));
//
//                 auto image_barrier = VkImageMemoryBarrier2{
//                     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//                     .srcStageMask        = to_vk_pipeline_stage(barrier.src_stage), // TODO: improve.
//                     .srcAccessMask       = to_vk_access_type(barrier.src_state),
//                     .dstStageMask        = to_vk_pipeline_stage(barrier.dst_stage),
//                     .dstAccessMask       = to_vk_access_type(barrier.dst_state),
//                     .oldLayout           = image_layout_from_access(barrier.src_state, is_depth_stencil_format(to_vk_format(vulkan_texture->info.format))),
//                     .newLayout           = image_layout_from_access(barrier.dst_state, is_depth_stencil_format(to_vk_format(vulkan_texture->info.format))),
//                     .srcQueueFamilyIndex = parent->queue_family(src_queue),
//                     .dstQueueFamilyIndex = parent->queue_family(dst_queue),
//                     .image               = vulkan_texture->image,
//                     .subresourceRange    = {
//                         .aspectMask     = aspect_flag_from_format(to_vk_format(vulkan_texture->info.format)),
//                         .baseMipLevel   = barrier.mip_level,
//                         .levelCount     = barrier.contain_all_resource ? vulkan_texture->info.mip_count : 1,
//                         .baseArrayLayer = barrier.array_layer,
//                         .layerCount     = barrier.contain_all_resource ? vulkan_texture->info.layer_count : 1,
//                     },
//                 };
//
//                 if (barrier.src_state == EResourceStates::present) {
//                     CNE_ASSERT(image_barrier.dstStageMask & VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
//                 }
//
//                 return image_barrier;
//             }
//         );
//
//         auto dependency_info = VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
//         dependency_info.bufferMemoryBarrierCount = vk_buffer_barriers.size();
//         dependency_info.pBufferMemoryBarriers    = vk_buffer_barriers.data();
//         dependency_info.imageMemoryBarrierCount  = vk_image_barriers.size();
//         dependency_info.pImageMemoryBarriers     = vk_image_barriers.data();
//
//         vkCmdPipelineBarrier2(active_command_buffer->command_buffer, &dependency_info);
//
//         resource_state_tracker.clear_barriers();
//     }
//
//     auto VulkanCommandList::wait_for_submit(EQueueType submit_queue_type, uint64_t submit_time, EPipelineStage wait_stage) -> void
//     {
//         auto parent = get_device<VulkanDevice>();
//         auto wait_queue = parent->queue(submit_queue_type);
//         auto queue = parent->queue(info.queue_type);
//         queue->add_wait_semaphore(wait_queue->timeline, submit_time, to_vk_pipeline_stage(wait_stage));
//     }
//
//     auto VulkanCommandList::device() -> IDevice*
//     {
//         auto parent = get_device<VulkanDevice>();
//
//         return parent;
//     }
//
//     auto VulkanCommandList::finish_submission(VulkanQueue* queue, uint64_t submission_time) -> void
//     {
//         resource_state_tracker.finish_tracking();
//
//         auto recording_time = active_command_buffer->recording_time;
//         block_pool->update_block_version(
//             make_version(recording_time, queue->type, false),
//             make_version(submission_time, queue->type, true)
//         );
//
//         active_command_buffer = {};
//     }
//
//     auto VulkanCommandList::end_rendering() -> void
//     {
//         if (current_graphics_state.render_target || current_mesh_state.render_target) {
//             vkCmdEndRendering(active_command_buffer->command_buffer);
//             current_graphics_state.render_target = {};
//             current_mesh_state.render_target = {};
//         }
//     }
//
//     auto VulkanCommandList::set_dynamic_state() -> void
//     {
//
//     }
}
