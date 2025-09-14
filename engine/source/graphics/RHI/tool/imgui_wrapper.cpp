#include "imgui_wrapper.hpp"
#include "async_uploader.hpp"

#include <platform/glfw_window.hpp>

#include <imgui_impl_glfw.h>
#include <common.slang.hpp>
#include <span>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        // Must match the definition in imgui.slang
        struct ImGuiDrawPushConstants
        {
            math::float2 scale;
            math::float2 translate;

            descriptor::Sampler2DHandle font_texture;
            math::uint use_font;
            math::uint pad_0;
        };
    }

    ImGuiWrapper::ImGuiWrapper(IDevice* device, platform::Window* window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(static_cast<platform::GLFWWindowImpl*>(window)->glfw_window, true);

        auto io = &ImGui::GetIO();
        auto pixels = (unsigned char*) nullptr;
        auto width  = 0;
        auto height = 0;
        io->Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        auto upload_size = width * height * 4 * sizeof(char);

        auto texture_info = TextureCreateInfo{
            .format        = EFormat::rgba8_unorm,
            .usage         = ETextureUsage::sampled | ETextureUsage::transfer_dst,
            .extent        = {(uint32_t) width, (uint32_t) height, 1},
            .final_state   = EResourceStates::sampled_texture
        };
        font_texture = device->create_texture("ImGuiFont Texture", &texture_info);
        auto pixel_view = TextureSliceDataView{reinterpret_cast<std::byte*>(pixels), width * 4, height, 1};
        CNE_ASSERT_WITH(pixel_view.size() == upload_size, std::format("Texture size mismatch, expected {} but got {}", upload_size, pixel_view.size()));

        auto async_uploader = device->async_uploader();
        async_uploader->add_task(
            [this, pixel_view = std::move(pixel_view)](CommandEncoder* command_encoder) {
                command_encoder->upload_texture_data(font_texture, pixel_view);
            },
            []() { CNE_TRACE("ImGui font texture upload finished"); }
        );

        auto sampler_info = SamplerCreateInfo{};
        font_sampler = device->create_sampler("imgui sampler", &sampler_info);
        io->Fonts->SetTexID(font_texture->view()->descriptor_handle().x);

        auto program = device->create_graphics_shader_program("imgui", "main_vs", "main_fs");
        program->compile_shader();
        // TODO: Recreate pipeline if format mismatch.
        auto pipeline_create_info = GraphicsPipelineCreateInfo{
            .program = program,
            .colors = {ColorAttachmentInfo{EFormat::rgba8_unorm}},
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

    auto ImGuiWrapper::render(CommandEncoderHandle cmd_encoder, TextureHandle texture) -> void
    {
        cmd_encoder->push_debug_label("ImGUI", math::float4{1.0f});
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
        auto index_data  = std::vector<std::byte>{index_size};
        auto p_vtx_dst = reinterpret_cast<ImDrawVert*>(vertex_data.data());
        auto p_idx_dst = reinterpret_cast<ImDrawIdx*>(index_data.data());
        for (auto n = 0; n < draw_data->CmdListsCount; n++) {
            auto cmd_list = draw_data->CmdLists[n];
            std::memcpy(p_vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(p_idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            p_vtx_dst += cmd_list->VtxBuffer.Size;
            p_idx_dst += cmd_list->IdxBuffer.Size;
        }

        auto device = cmd_encoder->get_device();

        if (!imgui_vertex_buffer || vertex_size > imgui_vertex_buffer->description()->size_bytes) {
            auto vertex_buffer_info = BufferCreateInfo{
                .memory_type       = EMemoryType::gpu_only,
                .usage      = EBufferUsage::vertex | EBufferUsage::transfer_dst,
                .size_bytes = vertex_size * 2,
            };
            imgui_vertex_buffer = device->create_buffer("ImGui Vertex Buffer", &vertex_buffer_info);
        }
        if (!imgui_index_buffer || index_size > imgui_index_buffer->description()->size_bytes) {
            auto index_buffer_info = BufferCreateInfo{
                .memory_type       = EMemoryType::gpu_only,
                .usage      = EBufferUsage::index | EBufferUsage::transfer_dst,
                .size_bytes = index_size * 2,
            };
            imgui_index_buffer = device->create_buffer("ImGui Index Buffer", &index_buffer_info);
        }

        cmd_encoder->upload_buffer_data(imgui_vertex_buffer, 0, vertex_data);
        cmd_encoder->upload_buffer_data(imgui_index_buffer, 0, index_data);

        auto scale_translate = math::float4{};
        scale_translate[0] = 2.0f / draw_data->DisplaySize.x;
        scale_translate[1] = 2.0f / draw_data->DisplaySize.y;
        scale_translate[2] = -1.0f - draw_data->DisplayPos.x * scale_translate[0];
        scale_translate[3] = -1.0f - draw_data->DisplayPos.y * scale_translate[1];

        auto push_constants = ImGuiDrawPushConstants{};
        push_constants.scale        = {2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y};
        push_constants.translate    = {-1.0f - draw_data->DisplayPos.x * push_constants.scale.x, -1.0f - draw_data->DisplayPos.y * push_constants.scale.y};
        push_constants.font_texture = {font_texture->view()->descriptor_handle().x, font_sampler->descriptor_handle().x};
        push_constants.use_font     = true;

        auto color_attachment = ColorAttachment{
            texture->view(),
            ELoadOp::load,
            EStoreOp::store,
            math::float4{0.5f, 0.5f, 0.5f, 0.5f},
        };

        auto graphics_encoder = cmd_encoder->begin_graphics_pass({&color_attachment, 1});
        auto object = graphics_encoder->bind_pipeline(imgui_pipeline);

        auto reflection = imgui_pipeline->program()->find_type_by_name("ImGuiDrawPushConstants");
        auto push_constant_object = device->create_shader_object(nullptr, reflection, ShaderObjectType::structured_buffer);
        auto constant_writer = ShaderObjectWriter{push_constant_object.get()};
        constant_writer["scale"].set_data(
            math::float2{2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y}
        );
        constant_writer["translate"].set_data(
            math::float2{-1.0f - draw_data->DisplayPos.x * push_constants.scale.x, -1.0f - draw_data->DisplayPos.y * push_constants.scale.y}
        );
        auto descriptor_handle = math::float2{font_texture->view()->descriptor_handle().x, font_sampler->descriptor_handle().x};
        constant_writer["font_texture"].set_descriptor_handle(
            descriptor_handle
        );
        constant_writer["use_font"].set_data(1u);

        auto viewports = std::vector<Viewport>{Viewport{0.0f, 0.0f, (float) framebuffer_size.x, (float) framebuffer_size.y}};
        auto vertex_buffers = std::vector<BufferView>{{imgui_vertex_buffer, 0}};
        auto blend_state = std::vector<BlendState>{BlendState{
            .enable_blend = true,
            .color_blend = {
                .src_factor = EBlendFactor::src_alpha,
                .dst_factor = EBlendFactor::one_minus_src_alpha,
                .blend_op = EBlendOperation::add,
            },
            .alpha_blend = {
                .src_factor = EBlendFactor::one,
                .dst_factor = EBlendFactor::one_minus_src_alpha,
                .blend_op = EBlendOperation::add,
            },
        }};
        auto graphics_state = GraphicsState{};
        graphics_state.viewports = viewports;
        graphics_state.vertex_buffers = vertex_buffers;
        graphics_state.vertex_input_state = &imgui_vertex_input_state;
        graphics_state.index_buffer = {imgui_index_buffer, 0};
        graphics_state.index_type = EIndexType::uint16;
        graphics_state.blend_states = blend_state;

        auto clip_offset = draw_data->DisplayPos;
        auto clip_scale  = draw_data->FramebufferScale;
        auto global_vtx_offset = 0;
        auto global_idx_offset = 0;
        auto descriptor_index  = 0;
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
                    auto scissors = std::vector{
                        Scissor{
                            .x = (int) clip_min.x,
                            .y = (int) clip_min.y,
                            .width  = (uint32_t) (clip_max.x - clip_min.x),
                            .height = (uint32_t) (clip_max.y - clip_min.y),
                        }
                    };
                    graphics_state.scissors = scissors;
                    auto texture_id = cmd->TexRef.GetTexID();
                    if (texture_id != descriptor_handle.x) {
                        constant_writer["font_texture"].set_data(math::float2{texture_id, descriptor_handle.y});
                        push_constants.use_font = false;
                    }

                    auto writer = ShaderObjectWriter{object};
                    writer["constants"].set_object(push_constant_object);
                    graphics_encoder->set_graphics_state(graphics_state);
                    // TODO: update descriptor set when texture is different from font texture.
                    auto draw_args = DrawArguments{
                        .vertex_count   = cmd->ElemCount,
                        .instance_count  = 1,
                        .first_vertex   = cmd->VtxOffset + global_vtx_offset,
                        .first_instance = 0,
                        .first_index    = cmd->IdxOffset + global_idx_offset,
                    };
                    graphics_encoder->draw_indexed(draw_args);
                }
            }

            global_vtx_offset += im_cmd_list->VtxBuffer.Size;
            global_idx_offset += im_cmd_list->IdxBuffer.Size;
        }
        auto scissor = Scissor{
            .x = 0,
            .y = 0,
            .width  = (uint32_t) framebuffer_size.x,
            .height = (uint32_t) framebuffer_size.y,
        };
        graphics_state.scissors = {&scissor, 1};
        graphics_encoder->set_graphics_state(graphics_state);
        graphics_encoder->finish();
        cmd_encoder->pop_debug_label();
    }
}
