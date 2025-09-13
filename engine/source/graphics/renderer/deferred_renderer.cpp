#include "renderer.hpp"
#include "nanite_rendering.hpp"

#include <platform/engine.hpp>
#include <builtin_mesh_draw.slang.hpp>
#include <scene.slang.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        using namespace rhi;

        REGISTER_SHADER_COMPOSITION(BuiltinMeshDrawVS, "builtin_mesh_draw", "main_built_in_mesh_vs", EShaderStage::vertex);
        REGISTER_SHADER_COMPOSITION(BuiltinMeshDrawFS, "builtin_mesh_draw", "main_built_in_mesh_fs", EShaderStage::fragment);

        REGISTER_SHADER_COMPOSITION(MeshDrawMS, "mesh_draw", "main_mesh_draw_ms", EShaderStage::mesh);
        REGISTER_SHADER_COMPOSITION(MeshDrawFS, "mesh_draw", "main_mesh_draw_fs", EShaderStage::fragment);

        bool buffer_ready = false;
    }

    DeferredRenderer::DeferredRenderer(RendererCreateInfo* info)
        : device(info->device)
        , swapchain(info->swapchain)
        , imgui_wrapper(info->imgui)
    {
        timer_querys.resize(swapchain->num_backbuffers());
        std::ranges::for_each(timer_querys, [this](auto& query) { query = device->create_timer_query(); });

        auto command_list_ci = CommandListCreateInfo{
            .queue_type = EQueueType::graphics,
        };
        command_list = device->create_command_list(&command_list_ci);
        async_transfer_command_list = device->async_uploader()->per_frame_transfer_list;

        context = std::make_unique<RenderContext>();

        auto view_buffer_info = BufferCreateInfo{
            .size_bytes = sizeof(FrameViewData),
            .type = EBufferType::cpu_write,
            .usage = EBufferUsage::uniform | EBufferUsage::storage,
        };
        context->frame_view_buffer = device->create_buffer("FrameViewData Buffer", &view_buffer_info);
        camera = std::make_unique<First_Person_Camera>();
        camera->aspect_ratio = 4.0f / 3.0f;
        camera->set_position({5.0, 5.0, 5.0});
        camera->set_lookat_position({});

        using namespace cannele::scene::resource;
        auto import_config = GLTFAssetImportConfig{};
        import_config.import_path = "engine/asset/gltf/Sponza/glTF/Sponza.gltf";
        import_config.store_path = "engine/asset/gltf/Sponza/glTF/Sponza.gltf_asset";
        import_config.generate_smooth_normals = true;
        context->asset = GLTFAsset::import_from_config(&import_config);

        {
            auto primitive_infos_data = &context->primitive_infos_data;
            auto instances_data = &context->instances_data;
            for (auto index = 0u; auto& mesh: context->asset->meshes) {
                for (auto& primitive: mesh.primitives) {
                    auto primitive_data = primitive.gpu_buffer();
                    primitive_data.data_buffer_index = 0;
                    primitive_infos_data->emplace_back(primitive_data);
                    auto instance_data = InstanceData{};
                    instance_data.matrix_local_to_world = math::float4x4{1.0f};
                    instance_data.matrix_world_to_local = math::float4x4{1.0f};
                    instance_data.matrix_local_to_world_last_frame = math::float4x4{1.0f};
                    instance_data.primitive_detail_index = index++;
                    instance_data.material_data_index = 0;
                    instances_data->emplace_back(instance_data);
                }
            }
            auto buffer_info = BufferCreateInfo{
                .type = EBufferType::cpu_write,
                .usage = EBufferUsage::storage,
            };

            buffer_info.size_bytes = instances_data->size() * sizeof(InstanceData);
            context->gltf_instance_info_buffer = device->create_buffer("Instance Info Buffer", &buffer_info);

            buffer_info.size_bytes = primitive_infos_data->size() * sizeof(GltfPrimitiveInfo);
            context->gltf_primitive_detail_buffer = device->create_buffer("Primitive Buffer", &buffer_info);

            auto primitive_data_buffer_data = &context->primitive_data_buffer_data;
            // TODO: Improve this, don't stuck when async upload.
            device->async_uploader()->wait_task_complete();
            CNE_ASSERT(context->asset->gpu_data.ready());

            primitive_data_buffer_data->emplace_back(context->asset->gpu_data.primitive_data_buffers());
            buffer_info.size_bytes = primitive_data_buffer_data->size() * sizeof(GltfPrimitiveDataBuffers);
            context->gltf_primitive_data_buffer = device->create_buffer("Primitive Data Buffer", &buffer_info);

            context->scene.gltf_instance_datas = context->gltf_instance_info_buffer->descriptor_handle();
            context->scene.gltf_primitive_details = context->gltf_primitive_detail_buffer->descriptor_handle();
            context->scene.gltf_primitive_datas = context->gltf_primitive_data_buffer->descriptor_handle();
            context->scene.gltf_primitive_materials = math::uint2{k_invalid_bindless_index};
            context->scene.gltf_object_count = 1;

            buffer_info.size_bytes = sizeof(GpuScene);
            context->gpu_scene_buffer = device->create_buffer("GpuScene Buffer", &buffer_info);

            auto uploader = device->async_uploader();
            uploader->add_task([context = context.get()](RHICommandList* command_list) {
                command_list->write_buffer(context->gpu_scene_buffer, {(std::byte*)(&context->scene), sizeof(GpuScene)});

                command_list->write_buffer(context->gltf_instance_info_buffer, std::as_writable_bytes(std::span{context->instances_data}));

                command_list->write_buffer(context->gltf_primitive_detail_buffer, std::as_writable_bytes(std::span{context->primitive_infos_data}));

                command_list->write_buffer(context->gltf_primitive_data_buffer, std::as_writable_bytes(std::span{context->primitive_data_buffer_data}));

                command_list->set_buffer_state(context->gltf_instance_info_buffer, EResourceStates::storage_read);
                command_list->set_buffer_state(context->gltf_primitive_detail_buffer, EResourceStates::storage_read);
                command_list->set_buffer_state(context->gltf_primitive_data_buffer, EResourceStates::storage_read);

                command_list->commit_barriers(EQueueType::transfer, EQueueType::graphics);
            }, [] { buffer_ready = true; });
        }
    }

    DeferredRenderer::~DeferredRenderer()
    {

    }

    auto DeferredRenderer::render() -> void
    {
        device->new_frame(frame_count);
        imgui_wrapper->new_frame();
        swapchain->acquire_next_backbuffer();
        swapchain->enqueue_backbuffer_ready_wait_semaphore();

        {
            auto timer_query = timer_querys[swapchain->backbuffer_index()].get();
            auto time = device->get_query_result(timer_query);
            ImGui::Begin("Debug Window");
            ImGui::Text("%.3f ms", time * 1000.0f);
            ImGui::SliderInt("Visualization Mode", &context->visualization_mode, 0, 1);
            ImGui::End();

            auto asset = context->asset;
            ImGui::Begin("Asset Info");
            ImGui::Text("Meshlets: %d", (int) asset->data.meshlets.size());
            ImGui::Text("GLTFBVHNode: %d", (int) asset->data.bvh_nodes.size());
            ImGui::Text("GLTFMeshletGroup: %d", (int) asset->data.meshlet_groups.size());
            ImGui::Text("triangles: %d", (int) asset->data.lod_0_indices.size() / 3);
            ImGui::End();

            auto backbuffer = swapchain->backbuffer();
            {
                camera->update(ImGui::GetIO().DeltaTime);
                auto matrix = camera->matrix();
                per_frame_view_data.world_to_view_matrix = matrix->matrix_view;
                per_frame_view_data.view_to_world_matrix = matrix->matrix_inv_view;
                per_frame_view_data.view_to_clip_matrix = matrix->matrix_proj;
                per_frame_view_data.clip_to_view_matrix = matrix->matrix_inv_proj;
                per_frame_view_data.world_to_clip_matrix_pre_frame = per_frame_view_data.world_to_clip_matrix;
                per_frame_view_data.world_to_clip_matrix = matrix->matrix_proj * matrix->matrix_view;

                auto frame_buffer_size = backbuffer->description()->extent;
                per_frame_view_data.viewport = {frame_buffer_size.x, frame_buffer_size.y, 1.0f / frame_buffer_size.x, 1.0f / frame_buffer_size.y};

                ImGui::Begin("Camera Info");
                ImGui::Text("Position: %.2f, %.2f, %.2f", camera->position.x, camera->position.y, camera->position.z);
                ImGui::Text("Forward: %.2f, %.2f, %.2f", camera->forward.x, camera->forward.y, camera->forward.z);
                ImGui::Text("Aspect Ratio: %.2f", camera->aspect_ratio);
                ImGui::Text("FOV: %.2f", camera->fov);
                ImGui::Text("Near: %.2f", camera->z_near);
                ImGui::Text("Far: %.2f", camera->z_far);
                ImGui::End();
            }

            auto depth_texture_info = depth_attachment_create_info(backbuffer->description()->extent, EFormat::d32_sfloat);
            auto depth_texture = device->create_texture("Depth Texture", &depth_texture_info);
            context->backbuffer = backbuffer;
            context->depth_texture = depth_texture;

            command_list->start();
            command_list->begin_timestep(timer_query);
            command_list->clear_texture_float(backbuffer, {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});
            command_list->write_buffer(context->frame_view_buffer, {(std::byte*) &per_frame_view_data, sizeof(FrameViewData)}, 0);

            auto instance_culling_result = std::pair<rhi::BufferHandle, rhi::BufferHandle>{nullptr, nullptr};
            if (buffer_ready) {
                instance_culling_result = instance_culling(command_list, context.get());
            }

            nanite_render_pass_0(command_list, context.get());

            nanite_visualize(command_list, context.get());

            nanite_shading(command_list, context.get());

            imgui_wrapper->render(command_list.get(), backbuffer);
            command_list->end_timestep(timer_query);
            command_list->finish();
            swapchain->enqueue_render_finish_signal_semaphore();
        }

        auto time = device->submit_command_lists({&command_list, 1});
        swapchain->present(time);
        frame_count++;
    }
}
