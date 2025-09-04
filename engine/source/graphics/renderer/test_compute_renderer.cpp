#include "renderer.hpp"

#include "platform/shader_compile.hpp"

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        REGISTER_SHADER_COMPOSITION(TestCS, "test_compute", "main_test_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(TestVS, "test_compute", "main_test_vs", EShaderStage::vertex);
        REGISTER_SHADER_COMPOSITION(TestFS, "test_compute", "main_test_fs", EShaderStage::fragment);

        using namespace rhi;

        std::vector<std::byte> spirv{};
    }

    TestComputeRenderer::TestComputeRenderer(RendererCreateInfo* info)
        : device(info->device)
        , swapchain(info->swapchain)
        , imgui_wrapper(info->imgui)
    {
        auto command_list_ci = CommandListCreateInfo{
            .queue_type = EQueueType::graphics,
        };
        command_list = device->create_command_list(&command_list_ci);

        // auto compiler = shader_compiler();
        // auto compile_info = ShaderCompileInfo("shader/slang/test_compute.slang", "main_test_cs", EShaderStage::compute);
        // auto env = ShaderCompileEnvironment{&compile_info};
        // auto args = env.build_args();
        // auto shader_data = compile_info.file.slurp_as_binary();
        // spirv = compiler->compile(shader_data, args).shader;
        // auto module_info = ShaderModuleCreateInfo{
        //     .name = compile_info.name,
        //     .file_name = compile_info.file.absolute_path(),
        //     .entry = "main_test_cs",
        //     .stage = compile_info.stage,
        //     .code = spirv,
        // };
        // shader_module = device->create_shader_module("Test shader", &module_info);
    }

    TestComputeRenderer::~TestComputeRenderer()
    {

    }

    auto TestComputeRenderer::render() -> void
    {
        device->new_frame(frame_count);
        imgui_wrapper->new_frame();
        swapchain->acquire_next_backbuffer();
        swapchain->enqueue_backbuffer_ready_wait_semaphore();

        auto backbuffer = swapchain->backbuffer();
        command_list->start();
        command_list->clear_texture_float(backbuffer, {}, math::float4{0.5f, 0.5f, 0.5f, 1.0f});

        auto framebuffer_size = backbuffer->description()->extent;
        auto render_target = RenderTarget{};
        render_target.info = RenderTargetInfo{};
        render_target.info.extent = framebuffer_size;
        render_target.info.color_formats.emplace_back(backbuffer->description()->format);
        render_target.info.blend_states.emplace_back();
        render_target.color_attachments.emplace_back(Attachment{swapchain->backbuffer()});
        auto graphics_pipeline_info = GraphicsPipelineCreateInfo{};
        graphics_pipeline_info.vs = device->shader_factory()->get_shader<TestVS>();
        graphics_pipeline_info.fs = device->shader_factory()->get_shader<TestFS>();
        graphics_pipeline_info.render_target_info = render_target.info;
        auto pipeline = device->create_graphics_pipeline("Test pipeline", &graphics_pipeline_info);

        auto graphics_state = GraphicsState{};
        graphics_state.pipeline = pipeline;
        graphics_state.render_target = &render_target;
        graphics_state.viewport_state.viewports.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);
        graphics_state.viewport_state.scissors.emplace_back(0.0f, 0.0f, framebuffer_size.x, framebuffer_size.y);

        command_list->set_graphics_state(&graphics_state);

        auto buffer_info = BufferCreateInfo{
            .size_bytes = sizeof(uint) * 900 * 600,
            .type = EBufferType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
        };
        auto test_buffer = device->create_buffer("Test Buffer", &buffer_info);

        auto compute_pipeline_info = ComputePipelineCreateInfo{
            .compute_shader = device->shader_factory()->get_shader<TestCS>(),
            .push_constant_size = sizeof(uint2),
        };
        auto compute_pipeline = device->create_compute_pipeline("Test Pipeline", &compute_pipeline_info);
        auto compute_state = ComputeState{
            .pipeline = compute_pipeline,
        };

        auto handle = test_buffer->descriptor_handle();
        command_list->push_constants(handle);
        auto draw_args = DrawArguments{.num_vertices = 3, .num_instances = 1};
        command_list->draw(&draw_args);
        command_list->set_buffer_state(test_buffer, EResourceStates::storage_buffer_read_write);
        command_list->commit_barriers();
        command_list->set_compute_state(&compute_state);
        command_list->push_constants(handle);
        CNE_TRACE("Dispatching 1x1x1, frame count: {}", frame_count);
        command_list->dispatch(1, 1, 1);

        command_list->finish();

        swapchain->enqueue_render_finish_signal_semaphore();
        auto time = device->submit_command_lists({&command_list, 1});
        swapchain->present(time);
        frame_count++;
    }
}
