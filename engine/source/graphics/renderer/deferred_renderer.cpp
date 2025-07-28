#include "renderer.hpp"

#include <platform/engine.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        using namespace rhi;

        DECLARE_DEFAULT_SHADER_AND_REGISTER(BuiltinMeshDrawVS, "shader/hlsl/builtin_mesh_draw.hlsl", "main_built_in_mesh_vs", EShaderStage::vertex);
        DECLARE_DEFAULT_SHADER_AND_REGISTER(BuiltinMeshDrawFS, "shader/hlsl/builtin_mesh_draw.hlsl", "main_built_in_mesh_fs", EShaderStage::fragment);
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
            ImGui::ShowDemoWindow();

            auto timer_query = timer_querys[swapchain->backbuffer_index()].get();
            auto time = device->get_query_result(timer_query);
            ImGui::Begin("Time");
            ImGui::Text("%.3f ms", time * 1000.0f);
            ImGui::End();

            auto asset = Engine::current()->asset;
            ImGui::Begin("Asset Info");
            ImGui::Text("Meshlets: %d", (int) asset->data.meshlets.size());
            ImGui::Text("GLTFBVHNode: %d", (int) asset->data.bvh_nodes.size());
            ImGui::Text("GLTFMeshletGroup: %d", (int) asset->data.meshlet_groups.size());
            ImGui::Text("triangles: %d", (int) asset->data.lod_0_indices.size() / 3);
            ImGui::End();

            async_transfer_command_list->start();
            command_list->start();
            command_list->begin_timestep(timer_query);
            command_list->clear_texture_float(swapchain->backbuffer(), {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});

            {

            }

            imgui_wrapper->render(command_list.get(), swapchain->backbuffer());
            command_list->end_timestep(timer_query);
            command_list->finish();
            async_transfer_command_list->finish();
            swapchain->enqueue_render_finish_signal_semaphore();
        }
        command_list->wait_for_submit(EQueueType::transfer, device->submit_command_lists({&async_transfer_command_list, 1}, EQueueType::transfer));
        auto time = device->submit_command_lists({&command_list, 1});
        swapchain->present(time);
        frame_count++;
    }
}
