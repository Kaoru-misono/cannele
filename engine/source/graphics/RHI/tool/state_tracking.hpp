#pragma once

#include "../resource.hpp"

namespace cannele::inline graphics::rhi
{
    struct BufferState final
    {
        EResourceStates state{EResourceStates::unknown};
    };

    struct TextureState final
    {
        std::vector<EResourceStates> subresource_states{};
        EResourceStates state{EResourceStates::unknown};
    };

    struct BufferBarrier final
    {
        RHIBuffer* buffer{};

        EResourceStates src_state{EResourceStates::unknown};
        EResourceStates dst_state{EResourceStates::unknown};
    };

    struct TextureBarrier final
    {
        RHITexture* texture{};

        TextureSubresourceRange subresources{};
        bool whole_texture{false};

        EResourceStates src_state{EResourceStates::unknown};
        EResourceStates dst_state{EResourceStates::unknown};
    };

    struct ResourceStateTracker final
    {
        std::vector<TextureBarrier> texture_barriers{};
        std::vector<BufferBarrier> buffer_barriers{};

        std::unordered_map<RHITexture*, TextureState> texture_states{};
        std::unordered_map<RHIBuffer*, BufferState> buffer_states{};

        auto set_buffer_state(RHIBuffer* buffer, EResourceStates state) -> void;
        auto set_texture_state(RHITexture* texture, TextureSubresourceRange subresources, EResourceStates state) -> void;

        auto finish_tracking() -> void;

        auto clear_barriers() -> void;

        auto reset() -> void;

        auto get_buffer_state(RHIBuffer* buffer) -> BufferState*;
        auto get_texture_state(RHITexture* texture) -> TextureState*;
    };
}
