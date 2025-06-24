#pragma once

#include "window.hpp"

#include "math/type.hpp"

struct GLFWwindow;

namespace cannele::inline platform
{
    struct GLFWWindowImpl final: Window
    {
        GLFWwindow* glfw_window{};

        GLFWWindowImpl(std::string_view title);
        ~GLFWWindowImpl() override;

        auto init(uint32_t width, uint32_t height) -> void override;
        auto window_handle() const noexcept -> void* override;
        auto size() const noexcept -> math::uint2 override;
        auto excute_perframe(std::function<auto () -> void> func) -> void override;
        auto window_time() const noexcept -> double override;
        auto wait_events() const noexcept -> void override;
        auto get_instance_extension() const noexcept -> std::vector<const char*> override;
        auto dpi_scale() const noexcept -> math::float2 override;
        auto hide(bool p_hide) const noexcept -> void override;
        auto register_resize_callback(Resize_Callback&& callback) -> void override;
        auto register_close_callback(Close_Callback&& callback) -> void override;
        auto register_mouse_callback(Mouse_Callback&& callback) -> void override;
        auto register_key_callback(Key_Callback&& callback) -> void override;
        auto register_mouse_move_callback(Mouse_Move_Callback&& callback) -> void override;
    };
}
