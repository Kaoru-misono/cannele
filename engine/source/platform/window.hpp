#pragma once

#include "platform/input/input.hpp"

#include <vector>
#include <string>
#include <functional>

namespace cannele::inline platform
{
    struct Window
    {
        using Resize_Callback = std::function<auto (Window*, math::int2) -> void>;
        using Close_Callback = std::function<auto (Window*) -> void>;
        using Mouse_Callback = std::function<auto (Window*, Mouse, PressState, float, float) -> void>;
        using Key_Callback = std::function<auto (Window*, Keyboard, PressState) -> void>;
        using Mouse_Move_Callback = std::function<auto (Window*, float, float) -> void>;

        Window(std::string_view title): title(title) {}
        virtual ~Window() = default;

        virtual auto init(uint32_t width, uint32_t height) -> void = 0;
        virtual auto window_handle() const noexcept -> void* = 0;
        virtual auto size() const noexcept -> math::uint2 = 0;
        virtual auto excute_perframe(std::function<auto () -> void> func) -> void = 0;
        virtual auto window_time() const noexcept -> double = 0;
        virtual auto wait_events() const noexcept -> void = 0;
        virtual auto get_instance_extension() const noexcept -> std::vector<const char*> = 0;
        virtual auto dpi_scale() const noexcept -> math::float2 = 0;
        virtual auto hide(bool p_hide) const noexcept -> void = 0;
        // TODO: Manage the lambdas lifetime, they should be move when the captured object is destroyed
        virtual auto register_resize_callback(Resize_Callback&& callback) -> void = 0;
        virtual auto register_close_callback(Close_Callback&& callback) -> void = 0;
        virtual auto register_mouse_callback(Mouse_Callback&& callback) -> void = 0;
        virtual auto register_key_callback(Key_Callback&& callback) -> void = 0;
        virtual auto register_mouse_move_callback(Mouse_Move_Callback&& callback) -> void = 0;

        std::string title{};

        std::vector<Resize_Callback> resize_callbacks{};
        std::vector<Close_Callback> close_callbacks{};
        std::vector<Mouse_Callback> mouse_callbacks{};
        std::vector<Key_Callback> key_callbacks{};
        std::vector<Mouse_Move_Callback> mouse_move_callbacks{};
    };
}
