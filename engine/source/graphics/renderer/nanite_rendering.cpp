#include "nanite_rendering.hpp"
#include "shading_type.hpp"

#include <graphics/RHI/device.hpp>
#include <nanite.slang.hpp>
#include <math/tool.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        REGISTER_SHADER_COMPOSITION(NaniteInstanceCullingCS, "nanite_culling", "main_instance_culling_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(IndirectCmdAssemblyCS, "assemble_indirect_dispatch_command", "main_assemble_indirect_dispatch_cmd_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(NaniteClusterCullingCS, "nanite_culling", "main_cluster_group_culling_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(NaniteRenderMS, "nanite_raster", "main_nanite_mesh_pass_ms", EShaderStage::mesh);
        REGISTER_SHADER_COMPOSITION(NaniteRenderFS, "nanite_raster", "main_nanite_visibility_buffer_pass_fs", EShaderStage::fragment);
        REGISTER_SHADER_COMPOSITION(NaniteVisualizeFS, "nanite_visualization", "main_nanite_visualize_fs", EShaderStage::fragment);
        REGISTER_SHADER_COMPOSITION(NaniteRenderIndirectCmdAssemblyCS, "nanite_raster", "main_assemble_indirect_cmd_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(NaniteRenderFilterMeshletCmdCS, "nanite_raster", "main_filter_meshlet_cmd_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(FullScreenVS, "full_screen", "main_fullscreen_vs", EShaderStage::vertex);
        REGISTER_SHADER_COMPOSITION(TileMarkerCS, "nanite_shading", "main_tile_marker_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(TilePrepareCS, "nanite_shading", "main_tile_prepare_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(TileIndirectCmdAssemblyCS, "nanite_shading", "main_tile_assemble_indirect_cmd_cs", EShaderStage::compute);

        using namespace rhi;
    }

    auto instance_culling(rhi::CommandListHandle command_list, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>
    {
        auto device = command_list->device();

        auto buffer_info = BufferCreateInfo{
            .type = EBufferType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
        };

        buffer_info.size_bytes = sizeof(uint2);
        auto cluster_group_count_buffer = device->create_buffer("Cluster Group Count Buffer", &buffer_info);

        auto cluster_group_count = context->asset->data.meshlet_groups.size();
        buffer_info.size_bytes = sizeof(uint2) * cluster_group_count;
        auto cluster_group_id_buffer = device->create_buffer("Cluster Group ID Buffer", &buffer_info);

        context->meshlet_group_count_buffer = cluster_group_count_buffer;
        context->meshlet_group_id_buffer = cluster_group_id_buffer;

        {
            command_list->clear_buffer_uint(cluster_group_count_buffer);
            command_list->clear_buffer_uint(cluster_group_id_buffer);
            auto push_constant = InstanceCullingPushConstant{};
            push_constant.frame_view_buffer = context->frame_view_buffer->descriptor_handle();
            push_constant.cluster_group_count_buffer = cluster_group_count_buffer->descriptor_handle();
            push_constant.cluster_group_id_buffer = cluster_group_id_buffer->descriptor_handle();
            push_constant.scene_buffer = context->gpu_scene_buffer->descriptor_handle();

            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .compute_shader = device->shader_factory()->get_shader<NaniteInstanceCullingCS>(),
                .push_constant_size = sizeof(InstanceCullingPushConstant),
            };
            auto compute_pipeline = device->create_compute_pipeline("Instance Culling Pipeline", &compute_pipeline_info);
            auto compute_state = ComputeState{
                .pipeline = compute_pipeline,
            };
            math::uint2 handle = cluster_group_count_buffer->descriptor_handle();
            command_list->set_buffer_state(cluster_group_count_buffer, EResourceStates::storage_write);
            command_list->set_buffer_state(cluster_group_id_buffer, EResourceStates::storage_write);

            command_list->push_command_label("Instance Culling");
            command_list->set_compute_state(&compute_state);
            command_list->push_constants(&push_constant, sizeof(push_constant));
            command_list->dispatch(context->instances_data.size(), 1, 1);
            command_list->pop_command_label();
        }

        buffer_info.usage = EBufferUsage::indirect;
        buffer_info.size_bytes = sizeof(math::uint4);
        auto indirect_buffer = device->create_buffer("Cluster Group Culling Indirect Buffer", &buffer_info);
        {
            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .compute_shader = device->shader_factory()->get_shader<IndirectCmdAssemblyCS>(),
                .push_constant_size = sizeof(IndirectDispatchCommandAssemblePushConstant)
            };
            auto assemble_indirect_cmd_pipeline = device->create_compute_pipeline("Indirect Command Assembly Pipeline", &compute_pipeline_info);
            auto compute_state = ComputeState{
                .pipeline = assemble_indirect_cmd_pipeline,
            };

            auto push_constant = IndirectDispatchCommandAssemblePushConstant{
                .count_buffer = cluster_group_count_buffer->descriptor_handle(),
                .indirect_dispatch_command_buffer = indirect_buffer->descriptor_handle(),
                .group_size = 64,
            };

            command_list->set_buffer_state(cluster_group_count_buffer, EResourceStates::storage_read);
            command_list->set_buffer_state(indirect_buffer, EResourceStates::storage_write);

            command_list->push_command_label("Assemble Indirect Command");
            command_list->set_compute_state(&compute_state);
            command_list->push_constants(push_constant);
            command_list->dispatch(1, 1, 1);
            command_list->pop_command_label();
        }

        buffer_info.usage = EBufferUsage::storage | EBufferUsage::transfer_dst;
        buffer_info.size_bytes = sizeof(uint);
        auto& meshlet_count_buffer = context->meshlet_count_buffer;
        meshlet_count_buffer = device->create_buffer("Meshlet Count Buffer", &buffer_info);
        command_list->clear_buffer_uint(meshlet_count_buffer);

        auto lod_0_meshlet_count = 0u;
        for (auto& primitive: context->primitive_infos_data) {
            lod_0_meshlet_count += primitive.lod_0_meshlet_count;
        }

        buffer_info.size_bytes = sizeof(math::uint4) * lod_0_meshlet_count;
        auto& meshlet_cmd_buffer = context->meshlet_cmd_buffer;
        meshlet_cmd_buffer = device->create_buffer("Meshlet Command Buffer", &buffer_info);
        command_list->clear_buffer_uint(meshlet_cmd_buffer);

        {
            auto push_constant = NaniteClusterCullingPushConstant{};
            push_constant.frame_view_buffer = context->frame_view_buffer->descriptor_handle();
            push_constant.cluster_group_count_buffer = cluster_group_count_buffer->descriptor_handle();
            push_constant.cluster_group_id_buffer = cluster_group_id_buffer->descriptor_handle();
            push_constant.scene_buffer = context->gpu_scene_buffer->descriptor_handle();
            push_constant.meshlet_count_buffer = meshlet_count_buffer->descriptor_handle();
            push_constant.meshlet_cmd_buffer = meshlet_cmd_buffer->descriptor_handle();

            command_list->set_buffer_state(cluster_group_count_buffer, EResourceStates::storage_read);
            command_list->set_buffer_state(cluster_group_id_buffer, EResourceStates::storage_read);
            command_list->set_buffer_state(meshlet_count_buffer, EResourceStates::storage_write);
            command_list->set_buffer_state(meshlet_cmd_buffer, EResourceStates::storage_write);

            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .compute_shader = device->shader_factory()->get_shader<NaniteClusterCullingCS>(),
                .push_constant_size = sizeof(NaniteClusterCullingPushConstant),
            };
            auto compute_pipeline = device->create_compute_pipeline("Cluster Culling Pipeline", &compute_pipeline_info);

            auto compute_state = ComputeState{
                .pipeline = compute_pipeline,
                .indirect_buffer = indirect_buffer,
            };

            command_list->push_command_label("Cluster Group Culling");
            command_list->set_compute_state(&compute_state);
            command_list->push_constants(&push_constant, sizeof(push_constant));
            command_list->dispatch_indirect();
            command_list->pop_command_label();
        }

        return {meshlet_count_buffer, meshlet_cmd_buffer};
    }

    auto nanite_render_pass_0(rhi::CommandListHandle command_list, RenderContext* context) -> void
    {
        auto device = command_list->device();

        auto buffer_info = BufferCreateInfo{};
        buffer_info.type = EBufferType::gpu_only;

        buffer_info.size_bytes = sizeof(math::uint4);
        buffer_info.usage = EBufferUsage::storage | EBufferUsage::indirect;
        auto meshlet_indirect_dispatch_buffer = device->create_buffer("Meshlet Indirect Dispatch Buffer", &buffer_info);

        auto& meshlet_count_buffer = context->meshlet_count_buffer;
        auto& meshlet_cmd_buffer = context->meshlet_cmd_buffer;

        {
            auto push_constant = IndirectDispatchCommandAssemblePushConstant{};
            push_constant.count_buffer = meshlet_count_buffer->descriptor_handle();
            push_constant.indirect_dispatch_command_buffer = meshlet_indirect_dispatch_buffer->descriptor_handle();

            command_list->set_buffer_state(meshlet_count_buffer, EResourceStates::storage_read);
            command_list->set_buffer_state(meshlet_indirect_dispatch_buffer, EResourceStates::storage_write);

            auto compute_pipeline_info = ComputePipelineCreateInfo{
                .compute_shader = device->shader_factory()->get_shader<NaniteRenderIndirectCmdAssemblyCS>(),
                .push_constant_size = sizeof(IndirectDispatchCommandAssemblePushConstant),
            };
            auto compute_pipeline = device->create_compute_pipeline("Indirect Draw Command Assemble Pipeline", &compute_pipeline_info);

            auto compute_state = ComputeState{
                .pipeline = compute_pipeline,
            };

            command_list->push_command_label("Meshlet Indirect Command Assemble");
            command_list->set_compute_state(&compute_state);
            command_list->push_constants(push_constant);
            command_list->dispatch(1, 1, 1);
            command_list->pop_command_label();
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
                .compute_shader = device->shader_factory()->get_shader<IndirectCmdAssemblyCS>(),
                .push_constant_size = sizeof(IndirectDispatchCommandAssemblePushConstant)
            };
            auto assemble_indirect_cmd_pipeline = device->create_compute_pipeline("Indirect Command Assembly Pipeline", &compute_pipeline_info);
            auto compute_state = ComputeState{
                .pipeline = assemble_indirect_cmd_pipeline,
            };

            auto push_constant = IndirectDispatchCommandAssemblePushConstant{
                .count_buffer = meshlet_count_buffer->descriptor_handle(),
                .indirect_dispatch_command_buffer = indirect_mesh_dipatch_buffer->descriptor_handle(),
                .group_size = 1,
            };

            command_list->set_buffer_state(meshlet_count_buffer, EResourceStates::storage_read);
            command_list->set_buffer_state(indirect_mesh_dipatch_buffer, EResourceStates::storage_write);

            command_list->push_command_label("Assemble Indirect Command");
            command_list->set_compute_state(&compute_state);
            command_list->push_constants(push_constant);
            command_list->dispatch(1, 1, 1);
            command_list->pop_command_label();
        }

        {
            auto& backbuffer = context->backbuffer;
            auto& depth_texture = context->depth_texture;
            auto framebuffer_size = backbuffer->description()->extent;
            auto texture_info = TextureCreateInfo{};
            texture_info.dimension = ETextureDimension::tex_2d;
            texture_info.extent = framebuffer_size;
            texture_info.format = EFormat::r32_uint;
            texture_info.usage = ETextureUsage::color_attachment | ETextureUsage::sampled;
            context->visibility_texture = device->create_texture("Visibility Texture", &texture_info);

            auto& visiblity_texture = context->visibility_texture;
            auto render_target = RenderTarget{};
            render_target.info = RenderTargetInfo{};
            render_target.info.extent = framebuffer_size;
            render_target.info.color_formats.emplace_back(visiblity_texture->description()->format);
            render_target.info.depth_stencil_format = depth_texture->description()->format;
            render_target.info.blend_states.emplace_back();
            render_target.info.depth_state.enable_depth_write = true;
            render_target.color_attachments.emplace_back(Attachment{visiblity_texture});
            render_target.depth_stencil_attachment = Attachment{depth_texture};

            auto pipeline_info = MeshPipelineCreateInfo{};
            pipeline_info.ms = device->shader_factory()->get_shader<NaniteRenderMS>();
            pipeline_info.fs = device->shader_factory()->get_shader<NaniteRenderFS>();
            pipeline_info.render_target_info = render_target.info;
            auto pipeline = device->create_mesh_pipeline("Nanite Render Pipeline", &pipeline_info);

            auto mesh_state = MeshState{};
            mesh_state.pipeline = pipeline;
            mesh_state.render_target = &render_target;
            mesh_state.viewport_state.viewports.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);
            mesh_state.viewport_state.scissors.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);
            mesh_state.indirect_buffer = indirect_mesh_dipatch_buffer;

            // buffer_info.size_bytes = sizeof(NaniteDebugData) * 128 * 2097;
            // buffer_info.usage = EBufferUsage::storage | EBufferUsage::transfer_dst;
            // auto debug_buffer = device->create_buffer("Debug Buffer", &buffer_info);

            auto push_constant = NaniteMeshRasterPushConstant{};
            push_constant.frame_view_buffer = context->frame_view_buffer->descriptor_handle();
            push_constant.scene_buffer = context->gpu_scene_buffer->descriptor_handle();
            push_constant.meshlet_cmd_buffer = meshlet_cmd_buffer->descriptor_handle();
            // push_constant.debug_buffer = debug_buffer->descriptor_handle();

            command_list->set_buffer_state(meshlet_cmd_buffer, EResourceStates::storage_read);

            command_list->set_mesh_state(&mesh_state);
            command_list->push_constants(push_constant);
            command_list->dispatch_mesh_indirect();
        }
    }

    auto nanite_visualize(rhi::CommandListHandle command_list, RenderContext* context) -> void
    {
        auto device = command_list->device();

        auto& backbuffer = context->backbuffer;
        auto& depth_texture = context->depth_texture;
        auto framebuffer_size = backbuffer->description()->extent;
        auto& visiblity_texture = context->visibility_texture;

        auto render_target = RenderTarget{};
        render_target.info = RenderTargetInfo{};
        render_target.info.extent = framebuffer_size;
        render_target.info.color_formats.emplace_back(context->backbuffer->description()->format);
        render_target.info.depth_stencil_format = depth_texture->description()->format;
        render_target.info.blend_states.emplace_back();
        render_target.info.depth_state.enable_depth_write = false;
        render_target.color_attachments.emplace_back(Attachment{context->backbuffer});
        render_target.depth_stencil_attachment = Attachment{.texture = depth_texture, .load = ELoadOp::load};

        auto pipeline_info = GraphicsPipelineCreateInfo{};
        pipeline_info.vs = device->shader_factory()->get_shader<FullScreenVS>();
        pipeline_info.fs = device->shader_factory()->get_shader<NaniteVisualizeFS>();
        pipeline_info.render_target_info = render_target.info;
        auto pipeline = device->create_graphics_pipeline("Nanite Visualize Pipeline", &pipeline_info);

        auto graphics_state = GraphicsState{};
        graphics_state.pipeline = pipeline;
        graphics_state.render_target = &render_target;
        graphics_state.viewport_state.viewports.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);
        graphics_state.viewport_state.scissors.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);

        auto sampler_info = SamplerCreateInfo{};
        auto sampler = device->create_sampler("Visibility buffer sampler", &sampler_info);
        auto push_constants = NaniteVisualizationPushConstant{};
        push_constants.visibility_texture_size = framebuffer_size;
        push_constants.visibility_texture = {visiblity_texture->descriptor_handle().x, sampler->descriptor_handle().x};
        push_constants.meshlet_cmd_buffer = context->meshlet_cmd_buffer->descriptor_handle();
        push_constants.scene_buffer = context->gpu_scene_buffer->descriptor_handle();
        push_constants.debug_type = context->visualization_mode;
        command_list->set_buffer_state(context->meshlet_cmd_buffer, EResourceStates::storage_read);

        command_list->set_graphics_state(&graphics_state);
        command_list->push_constants(push_constants);
        auto draw_args = DrawArguments{.num_vertices = 3, .num_instances = 1};
        command_list->draw(&draw_args);

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

        auto visibility_mark(CommandListHandle command_list, RenderContext* context) -> TextureHandle
        {
            auto device = command_list->device();

            auto visibility_extent = context->visibility_texture->description()->extent;
            // Use 8x8 tiles for now
            auto texture_info = TextureCreateInfo{
                .dimension = ETextureDimension::tex_2d,
                .format = EFormat::rgba32_uint,
                .usage = ETextureUsage::storage | ETextureUsage::sampled,
                .extent = math::divide_rounding_up(visibility_extent, math::uint2{8})
            };
            auto marker_texture = device->create_texture(
                "Visibility Marker Texture", &texture_info
            );

            auto sampler_info = SamplerCreateInfo{};
            sampler_info.filter_min = ESamplerFilter::nearest;
            sampler_info.filter_mag = ESamplerFilter::nearest;
            sampler_info.filter_mip = ESamplerFilter::nearest;
            sampler_info.address_u = ESamplerAddressMode::clamp_to_edge;
            sampler_info.address_v = ESamplerAddressMode::clamp_to_edge;
            sampler_info.address_w = ESamplerAddressMode::clamp_to_edge;
            auto sampler = device->create_sampler("Nearest Clamp Edge Sampler", &sampler_info);

            auto push_constants = TileMarkerPushConstant{};
            push_constants.view_buffer = context->frame_view_buffer->descriptor_handle();
            push_constants.scene_buffer = context->gpu_scene_buffer->descriptor_handle();
            push_constants.visibility_texel_size = math::float2{1.0f} / math::float2{visibility_extent};
            push_constants.visibility_texture = context->visibility_texture->descriptor_handle();
            push_constants.tile_marker_texture = marker_texture->descriptor_handle();
            push_constants.gather_sampler = sampler->descriptor_handle();
            push_constants.meshlet_cmd_buffer = context->meshlet_cmd_buffer->descriptor_handle();

            {
                command_list->set_buffer_state(context->meshlet_cmd_buffer, EResourceStates::storage_read);
                command_list->set_texture_state(context->visibility_texture, {}, EResourceStates::sampled_texture);
                command_list->set_texture_state(marker_texture, {}, EResourceStates::storage_write);

                auto compute_pipeline_info = ComputePipelineCreateInfo{
                    .compute_shader = device->shader_factory()->get_shader<TileMarkerCS>(),
                    .push_constant_size = sizeof(TileMarkerPushConstant),
                };
                auto compute_pipeline = device->create_compute_pipeline("Visibility Tile Marker Pipeline", &compute_pipeline_info);

                auto compute_state = ComputeState{
                    .pipeline = compute_pipeline,
                };

                auto marker_texture_extent = marker_texture->description()->extent;

                command_list->push_command_label("Visibility Tile Marker");
                command_list->set_compute_state(&compute_state);
                command_list->push_constants(push_constants);
                command_list->dispatch((marker_texture_extent.x + 3) / 4, (marker_texture_extent.y + 3) / 4, 1);
                command_list->pop_command_label();
            }

            return marker_texture;
        }

        auto prepare_shading_tile(CommandListHandle command_list, RenderContext* context, EShadingType shading_type, TextureHandle marker_texture) -> VisibilityTileContext
        {
            auto tile_context = VisibilityTileContext{};
            auto device = command_list->device();

            auto buffer_info = BufferCreateInfo{};
            buffer_info.type = EBufferType::gpu_only;

            auto marker_extent = marker_texture->description()->extent;
            buffer_info.size_bytes = sizeof(math::uint2) * marker_extent.x * marker_extent.y;
            buffer_info.usage = EBufferUsage::storage;
            tile_context.tile_cmd_buffer = device->create_buffer("Tile Command Buffer", &buffer_info);

            buffer_info.size_bytes = sizeof(math::uint);
            buffer_info.usage = EBufferUsage::storage | EBufferUsage::transfer_dst;
            auto tile_count_buffer = device->create_buffer("Tile Count Buffer", &buffer_info);
            command_list->clear_buffer_uint(tile_count_buffer);

            {
                auto push_constants = TilePreparePushConstant{};
                push_constants.tile_marker_texture = marker_texture->descriptor_handle();
                push_constants.marker_index = uint32_t(shading_type) / 32;
                push_constants.marker_bit = 1u << (uint32_t(shading_type) % 32);
                push_constants.marker_dim = marker_extent;
                push_constants.tile_cmd_buffer = tile_context.tile_cmd_buffer->descriptor_handle();
                push_constants.tile_count_buffer = tile_count_buffer->descriptor_handle();

                auto compute_pipeline_info = ComputePipelineCreateInfo{
                    .compute_shader = device->shader_factory()->get_shader<TilePrepareCS>(),
                    .push_constant_size = sizeof(TilePreparePushConstant),
                };
                auto compute_pipeline = device->create_compute_pipeline("Visibility Tile Prepare Pipeline", &compute_pipeline_info);

                auto compute_state = ComputeState{
                    .pipeline = compute_pipeline,
                };

                command_list->set_buffer_state(tile_context.tile_cmd_buffer, EResourceStates::storage_write);
                command_list->set_buffer_state(tile_count_buffer, EResourceStates::storage_write);
                command_list->set_texture_state(marker_texture, k_all_subresources, EResourceStates::sampled_texture);

                auto dipatch_size = math::divide_rounding_up(marker_extent, math::uint2{16});

                command_list->push_command_label("Visibility Tile Prepare");
                command_list->set_compute_state(&compute_state);
                command_list->push_constants(push_constants);
                command_list->dispatch(dipatch_size.x, dipatch_size.y, 1);
                command_list->pop_command_label();
            }

            buffer_info.size_bytes = sizeof(math::uint4);
            buffer_info.usage = EBufferUsage::storage | EBufferUsage::indirect;
            tile_context.dispatch_indirect_buffer = device->create_buffer("Tile Dispatch Indirect Buffer", &buffer_info);

            {
                auto push_constants = TileIndirectParameterPushConstant{};
                push_constants.tile_count_buffer = tile_count_buffer->descriptor_handle();
                push_constants.indirect_parameter_buffer = tile_context.dispatch_indirect_buffer->descriptor_handle();

                auto compute_pipeline_info = ComputePipelineCreateInfo{
                    .compute_shader = device->shader_factory()->get_shader<TileIndirectCmdAssemblyCS>(),
                    .push_constant_size = sizeof(TileIndirectParameterPushConstant),
                };

                auto compute_pipeline = device->create_compute_pipeline("Tile Indirect Command Assemble Pipeline", &compute_pipeline_info);
                auto compute_state = ComputeState{
                    .pipeline = compute_pipeline,
                };

                command_list->set_buffer_state(tile_count_buffer, EResourceStates::storage_read);
                command_list->set_buffer_state(tile_context.dispatch_indirect_buffer, EResourceStates::storage_write);

                command_list->push_command_label("Tile Indirect Command Assemble");
                command_list->set_compute_state(&compute_state);
                command_list->push_constants(push_constants);
                command_list->dispatch(1, 1, 1);
                command_list->pop_command_label();
            }

            return tile_context;
        }
    }

    auto nanite_shading(rhi::CommandListHandle command_list, RenderContext* context) -> void
    {
        auto marker_texture = visibility_mark(command_list, context);

        auto tile_context = prepare_shading_tile(command_list, context, EShadingType::pbr, marker_texture);
    }
}
