#include "input.hpp"

#include "platform/window.hpp"

namespace cannele::inline platform
{
    auto InputEvent::register_window_event(platform::Window* window) -> void
    {
        window->register_key_callback([&] (platform::Window* p_window, Keyboard p_key, PressState p_action) -> void {

            if (p_key == Keyboard::w) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
            if (p_key == Keyboard::s) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
            if (p_key == Keyboard::a) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
            if (p_key == Keyboard::d) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
            if (p_key == Keyboard::q) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
            if (p_key == Keyboard::e) {
                keys[p_key] = p_action == PressState::release ? 0 : 1;
            }
        });


        window->register_mouse_callback([&] (platform::Window* p_window, Mouse p_mouse, PressState p_action, float p_x, float p_y) -> void {

            if (p_action == PressState::press) {
                mouse_buttons[p_mouse] = 1;
                mouse_clicked_position[p_mouse] = {p_x, p_y};
            }
            if (p_action == PressState::release) {
                mouse_buttons[p_mouse] = 0;
                mouse_clicked_position[p_mouse] = {};
            }
        });

        window->register_mouse_move_callback([&] (platform::Window* p_window, float p_x, float p_y) -> void {
            mouse_position = {p_x, p_y};
            if (is_mouse_down(Mouse::right)) {
                auto delta_x = mouse_position.x - mouse_clicked_position[Mouse::right].x;
                auto delta_y = mouse_position.y - mouse_clicked_position[Mouse::right].y;
                mouse_draging[Mouse::right] = std::sqrt(delta_x * delta_x + delta_y * delta_y) > mouse_drag_min_distance;
            }
        });
    }

    auto InputEvent::cache_state() -> void
    {
        for (auto& [key, value]: keys) pre_keys[key] = value;
        for (auto& [key, value]: mouse_buttons) pre_mouse_buttons[key] = value;

        for (auto& [key, value]: mouse_draging) value = false;
        pre_mouse_position = mouse_position;
    }

    auto InputEvent::is_key_down(Keyboard key) -> bool
    {
        return keys[key];
    }

    auto InputEvent::is_key_pressed(Keyboard key) -> bool
    {
        return keys[key] && !pre_keys[key];
    }

    auto InputEvent::is_key_released(Keyboard key) -> bool
    {
        return !keys[key] && pre_keys[key];
    }

    auto InputEvent::is_mouse_down(Mouse mouse) -> bool
    {
        return mouse_buttons[mouse];
    }

    auto InputEvent::is_mouse_pressed(Mouse mouse) -> bool
    {
        return mouse_buttons[mouse] && !pre_mouse_buttons[mouse];
    }

    auto InputEvent::is_mouse_released(Mouse mouse) -> bool
    {
        return !mouse_buttons[mouse] && pre_mouse_buttons[mouse];
    }

    auto InputEvent::is_mouse_dragging(Mouse mouse) -> bool
    {
        if (!mouse_buttons[mouse]) return false;

        return mouse_draging[mouse];
    }
}
