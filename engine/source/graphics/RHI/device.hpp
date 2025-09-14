#pragma once

#include "resource.hpp"
#include "shader.hpp"
#include "tool/buffer_block.hpp"
#include "tool/async_uploader.hpp"
#include "tool/slang_context.hpp"

#include <platform/window.hpp>

#include <string>
#include <span>

namespace cannele::inline graphics::rhi
{
    enum struct EBackend: uint8_t
    {
        unknown,
        vulkan,
    };

    struct IDevice: std::enable_shared_from_this<IDevice>
    {
        CNE_INTERFACE(IDevice);

        IDevice();

        virtual auto name() -> std::string = 0;
        virtual auto backend() -> EBackend { return EBackend::unknown; }

        // Call this at the beginning of the frame to run garbage collection.
        virtual auto new_frame(uint32_t frame_count) -> void = 0;

        [[nodiscard]] virtual auto create_buffer(std::string_view name, BufferCreateInfo const* info) -> BufferHandle = 0;
        [[nodiscard]] virtual auto create_texture(std::string_view name, TextureCreateInfo const* info) -> TextureHandle = 0;
        [[nodiscard]] virtual auto create_sampler(std::string_view name, SamplerCreateInfo const* info) -> SamplerHandle = 0;
        [[nodiscard]] virtual auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo const* info) -> GraphicsPipelineHandle = 0;
        [[nodiscard]] virtual auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo const* info) -> ComputePipelineHandle = 0;
        [[nodiscard]] virtual auto create_shader_module(std::string_view name, ShaderModuleCreateInfo const* info) -> ShaderModuleHandle = 0;
        [[nodiscard]] virtual auto create_command_encoder(EQueueType queue_type) -> std::shared_ptr<CommandEncoder> = 0;
        [[nodiscard]] virtual auto create_swapchain(SwapchainCreateInfo const* info) -> SwapchainHandle = 0;
        [[nodiscard]] virtual auto create_shader_program(ShaderProgramCreateInfo const* info) -> std::shared_ptr<RHIShaderProgram> = 0;
        virtual auto map_buffer(BufferHandle buffer) -> std::byte* = 0;
        virtual auto unmap_buffer(BufferHandle buffer) -> void = 0;
        virtual auto async_uploader() -> AsyncUploader* = 0;
        virtual auto submit_command_buffers(SubmitInfo* info) -> void = 0;
        virtual auto current_timeline_value(EQueueType type) -> uint64_t = 0;
        virtual auto wait_for_queue(EQueueType type) -> void = 0;

        [[nodiscard]] virtual auto create_timer_query() -> TimerQueryHandle = 0;
        virtual auto poll_query(RHITimerQuery* query) -> bool = 0;
        virtual auto get_query_result(RHITimerQuery* query) -> float = 0;
        virtual auto reset_query(RHITimerQuery* query) -> void = 0;

        virtual auto wait_idle() -> void = 0;

        auto submit_command_buffer(EQueueType queue_type, std::shared_ptr<RHICommandBuffer> command_buffer) -> void
        {
            SubmitInfo info{};
            info.queue_type = queue_type;
            info.command_buffers = {&command_buffer, 1};
            submit_command_buffers(&info);
        }

        auto entry_point_code_from_shader_cache(
            RHIShaderProgram* program,
            slang::IComponentType* component,
            std::string_view entry_point_name,
            uint32_t entry_point_index,
            uint32_t target_index
        ) -> std::span<std::byte const>;

        [[nodiscard]] auto create_graphics_shader_program(
            std::string_view module_name,
            std::string_view vertex_entry_name,
            std::string_view fragment_entry_name
        ) -> std::shared_ptr<RHIShaderProgram>;

        [[nodiscard]] auto create_graphics_shader_program(
            std::string_view vertex_module_name,
            std::string_view vertex_entry_name,
            std::string_view fragment_module_name,
            std::string_view fragment_entry_name
        ) -> std::shared_ptr<RHIShaderProgram>;

        [[nodiscard]] auto create_compute_shader_program(
            std::string_view module_name,
            std::string_view entry_point_name
        ) -> std::shared_ptr<RHIShaderProgram>;

        [[nodiscard]] virtual auto create_shader_object_layout(slang::ISession* session, slang::TypeLayoutReflection* type_layout) -> std::shared_ptr<ShaderObjectLayout> = 0;
        auto get_shader_object_layout(slang::ISession* session, slang::TypeReflection* type, ShaderObjectType container) -> std::shared_ptr<ShaderObjectLayout>;
        auto get_shader_object_layout(slang::ISession* session, slang::TypeLayoutReflection* type_layout) -> std::shared_ptr<ShaderObjectLayout>;

        [[nodiscard]] auto create_root_shader_object(RHIShaderProgram const* program) -> RootShaderObjectHandle;
        [[nodiscard]] auto create_shader_object(slang::ISession* session, slang::TypeReflection* type, ShaderObjectType container) -> ShaderObjectHandle;

        std::unique_ptr<BufferBlockPool> buffer_block_pool{};
        std::atomic_uint64_t next_shader_program_id{};

        SlangContext slang_context{};
        std::unordered_map<uint64_t, std::vector<std::byte>> shader_cache{};
        std::unordered_map<uint64_t, std::vector<std::byte>> pipeline_cache{};
        std::unordered_map<slang::TypeLayoutReflection*, std::shared_ptr<ShaderObjectLayout>> shader_object_layout_cache{};
        std::unordered_map<uint64_t, ShaderProgramHandle> shader_programs{};
        // Pipeline caches for graphics and compute pipelines.
        std::unordered_map<uint64_t, GraphicsPipelineHandle> graphics_pipelines{};
        std::unordered_map<uint64_t, ComputePipelineHandle> compute_pipelines{};
    };

    using DeviceHandle = std::shared_ptr<IDevice>;
}
