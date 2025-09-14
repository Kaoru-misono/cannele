#pragma once

#include "RHI_forward.hpp"
#include "RHI_definitions.hpp"

#include <core/arena.hpp>

#include <set>
#include <optional>

namespace cannele::inline graphics::rhi
{
    struct RHICommand {};

    #define RHI_COMMANDS(X) \
        X(copy_buffer) \
        X(copy_texture) \
        X(copy_texture_to_buffer) \
        X(clear_buffer_uint) \
        X(clear_texture_float) \
        X(clear_texture_uint) \
        X(clear_texture_depth_stencil) \
        X(upload_texture_data) \
        X(resolve_query) \
        X(begin_render_pass) \
        X(end_render_pass) \
        X(set_graphics_state) \
        X(draw) \
        X(draw_indexed) \
        X(draw_indirect) \
        X(draw_indexed_indirect) \
        X(dispatch_mesh) \
        X(begin_compute_pass) \
        X(end_compute_pass) \
        X(set_compute_state) \
        X(dispatch_compute) \
        X(dispatch_compute_indirect) \
        X(begin_ray_tracing_pass) \
        X(end_ray_tracing_pass) \
        X(set_ray_tracing_state) \
        X(dispatch_rays) \
        X(set_buffer_state) \
        X(set_texture_state) \
        X(commit_barrier) \
        X(push_command_label) \
        X(pop_command_label) \
        X(insert_debug_marker) \
        X(write_timestamp)

        // TODO:
        // X(build_acceleration_structure) \
        // X(copy_acceleration_structure) \
        // X(query_acceleration_structure_properties) \
        // X(serialize_acceleration_structure) \
        // X(deserialize_acceleration_structure) \
        // X(convert_cooperative_vertex_matrix) \


    #define GENERATE_ENUM(ENUM) ENUM,
    enum struct CommandID: uint32_t
    {
        RHI_COMMANDS(GENERATE_ENUM)
    };
    #undef GENERATE_ENUM

    namespace commands
    {
        struct copy_buffer
        {
            RHIBuffer* src_buffer{};
            size_t src_offset{};
            RHIBuffer* dst_buffer{};
            size_t dst_offset{};
            size_t size{};
        };

        struct copy_texture
        {
            RHITexture* src_texture{};
            TextureSubresourceSet src_subresources{};
            RHITexture* dst_texture{};
            TextureSubresourceSet dst_subresources{};
            uint32_t layer_count{};
        };

        struct copy_texture_to_buffer {};

        struct clear_buffer_uint
        {
            RHIBuffer* buffer{};
            BufferRange range{};
            uint32_t clear_value{0};
        };

        struct clear_texture_float
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{};
            math::float4 clear_color{0.0f};
        };

        struct clear_texture_uint
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{};
            math::uint4 clear_color{0};
        };

        struct clear_texture_depth_stencil
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{};
            std::optional<float> clear_depth{};
            std::optional<uint8_t> clear_stencil{};
        };

        struct upload_texture_data
        {
            RHITexture* texture{};
            uint32_t mip_level{};
            uint32_t array_layer{};
            TextureSliceDataView data{};
        };

        struct resolve_query
        {
            RHITimerQuery* query{};
            uint32_t query_index{};
            uint32_t query_count{1};
            RHIBuffer* buffer{};
            size_t offset{};
        };

        struct ColorAttachment final
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{0, 1, 0, 1};
            ELoadOp load{ELoadOp::clear};
            EStoreOp store{EStoreOp::store};
            math::float4 clear_color{0.0f};

            explicit constexpr operator bool () noexcept
            {
                return (bool) texture;
            }

            auto operator == (ColorAttachment const& other) const -> bool = default;
        };

        struct DepthStencilAttachment final
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{0, 1, 0, 1};
            ELoadOp depth_load{ELoadOp::clear};
            EStoreOp depth_store{EStoreOp::store};
            ELoadOp stencil_load{ELoadOp::clear};
            EStoreOp stencil_store{EStoreOp::store};
            float clear_depth{0.0f};
            uint8_t clear_stencil{0};
            bool depth_read_only{false};
            bool stencil_read_only{false};

            explicit constexpr operator bool () noexcept
            {
                return (bool) texture;
            }

            auto operator == (DepthStencilAttachment const& other) const -> bool = default;
        };

        struct RenderTarget
        {
            std::span<ColorAttachment> color_attachments{};
            std::optional<DepthStencilAttachment> depth_stencil_attachment{};
        };

        struct begin_render_pass
        {
            std::span<ColorAttachment> color_attachments{};
            DepthStencilAttachment* depth_stencil_attachment{};
        };

        struct end_render_pass {};

        struct BufferView
        {
            RHIBuffer* buffer{};
            size_t offset{};
        };

        struct GraphicsState
        {
            std::span<Viewport> viewports{};
            std::span<Scissor> scissors{};
            std::span<BufferView> vertex_buffers{};
            BufferView index_buffer{};
            EIndexType index_type{EIndexType::uint16};
        };

        struct set_graphics_state
        {
            GraphicsState state{};
            RHIGraphicsPipeline* pipeline{};
            void* push_constants{};
            size_t data_size{};
        };

        struct draw
        {
            DrawArguments args{};
        };

        struct draw_indexed
        {
            DrawArguments args{};
        };

        struct draw_indirect
        {
            uint32_t draw_count{1};
            BufferView indirect_buffer{};
        };

        struct draw_indexed_indirect
        {
            uint32_t draw_count{1};
            BufferView indirect_buffer{};
        };

        struct dispatch_mesh
        {
            uint32_t group_count_x{1};
            uint32_t group_count_y{1};
            uint32_t group_count_z{1};
        };

        struct begin_compute_pass {};
        struct end_compute_pass {};

        struct set_compute_state
        {
            RHIComputePipeline* pipeline{};
            BufferView indirect_buffer{};
            void* push_constants{};
            size_t data_size{};
        };

        struct dispatch_compute
        {
            uint32_t group_count_x{1};
            uint32_t group_count_y{1};
            uint32_t group_count_z{1};
        };

        struct dispatch_compute_indirect
        {
            BufferView indirect_buffer{};
        };

        struct begin_ray_tracing_pass {};

        struct end_ray_tracing_pass {};

        struct set_ray_tracing_state
        {
            RHIRayTracingPipeline* pipeline{};
            BufferView indirect_buffer{};
            void* push_constants{};
            size_t data_size{};
        };

        struct dispatch_rays
        {
            uint32_t raygen_shader_index{};
            uint32_t width{1};
            uint32_t height{1};
            uint32_t depth{1};
        };

        struct set_buffer_state
        {
            RHIBuffer* buffer{};
            EResourceStates state{};
        };

        struct set_texture_state
        {
            RHITexture* texture{};
            TextureSubresourceSet subresources{};
            EResourceStates state{};
        };

        struct commit_barrier {};

        struct push_command_label
        {
            std::string_view name{};
            math::float4 color{};
        };

        struct pop_command_label {};

        struct insert_debug_marker
        {
            std::string_view name{};
            math::float4 color{};
        };

        struct write_timestamp
        {
            RHITimerQuery* query{};
            uint32_t query_index{};
        };

        #define CNE_COMMAND_CHECK(COMMAND) \
            static_assert( \
                std::is_default_constructible_v<COMMAND> && std::is_trivially_copyable_v<COMMAND>, \
                #COMMAND " must be default constructible and trivially copyable." \
            );
        RHI_COMMANDS(CNE_COMMAND_CHECK)
        #undef CNE_COMMAND_CHECK

        template <typename T>
        struct Traits {};

        #define CNE_COMMAND_TRAITS(COMMAND) \
            template <> \
            struct Traits<COMMAND> \
            { \
                static constexpr auto id = CommandID::COMMAND; \
                static constexpr auto name = #COMMAND; \
            };
        RHI_COMMANDS(CNE_COMMAND_TRAITS)
        #undef CNE_COMMAND_TRAITS

    }

    struct CommandList final
    {
        struct CommandSlot final
        {
            CommandID id{};
            CommandSlot* next{};
            void* data{};
        };

    private:

        Arena* arena{};
        std::set<std::shared_ptr<IResource>>* tracked_resources{};
        CommandSlot* command_slots{};
        CommandSlot* last_command_slot{};

    public:

        CommandList(Arena* arena, std::set<std::shared_ptr<IResource>>* tracked_resources);
        ~CommandList();

        auto reset() -> void;

        auto write(commands::copy_buffer&& cmd) -> void;
        auto write(commands::copy_texture&& cmd) -> void;
        auto write(commands::copy_texture_to_buffer&& cmd) -> void;
        auto write(commands::clear_buffer_uint&& cmd) -> void;
        auto write(commands::clear_texture_float&& cmd) -> void;
        auto write(commands::clear_texture_uint&& cmd) -> void;
        auto write(commands::clear_texture_depth_stencil&& cmd) -> void;
        auto write(commands::upload_texture_data&& cmd) -> void;
        auto write(commands::resolve_query&& cmd) -> void;
        auto write(commands::begin_render_pass&& cmd) -> void;
        auto write(commands::end_render_pass&& cmd) -> void;
        auto write(commands::set_graphics_state&& cmd) -> void;
        auto write(commands::draw&& cmd) -> void;
        auto write(commands::draw_indexed&& cmd) -> void;
        auto write(commands::draw_indirect&& cmd) -> void;
        auto write(commands::draw_indexed_indirect&& cmd) -> void;
        auto write(commands::dispatch_mesh&& cmd) -> void;
        auto write(commands::begin_compute_pass&& cmd) -> void;
        auto write(commands::end_compute_pass&& cmd) -> void;
        auto write(commands::set_compute_state&& cmd) -> void;
        auto write(commands::dispatch_compute&& cmd) -> void;
        auto write(commands::dispatch_compute_indirect&& cmd) -> void;
        auto write(commands::begin_ray_tracing_pass&& cmd) -> void;
        auto write(commands::end_ray_tracing_pass&& cmd) -> void;
        auto write(commands::set_ray_tracing_state&& cmd) -> void;
        auto write(commands::dispatch_rays&& cmd) -> void;
        auto write(commands::set_buffer_state&& cmd) -> void;
        auto write(commands::set_texture_state&& cmd) -> void;
        auto write(commands::commit_barrier&& cmd) -> void;
        auto write(commands::push_command_label&& cmd) -> void;
        auto write(commands::pop_command_label&& cmd) -> void;
        auto write(commands::insert_debug_marker&& cmd) -> void;
        auto write(commands::write_timestamp&& cmd) -> void;

        auto get_commands() -> CommandSlot const* { return command_slots; }

        template <typename T>
        auto get_command(this auto&& self, CommandSlot const* slot) -> decltype(auto)
        {
            if constexpr (std::is_const_v<std::remove_reference_t<decltype(self)>>) {
                return reinterpret_cast<T const*>(slot->data);
            } else {
                return reinterpret_cast<T*>(slot->data);
            }
        }

        auto track_resource(IResource* resource) -> void
        {
            if (resource) {
                tracked_resources->insert(resource->shared_from_this());
            }
        }

        auto allocate_data(size_t size) -> void* { return arena->allocate(size); }

        auto write_data(void const* data, size_t size) -> void*
        {
            auto dst = arena->allocate(size);
            std::memcpy(dst, data, size);
            return dst;
        }

        template <typename T>
        auto write_command(T&& cmd) -> void
        {
            auto slot = arena->allocate<CommandSlot>();
            slot->id = commands::Traits<std::remove_cvref_t<T>>::id;
            slot->next = nullptr;
            slot->data = nullptr;

            if (last_command_slot) {
                last_command_slot->next = slot;
            } else {
                command_slots = slot;
            }
            last_command_slot = slot;

            slot->data = arena->allocate(sizeof(T));
            new (slot->data) T(std::forward<T>(cmd));
        }
    };
}
