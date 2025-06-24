#include "../glfw_window.hpp"

#include <GLFW/glfw3.h>

#ifdef _WIN32

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace cannele::inline platform
{
    auto GLFWWindowImpl::window_handle() const noexcept -> void*
    {
        return (void*) glfwGetWin32Window(glfw_window);
    }
}

#endif
