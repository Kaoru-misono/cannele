#include "renderer.hpp"

#include "platform/shader_compile.hpp"

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        using namespace rhi;
    }

    TestComputeRenderer::TestComputeRenderer(RendererCreateInfo* info)
        : device(info->device)
        , swapchain(info->swapchain)
        , imgui_wrapper(info->imgui)
    {
        // auto object = ShaderObject(compute_program->root_shader_object_layout()->entry_point_layout(0));
        // auto writer = ShaderObjectWriter{&object};
        // auto data = math::uint2{1, 2};
        // writer["test_buffer"].set_data(&data, sizeof(math::uint2));
    }

    TestComputeRenderer::~TestComputeRenderer()
    {

    }

    auto TestComputeRenderer::render() -> void
    {
        device->new_frame(frame_count);
        imgui_wrapper->new_frame();
        auto backbuffer = swapchain->acquire_next_backbuffer();

        ImGui::ShowDebugLogWindow();

        auto command_encoder = device->create_command_encoder(EQueueType::graphics);
        command_encoder->clear_texture_float(backbuffer, {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});

        auto framebuffer_size = backbuffer->description()->extent;

        auto graphics_pipeline_info = GraphicsPipelineCreateInfo{};
        graphics_pipeline_info.program = device->create_graphics_shader_program("test_compute", "main_test_vs", "main_test_fs");
        graphics_pipeline_info.colors.emplace_back(ColorAttachmentInfo{backbuffer->description()->format});
        auto pipeline = device->create_graphics_pipeline("Test pipeline", &graphics_pipeline_info);

        auto color_attachment = ColorAttachment{
            backbuffer->view(),
        };
        auto graphics_encoder = command_encoder->begin_graphics_pass({&color_attachment, 1});
        auto object = graphics_encoder->bind_pipeline(pipeline);

        auto viewports = std::vector<Viewport>{Viewport(0.0f, 0.0f, framebuffer_size.width, framebuffer_size.height)};
        auto scissors = std::vector<Scissor>{Scissor(0, 0, framebuffer_size.width, framebuffer_size.height)};
        auto blend_states = std::vector<BlendState>{BlendState{}};
        auto graphics_state = GraphicsState{};
        graphics_state.viewports = viewports;
        graphics_state.scissors = scissors;
        graphics_state.blend_states = blend_states;

        auto buffer_info = BufferCreateInfo{
            .memory_type = EMemoryType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
            .size_bytes = sizeof(uint3),
        };
        auto test_buffer = device->create_buffer("Test Buffer", &buffer_info);
        auto handle = test_buffer->descriptor_handle();

        auto writer = ShaderObjectWriter{object};
        writer["test_buffer"].set_bindless_buffer(test_buffer, EResourceStates::storage_write);
        graphics_encoder->set_graphics_state(graphics_state);

        auto draw_args = DrawArguments{.vertex_count = 3, .instance_count = 1};
        graphics_encoder->draw(draw_args);

        graphics_encoder->finish();

        auto compute_pipeline_info = ComputePipelineCreateInfo{
            .program = device->create_compute_shader_program("test_compute", "main_test_cs"),
        };
        auto compute_pipeline = device->create_compute_pipeline("Test Pipeline", &compute_pipeline_info);

        auto compute_encoder = command_encoder->begin_compute_pass();
        object = compute_encoder->bind_pipeline(compute_pipeline);
        writer = ShaderObjectWriter{object};
        writer["test_buffer"].set_bindless_buffer(test_buffer, EResourceStates::storage_write);
        compute_encoder->set_compute_state();
        compute_encoder->dispatch(1, 1, 1);
        compute_encoder->finish();

        imgui_wrapper->render(command_encoder, backbuffer);

        auto command_buffer = command_encoder->finish();

        device->submit_command_buffer(EQueueType::graphics, command_buffer);
        swapchain->present();
        frame_count++;
    }
}
