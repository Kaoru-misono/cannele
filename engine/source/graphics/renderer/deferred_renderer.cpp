#include "renderer.hpp"

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        using namespace rhi;
    }

    DeferredRenderer::DeferredRenderer(RendererCreateInfo* info)
        : device(info->device)
        , swapchain(info->swapchain)
        , imgui_wrapper(info->imgui)
    {
    }

    DeferredRenderer::~DeferredRenderer()
    {

    }

    auto DeferredRenderer::render() -> void
    {
        if (!command_list) {
            auto command_list_ci = CommandListCreateInfo{
                .queue_type = EQueueType::graphics,
            };
            command_list = device->create_command_list(&command_list_ci);
        }

        device->new_frame(frame_count);
        imgui_wrapper->new_frame();
        swapchain->acquire_next_backbuffer();
        swapchain->enqueue_backbuffer_ready_wait_semaphore();

        {
            ImGui::ShowDemoWindow();

            command_list->start();
            command_list->clear_texture_float(swapchain->backbuffer(), {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});
            imgui_wrapper->render(command_list.get(), swapchain->backbuffer());
            command_list->finish();
            swapchain->enqueue_render_finish_signal_semaphore();
        }

        auto time = device->submit_command_lists({&command_list, 1});
        swapchain->present(time);
        frame_count++;
    }
}
