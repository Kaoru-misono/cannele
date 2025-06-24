#include "glfw_window.hpp"

#include <GLFW/glfw3.h>

#include <string>

namespace cannele::inline platform
{
    inline namespace
    {
        auto action_state(int p_action) -> PressState
        {
            return p_action == GLFW_PRESS ? PressState::press : p_action == GLFW_RELEASE ? PressState::release : PressState::repeat;
        }

        auto glfw_resize_callback(GLFWwindow* glfw_window, int width, int height) -> void
        {
            auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));

            for (auto& func: window->resize_callbacks) {
                func(window, math::int2{width, height});
            }
        }

        auto glfw_close_callback(GLFWwindow* glfw_window) -> void
        {
            auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));

            glfwHideWindow(glfw_window);

            for (auto& func: window->close_callbacks) {
                func(window);
            }
        }

        auto glfw_mouse_callback(GLFWwindow* glfw_window, int button, int action, int mods) -> void
        {
            auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));

            auto mouse = button == GLFW_MOUSE_BUTTON_LEFT ? Mouse::left
                : button == GLFW_MOUSE_BUTTON_RIGHT ? Mouse::right
                : Mouse::middle;
            auto xpos = (double) 0;
            auto ypos = (double) 0;
            glfwGetCursorPos(glfw_window, &xpos, &ypos);

            for (auto& func: window->mouse_callbacks) {
                func(window, mouse, action_state(action), xpos, ypos);
            }
        }

        auto glfw_key_callback(GLFWwindow* glfw_window, int key, int scancode, int action, int mods) -> void
        {
            auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));

            for (auto& func: window->key_callbacks) {
                func(window, (Keyboard) key, action_state(action));
            }
        }

        auto glfw_mouse_move_callback(GLFWwindow* glfw_window, double x, double y) -> void
        {
            auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfw_window));

            for (auto& func: window->mouse_move_callbacks) {
                func(window, x, y);
            }
        }
    }

    GLFWWindowImpl::GLFWWindowImpl(std::string_view title) : Window{title}
    {
    }

    GLFWWindowImpl::~GLFWWindowImpl()
    {
        if (glfw_window) {
            glfwDestroyWindow(glfw_window);
            glfwTerminate();
        }
    }

    auto GLFWWindowImpl::init(uint32_t width, uint32_t height) -> void
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        glfw_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        glfwSetWindowUserPointer(glfw_window, this);
        glfwSetFramebufferSizeCallback(glfw_window, glfw_resize_callback);
        glfwSetWindowCloseCallback(glfw_window, glfw_close_callback);
        glfwSetCursorPosCallback(glfw_window, glfw_mouse_move_callback);
        glfwSetKeyCallback(glfw_window, glfw_key_callback);
        glfwSetMouseButtonCallback(glfw_window, glfw_mouse_callback);
    }

    auto GLFWWindowImpl::excute_perframe(std::function<auto() -> void> function) -> void
    {
        while (!glfwWindowShouldClose(glfw_window)) {
            glfwPollEvents();

            function();
        }
    }

    auto GLFWWindowImpl::window_time() const noexcept -> double
    {
        return glfwGetTime();
    }

    auto GLFWWindowImpl::size() const noexcept -> math::uint2
    {
        auto size = math::int2{};
        glfwGetFramebufferSize(glfw_window, &size.x, &size.y);
        return size;
    }

    auto GLFWWindowImpl::wait_events() const noexcept -> void
    {
        glfwWaitEvents();
    }

    auto GLFWWindowImpl::get_instance_extension() const noexcept -> std::vector<const char*>
    {
        auto glfw_extension_count = (uint32_t) 0;
        auto glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        return {glfw_extensions, glfw_extensions + glfw_extension_count};
    }

    auto GLFWWindowImpl::dpi_scale() const noexcept -> math::float2
    {
        auto width_scale = 0.0f;
        auto height_scale = 0.0f;
        glfwGetWindowContentScale(glfw_window, &width_scale, &height_scale);
        return math::float2(width_scale, height_scale);
    }

    auto GLFWWindowImpl::hide(bool p_hide) const noexcept -> void
    {
        if (p_hide) {
            glfwHideWindow(glfw_window);
        } else {
            glfwShowWindow(glfw_window);
        }
    }

    auto GLFWWindowImpl::register_resize_callback(Resize_Callback&& callback) -> void
    {
        resize_callbacks.emplace_back(std::move(callback));
    }

    auto GLFWWindowImpl::register_close_callback(Close_Callback&& callback) -> void
    {
        close_callbacks.emplace_back(std::move(callback));
    }

    auto GLFWWindowImpl::register_mouse_callback(Mouse_Callback&& callback) -> void
    {
        mouse_callbacks.emplace_back(std::move(callback));
    }

    auto GLFWWindowImpl::register_key_callback(Key_Callback&& callback) -> void
    {
        key_callbacks.emplace_back(std::move(callback));
    }

    auto GLFWWindowImpl::register_mouse_move_callback(Mouse_Move_Callback&& callback) -> void
    {
        mouse_move_callbacks.emplace_back(std::move(callback));
    }
}
