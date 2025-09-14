#include "resource.hpp"
#include "device.hpp"
#include <core/assert.hpp>

namespace cannele::inline graphics::rhi
{
    DeviceChild::DeviceChild(IDevice* device)
        : device(device->weak_from_this())
    {}

    IResource::IResource(IDevice* device)
        : DeviceChild(device)
    {}

    auto depth_attachment_create_info(math::uint2 in_extent, EFormat in_format) -> TextureCreateInfo
    {
        return TextureCreateInfo{
            .dimension     = ETextureDimension::tex_2d,
            .format        = in_format,
            .usage         = ETextureUsage::depth_stencil_attachment | ETextureUsage::transfer_src,
            .extent        = {in_extent.x, in_extent.y, 1},
            .final_state   = EResourceStates::depth_stencil_attachment,
        };
    }

    auto adapt_to_buffer(BufferRange* range, BufferCreateInfo const* info) -> void
    {
        range->size = std::min(range->size, info->size_bytes);
        CNE_ASSERT_WITH(range->offset + range->size <= info->size_bytes, "Buffer range out of bound.");
    }

    auto contain_all_resources(TextureSubresourceRange* subresources, TextureCreateInfo const* info) -> bool
    {
        if (subresources->mip_level > 0u || subresources->mip_level + subresources->mip_count < info->mip_count) return false;

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                if (subresources->layer > 0u || subresources->layer + subresources->layer_count < info->layer_count) return false;
            }
            default: return true;
        }
    }

    auto adapt_to_texture(TextureSubresourceRange* subresources, TextureCreateInfo const* info, bool signle_mip_level) -> void
    {
        auto result = TextureSubresourceRange{};
        result.mip_level = subresources->mip_level;

        if (signle_mip_level) {
            subresources->mip_count = 1;
        } else {
            auto max_mip_levels = std::min(subresources->mip_level + subresources->mip_count, info->mip_count);
            subresources->mip_count = std::max(1u, max_mip_levels - subresources->mip_level);
        }

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                auto max_array_layers = std::min(subresources->layer + subresources->layer_count, info->layer_count);
                subresources->layer_count = std::max(1u, max_array_layers - subresources->layer);
                break;
            }
            default: {
                subresources->layer = 0;
                subresources->layer_count = 1;
                break;
            }
        }
    }

    auto RHIBuffer::resolve_range(BufferRange const& range) -> BufferRange
    {
        auto result = range;
        auto desc = description();
        result.size = std::min(range.size, desc->size_bytes);

        return result;
    }

    auto RHITexture::resolve_subresource_rage(TextureSubresourceRange const& subresources) -> TextureSubresourceRange
    {
        auto result = TextureSubresourceRange{};
        auto desc = description();
        result.mip_level = std::min(subresources.mip_level, desc->mip_count - 1);
        result.mip_count = std::min(subresources.mip_count, desc->mip_count - result.mip_level);
        result.layer = std::min(subresources.layer, desc->layer_count - 1);
        result.layer_count = std::min(subresources.layer_count, desc->layer_count - result.layer);

        return result;
    }

    auto RHITexture::contains_all_subresources(TextureSubresourceRange const& subresources) -> bool
    {
        auto desc = description();

        return (true
            && subresources.mip_level == 0
            && subresources.mip_count == desc->mip_count
            && subresources.layer == 0
            && subresources.layer_count == desc->layer_count
        );
    }

    CommandEncoder::CommandEncoder(IDevice* device)
        : IResource(device)
        , graphics_encoder(std::make_unique<GraphicsCommandEncoder>(this))
        , compute_encoder(std::make_unique<ComputeCommandEncoder>(this))
    {}

    auto CommandEncoder::copy_buffer(
        BufferHandle src_buffer,
        size_t src_offset,
        BufferHandle dst_buffer,
        size_t dst_offset,
        size_t size
    ) -> void
    {
        command_list->write(commands::copy_buffer{
            .src_buffer = src_buffer.get(),
            .src_offset = src_offset,
            .dst_buffer = dst_buffer.get(),
            .dst_offset = dst_offset,
            .size       = size,
        });
    }

    auto CommandEncoder::copy_texture(
        TextureHandle src_texture,
        TextureSubresourceRange src_subresources,
        Offset3D src_offset,
        TextureHandle dst_texture,
        TextureSubresourceRange dst_subresources,
        Offset3D dst_offset,
        Extent3D extent
    ) -> void
    {
        command_list->write(commands::copy_texture{
            .src_texture        = src_texture.get(),
            .src_subresources   = src_texture->resolve_subresource_rage(src_subresources),
            .src_offset         = src_offset,
            .dst_texture        = dst_texture.get(),
            .dst_subresources   = dst_texture->resolve_subresource_rage(dst_subresources),
            .dst_offset         = dst_offset,
            .extent             = extent,
        });
    }

    auto CommandEncoder::upload_texture_data(
        TextureHandle texture,
        TextureSliceDataView data,
        TextureSubresourceRange subresources,
        Offset3D offset,
        Extent3D extent
    ) -> void
    {
        auto texture_desc = texture->description();
        subresources = texture->resolve_subresource_rage(subresources);
        auto layouts = command_list->allocate_data<SubresourceLayout>(subresources.mip_count * subresources.layer_count);
        auto iter = layouts.begin();

        auto total_size = 0zu;
        for (auto layer = subresources.layer; layer < subresources.layer_count; layer++) {
            for (auto mip = subresources.mip_level; mip < subresources.mip_count; mip++) {
                iter->size.width = std::max(texture_desc->extent.width >> mip, 1u);
                iter->size.height = std::max(texture_desc->extent.height >> mip, 1u);
                iter->size.depth = std::max(texture_desc->extent.depth >> mip, 1u);

                auto format_info = get_format_info(texture_desc->format);
                auto num_cols    = (iter->size.width + format_info->blocks - 1) / format_info->blocks;
                auto num_rows    = (iter->size.height + format_info->blocks - 1) / format_info->blocks;

                iter->block_width      = format_info->bytes_per_block;
                iter->block_height     = format_info->bytes_per_block;
                iter->stride_per_row   = num_cols * format_info->bytes_per_block;
                iter->stride_per_col   = num_rows * format_info->bytes_per_block;
                iter->stride_per_layer = iter->stride_per_row * num_rows;
                iter->row_count        = num_rows;
                iter->total_size       = iter->stride_per_row * num_rows * iter->size.depth;

                total_size += iter->total_size ;

                iter++;
            }
        }

        iter = layouts.begin();

        // TODO: Other slices.
        auto device = get_device();
        auto buffer_block = get_device()->buffer_block_pool->allocate_buffer_block(iter->total_size);
        auto staging_buffer = buffer_block->buffer();

        auto min_row_bytes = std::min(iter->stride_per_row, (size_t) data.extent(0));
        auto mapped_ptr = buffer_block->map();
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
        buffer_block->unmap();

        command_list->write(commands::upload_texture_data{
            .src_buffer  = buffer_block->buffer(),
            .src_offset  = buffer_block->offset(),
            .dst_texture = texture.get(),
            .dst_offset  = offset,
            .extent      = extent,
            .layouts     = layouts,
        });
    }

    auto CommandEncoder::upload_buffer_data(
        BufferHandle buffer,
        size_t offset,
        std::span<std::byte const> data
    ) -> void
    {
        auto device = get_device();
        if (buffer->description()->memory_type == EMemoryType::cpu_upload) {
            auto mapped_ptr = device->map_buffer(buffer);
            std::memcpy(mapped_ptr + offset, data.data(), data.size());
            device->unmap_buffer(buffer);
            return;
        }

        auto buffer_block = get_device()->buffer_block_pool->allocate_buffer_block(buffer->description()->size_bytes);
        auto staging_buffer = buffer_block->buffer();
        auto mapped_ptr = buffer_block->map();
        std::memcpy(mapped_ptr, data.data(), data.size());
        buffer_block->unmap();

        command_list->write(commands::copy_buffer{
            .src_buffer = buffer_block->buffer(),
            .src_offset = buffer_block->offset(),
            .dst_buffer = buffer.get(),
            .dst_offset = offset,
            .size       = data.size(),
        });
    }

    auto CommandEncoder::clear_buffer_uint(
        BufferHandle buffer,
        BufferRange range,
        uint32_t clear_value
    ) -> void
    {
        command_list->write(commands::clear_buffer_uint{
            .buffer      = buffer.get(),
            .range       = range,
            .clear_value = clear_value,
        });
    }

    auto CommandEncoder::clear_texture_uint(
        TextureHandle texture,
        TextureSubresourceRange subresources,
        math::uint4 clear_color
    ) -> void
    {
        command_list->write(commands::clear_texture_uint{
            .texture      = texture.get(),
            .subresources = texture->resolve_subresource_rage(subresources),
            .clear_color  = clear_color,
        });
    }

    auto CommandEncoder::clear_texture_float(
        TextureHandle texture,
        TextureSubresourceRange subresources,
        math::float4 clear_color
    ) -> void
    {
        command_list->write(commands::clear_texture_float{
            .texture      = texture.get(),
            .subresources = texture->resolve_subresource_rage(subresources),
            .clear_color  = clear_color,
        });
    }

    auto CommandEncoder::clear_texture_depth_stencil(
        TextureHandle texture,
        TextureSubresourceRange subresources,
        std::optional<float> clear_depth,
        std::optional<uint8_t> clear_stencil
    ) -> void
    {
        command_list->write(commands::clear_texture_depth_stencil{
            .texture      = texture.get(),
            .subresources = texture->resolve_subresource_rage(subresources),
            .clear_depth  = clear_depth,
            .clear_stencil= clear_stencil,
        });
    }

    auto CommandEncoder::resolve_query(
        TimerQueryHandle query,
        uint32_t query_index,
        BufferHandle buffer,
        size_t offset,
        uint32_t query_count
    ) -> void
    {
        command_list->write(commands::resolve_query{
            .query       = query.get(),
            .query_index = query_index,
            .query_count = query_count,
            .buffer      = buffer.get(),
            .offset      = offset,
        });
    }

    auto CommandEncoder::set_buffer_state(BufferHandle buffer, EResourceStates state) -> void
    {
        command_list->write(commands::set_buffer_state{
            .buffer = buffer.get(),
            .state  = state,
        });
    }

    auto CommandEncoder::set_texture_state(TextureHandle texture, TextureSubresourceRange subresources, EResourceStates state) -> void
    {
        command_list->write(commands::set_texture_state{
            .texture      = texture.get(),
            .subresources = texture->resolve_subresource_rage(subresources),
            .state        = state,
        });
    }

    auto CommandEncoder::commit_barriers() -> void
    {
        command_list->write(commands::insert_global_barrier{});
    }

    auto CommandEncoder::push_debug_label(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::push_command_label{
            .name  = name,
            .color = color,
        });
    }

    auto CommandEncoder::pop_debug_label() -> void
    {
        command_list->write(commands::pop_command_label{});
    }

    auto CommandEncoder::insert_debug_marker(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::insert_debug_marker{
            .name  = name,
            .color = color,
        });
    }

    auto CommandEncoder::write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void
    {
        command_list->write(commands::write_timestamp{
            .query       = query.get(),
            .query_index = query_index,
        });
    }

    auto CommandEncoder::begin_graphics_pass(
        std::span<ColorAttachment> color_attachments,
        std::optional<DepthStencilAttachment> depth_stencil_attachment
    ) -> GraphicsCommandEncoder*
    {
        command_list->write(commands::begin_graphics_pass{
            .color_attachments        = color_attachments,
            .depth_stencil_attachment = depth_stencil_attachment ? &depth_stencil_attachment.value() : nullptr
        });
        graphics_encoder->command_list = command_list;

        return graphics_encoder.get();
    }

    auto CommandEncoder::begin_compute_pass() -> ComputeCommandEncoder*
    {
        command_list->write(commands::begin_compute_pass{});
        compute_encoder->command_list = command_list;

        return compute_encoder.get();
    }

    GraphicsCommandEncoder::GraphicsCommandEncoder(CommandEncoder* encoder)
        : encoder(encoder)
    {}

    auto GraphicsCommandEncoder::bind_pipeline(GraphicsPipelineHandle in_pipeline) -> ShaderObject*
    {
        pipeline = std::move(in_pipeline);
        root_object = encoder->get_device()->create_root_shader_object(pipeline->program());

        return root_object.get();
    }

    auto GraphicsCommandEncoder::set_graphics_state(GraphicsState state) -> void
    {
        CNE_ASSERT(!state.scissors.empty());
        command_list->write(commands::set_graphics_state{
            .state          = state,
            .pipeline       = pipeline.get(),
            .binding_data   = encoder->binding_data(root_object.get()),
        });
    }

    auto GraphicsCommandEncoder::draw(DrawArguments const& args) -> void
    {
        command_list->write(commands::draw{
            .args = args,
        });
    }

    auto GraphicsCommandEncoder::draw_indexed(DrawArguments const& args) -> void
    {
        command_list->write(commands::draw_indexed{
            .args = args,
        });
    }

    auto GraphicsCommandEncoder::draw_indirect(BufferHandle indirect_buffer, uint32_t offset, uint32_t draw_count) -> void
    {
        command_list->write(commands::draw_indirect{
            .draw_count = draw_count,
            .args_buffer = {indirect_buffer, offset},
        });
    }

    auto GraphicsCommandEncoder::draw_indexed_indirect(BufferHandle indirect_buffer, uint32_t offset, uint32_t draw_count) -> void
    {
        command_list->write(commands::draw_indexed_indirect{
            .draw_count = draw_count,
            .args_buffer = {indirect_buffer, offset},
        });
    }

    auto GraphicsCommandEncoder::dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
    {
        command_list->write(commands::dispatch_mesh{
            .group_count_x = group_count_x,
            .group_count_y = group_count_y,
            .group_count_z = group_count_z,
        });
    }

    auto GraphicsCommandEncoder::dispatch_mesh_indirect(BufferHandle indirect_buffer, uint32_t offset, uint32_t count) -> void
    {
        command_list->write(commands::dispatch_mesh_indirect{
            .args_buffer = {indirect_buffer, offset},
        });
    }

    auto GraphicsCommandEncoder::push_command_label(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::push_command_label{
            .name  = name,
            .color = color,
        });
    }

    auto GraphicsCommandEncoder::pop_command_label() -> void
    {
        command_list->write(commands::pop_command_label{});
    }

    auto GraphicsCommandEncoder::insert_debug_marker(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::insert_debug_marker{
            .name  = name,
            .color = color,
        });
    }

    auto GraphicsCommandEncoder::write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void
    {
        command_list->write(commands::write_timestamp{
            .query       = query.get(),
            .query_index = query_index,
        });
    }

    auto GraphicsCommandEncoder::finish() -> void
    {
        command_list->write(commands::end_graphics_pass{});
        command_list = nullptr;
        pipeline = {};
    }

    ComputeCommandEncoder::ComputeCommandEncoder(CommandEncoder* encoder)
        : encoder(encoder)
    {}

    auto ComputeCommandEncoder::bind_pipeline(ComputePipelineHandle in_pipeline) -> ShaderObject*
    {
        pipeline = std::move(in_pipeline);
        root_object = encoder->get_device()->create_root_shader_object(pipeline->program());

        return root_object.get();
    }

    auto ComputeCommandEncoder::set_compute_state() -> void
    {
        command_list->write(commands::set_compute_state{
            .pipeline       = pipeline.get(),
            .binding_data   = encoder->binding_data(root_object.get()),
        });
    }

    auto ComputeCommandEncoder::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void
    {
        command_list->write(commands::dispatch_compute{
            .group_count_x = group_count_x,
            .group_count_y = group_count_y,
            .group_count_z = group_count_z,
        });
    }

    auto ComputeCommandEncoder::dispatch_indirect(BufferHandle indirect_buffer, uint32_t offset) -> void
    {
        command_list->write(commands::dispatch_compute_indirect{
            .args_buffer = {indirect_buffer, offset},
        });
    }

    auto ComputeCommandEncoder::push_command_label(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::push_command_label{
            .name  = name,
            .color = color,
        });
    }

    auto ComputeCommandEncoder::pop_command_label() -> void
    {
        command_list->write(commands::pop_command_label{});
    }

    auto ComputeCommandEncoder::insert_debug_marker(std::string_view name, math::float4 color) -> void
    {
        command_list->write(commands::insert_debug_marker{
            .name  = name,
            .color = color,
        });
    }

    auto ComputeCommandEncoder::write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void
    {
        command_list->write(commands::write_timestamp{
            .query       = query.get(),
            .query_index = query_index,
        });
    }

    auto ComputeCommandEncoder::finish() -> void
    {
        command_list->write(commands::end_compute_pass{});
        command_list = nullptr;

        pipeline = {};
    }

    RHICommandBuffer::RHICommandBuffer(IDevice* device)
        : IResource(device)
        , arena(std::make_unique<Arena>())
        , command_list(std::make_unique<CommandList>(arena.get(), &tracked_resources))
    {}
}
