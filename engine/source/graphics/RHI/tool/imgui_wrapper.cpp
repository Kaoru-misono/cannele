#include "imgui_wrapper.hpp"

#include <platform/glfw_window.hpp>

#include <imgui_impl_glfw.h>
#include <structure.hpp>
#include <span>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        DECLARE_DEFAULT_SHADER_AND_REGISTER(ImGuiShaderVS, "shader/hlsl/imgui.hlsl", "main_vs", EShaderStage::vertex);
        DECLARE_DEFAULT_SHADER_AND_REGISTER(ImGuiShaderFS, "shader/hlsl/imgui.hlsl", "main_fs", EShaderStage::fragment);
    }

    ImGuiWrapper::ImGuiWrapper(IDevice* device, platform::Window* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(static_cast<platform::GLFWWindowImpl*>(window)->glfw_window, true);

        auto io = &ImGui::GetIO();
        auto pixels = (unsigned char*) nullptr;
        auto width = 0;
        auto height = 0;
        io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        auto upload_size = width * height * 4 * sizeof(char);

        if (!device) {
            CNE_ERROR("Device is nullptr, ImGui will not work properly.");
            return;
        }

        auto texture_info = TextureCreateInfo{
            .format = EFormat::rgba8_unorm,
            .usage = ETextureUsage::sampled | ETextureUsage::transfer_dst,
            .extent = {width, height},
            .initial_state = EResourceStates::sampled_texture
        };
        font_texture = device->create_texture("ImGuiFont Texture", &texture_info);
        auto cmd_list_info = CommandListCreateInfo{.enable_immediate_submit = true, .queue_type = EQueueType::transfer};
        auto cmd_list = device->create_command_list(&cmd_list_info);
        auto pixel_view = TextureSliceDataView{reinterpret_cast<std::byte*>(pixels), width * 4, height, 1};
        CNE_ASSERT(pixel_view.size() == upload_size);
        cmd_list->start();
        cmd_list->write_texture(font_texture, 0, 0, pixel_view);
        cmd_list->lock_texture_state(font_texture, EResourceStates::sampled_texture);
        cmd_list->commit_barriers(EQueueType::transfer, EQueueType::graphics);
        cmd_list->finish();
        device->submit_command_lists({&cmd_list, 1}, EQueueType::transfer);

        auto sampler_info = SamplerCreateInfo{};
        font_sampler = device->create_sampler("imgui sampler", &sampler_info);
        io->Fonts->SetTexID(font_texture->bindless_index());

        auto shader_factory = device->get_shader_factory();
        if (!shader_factory) {
            CNE_ERROR("ShaderFactory is not initialized, ImGui will not work properly.");
            return;
        }
        auto imgui_vertex_shader = shader_factory->get_shader<ImGuiShaderVS>();
        auto imgui_fragment_shader = shader_factory->get_shader<ImGuiShaderFS>();
        // TODO: Recreate pipeline if format mismatch.
        auto pipeline_create_info = GraphicsPipelineCreateInfo{
            .vs = imgui_vertex_shader,
            .ps = imgui_fragment_shader,
            .render_target_info = {
                .color_formats = {EFormat::rgba8_unorm},
            },
            .topology = ERasterizerTopologyType::triangle_list,
        };
        imgui_pipeline = device->create_graphics_pipeline("ImGui Pipeline", &pipeline_create_info);

        auto stream = imgui_vertex_input_state.add_stream(sizeof(ImDrawVert),  EVertexInputRate::vertex);
        stream->add_attribute(0, 0, EFormat::rg32_float);
        stream->add_attribute(1, sizeof(math::float2), EFormat::rg32_float);
        stream->add_attribute(2, sizeof(math::float4), EFormat::rgba8_unorm);
    }

    ImGuiWrapper::~ImGuiWrapper()
    {
        auto io = &ImGui::GetIO();
        io->Fonts->SetTexID(nullptr);
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    auto ImGuiWrapper::new_frame() -> void
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::PushFont(imgui_current_font);
    }

    auto ImGuiWrapper::render(RHICommandList* cmd_list, TextureHandle texture) -> void
    {
        cmd_list->push_command_label("ImGUI", math::float4{1.0f});
        ImGui::PopFont();
        ImGui::Render();
        auto draw_data = ImGui::GetDrawData();
        auto framebuffer_size = math::int2{
            draw_data->DisplaySize.x * draw_data->FramebufferScale.x,
            draw_data->DisplaySize.y * draw_data->FramebufferScale.y
        };
        if (framebuffer_size.x <= 0 || framebuffer_size.y <= 0 || draw_data->TotalVtxCount <= 0) return;

        auto vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        auto index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

        auto vertex_data = std::vector<std::byte>{vertex_size};
        auto index_data = std::vector<std::byte>{index_size};
        auto p_vtx_dst = reinterpret_cast<ImDrawVert*>(vertex_data.data());
        auto p_idx_dst = reinterpret_cast<ImDrawIdx*>(index_data.data());
        for (auto n = 0; n < draw_data->CmdListsCount; n++) {
            auto cmd_list = draw_data->CmdLists[n];
            std::memcpy(p_vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(p_idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            p_vtx_dst += cmd_list->VtxBuffer.Size;
            p_idx_dst += cmd_list->IdxBuffer.Size;
        }

        auto device = cmd_list->device();
        auto cmd_list_info = CommandListCreateInfo{
            .enable_immediate_submit = true,
            .queue_type = EQueueType::transfer
        };

        if (!imgui_vertex_buffer || vertex_size > imgui_vertex_buffer->description()->size_bytes) {
            auto vertex_buffer_info = BufferCreateInfo{
                .size_bytes = vertex_size * 2,
                .type       = EBufferType::gpu_only,
                .usage      = EBufferUsage::vertex | EBufferUsage::transfer_dst
            };
            imgui_vertex_buffer = device->create_buffer("ImGui Vertex Buffer", &vertex_buffer_info);
        }
        if (!imgui_index_buffer || index_size > imgui_index_buffer->description()->size_bytes) {
            auto index_buffer_info = BufferCreateInfo{
                .size_bytes = index_size * 2,
                .type       = EBufferType::gpu_only,
                .usage      = EBufferUsage::index | EBufferUsage::transfer_dst
            };
            imgui_index_buffer = device->create_buffer("ImGui Index Buffer", &index_buffer_info);
        }

        auto transfer_cmd_list = device->create_command_list(&cmd_list_info);
        transfer_cmd_list->start();
        transfer_cmd_list->write_buffer(imgui_vertex_buffer, vertex_data, 0);
        transfer_cmd_list->write_buffer(imgui_index_buffer, index_data, 0);
        transfer_cmd_list->set_buffer_state(imgui_vertex_buffer, EResourceStates::vertex_buffer);
        transfer_cmd_list->set_buffer_state(imgui_index_buffer, EResourceStates::index_buffer);
        transfer_cmd_list->commit_barriers(EQueueType::transfer, EQueueType::graphics);
        transfer_cmd_list->finish();
        auto submit_time = device->submit_command_lists({&transfer_cmd_list, 1}, EQueueType::transfer);
        // Because we insert a queue transfer barrier, maybe we need not to wait?
        // cmd_list->wait_for_submit(EQueueType::transfer, submit_time);

        auto scale_translate = math::float4{};
        scale_translate[0] = 2.0f / draw_data->DisplaySize.x;
        scale_translate[1] = 2.0f / draw_data->DisplaySize.y;
        scale_translate[2] = -1.0f - draw_data->DisplayPos.x * scale_translate[0];
        scale_translate[3] = -1.0f - draw_data->DisplayPos.y * scale_translate[1];

        auto push_coustants_data = std::vector<std::byte>{sizeof(ImGuiDrawPushConsts)};
        auto push_constants = reinterpret_cast<ImGuiDrawPushConsts*>(push_coustants_data.data());
        push_constants->scale      = {2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y};
        push_constants->translate  = {-1.0f - draw_data->DisplayPos.x * push_constants->scale.x, -1.0f - draw_data->DisplayPos.y * push_constants->scale.y};
        push_constants->sampler_id = font_sampler->bindless_index();
        push_constants->use_font   = true;

        auto render_target = RenderTarget{};
        auto target_info = &render_target.info;
        target_info->extent        = framebuffer_size;
        target_info->color_formats = {texture->description()->format};
        target_info->blend_states  = {BlendState{
            true,
            EBlendOperation::add,
            EBlendFactor::src_alpha,
            EBlendFactor::one_minus_src_alpha,
            EBlendOperation::add,
            EBlendFactor::one,
            EBlendFactor::one_minus_src_alpha,
        }};
        render_target.color_attachments = {Attachment{
            .texture      = texture,
            .subresources = TextureSubresourceSet{0, 1, 0, 1},
            .load         = ELoadOp::load,
            .store        = EStoreOp::store,
        }};
        render_target.clear_colors = {math::float4{0.5f, 0.5f, 0.5f, 0.5f}};

        auto graphics_state = GraphicsState{};
        graphics_state.pipeline                 = imgui_pipeline;
        graphics_state.render_target            = &render_target;
        graphics_state.viewport_state.viewports = {Viewport{0.0f, 0.0f, (float) framebuffer_size.x, (float) framebuffer_size.y}};
        graphics_state.vertex_input_state       = &imgui_vertex_input_state;
        graphics_state.vertex_buffer_bindings   = {VertexBufferBinding{imgui_vertex_buffer}};
        graphics_state.index_buffer_binding     = IndexBufferBinding{imgui_index_buffer, EFormat::index_uint16};

        cmd_list->set_graphics_state(&graphics_state);
        cmd_list->push_constants(push_coustants_data);

        auto clip_offset = draw_data->DisplayPos;
        auto clip_scale = draw_data->FramebufferScale;
        auto global_vtx_offset = 0;
        auto global_idx_offset = 0;
        auto descriptor_index = 0;
        for (auto n = 0; n < draw_data->CmdListsCount; n++) {
            auto im_cmd_list = draw_data->CmdLists[n];
            for (auto i = 0; i < im_cmd_list->CmdBuffer.Size; i++) {
                auto cmd = &im_cmd_list->CmdBuffer[i];
                if (cmd->UserCallback) {

                } else {
                    auto clip_min = ImVec2{
                        (cmd->ClipRect.x - clip_offset.x) * clip_scale.x,
                        (cmd->ClipRect.y - clip_offset.y) * clip_scale.y,
                    };
                    auto clip_max = ImVec2{
                        (cmd->ClipRect.z - clip_offset.x) * clip_scale.x,
                        (cmd->ClipRect.w - clip_offset.y) * clip_scale.y,
                    };
                    clip_min.x = clip_min.x < 0.0f ? 0.0f : clip_min.x;
                    clip_min.y = clip_min.y < 0.0f ? 0.0f : clip_min.y;
                    clip_max.x = clip_max.x > framebuffer_size.x ? framebuffer_size.x : clip_max.x;
                    clip_max.y = clip_max.y > framebuffer_size.y ? framebuffer_size.y : clip_max.y;
                    if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) continue;
                    graphics_state.viewport_state.scissors = {
                        Scissor{
                            .x = (int) clip_min.x,
                            .y = (int) clip_min.y,
                            .width  = (uint32_t) (clip_max.x - clip_min.x),
                            .height = (uint32_t) (clip_max.y - clip_min.y),
                        }
                    };
                    cmd_list->set_viewport_state(&graphics_state.viewport_state);
                    auto texture_id = cmd->TexRef.GetTexID();
                    if (texture_id != push_constants->texture_id) {
                        push_constants->texture_id = texture_id;
                        push_constants->use_font = false;
                        cmd_list->push_constants(push_coustants_data);
                    }
                    // TODO: update descriptor set when texture is different from font texture.
                    auto draw_args = DrawArguments{
                        .num_vertices   = cmd->ElemCount,
                        .num_instances  = 1,
                        .first_index    = cmd->IdxOffset + global_idx_offset,
                        .first_vertex   = cmd->VtxOffset + global_vtx_offset,
                        .first_instance = 0
                    };
                    cmd_list->draw_indexed(&draw_args);
                }
            }

            global_vtx_offset += im_cmd_list->VtxBuffer.Size;
            global_idx_offset += im_cmd_list->IdxBuffer.Size;
        }
        graphics_state.viewport_state.scissors = {
            Scissor{
                .x = 0,
                .y = 0,
                .width  = (uint32_t) framebuffer_size.x,
                .height = (uint32_t) framebuffer_size.y,
            }
        };
        cmd_list->set_viewport_state(&graphics_state.viewport_state);
        cmd_list->pop_command_label();
    }
}
