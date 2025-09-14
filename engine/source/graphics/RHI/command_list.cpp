#include "command_list.hpp"
#include "resource.hpp"

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
    }

    CommandList::CommandList(Arena* arena, std::set<std::shared_ptr<IResource>>* tracked_resources)
        : arena(arena)
        , tracked_resources(tracked_resources)
    {}

    CommandList::~CommandList()
    {
        command_slots = {};
        last_command_slot = {};
    }

    auto CommandList::reset() -> void
    {
        command_slots = {};
        last_command_slot = {};
    }

    auto CommandList::write(commands::copy_buffer&& cmd) -> void
    {
        track_resource(cmd.src_buffer);
        track_resource(cmd.dst_buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::copy_texture&& cmd) -> void
    {
        track_resource(cmd.src_texture);
        track_resource(cmd.dst_texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::copy_texture_to_buffer&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::clear_buffer_uint&& cmd) -> void
    {
        track_resource(cmd.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::clear_texture_float&& cmd) -> void
    {
        track_resource(cmd.texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::clear_texture_uint&& cmd) -> void
    {
        track_resource(cmd.texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::clear_texture_depth_stencil&& cmd) -> void
    {
        track_resource(cmd.texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::upload_texture_data&& cmd) -> void
    {
        track_resource(cmd.texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::resolve_query&& cmd) -> void
    {
        track_resource(cmd.query);
        track_resource(cmd.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::begin_render_pass&& cmd) -> void
    {
        if (!cmd.color_attachments.empty()) {
            auto write_ptr = (commands::ColorAttachment*) write_data(cmd.color_attachments.data(), sizeof(commands::ColorAttachment) * cmd.color_attachments.size());
            cmd.color_attachments = std::span{write_ptr, cmd.color_attachments.size()};
            for (auto& attachment : cmd.color_attachments) {
                track_resource(attachment.texture);
            }
        }
        if (cmd.depth_stencil_attachment) {
            cmd.depth_stencil_attachment = (commands::DepthStencilAttachment*) write_data(cmd.depth_stencil_attachment, sizeof(commands::DepthStencilAttachment));
            track_resource(cmd.depth_stencil_attachment->texture);
        }
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::end_render_pass&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::set_graphics_state&& cmd) -> void
    {
        track_resource(cmd.pipeline);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::draw&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::draw_indexed&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::draw_indirect&& cmd) -> void
    {
        track_resource(cmd.indirect_buffer.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::draw_indexed_indirect&& cmd) -> void
    {
        track_resource(cmd.indirect_buffer.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::dispatch_mesh&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::begin_compute_pass&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::end_compute_pass&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::set_compute_state&& cmd) -> void
    {
        track_resource(cmd.pipeline);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::dispatch_compute&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::dispatch_compute_indirect&& cmd) -> void
    {
        track_resource(cmd.indirect_buffer.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::begin_ray_tracing_pass&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::end_ray_tracing_pass&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::set_ray_tracing_state&& cmd) -> void
    {
        track_resource(cmd.pipeline);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::dispatch_rays&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::set_buffer_state&& cmd) -> void
    {
        track_resource(cmd.buffer);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::set_texture_state&& cmd) -> void
    {
        track_resource(cmd.texture);
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::commit_barrier&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::push_command_label&& cmd) -> void
    {
        cmd.name = std::string_view{(char*) write_data(cmd.name.data(), cmd.name.size()), cmd.name.size()};
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::pop_command_label&& cmd) -> void
    {
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::insert_debug_marker&& cmd) -> void
    {
        cmd.name = std::string_view{(char*) write_data(cmd.name.data(), cmd.name.size()), cmd.name.size()};
        write_command(std::move(cmd));
    }

    auto CommandList::write(commands::write_timestamp&& cmd) -> void
    {
        track_resource(cmd.query);
        write_command(std::move(cmd));
    }
}
