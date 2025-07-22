#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include "../tool/version.hpp"

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
    }

    auto VulkanDevice::create_command_list(CommandListCreateInfo* info) -> CommandListHandle
    {
        return std::make_shared<VulkanCommandList>(this, info);
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device, uint32_t queue_family_index)
        : VulkanDeviceChild<VulkanCommandBuffer>(device)
    {
        auto pool_ci = VkCommandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool_ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_ci.queueFamilyIndex = queue_family_index;

        auto result_pool_create = vkCreateCommandPool(parent->device, &pool_ci, nullptr, &command_pool);
        CNE_ASSERT_WITH(result_pool_create == VK_SUCCESS, std::format("Failed to create command pool: {}", vk_error_to_string(result_pool_create)));

        auto cmd_buffer_alloi = VkCommandBufferAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cmd_buffer_alloi.commandPool        = command_pool;
        cmd_buffer_alloi.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buffer_alloi.commandBufferCount = 1;

        auto result_cmd_create = vkAllocateCommandBuffers(parent->device, &cmd_buffer_alloi, &command_buffer);
        CNE_ASSERT_WITH(result_cmd_create == VK_SUCCESS, std::format("Failed to allocate command buffer: {}", vk_error_to_string(result_cmd_create)));
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        vkDestroyCommandPool(parent->device, command_pool, parent->allocation_callbacks);

        referenced_resources.clear();
        referenced_sataging_buffers.clear();
    }

    auto VulkanCommandBuffer::add_reference(RefCountPtr<IResource> resource) -> void
    {
        referenced_resources.emplace_back(resource);
    }

    auto VulkanCommandBuffer::add_reference_sataging_buffer(RefCountPtr<VulkanBuffer> buffer) -> void
    {
        referenced_sataging_buffers.emplace_back(buffer);
    }

    auto VulkanCommandBuffer::reset() -> void
    {
        // NOTE: Must reset command buffer or it will cause memory leak, implicit reset by begin seems some problems.
        // vkResetCommandBuffer(command_buffer, 0);
        vkResetCommandPool(parent->device, command_pool, 0);
    }

    auto VulkanCommandBuffer::clear_references() -> void
    {
        referenced_resources.clear();
        referenced_sataging_buffers.clear();
    }

    VulkanCommandList::VulkanCommandList(VulkanDevice* device, CommandListCreateInfo* info)
        : VulkanDeviceChild<VulkanCommandList>(device)
        , info(*info)
        , block_pool(device->queue(info->queue_type)->buffer_block.get())
    {
        resource_state_tracker.buffer_barriers.reserve(16);
        resource_state_tracker.texture_barriers.reserve(16);
    }

    VulkanCommandList::~VulkanCommandList()
    {

    }

    auto VulkanCommandList::start() -> void
    {
        active_command_buffer = parent->queue(info.queue_type)->allocate_command_buffer();
        active_command_buffer->reset();

        auto begin_info = VkCommandBufferBeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(active_command_buffer->command_buffer, &begin_info);

        reset();
    }

    auto VulkanCommandList::finish() -> void
    {
        end_rendering();

        resource_state_tracker.keep_initial_state();
        commit_barriers();

        vkEndCommandBuffer(active_command_buffer->command_buffer);

        reset();
    }

    auto VulkanCommandList::reset() -> void
    {
        end_rendering();

        current_pipeline_layout = VK_NULL_HANDLE;
        current_push_constant_visibility = {};

        current_graphics_state = {};
        current_compute_state = {};
    }

    // Clear Operations
    auto VulkanCommandList::clear_buffer_uint(BufferHandle buffer, uint32_t clear_value) -> void
    {
        end_rendering();

        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        if (automatic_barriers) {
            resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, EResourceStates::transfer_dst);
        }
        commit_barriers();

        vkCmdFillBuffer(active_command_buffer->command_buffer, vulkan_buffer->buffer, 0, vulkan_buffer->info.size_bytes, clear_value);
    }

    auto VulkanCommandList::clear_texture_float(TextureHandle texture, TextureSubresourceSet subresources, math::float4 clear_color) -> void
    {
        auto clear_color_value = VkClearColorValue{.float32 = {clear_color.x, clear_color.y, clear_color.z, clear_color.w}};

        clear_texture(texture, &subresources, &clear_color_value);
    }

    auto VulkanCommandList::clear_texture_uint(TextureHandle texture, TextureSubresourceSet subresources, uint32_t clear_color) -> void
    {
        auto clear_color_value = VkClearColorValue{.uint32 = {clear_color, clear_color, clear_color, clear_color}};

        clear_texture(texture, &subresources, &clear_color_value);
    }

    auto VulkanCommandList::clear_texture(TextureHandle texture, TextureSubresourceSet* subresources, VkClearColorValue* clear_color) -> void
    {
        end_rendering();

        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        *subresources = subresources->adapt_to_texture(&vulkan_texture->info, false);

        if (automatic_barriers) {
            resource_state_tracker.require_texture_state(
                &vulkan_texture->tracker,
                *subresources,
                EResourceStates::transfer_dst
            );
        }
        commit_barriers();

        auto image_subresource_range = VkImageSubresourceRange{
            .aspectMask     = aspect_flag_from_format(convert_to_vk_format(vulkan_texture->info.format)),
            .baseMipLevel   = subresources->base_mip_level,
            .levelCount     = subresources->num_mip_levels,
            .baseArrayLayer = subresources->base_array_layer,
            .layerCount     = subresources->num_array_layers
        };

        vkCmdClearColorImage(
            active_command_buffer->command_buffer,
            vulkan_texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            clear_color,
            1,
            &image_subresource_range
        );
    }

    auto VulkanCommandList::clear_depth_stencil(TextureHandle texture, TextureSubresourceSet subresources, std::optional<float> clear_depth, std::optional<uint8_t> clear_stencil) -> void
    {
        end_rendering();

        if (!clear_depth && !clear_stencil) return;

        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        subresources = subresources.adapt_to_texture(&vulkan_texture->info, false);

        if (automatic_barriers) {
            resource_state_tracker.require_texture_state(
                &vulkan_texture->tracker,
                subresources,
                EResourceStates::transfer_dst
            );
        }
        commit_barriers();

        auto aspect_flags = VkImageAspectFlags{};

        if (clear_depth) {
            aspect_flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        if (clear_stencil) {
            aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        auto subresource_range = VkImageSubresourceRange{
            .aspectMask     = aspect_flags,
            .baseMipLevel   = subresources.base_mip_level,
            .levelCount     = subresources.num_mip_levels,
            .baseArrayLayer = subresources.base_array_layer,
            .layerCount     = subresources.num_array_layers,
        };

        auto clear_value = VkClearDepthStencilValue{
            .depth   = clear_depth.value_or(0.0f),
            .stencil = clear_stencil.value_or(0)
        };

        vkCmdClearDepthStencilImage(
            active_command_buffer->command_buffer,
            vulkan_texture->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear_value,
            1,
            &subresource_range
        );
    }

    // Resource Transfers
    auto VulkanCommandList::copy_buffer(BufferHandle src_buffer, size_t src_offset_bytes, BufferHandle dst_buffer, size_t dst_offset_bytes, size_t data_size_bytes) -> void
    {
        auto src_vulkan_buffer = assert_cast<VulkanBuffer>(src_buffer);
        auto dst_vulkan_buffer = assert_cast<VulkanBuffer>(dst_buffer);

        CNE_ASSERT(src_offset_bytes + data_size_bytes <= src_vulkan_buffer->info.size_bytes);
        CNE_ASSERT(dst_offset_bytes + data_size_bytes <= dst_vulkan_buffer->info.size_bytes);

        if (src_vulkan_buffer->info.type == EBufferType::gpu_only) {
            active_command_buffer->add_reference(src_buffer);
        } else {
            active_command_buffer->add_reference_sataging_buffer(src_vulkan_buffer);
        }

        if (dst_vulkan_buffer->info.type == EBufferType::gpu_only) {
            active_command_buffer->add_reference(dst_buffer);
        } else {
            active_command_buffer->add_reference_sataging_buffer(dst_vulkan_buffer);
        }

        if (automatic_barriers) {
            resource_state_tracker.require_buffer_state(&src_vulkan_buffer->tracker, EResourceStates::transfer_src);
            resource_state_tracker.require_buffer_state(&dst_vulkan_buffer->tracker, EResourceStates::transfer_dst);
        }
        commit_barriers();

        auto copy_region = VkBufferCopy2{VK_STRUCTURE_TYPE_BUFFER_COPY_2};
        copy_region.size      = data_size_bytes;
        copy_region.srcOffset = src_offset_bytes;
        copy_region.dstOffset = dst_offset_bytes;

        auto copy_info = VkCopyBufferInfo2{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
        copy_info.srcBuffer   = src_vulkan_buffer->buffer;
        copy_info.dstBuffer   = dst_vulkan_buffer->buffer;
        copy_info.regionCount = 1;
        copy_info.pRegions    = &copy_region;

        vkCmdCopyBuffer2(active_command_buffer->command_buffer, &copy_info);
    }

    auto VulkanCommandList::copy_texture(TextureHandle src_texture, TextureSlice src_slice, TextureHandle dst_texture, TextureSlice dst_slice) -> void
    {
        auto src_vulkan_texture = assert_cast<VulkanTexture>(src_texture);
        auto dst_vulkan_texture = assert_cast<VulkanTexture>(dst_texture);

        active_command_buffer->add_reference(src_texture);
        active_command_buffer->add_reference(dst_texture);

        if (automatic_barriers) {
            resource_state_tracker.require_texture_state(
                &src_vulkan_texture->tracker,
                TextureSubresourceSet{src_slice.level, 1, src_slice.layer, 1},
                EResourceStates::transfer_src
            );
            resource_state_tracker.require_texture_state(
                &dst_vulkan_texture->tracker,
                TextureSubresourceSet{dst_slice.level, 1, dst_slice.layer, 1},
                EResourceStates::transfer_dst
            );
        }
        commit_barriers();

        auto src_subresource = VkImageSubresourceLayers{
            .aspectMask     = aspect_flag_from_format(convert_to_vk_format(src_vulkan_texture->info.format)),
            .mipLevel       = src_slice.level,
            .baseArrayLayer = src_slice.layer,
            .layerCount     = 1
        };

        auto dst_subresource = VkImageSubresourceLayers{
            .aspectMask     = aspect_flag_from_format(convert_to_vk_format(dst_vulkan_texture->info.format)),
            .mipLevel       = dst_slice.level,
            .baseArrayLayer = dst_slice.layer,
            .layerCount     = 1
        };

        auto extent = VkExtent3D{
            std::min(src_slice.extent.x, dst_slice.extent.x),
            std::min(src_slice.extent.y, dst_slice.extent.y),
            std::min(src_slice.extent.z, dst_slice.extent.z)
        };

        auto copy_region = VkImageCopy2{VK_STRUCTURE_TYPE_IMAGE_COPY_2};
        copy_region.srcSubresource = src_subresource;
        copy_region.srcOffset      = VkOffset3D{(int) src_slice.origin.x, (int) src_slice.origin.y, (int) src_slice.origin.z};
        copy_region.dstSubresource = dst_subresource;
        copy_region.dstOffset      = VkOffset3D{(int) dst_slice.origin.x, (int) dst_slice.origin.y, (int) dst_slice.origin.z};
        copy_region.extent         = extent;

        auto copy_image_info = VkCopyImageInfo2{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2};
        copy_image_info.srcImage       = src_vulkan_texture->image;
        copy_image_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy_image_info.dstImage       = dst_vulkan_texture->image;
        copy_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_image_info.regionCount    = 1;
        copy_image_info.pRegions       = &copy_region;

        vkCmdCopyImage2(active_command_buffer->command_buffer, &copy_image_info);
    }

    auto VulkanCommandList::write_buffer(BufferHandle buffer, std::span<std::byte> data, size_t offset_bytes) -> void
    {
        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        CNE_ASSERT(offset_bytes + data.size() <= vulkan_buffer->info.size_bytes);

        if (vulkan_buffer->info.type == EBufferType::cpu_write) {
            std::memcpy(vulkan_buffer->map<std::byte>() + offset_bytes, data.data(), data.size());
            vulkan_buffer->unmap();

            return;
        }

        end_rendering();

        constexpr auto upload_buffer_limit = 65536;

        if (data.size() <= upload_buffer_limit && (offset_bytes & 3) == 0) {
            if (automatic_barriers) {
                resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, EResourceStates::transfer_dst);
            }
            commit_barriers();

            auto size_to_write = (data.size() + 3) & ~3ull;

            vkCmdUpdateBuffer(active_command_buffer->command_buffer, vulkan_buffer->buffer, offset_bytes, size_to_write, data.data());
        } else {
            auto sub_buffer_block = block_pool->suballocate_buffer(data.size(), make_version(active_command_buffer->recording_time, this->info.queue_type, false));
            auto staging_buffer = assert_cast<VulkanBuffer>(sub_buffer_block.buffer);
            auto mapped_ptr = staging_buffer->map<std::byte>() + sub_buffer_block.range.byte_offset;
            std::memcpy(mapped_ptr, data.data(), data.size());
            staging_buffer->unmap();

            if (!automatic_barriers) {
                // If not set automatically, we need to insert barrier for staging buffer here.
                resource_state_tracker.require_buffer_state(&staging_buffer->tracker, EResourceStates::transfer_src);
            }

            copy_buffer(
                sub_buffer_block.buffer,
                sub_buffer_block.range.byte_offset,
                buffer,
                offset_bytes,
                data.size()
            );
        }
    }

    auto VulkanCommandList::write_texture(TextureHandle texture, uint32_t level, uint32_t layer, TextureSliceDataView data) -> void
    {
        end_rendering();

        auto vulkan_texture = assert_cast<VulkanTexture>(texture);
        auto info = vulkan_texture->info;

        active_command_buffer->add_reference(texture);

        if (automatic_barriers) {
            resource_state_tracker.require_texture_state(
                &vulkan_texture->tracker,
                TextureSubresourceSet{level, 1, layer, 1},
                EResourceStates::transfer_dst
            );
        }
        commit_barriers();

        auto mip_width = std::max(info.extent.x >> level, 1u);
        auto mip_height = std::max(info.extent.y >> level, 1u);
        auto mip_depth = std::max(info.depth >> level, 1u);

        auto format_info = get_format_info(info.format);
        auto num_cols    = (mip_width + format_info->blocks - 1) / format_info->blocks;
        auto num_rows    = (mip_height + format_info->blocks - 1) / format_info->blocks;
        auto row_bytes   = (size_t) num_cols * format_info->bytes_per_block;
        auto memory_size = (size_t) row_bytes * num_rows * mip_depth;

        auto sub_buffer_block = block_pool->suballocate_buffer(
            memory_size,
            make_version(active_command_buffer->recording_time, this->info.queue_type, false)
        );
        auto staging_buffer = assert_cast<VulkanBuffer>(sub_buffer_block.buffer);
        active_command_buffer->add_reference(staging_buffer);

        auto min_row_bytes = std::min(row_bytes, (size_t) data.extent(0));
        auto mapped_ptr = staging_buffer->map<std::byte>() + sub_buffer_block.range.byte_offset;
        for (auto layer = 0; layer < data.extent(2); layer++) {
            auto source_ptr = data.data_handle() + layer * data.extent(0) * data.extent(1);
            for (auto row = 0; row < data.extent(1); row++) {
                std::memcpy(
                    mapped_ptr + row * min_row_bytes,
                    source_ptr + row * data.extent(0),
                    min_row_bytes
                );
            }
        }
        staging_buffer->unmap();

        auto buffer_image_copy = VkBufferImageCopy2{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
        buffer_image_copy.bufferOffset                    = 0;
        buffer_image_copy.bufferRowLength                 = num_cols * format_info->blocks;
        buffer_image_copy.bufferImageHeight               = num_rows * format_info->blocks;
        buffer_image_copy.imageSubresource.aspectMask     = aspect_flag_from_format(convert_to_vk_format(info.format));
        buffer_image_copy.imageSubresource.mipLevel       = level;
        buffer_image_copy.imageSubresource.baseArrayLayer = layer;
        buffer_image_copy.imageSubresource.layerCount     = 1;
        buffer_image_copy.imageOffset                     = {0, 0, 0};
        buffer_image_copy.imageExtent                     = {mip_width, mip_height, mip_depth};

        auto buffer_image_info = VkCopyBufferToImageInfo2{VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
        buffer_image_info.srcBuffer      = staging_buffer->buffer;
        buffer_image_info.dstImage       = vulkan_texture->image;
        buffer_image_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        buffer_image_info.regionCount    = 1;
        buffer_image_info.pRegions       = &buffer_image_copy;

        vkCmdCopyBufferToImage2(active_command_buffer->command_buffer, &buffer_image_info);
    }

    // Compute Operations
    auto VulkanCommandList::push_constants(std::span<std::byte> data) -> void
    {
        vkCmdPushConstants(
            active_command_buffer->command_buffer,
            current_pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // current_push_constant_visibility
            0,
            data.size(),
            data.data()
        );
    }

    auto VulkanCommandList::set_compute_state(ComputeState* state) -> void
    {

    }

    auto VulkanCommandList::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
    {
        vkCmdDispatch(active_command_buffer->command_buffer, group_count_x, group_count_y, group_count_z);
    }

    auto VulkanCommandList::dispatch_indirect(BufferHandle buffer, uint32_t offset) -> void
    {

    }

    // Graphics Operations
    auto VulkanCommandList::set_graphics_state(GraphicsState* state) -> void
    {
        auto vulkan_pipeline = assert_cast<VulkanGraphicsPipeline>(state->pipeline);

        // TODO: Track all states releated resource.

        auto pipeline_need_update = false;
        if (current_graphics_state.pipeline != state->pipeline) {
            vkCmdBindPipeline(active_command_buffer->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_pipeline->pipeline);
            // Bind bindless descriptor sets here.
            vkCmdBindDescriptorSets(
                active_command_buffer->command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                vulkan_pipeline->pipeline_layout,
                0, // TODO: Bindless set index.
                1, // TODO: Bindless set count.
                &parent->bindless_manager->descriptor_set,
                0, nullptr
            );

            active_command_buffer->add_reference(state->pipeline);
            pipeline_need_update = true;
        }

        if (resource_state_tracker.has_barrier()) {
            end_rendering();
        }

        // TODO: Set all attachment states.

        commit_barriers();

        current_pipeline_layout = vulkan_pipeline->pipeline_layout;
        // current_push_constant_visibility = vulkan_pipeline->push_constant_visibility;

        if(auto render_target = state->render_target) {
            std::vector<VkRenderingAttachmentInfo> color_attachments{};
            color_attachments.reserve(render_target->color_attachments.size());
            std::ranges::transform(
                render_target->color_attachments,
                render_target->clear_colors,
                std::back_inserter(color_attachments),
                [&] (auto const& attachment, auto const& clear_color) -> VkRenderingAttachmentInfo {
                    auto vulkan_texture = assert_cast<VulkanTexture>(attachment.texture);

                    if (automatic_barriers) {
                        resource_state_tracker.require_texture_state(
                            &vulkan_texture->tracker,
                            TextureSubresourceSet::all(&vulkan_texture->info),
                            EResourceStates::color_attachment
                        );
                    }

                    return VkRenderingAttachmentInfo{
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView          = vulkan_texture->default_view()->image_view,
                        .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        .resolveMode        = VK_RESOLVE_MODE_NONE,
                        .resolveImageView   = VK_NULL_HANDLE,
                        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .loadOp             = convert_to_vk_load_op(attachment.load),
                        .storeOp            = convert_to_vk_store_op(attachment.store),
                        .clearValue         = {.color = {clear_color.x, clear_color.y, clear_color.z, clear_color.w}},
                    };
                }
            );

            auto has_stencil = false;
            auto vk_depth_stencil_attachment = VkRenderingAttachmentInfo{};
            if (auto attachment = render_target->depth_stencil_attachment) {
                auto vulkan_texture = assert_cast<VulkanTexture>(attachment.texture);

                if (automatic_barriers) {
                    resource_state_tracker.require_texture_state(
                        &vulkan_texture->tracker,
                        TextureSubresourceSet::all(&vulkan_texture->info),
                        EResourceStates::depth_stencil_attachment // TODO: read only.
                    );
                }

                has_stencil = !is_depth_only_format(convert_to_vk_format(vulkan_texture->info.format));
                vk_depth_stencil_attachment = VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                vk_depth_stencil_attachment.imageView          = vulkan_texture->default_view()->image_view;
                vk_depth_stencil_attachment.imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                vk_depth_stencil_attachment.resolveMode        = VK_RESOLVE_MODE_NONE;
                vk_depth_stencil_attachment.resolveImageView   = VK_NULL_HANDLE;
                vk_depth_stencil_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                vk_depth_stencil_attachment.loadOp             = convert_to_vk_load_op(attachment.load);
                vk_depth_stencil_attachment.storeOp            = convert_to_vk_store_op(attachment.store);
                vk_depth_stencil_attachment.clearValue = VkClearValue{.depthStencil = VkClearDepthStencilValue{render_target->clear_depth, render_target->clear_stencil}};
            }

            commit_barriers();

            auto offset = VkOffset2D{render_target->info.offset.x, render_target->info.offset.y};
            auto extent = VkExtent2D{render_target->info.extent.x, render_target->info.extent.y};
            auto rendering_info = VkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
            rendering_info.flags                = 0;
            rendering_info.renderArea           = {offset, extent};
            rendering_info.layerCount           = 1;
            rendering_info.viewMask             = 0;
            rendering_info.colorAttachmentCount = (uint32_t) color_attachments.size();
            rendering_info.pColorAttachments    = color_attachments.data();
            rendering_info.pDepthAttachment     = render_target->depth_stencil_attachment ? &vk_depth_stencil_attachment : nullptr;
            rendering_info.pStencilAttachment   = has_stencil ? &vk_depth_stencil_attachment : nullptr;

            vkCmdBeginRendering(active_command_buffer->command_buffer, &rendering_info);

            auto blend_states = &render_target->info.blend_states;
            std::vector<VkBool32> blend_enable{};
            std::vector<VkColorBlendEquationEXT> color_blend_equation{};
            std::vector<VkColorComponentFlags> color_component_flags{};
            blend_enable.reserve(blend_states->size());
            color_blend_equation.reserve(blend_states->size());
            color_component_flags.reserve(blend_states->size());
            for (auto& blend_state: *blend_states) {

                blend_enable.emplace_back(blend_state.enable_blend ? VK_TRUE : VK_FALSE);
                color_blend_equation.emplace_back(VkColorBlendEquationEXT{
                    .srcColorBlendFactor = convert_to_vk_blend_factor(blend_state.color_src_blend),
                    .dstColorBlendFactor = convert_to_vk_blend_factor(blend_state.color_dst_blend),
                    .colorBlendOp        = convert_to_vk_blend_op(blend_state.color_blend_op),
                    .srcAlphaBlendFactor = convert_to_vk_blend_factor(blend_state.alpha_src_blend),
                    .dstAlphaBlendFactor = convert_to_vk_blend_factor(blend_state.alpha_dst_blend),
                    .alphaBlendOp        = convert_to_vk_blend_op(blend_state.alpha_blend_op)
                });
                color_component_flags.emplace_back(
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::r) ? VK_COLOR_COMPONENT_R_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::g) ? VK_COLOR_COMPONENT_G_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::b) ? VK_COLOR_COMPONENT_B_BIT : 0) |
                    (enum_has_any_flags(blend_state.color_write_mask, EColorWriteMask::a) ? VK_COLOR_COMPONENT_A_BIT : 0)
                );
            }
            vkCmdSetColorBlendEnableEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), blend_enable.data());
            vkCmdSetColorBlendEquationEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_blend_equation.data());
            vkCmdSetColorWriteMaskEXT(active_command_buffer->command_buffer, 0, color_attachments.size(), color_component_flags.data());

            // Set or clear dynamic settings:
            vkCmdSetCullMode(active_command_buffer->command_buffer, VK_CULL_MODE_NONE);
            vkCmdSetPolygonModeEXT(active_command_buffer->command_buffer, VK_POLYGON_MODE_FILL);
            vkCmdSetRasterizationSamplesEXT(active_command_buffer->command_buffer, VK_SAMPLE_COUNT_1_BIT);
            vkCmdSetFrontFace(active_command_buffer->command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);

            auto has_depth_stencil_attachment = (bool) render_target->depth_stencil_attachment;
            auto enable_depth_test  = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_test ? VK_TRUE : VK_FALSE);
            auto enable_depth_write = !has_depth_stencil_attachment ? VK_FALSE : (render_target->info.depth_state.enable_depth_write ? VK_TRUE : VK_FALSE);
            auto depth_compare_op   = !has_depth_stencil_attachment ? VK_COMPARE_OP_ALWAYS : convert_to_vk_compare_op(render_target->info.depth_state.depth_compare);
            vkCmdSetDepthTestEnable(active_command_buffer->command_buffer, enable_depth_test);
            vkCmdSetDepthWriteEnable(active_command_buffer->command_buffer, enable_depth_write);
            vkCmdSetDepthBoundsTestEnable(active_command_buffer->command_buffer, VK_FALSE);
            vkCmdSetDepthBiasEnable(active_command_buffer->command_buffer, VK_FALSE);
            vkCmdSetDepthClampEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
            vkCmdSetDepthCompareOp(active_command_buffer->command_buffer, depth_compare_op);

            vkCmdSetStencilTestEnable(active_command_buffer->command_buffer, VK_FALSE);
            vkCmdSetStencilOp(active_command_buffer->command_buffer,
                VK_STENCIL_FACE_FRONT_AND_BACK,
                VK_STENCIL_OP_KEEP,
                VK_STENCIL_OP_KEEP,
                VK_STENCIL_OP_KEEP,
                VK_COMPARE_OP_ALWAYS
            );

            vkCmdSetLogicOpEnableEXT(active_command_buffer->command_buffer, VK_FALSE);
            vkCmdSetLogicOpEXT(active_command_buffer->command_buffer, VK_LOGIC_OP_NO_OP);
        }

        if (state->viewport_state) {
            set_viewport_state(&state->viewport_state);
        }

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
                    vk_attribute->format   = convert_to_vk_format(attribute.format);
                    vk_attribute->offset   = attribute.offset_bytes;
                }
            }

            vkCmdSetVertexInputEXT(
                active_command_buffer->command_buffer,
                vk_bindings.size(),
                vk_bindings.data(),
                vk_attributes.size(),
                vk_attributes.data()
            );
        }

        // TODO: other dynamic state.

        if (state->index_buffer_binding && state->index_buffer_binding != current_graphics_state.index_buffer_binding) {
            vkCmdBindIndexBuffer(
                active_command_buffer->command_buffer,
                assert_cast<VulkanBuffer>(state->index_buffer_binding.buffer)->buffer,
                state->index_buffer_binding.offset_bytes,
                state->index_buffer_binding.format == EFormat::r16_uint ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32
            );
        }

        if (!state->vertex_buffer_bindings.empty() && state->vertex_buffer_bindings != current_graphics_state.vertex_buffer_bindings) {
            auto vk_vertex_buffers = std::vector<VkBuffer>{};
            auto vk_vertex_offsets = std::vector<VkDeviceSize>{};

            for (auto& binding: state->vertex_buffer_bindings) {
                vk_vertex_buffers.emplace_back(assert_cast<VulkanBuffer>(binding.buffer)->buffer);
                vk_vertex_offsets.emplace_back(binding.offset_bytes);

                active_command_buffer->add_reference(binding.buffer);
            }

            vkCmdBindVertexBuffers(
                active_command_buffer->command_buffer,
                0,
                vk_vertex_buffers.size(),
                vk_vertex_buffers.data(),
                vk_vertex_offsets.data()
            );
        }

        if (state->indirect_buffer) {
            active_command_buffer->add_reference(state->indirect_buffer);
        }

        // TODO: Shading rate.

        current_graphics_state = *state;
        current_compute_state = {};
    }

    auto VulkanCommandList::set_viewport_state(ViewportState* state) -> void
    {
        if (!state) return;

        auto current_viewport_state = &current_graphics_state.viewport_state;
        if (current_viewport_state->viewports != state->viewports && !state->viewports.empty()) {
            auto vk_viewports = std::vector<VkViewport>{};
            vk_viewports.reserve(state->viewports.size());
            std::ranges::transform(
                state->viewports,
                std::back_inserter(vk_viewports),
                [] (auto& viewport) -> VkViewport {
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

            vkCmdSetViewportWithCount(active_command_buffer->command_buffer, vk_viewports.size(), vk_viewports.data());
            current_viewport_state->viewports = state->viewports;
        }
        if (current_viewport_state->scissors != state->scissors && !state->scissors.empty()) {
            auto vk_scissors = std::vector<VkRect2D>{};
            vk_scissors.reserve(state->scissors.size());
            std::ranges::transform(
                state->scissors,
                std::back_inserter(vk_scissors),
                [] (auto& scissor) -> VkRect2D {
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

            vkCmdSetScissorWithCount(active_command_buffer->command_buffer, vk_scissors.size(), vk_scissors.data());
            current_viewport_state->scissors = state->scissors;
        }
    }

    auto VulkanCommandList::draw(DrawArguments* args) -> void
    {
        vkCmdDraw(
            active_command_buffer->command_buffer,
            args->num_vertices,
            args->num_instances,
            args->first_vertex,
            args->first_instance
        );
    }

    auto VulkanCommandList::draw_indexed(DrawArguments* args) -> void
    {
        vkCmdDrawIndexed(
            active_command_buffer->command_buffer,
            args->num_vertices,
            args->num_instances,
            args->first_index,
            args->first_vertex,
            args->first_instance
        );
    }

    auto VulkanCommandList::draw_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void
    {
        static_assert(sizeof(VkDrawIndirectCommand) == sizeof(DrawIndirectCommand));

        vkCmdDrawIndirect(
            active_command_buffer->command_buffer,
            assert_cast<VulkanBuffer>(current_graphics_state.indirect_buffer)->buffer,
            offset_bytes,
            draw_count,
            sizeof(DrawIndirectCommand)
        );
    }

    auto VulkanCommandList::draw_indexed_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void
    {
        static_assert(sizeof(VkDrawIndexedIndirectCommand) == sizeof(DrawIndexedIndirectCommand));

        vkCmdDrawIndexedIndirect(
            active_command_buffer->command_buffer,
            assert_cast<VulkanBuffer>(current_graphics_state.indirect_buffer)->buffer,
            offset_bytes,
            draw_count,
            sizeof(DrawIndexedIndirectCommand)
        );
    }

    // Debug Markers
    auto VulkanCommandList::push_command_label(std::string_view name, math::float4 color) -> void
    {
        auto debug_label = VkDebugUtilsLabelEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        debug_label.pLabelName = name.data();
        debug_label.color[0] = color.x;
        debug_label.color[1] = color.y;
        debug_label.color[2] = color.z;
        debug_label.color[3] = color.w;
        vkCmdBeginDebugUtilsLabelEXT(active_command_buffer->command_buffer, &debug_label);
    }

    auto VulkanCommandList::pop_command_label() -> void
    {
        vkCmdEndDebugUtilsLabelEXT(active_command_buffer->command_buffer);
    }

    // Profiling
    auto VulkanCommandList::begin_time_query() -> void {}
    auto VulkanCommandList::end_time_query() -> void {}

    // Resource State Management
    auto VulkanCommandList::enbale_automatic_barriers(bool enable) -> void
    {
        automatic_barriers = enable;
    }

    auto VulkanCommandList::begin_tracking_buffer(BufferHandle buffer, EResourceStates current_state) -> void
    {
        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        resource_state_tracker.begin_tracking_buffer_state(&vulkan_buffer->tracker, current_state);
    }

    auto VulkanCommandList::begin_tracking_texture(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates current_state) -> void
    {
        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        resource_state_tracker.begin_tracking_texture_state(&vulkan_texture->tracker, subresources, current_state);
    }

    auto VulkanCommandList::set_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void
    {
        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        resource_state_tracker.require_buffer_state(&vulkan_buffer->tracker, dst_state);
    }

    auto VulkanCommandList::set_texture_state(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates dst_state) -> void
    {
        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        resource_state_tracker.require_texture_state(&vulkan_texture->tracker, subresources, dst_state);
    }

    auto VulkanCommandList::lock_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void
    {
        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        resource_state_tracker.lock_buffer_state(&vulkan_buffer->tracker, dst_state);
    }

    auto VulkanCommandList::lock_texture_state(TextureHandle texture, EResourceStates dst_state) -> void
    {
        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        resource_state_tracker.lock_texture_state(&vulkan_texture->tracker, TextureSubresourceSet::all(&vulkan_texture->info), dst_state);
    }

    // Execution Control
    auto VulkanCommandList::flush() -> void
    {
        if (!info.enable_immediate_submit) {
            CNE_ERROR("Flush only supported in immediate mode! Please submit the command list by device.");
            return;
        }

        auto queue = parent->queue(info.queue_type);

        auto cmd_list = this;
        auto wait_time = queue->submit({&cmd_list, 1});
        queue->wait_command_list(wait_time, UINT64_MAX);
    }

    // State Queries
    auto VulkanCommandList::buffer_state(BufferHandle buffer) -> EResourceStates
    {
        auto vulkan_buffer = assert_cast<VulkanBuffer>(buffer);

        return resource_state_tracker.buffer_state(&vulkan_buffer->tracker);
    }

    auto VulkanCommandList::texture_state(TextureHandle texture, uint32_t level, uint32_t layer) -> EResourceStates
    {
        auto vulkan_texture = assert_cast<VulkanTexture>(texture);

        return resource_state_tracker.texture_subresource_state(&vulkan_texture->tracker, level, layer);
    }

    // Synchronization
    auto VulkanCommandList::commit_barriers(EQueueType src_queue, EQueueType dst_queue) -> void
    {
        if (resource_state_tracker.texture_barriers.empty() && resource_state_tracker.buffer_barriers.empty()) return;

        end_rendering();

        auto vk_buffer_barriers = std::vector<VkBufferMemoryBarrier2>{};
        auto vk_image_barriers = std::vector<VkImageMemoryBarrier2>{};

        vk_buffer_barriers.reserve(resource_state_tracker.buffer_barriers.size());
        vk_image_barriers.reserve(resource_state_tracker.texture_barriers.size());

        // TODO: use ranges::.
        std::ranges::transform(
            resource_state_tracker.buffer_barriers,
            std::back_inserter(vk_buffer_barriers),
            [&] (auto& barrier) -> VkBufferMemoryBarrier2 {
                auto vulkan_buffer = (VulkanBuffer*) barrier.buffer;
                // Directly update the state of the tracked buffer.
                resource_state_tracker.find_tracked_buffer_state(&vulkan_buffer->tracker, true)->state = barrier.dst_state;
                return VkBufferMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .srcStageMask        = pipeline_stage_from_states(barrier.src_state), // TODO: improve.
                    .srcAccessMask       = convert_to_vk_access_type(barrier.src_state),
                    .dstStageMask        = pipeline_stage_from_states(barrier.dst_state),
                    .dstAccessMask       = convert_to_vk_access_type(barrier.dst_state),
                    .srcQueueFamilyIndex = parent->queue_family(src_queue),
                    .dstQueueFamilyIndex = parent->queue_family(dst_queue),
                    .buffer              = vulkan_buffer->buffer,
                    .offset              = 0,
                    .size                = vulkan_buffer->info.size_bytes,
                };
            }
        );

        std::ranges::transform(
            resource_state_tracker.texture_barriers,
            std::back_inserter(vk_image_barriers),
            [&] (auto& barrier) -> VkImageMemoryBarrier2 {
                auto vulkan_texture = (VulkanTexture*) barrier.texture;
                // Directly update the state of the tracked texture. Maybe needn't to process subresource?
                resource_state_tracker.find_tracked_texture_state(&vulkan_texture->tracker, true)->state = barrier.dst_state;
                return VkImageMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    .srcStageMask        = pipeline_stage_from_states(barrier.src_state), // TODO: improve.
                    .srcAccessMask       = convert_to_vk_access_type(barrier.src_state),
                    .dstStageMask        = pipeline_stage_from_states(barrier.dst_state),
                    .dstAccessMask       = convert_to_vk_access_type(barrier.dst_state),
                    .oldLayout           = image_layout_from_access(barrier.src_state, is_depth_stencil_format(convert_to_vk_format(vulkan_texture->info.format))),
                    .newLayout           = image_layout_from_access(barrier.dst_state, is_depth_stencil_format(convert_to_vk_format(vulkan_texture->info.format))),
                    .srcQueueFamilyIndex = parent->queue_family(src_queue),
                    .dstQueueFamilyIndex = parent->queue_family(dst_queue),
                    .image               = vulkan_texture->image,
                    .subresourceRange    = {
                        .aspectMask     = aspect_flag_from_format(convert_to_vk_format(vulkan_texture->info.format)),
                        .baseMipLevel   = barrier.mip_level,
                        .levelCount     = barrier.contain_all_resource ? vulkan_texture->info.num_mips : 1,
                        .baseArrayLayer = barrier.array_layer,
                        .layerCount     = barrier.contain_all_resource ? vulkan_texture->info.num_layers : 1,
                    },
                };
            }
        );

        auto dependency_info = VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency_info.bufferMemoryBarrierCount = (uint32_t) vk_buffer_barriers.size();
        dependency_info.pBufferMemoryBarriers    = vk_buffer_barriers.data();
        dependency_info.imageMemoryBarrierCount  = (uint32_t) vk_image_barriers.size();
        dependency_info.pImageMemoryBarriers     = vk_image_barriers.data();

        vkCmdPipelineBarrier2(active_command_buffer->command_buffer, &dependency_info);

        resource_state_tracker.clear_barriers();
    }

    auto VulkanCommandList::wait_for_submit(EQueueType submit_queue_type, uint64_t submit_time) -> void
    {
        auto wait_queue = parent->queue(submit_queue_type);
        auto queue = parent->queue(info.queue_type);
        queue->add_wait_semaphore(wait_queue->timeline, submit_time);
    }

    auto VulkanCommandList::device() -> IDevice*
    {
        auto ret = parent;
        return parent;
    }

    auto VulkanCommandList::finish_submission(VulkanQueue* queue, uint64_t submission_time) -> void
    {
        resource_state_tracker.finish_tracking();

        auto recording_time = active_command_buffer->recording_time;
        block_pool->update_block_version(
            make_version(recording_time, queue->type, false),
            make_version(submission_time, queue->type, true)
        );

        active_command_buffer = {};
    }

    auto VulkanCommandList::end_rendering() -> void
    {
        if (current_graphics_state.render_target) {
            vkCmdEndRendering(active_command_buffer->command_buffer);
            current_graphics_state.render_target = {};
        }
    }

    auto VulkanCommandList::set_dynamic_state() -> void
    {

    }
}
