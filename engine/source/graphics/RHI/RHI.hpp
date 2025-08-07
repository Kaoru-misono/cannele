#pragma once

#include "RHI_resource.hpp"
#include "tool/shader_factory.hpp"
#include "tool/async_uploader.hpp"

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

    struct IDevice: IResource
    {
        CNE_INTERFACE(IDevice);

        virtual auto name() -> std::string = 0;
        virtual auto backend() -> EBackend { return EBackend::unknown; }

        // Call this at the beginning of the frame to run garbage collection.
        virtual auto new_frame(uint32_t frame_count) -> void = 0;

        [[nodiscard]] virtual auto create_buffer(std::string_view name, BufferCreateInfo* info) -> BufferHandle = 0;
        [[nodiscard]] virtual auto create_texture(std::string_view name, TextureCreateInfo* info) -> TextureHandle = 0;
        [[nodiscard]] virtual auto create_sampler(std::string_view name, SamplerCreateInfo* info) -> SamplerHandle = 0;
        [[nodiscard]] virtual auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo* info) -> GraphicsPipelineHandle = 0;
        [[nodiscard]] virtual auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo* info) -> ComputePipelineHandle = 0;
        [[nodiscard]] virtual auto create_shader_module(std::string_view name, ShaderModuleCreateInfo* info) -> ShaderModuleHandle = 0;
        [[nodiscard]] virtual auto create_command_list(CommandListCreateInfo* info) -> CommandListHandle = 0;
        [[nodiscard]] virtual auto create_swapchain(SwapchainCreateInfo* info) -> SwapchainHandle = 0;
        virtual auto shader_factory() -> ShaderFactory* = 0;
        virtual auto async_uploader() -> AsyncUploader* = 0;
        virtual auto submit_command_lists(std::span<CommandListHandle> lists, EQueueType type = EQueueType::graphics) -> uint64_t = 0;
        virtual auto current_timeline_value(EQueueType type) -> uint64_t = 0;
        virtual auto wait_for_submission(EQueueType type, uint64_t submission_time) -> void = 0;

        [[nodiscard]] virtual auto create_timer_query() -> TimerQueryHandle = 0;
        virtual auto poll_query(RHITimerQuery* query) -> bool = 0;
        virtual auto get_query_result(RHITimerQuery* query) -> float = 0;
        virtual auto reset_query(RHITimerQuery* query) -> void = 0;

        virtual auto wait_idle() -> void = 0;
    };

    using DeviceHandle = RefCountPtr<IDevice>;
}
