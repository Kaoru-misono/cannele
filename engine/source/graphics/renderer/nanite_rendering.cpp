#include "nanite_rendering.hpp"
#include "shading_type.hpp"

#include <graphics/RHI/device.hpp>
#include <nanite.slang.hpp>
#include <math/tool.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        using namespace rhi;
    }

    auto instance_culling(rhi::CommandEncoderHandle encoder, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>
    {
        auto device = encoder->get_device();

        auto buffer_info = BufferCreateInfo{
            .memory_type = EMemoryType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
            .final_state = EResourceStates::storage_read,
        };

        buffer_info.size_bytes = sizeof(uint2);
        auto cluster_group_count_buffer = device->create_buffer("Cluster Group Count Buffer", &buffer_info);

        auto cluster_group_count = context->asset->data.meshlet_groups.size();
        buffer_info.size_bytes = sizeof(uint2) * cluster_group_count;
        auto cluster_group_id_buffer = device->create_buffer("Cluster Group ID Buffer", &buffer_info);

        context->meshlet_group_count_buffer = cluster_group_count_buffer;
        context->meshlet_group_id_buffer = cluster_group_id_buffer;

        {
            encoder->clear_buffer_uint(cluster_group_count_buffer);
            encoder->clear_buffer_uint(cluster_group_id_buffer);
            auto push_constant = InstanceCullingPushConstant{};
            push_constant.frame_view_buffer = context->frame_view_buffer->descriptor_handle();
            push_constant.cluster_group_count_buffer = cluster_group_count_buffer->descriptor_handle();
            push_constant.cluster_group_id_buffer = cluster_group_id_buffer->descriptor_handle();
            push_constant.scene_buffer = context->gpu_scene_buffer->descriptor_handle();

            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("nanite_culling", "main_instance_culling_cs")
            };
            auto compute_pipeline = device->create_compute_pipeline("Instance Culling Pipeline", &compute_pipeline_info);

            encoder->push_debug_label("Instance Culling");

            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(compute_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["frame_views"].set_bindless_buffer(context->frame_view_buffer, EResourceStates::storage_read);
            writer["scene"].set_bindless_buffer(context->gpu_scene_buffer, EResourceStates::storage_read);
            writer["cluster_group_counts"].set_bindless_buffer(cluster_group_count_buffer, EResourceStates::storage_write);
            writer["cluster_group_ids"].set_bindless_buffer(cluster_group_id_buffer, EResourceStates::storage_write);

            compute_encoder->set_compute_state();
            compute_encoder->dispatch(context->instances_data.size(), 1, 1);
            compute_encoder->finish();

            encoder->pop_debug_label();
        }

        buffer_info.usage = EBufferUsage::indirect;
        buffer_info.size_bytes = sizeof(math::uint4);
        auto indirect_buffer = device->create_buffer("Cluster Group Culling Indirect Buffer", &buffer_info);
        {
            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("assemble_indirect_dispatch_command", "main_assemble_indirect_dispatch_cmd_cs")
            };
            auto assemble_indirect_cmd_pipeline = device->create_compute_pipeline("Indirect Command Assembly Pipeline", &compute_pipeline_info);
            encoder->push_debug_label("Assemble Indirect Command");

            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(assemble_indirect_cmd_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["count_buffer"].set_bindless_buffer(cluster_group_count_buffer, EResourceStates::storage_read);
            writer["cmds_buffer"].set_bindless_buffer(indirect_buffer, EResourceStates::storage_write);
            writer["group_size"].set_data(64);

            compute_encoder->set_compute_state();
            compute_encoder->dispatch(1, 1, 1);
            compute_encoder->finish();

            encoder->pop_debug_label();
        }

        buffer_info.usage = EBufferUsage::storage | EBufferUsage::transfer_dst;
        buffer_info.size_bytes = sizeof(uint);
        auto& meshlet_count_buffer = context->meshlet_count_buffer;
        meshlet_count_buffer = device->create_buffer("Meshlet Count Buffer", &buffer_info);
        encoder->clear_buffer_uint(meshlet_count_buffer);

        auto lod_0_meshlet_count = 0u;
        for (auto& primitive: context->primitive_infos_data) {
            lod_0_meshlet_count += primitive.lod_0_meshlet_count;
        }

        buffer_info.size_bytes = sizeof(math::uint4) * lod_0_meshlet_count;
        auto& meshlet_cmd_buffer = context->meshlet_cmd_buffer;
        meshlet_cmd_buffer = device->create_buffer("Meshlet Command Buffer", &buffer_info);
        encoder->clear_buffer_uint(meshlet_cmd_buffer);

        {
            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("nanite_culling", "main_cluster_group_culling_cs"),
            };
            auto compute_pipeline = device->create_compute_pipeline("Cluster Culling Pipeline", &compute_pipeline_info);

            encoder->push_debug_label("Cluster Group Culling");

            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(compute_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["frame_views"].set_bindless_buffer(context->frame_view_buffer, EResourceStates::storage_read);
            writer["scene"].set_bindless_buffer(context->gpu_scene_buffer, EResourceStates::storage_read);
            writer["cluster_group_counts"].set_bindless_buffer(cluster_group_count_buffer, EResourceStates::storage_read);
            writer["cluster_group_ids"].set_bindless_buffer(cluster_group_id_buffer, EResourceStates::storage_read);
            writer["meshlet_counter"].set_bindless_buffer(meshlet_count_buffer, EResourceStates::storage_write);
            writer["meshlet_cmds"].set_bindless_buffer(meshlet_cmd_buffer, EResourceStates::storage_write);
            compute_encoder->set_compute_state();
            compute_encoder->dispatch_indirect(indirect_buffer);
            compute_encoder->finish();

            encoder->pop_debug_label();
        }

        return {meshlet_count_buffer, meshlet_cmd_buffer};
    }

    auto nanite_render_pass_0(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void
    {
        auto device = encoder->get_device();

        auto buffer_info = BufferCreateInfo{};
        buffer_info.memory_type = EMemoryType::gpu_only;

        buffer_info.size_bytes = sizeof(math::uint4);
        buffer_info.usage = EBufferUsage::storage | EBufferUsage::indirect;
        auto meshlet_indirect_dispatch_buffer = device->create_buffer("Meshlet Indirect Dispatch Buffer", &buffer_info);

        auto& meshlet_count_buffer = context->meshlet_count_buffer;
        auto& meshlet_cmd_buffer = context->meshlet_cmd_buffer;

        {
            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("nanite_raster", "main_assemble_indirect_cmd_cs"),
            };
            auto compute_pipeline = device->create_compute_pipeline("Indirect Draw Command Assemble Pipeline", &compute_pipeline_info);

            encoder->push_debug_label("Meshlet Indirect Command Assemble");
            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(compute_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["meshlet_counter"].set_bindless_buffer(meshlet_count_buffer, EResourceStates::storage_read);
            writer["indirect_param_buffer"].set_bindless_buffer(meshlet_indirect_dispatch_buffer, EResourceStates::storage_write);
            compute_encoder->set_compute_state();
            compute_encoder->dispatch(1, 1, 1);
            compute_encoder->finish();

            encoder->pop_debug_label();
        }

//         auto passed_meshlet_count_buffer = device->create_buffer("Passed Meshlet Count Buffer", meshlet_count_buffer->description());
//         command_list->clear_buffer_uint(passed_meshlet_count_buffer);
//         auto passed_meshlet_cmd_buffer = device->create_buffer("Passed Meshlet Command Buffer", meshlet_cmd_buffer->description());
//         command_list->clear_buffer_uint(passed_meshlet_cmd_buffer);
//         context->meshlet_filtered_cmd_buffer = passed_meshlet_cmd_buffer;
//         {
//             auto push_constant = NaniteRenderFilterMeshletCmdsPushConstant{};
//             push_constant.camera_view = context->frame_view_buffer->descriptor_handle();
//             push_constant.gpu_scene = context->gpu_scene_buffer->descriptor_handle();
//             push_constant.meshlet_count_buffer = meshlet_count_buffer->descriptor_handle();
//             push_constant.meshlet_cmd_buffer = meshlet_cmd_buffer->descriptor_handle();
//             push_constant.passed_meshlet_count_buffer = passed_meshlet_count_buffer->descriptor_handle();
//             push_constant.passed_meshlet_cmd_buffer = passed_meshlet_cmd_buffer->descriptor_handle();
//
//             command_list->set_buffer_state(meshlet_count_buffer, EResourceStates::storage_buffer_read_only);
//             command_list->set_buffer_state(meshlet_cmd_buffer, EResourceStates::storage_buffer_read_only);
//             command_list->set_buffer_state(passed_meshlet_count_buffer, EResourceStates::storage_buffer_read_write);
//             command_list->set_buffer_state(passed_meshlet_cmd_buffer, EResourceStates::storage_buffer_read_write);
//
//             auto compute_pipeline_info = ComputePipelineCreateInfo{
//                 .compute_shader = device->shader_factory()->get_shader<NaniteRenderFilterMeshletCmdCS>(),
//                 .push_constant_size = sizeof(NaniteRenderFilterMeshletCmdsPushConstant),
//             };
//             auto compute_pipeline = device->create_compute_pipeline("Meshlet Command Filter Pipeline", &compute_pipeline_info);
//
//             auto compute_state = ComputeState{
//                 .pipeline = compute_pipeline,
//                 .indirect_buffer = meshlet_indirect_dispatch_buffer,
//             };
//
//             command_list->push_command_label("Meshlet Command Filter");
//             command_list->set_compute_state(&compute_state);
//             command_list->push_constants(push_constant);
//             command_list->dispatch_indirect();
//             command_list->pop_command_label();
//         }

        buffer_info.size_bytes = sizeof(math::uint4);
        buffer_info.usage = EBufferUsage::storage | EBufferUsage::indirect;
        auto indirect_mesh_dipatch_buffer = device->create_buffer("Indirect Mesh Dispatch Buffer", &buffer_info);
        {
            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("assemble_indirect_dispatch_command", "main_assemble_indirect_dispatch_cmd_cs"),
            };
            auto assemble_indirect_cmd_pipeline = device->create_compute_pipeline("Indirect Command Assembly Pipeline", &compute_pipeline_info);

            encoder->push_debug_label("Indirect Command Assembly");
            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(assemble_indirect_cmd_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["count_buffer"].set_bindless_buffer(meshlet_count_buffer, EResourceStates::storage_read);
            writer["cmds_buffer"].set_bindless_buffer(indirect_mesh_dipatch_buffer, EResourceStates::storage_write);
            writer["group_size"].set_data(1);
            compute_encoder->set_compute_state();
            compute_encoder->dispatch(1, 1, 1);
            compute_encoder->finish();

            encoder->pop_debug_label();
        }

        auto& backbuffer = context->backbuffer;
        auto framebuffer_size = math::float2{backbuffer->description()->extent.width, backbuffer->description()->extent.height};
        {
            auto& depth_texture = context->depth_texture;
            auto texture_info = TextureCreateInfo{};
            texture_info.dimension   = ETextureDimension::tex_2d;
            texture_info.extent      = backbuffer->description()->extent;
            texture_info.format      = EFormat::r32_uint;
            texture_info.usage       = ETextureUsage::color_attachment | ETextureUsage::sampled;
            texture_info.final_state = EResourceStates::color_attachment;
            context->visibility_texture = device->create_texture("Visibility Texture", &texture_info);

            auto& visiblity_texture = context->visibility_texture;
            auto pipeline_info = GraphicsPipelineCreateInfo{};
            pipeline_info.program = device->create_graphics_shader_program("nanite_raster", "main_nanite_mesh_pass_ms", "main_nanite_visibility_buffer_pass_fs");
            pipeline_info.colors.emplace_back(ColorAttachmentInfo{visiblity_texture->description()->format});
            pipeline_info.depth_stencil = DepthStencilAttachmentInfo{depth_texture->description()->format};
            auto pipeline = device->create_graphics_pipeline("Nanite Render Pipeline", &pipeline_info);

            auto viewports = std::vector<Viewport>{Viewport{0.0f, 0.0f, (float) framebuffer_size.x, (float) framebuffer_size.y}};
            auto scissors = std::vector<Scissor>{Scissor(0, 0, framebuffer_size.x, framebuffer_size.y)};
            auto blend_state = std::vector<BlendState>{BlendState{}};
            auto graphics_state = GraphicsState{};
            graphics_state.viewports = viewports;
            graphics_state.scissors = scissors;
            graphics_state.blend_states = blend_state;

            encoder->push_debug_label("Mesh Raster");
            auto color_attachments = std::vector<ColorAttachment>{
                ColorAttachment{
                    .view = visiblity_texture->view(),
                },
            };
            auto depth_stencil_attachment = DepthStencilAttachment{
                .view = depth_texture->view(),
            };
            auto graphics_encoder = encoder->begin_graphics_pass(color_attachments, depth_stencil_attachment);
            auto object = graphics_encoder->bind_pipeline(pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["camera_view"].set_bindless_buffer(context->frame_view_buffer, EResourceStates::storage_read);
            writer["scene"].set_bindless_buffer(context->gpu_scene_buffer, EResourceStates::storage_read);
            writer["draw_meshlet_cmd"].set_bindless_buffer(meshlet_cmd_buffer, EResourceStates::storage_read);
            graphics_encoder->set_graphics_state(graphics_state);
            graphics_encoder->dispatch_mesh_indirect(indirect_mesh_dipatch_buffer);
            graphics_encoder->finish();

            encoder->pop_debug_label();
        }
    }

    auto nanite_visualize(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void
    {
        auto device = encoder->get_device();

        auto& backbuffer = context->backbuffer;
        auto& depth_texture = context->depth_texture;
        auto& visiblity_texture = context->visibility_texture;
        auto framebuffer_size = math::float2{backbuffer->description()->extent.width, backbuffer->description()->extent.height};

        auto pipeline_info = GraphicsPipelineCreateInfo{};
        pipeline_info.program = device->create_graphics_shader_program("full_screen", "main_fullscreen_vs", "nanite_visualization", "main_nanite_visualize_fs");
        pipeline_info.colors.emplace_back(ColorAttachmentInfo{context->backbuffer->description()->format});
        pipeline_info.depth_stencil = DepthStencilAttachmentInfo{depth_texture->description()->format};
        pipeline_info.depth_stencil.state.enable_depth_write = false;
        auto pipeline = device->create_graphics_pipeline("Nanite Visualize Pipeline", &pipeline_info);

        auto viewports = std::vector<Viewport>{Viewport{0.0f, 0.0f, (float) framebuffer_size.x, (float) framebuffer_size.y}};
        auto scissors = std::vector<Scissor>{Scissor(0, 0, framebuffer_size.x, framebuffer_size.y)};
        auto blend_state = std::vector<BlendState>{BlendState{}};
        auto graphics_state = GraphicsState{};
        graphics_state.viewports = viewports;
        graphics_state.scissors = scissors;
        graphics_state.blend_states = blend_state;

        encoder->push_debug_label("Visualize Nanite");
        auto color_attachments = std::vector<ColorAttachment>{
            ColorAttachment{backbuffer->view()},
        };
        auto depth_stencil_attachment = DepthStencilAttachment{depth_texture->view()};
        auto graphics_encoder = encoder->begin_graphics_pass(color_attachments, depth_stencil_attachment);
        auto object = graphics_encoder->bind_pipeline(pipeline);
        auto writer = ShaderObjectWriter{object};
        auto sampler_info = SamplerCreateInfo{};
        auto sampler = device->create_sampler("Visibility buffer sampler", &sampler_info);
        writer["visibility_texture_size"].set_data(framebuffer_size);
        writer["visibility_texture"].set_bindless_texture(visiblity_texture->view(), sampler, EResourceStates::sampled_texture);
        writer["meshlet_cmds"].set_bindless_buffer(context->meshlet_cmd_buffer, EResourceStates::storage_read);
        writer["gpu_scene"].set_bindless_buffer(context->gpu_scene_buffer, EResourceStates::storage_read);
        writer["debug_type"].set_data(context->visualization_mode);
        graphics_encoder->set_graphics_state(graphics_state);
        auto draw_args = DrawArguments{.vertex_count = 3, .instance_count = 1};
        graphics_encoder->draw(draw_args);
        graphics_encoder->finish();

        encoder->pop_debug_label();

        // context->meshlet_cmd_buffer = {};
        context->meshlet_count_buffer = {};
        // context->meshlet_filtered_cmd_buffer = {};
        context->meshlet_group_count_buffer = {};
        context->meshlet_group_id_buffer = {};
    }

    inline namespace
    {
        struct VisibilityTileContext final
        {
            rhi::TextureHandle visibility_texture{};
            rhi::TextureHandle marker_texture{};

            rhi::BufferHandle tile_cmd_buffer{};
            rhi::BufferHandle dispatch_indirect_buffer{};
        };

        auto visibility_mark(CommandEncoderHandle encoder, RenderContext* context) -> TextureHandle
        {
            auto device = encoder->get_device();

            auto visibility_extent = context->visibility_texture->description()->extent;
            // Use 8x8 tiles for now
            auto texture_info = TextureCreateInfo{
                .dimension = ETextureDimension::tex_2d,
                .format    = EFormat::rgba32_uint,
                .usage     = ETextureUsage::storage | ETextureUsage::sampled,
            };
            texture_info.extent.width  = math::divide_rounding_up(visibility_extent.width, 8u);
            texture_info.extent.height = math::divide_rounding_up(visibility_extent.height, 8u);
            texture_info.final_state   = EResourceStates::storage_read;
            auto marker_texture = device->create_texture(
                "Visibility Marker Texture", &texture_info
            );

            auto sampler_info = SamplerCreateInfo{};
            sampler_info.filter_min = ESamplerFilter::nearest;
            sampler_info.filter_mag = ESamplerFilter::nearest;
            sampler_info.filter_mip = ESamplerFilter::nearest;
            sampler_info.address_u  = ESamplerAddressMode::clamp_to_edge;
            sampler_info.address_v  = ESamplerAddressMode::clamp_to_edge;
            sampler_info.address_w  = ESamplerAddressMode::clamp_to_edge;
            auto sampler = device->create_sampler("Nearest Clamp Edge Sampler", &sampler_info);

            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .program = device->create_compute_shader_program("nanite_shading", "main_tile_marker_cs"),
            };
            auto compute_pipeline = device->create_compute_pipeline("Visibility Tile Marker Pipeline", &compute_pipeline_info);

            encoder->push_debug_label("Visibility Tile Mark");
            auto compute_encoder = encoder->begin_compute_pass();
            auto object = compute_encoder->bind_pipeline(compute_pipeline);
            auto writer = ShaderObjectWriter{object};
            writer["view_buffer"].set_bindless_buffer(context->frame_view_buffer, EResourceStates::storage_read);
            writer["scene_buffer"].set_bindless_buffer(context->gpu_scene_buffer, EResourceStates::storage_read);
            writer["visibility_texture"].set_bindless_texture(context->visibility_texture->view(), EResourceStates::sampled_texture);
            writer["visibility_texel_size"].set_data(math::float2{1.0f} / math::float2{visibility_extent.width, visibility_extent.height});
            writer["tile_marker_texture"].set_bindless_texture(marker_texture->view(), EResourceStates::storage_write);
            writer["gather_sampler"].set_data(sampler->descriptor_handle());
            writer["meshlet_cmd_buffer"].set_bindless_buffer(context->meshlet_cmd_buffer, EResourceStates::storage_read);
            compute_encoder->set_compute_state();
            auto marker_texture_extent = marker_texture->description()->extent;
            compute_encoder->dispatch((marker_texture_extent.width + 3) / 4, (marker_texture_extent.height + 3) / 4, 1);
            compute_encoder->finish();

            encoder->pop_debug_label();

            return marker_texture;
        }

        auto prepare_shading_tile(rhi::CommandEncoderHandle encoder, RenderContext* context, EShadingType shading_type, TextureHandle marker_texture) -> VisibilityTileContext
        {
            auto tile_context = VisibilityTileContext{};
            auto device = encoder->get_device();

            auto buffer_info = BufferCreateInfo{};
            buffer_info.memory_type = EMemoryType::gpu_only;

            auto marker_extent = math::uint2{marker_texture->description()->extent.width, marker_texture->description()->extent.height};
            buffer_info.size_bytes = sizeof(math::uint2) * marker_extent.x * marker_extent.y;
            buffer_info.usage = EBufferUsage::storage;
            tile_context.tile_cmd_buffer = device->create_buffer("Tile Command Buffer", &buffer_info);

            buffer_info.size_bytes = sizeof(math::uint);
            buffer_info.usage = EBufferUsage::storage | EBufferUsage::transfer_dst;
            auto tile_count_buffer = device->create_buffer("Tile Count Buffer", &buffer_info);
            encoder->clear_buffer_uint(tile_count_buffer);

            {
                auto compute_pipeline_info = ComputePipelineCreateInfo{
                    .program = device->create_compute_shader_program("nanite_shading", "main_tile_prepare_cs"),
                };
                auto compute_pipeline = device->create_compute_pipeline("Visibility Tile Prepare Pipeline", &compute_pipeline_info);

                encoder->push_debug_label("Visibility Tile Prepare");
                auto compute_encoder = encoder->begin_compute_pass();
                auto object = compute_encoder->bind_pipeline(compute_pipeline);
                auto writer = ShaderObjectWriter{object};
                writer["tile_marker_texture"].set_bindless_texture(marker_texture->view(), EResourceStates::sampled_texture);
                writer["marker_index"].set_data(uint32_t(shading_type) / 32);
                writer["marker_bit"].set_data(1u << (uint32_t(shading_type) % 32));
                writer["marker_dim"].set_data(math::float2{marker_extent.x, marker_extent.y});
                writer["tile_marker_texture"].set_bindless_texture(marker_texture->view(), EResourceStates::sampled_texture);
                writer["tile_count_buffer"].set_bindless_buffer(tile_count_buffer, EResourceStates::storage_write);
                writer["tile_cmd_buffer"].set_bindless_buffer(tile_context.tile_cmd_buffer, EResourceStates::storage_write);
                compute_encoder->set_compute_state();
                auto dipatch_size = math::divide_rounding_up(marker_extent, math::uint2{16});
                compute_encoder->dispatch(dipatch_size.x, dipatch_size.y, 1);
                compute_encoder->finish();

                encoder->pop_debug_label();
            }

            buffer_info.size_bytes = sizeof(math::uint4);
            buffer_info.usage = EBufferUsage::storage | EBufferUsage::indirect;
            tile_context.dispatch_indirect_buffer = device->create_buffer("Tile Dispatch Indirect Buffer", &buffer_info);

            {
                auto push_constants = TileIndirectParameterPushConstant{};
                push_constants.tile_count_buffer = tile_count_buffer->descriptor_handle();
                push_constants.indirect_parameter_buffer = tile_context.dispatch_indirect_buffer->descriptor_handle();

                auto compute_pipeline_info = ComputePipelineCreateInfo{
                    .program = device->create_compute_shader_program("nanite_shading", "main_tile_assemble_indirect_cmd_cs"),
                };
                auto compute_pipeline = device->create_compute_pipeline("Tile Indirect Command Assemble Pipeline", &compute_pipeline_info);

                encoder->push_debug_label("Tile Indirect Command Assemble");
                auto compute_encoder = encoder->begin_compute_pass();
                auto object = compute_encoder->bind_pipeline(compute_pipeline);
                auto writer = ShaderObjectWriter{object};
                writer["tile_count_buffer"].set_bindless_buffer(tile_count_buffer, EResourceStates::storage_read);
                writer["indirect_buffer"].set_bindless_buffer(tile_context.dispatch_indirect_buffer, EResourceStates::storage_write);
                compute_encoder->set_compute_state();
                compute_encoder->dispatch(1, 1, 1);
                compute_encoder->finish();

                encoder->pop_debug_label();
            }

            return tile_context;
        }
    }

    auto nanite_shading(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void
    {
        auto marker_texture = visibility_mark(encoder, context);

        auto tile_context = prepare_shading_tile(encoder, context, EShadingType::pbr, marker_texture);
    }
}
