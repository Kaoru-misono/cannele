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


        bool buffer_ready = false;
    }

    DeferredRenderer::DeferredRenderer(RendererCreateInfo* info)
        : device(info->device)
        , swapchain(info->swapchain)
        , imgui_wrapper(info->imgui)
    {
        timer_querys.resize(swapchain->num_backbuffers());
        std::ranges::for_each(timer_querys, [this](auto& query) { query = device->create_timer_query(); });

        context = std::make_unique<RenderContext>();

        auto view_buffer_info = BufferCreateInfo{
            .memory_type = EMemoryType::cpu_upload,
            .usage = EBufferUsage::uniform | EBufferUsage::storage,
            .size_bytes = sizeof(FrameViewData),
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
                .memory_type = EMemoryType::gpu_only,
                .usage       = EBufferUsage::storage | EBufferUsage::transfer_dst,
                .final_state = EResourceStates::storage_read,
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
            uploader->add_task([context = context.get()](CommandEncoder* encoder) {
                encoder->upload_buffer_data(context->gpu_scene_buffer, 0, {(std::byte*)(&context->scene), sizeof(GpuScene)});

                encoder->upload_buffer_data(context->gltf_instance_info_buffer, 0, std::as_writable_bytes(std::span{context->instances_data}));

                encoder->upload_buffer_data(context->gltf_primitive_detail_buffer, 0, std::as_writable_bytes(std::span{context->primitive_infos_data}));

                encoder->upload_buffer_data(context->gltf_primitive_data_buffer, 0, std::as_writable_bytes(std::span{context->primitive_data_buffer_data}));
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
        auto backbuffer = swapchain->acquire_next_backbuffer();
        context->backbuffer = backbuffer;

        {
            // auto timer_query = timer_querys[swapchain->backbuffer_index()].get();
            // auto time = device->get_query_result(timer_query);
            ImGui::Begin("Debug Window");
            // ImGui::Text("%.3f ms", time * 1000.0f);
            ImGui::SliderInt("Visualization Mode", &context->visualization_mode, 0, 1);
            ImGui::End();

            auto asset = context->asset;
            ImGui::Begin("Asset Info");
            ImGui::Text("Meshlets: %d", (int) asset->data.meshlets.size());
            ImGui::Text("GLTFBVHNode: %d", (int) asset->data.bvh_nodes.size());
            ImGui::Text("GLTFMeshletGroup: %d", (int) asset->data.meshlet_groups.size());
            ImGui::Text("triangles: %d", (int) asset->data.lod_0_indices.size() / 3);
            ImGui::End();

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
                per_frame_view_data.viewport = {frame_buffer_size.width, frame_buffer_size.height, 1.0f / frame_buffer_size.width, 1.0f / frame_buffer_size.height};

                ImGui::Begin("Camera Info");
                ImGui::Text("Position: %.2f, %.2f, %.2f", camera->position.x, camera->position.y, camera->position.z);
                ImGui::Text("Forward: %.2f, %.2f, %.2f", camera->forward.x, camera->forward.y, camera->forward.z);
                ImGui::Text("Aspect Ratio: %.2f", camera->aspect_ratio);
                ImGui::Text("FOV: %.2f", camera->fov);
                ImGui::Text("Near: %.2f", camera->z_near);
                ImGui::Text("Far: %.2f", camera->z_far);
                ImGui::End();
            }

            auto depth_texture_info = depth_attachment_create_info({backbuffer->description()->extent.width, backbuffer->description()->extent.height}, EFormat::d32_sfloat);
            auto depth_texture = device->create_texture("Depth Texture", &depth_texture_info);
            context->backbuffer = backbuffer;
            context->depth_texture = depth_texture;

            auto encoder = device->create_command_encoder(EQueueType::graphics);
            encoder->clear_texture_float(backbuffer, {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});
            encoder->upload_buffer_data(context->frame_view_buffer, 0, {(std::byte*) &per_frame_view_data, sizeof(FrameViewData)});

            auto instance_culling_result = std::pair<rhi::BufferHandle, rhi::BufferHandle>{nullptr, nullptr};
            if (buffer_ready) {
                instance_culling_result = instance_culling(encoder, context.get());
            }

            nanite_render_pass_0(encoder, context.get());

            nanite_visualize(encoder, context.get());
//
//             nanite_shading(encoder, context.get());

            imgui_wrapper->render(encoder, backbuffer);

            auto command_buffer = encoder->finish();
            device->submit_command_buffer(EQueueType::graphics, command_buffer);
        }

        swapchain->present();
        frame_count++;
    }
}
