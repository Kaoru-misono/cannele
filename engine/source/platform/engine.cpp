#include "engine.hpp"
#include "glfw_window.hpp"

#include <graphics/RHI/vulkan.hpp>

namespace cannele::inline platform
{
    inline namespace
    {
        using namespace rhi;
        using namespace renderer;
    }

    Engine::Engine(EngineCreateInfo* info)
    {
        window = std::make_unique<GLFWWindowImpl>("cannele");
        window->init(info->initial_window_size.x, info->initial_window_size.y);

        auto task_scheduler = cannele::try_task_scheduler();
        task_scheduler->Initialize();

        auto vulkan_rhi_create_info = VulkanDeviceCreateInfo{
            .instance_extensions = window->get_instance_extension()
        };
        device = create_device(&vulkan_rhi_create_info);

        auto window_size = window->size();
        auto swapchain_info = SwapchainCreateInfo{
            .width          = window_size.x,
            .height         = window_size.y,
            .window_handle  = window->window_handle(),
            .present_mode   = EPresentMode::immediate,
            .surface_format = EFormat::rgba8_unorm,
        };
        swapchain = device->create_swapchain(&swapchain_info);

        imgui = std::make_shared<cannele::rhi::ImGuiWrapper>(device.get(), window.get());

        asset_manager = std::make_unique<cannele::core::resource::AssetManager>();

        auto renderer_info = cannele::graphics::renderer::RendererCreateInfo{
            .device = device.get(),
            .swapchain = swapchain.get(),
            .imgui = imgui.get()
        };
        // renderer = std::make_unique<cannele::graphics::renderer::TestComputeRenderer>(&renderer_info);
        renderer = std::make_unique<cannele::graphics::renderer::DeferredRenderer>(&renderer_info);
    }

    Engine::~Engine()
    {
        device->wait_idle();

        renderer.reset();

        asset_manager.reset();

        imgui.reset();

        swapchain.reset();

        device.reset();

        window.reset();
    }

    auto Engine::run() -> void
    {
        window->excute_perframe([&]() -> void {
            renderer->render();
        });
    }
}
