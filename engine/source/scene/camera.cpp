#include "camera.hpp"

#include <math/tool.hpp>

namespace cannele::inline scene
{
    Camera::Camera()
    {}

    auto Camera::set_orthogonal(float near_z, float far_z, float width, float height, float zoom) -> void
    {
        mode = Projection::orthogonal;
        this->near_z = near_z;
        this->far_z = far_z;
        viewport_width = width;
        viewport_height = height;
        this->zoom = zoom;
        reset_state();

        is_projection_dirty = true;
    }

    auto Camera::set_perspective(float near_z, float far_z, float fov, float aspect_ratio) -> void
    {
        mode = Projection::perspective;
        this->near_z = near_z;
        this->far_z = far_z;
        this->fov = fov;
        this->aspect_ratio = aspect_ratio;
        reset_state();

        is_projection_dirty = true;
    }

    auto Camera::set_near_far_plane(float near_z, float far_z) -> void
    {
        this->near_z = near_z;
        this->far_z = far_z;

        is_projection_dirty = true;
    }

    auto Camera::set_position(math::float3 const& position) -> void
    {
        this->position = position;

        is_view_dirty = true;
    }

    auto Camera::set_lookat_position(math::float3 lookat_point) -> void
    {
        auto pitch_yaw = direction_to_pitch_yaw(glm::normalize(lookat_point - position));
        // pitch = pitch_yaw.x;
        // yaw = pitch_yaw.y;

        is_view_dirty = true;
    }

    auto Camera::set_viewport(float width, float height) -> void
    {
        viewport_width = width;
        viewport_height = height;

        is_projection_dirty = true;
    }

    auto Camera::set_zoom(float zoom) -> void
    {
        this->zoom = zoom;

        is_projection_dirty = true;
    }

    auto Camera::set_fov(float fov) -> void
    {
        this->fov = fov;

        is_projection_dirty = true;
    }

    auto Camera::set_aspect_ratio(float aspect_ratio) -> void
    {
        this->aspect_ratio = aspect_ratio;

        is_projection_dirty = true;
    }

    auto Camera::rotate(float delta_pitch, float delta_yaw) -> void
    {
        pitch += delta_pitch;
        yaw += delta_yaw;

        if (pitch > constraint_pitch) pitch = constraint_pitch;
        if (pitch < -constraint_pitch) pitch = -constraint_pitch;

        is_view_dirty = true;
    }

    auto Camera::update() -> void
    {
        if (!is_view_dirty && !is_projection_dirty) return;

        if (is_view_dirty) update_view();
        if (is_projection_dirty) update_projection();

        matrixes.matrix_proj_view = matrixes.matrix_proj * matrixes.matrix_view;
        matrixes.matrix_inv_proj_view = glm::inverse(matrixes.matrix_proj_view);
    };

    auto Camera::reset_state() -> void
    {
        position = math::float3{0.0f};
        pitch = 0.0f;
        yaw = -math::pi;
    }

    auto Camera::direction_to_pitch_yaw(math::float3 direction) -> math::float2
    {
        auto pitch = glm::degrees(std::asin(direction.y));
        auto yaw = glm::degrees(std::atan2(direction.z, direction.x));

        return {pitch, yaw};
    }

    auto Camera::update_view() -> void
    {
        forward.x = std::cos(yaw) * std::cos(pitch);
        forward.y = std::sin(pitch);
        forward.z = std::sin(yaw) * std::cos(pitch);
        forward = glm::normalize(forward);

        right = glm::normalize(glm::cross(forward, math::float3{0.0f, 1.0f, 0.0f}));
        up = glm::normalize(glm::cross(right, forward));

        matrixes.matrix_view = glm::lookAt(position, position + forward, math::float3{0.0f, 1.0f, 0.0f});
        matrixes.matrix_inv_view = glm::inverse(matrixes.matrix_view);
    }

    auto Camera::update_projection() -> void
    {
        if (mode == Projection::orthogonal) {
            matrixes.matrix_proj = glm::ortho(zoom * -viewport_width / 2.0f, zoom * viewport_width / 2.0f, zoom * -viewport_height / 2.0f, zoom * viewport_height / 2.0f, near_z, far_z);
        }
        else if (mode == Projection::perspective) {
            matrixes.matrix_proj = glm::perspective(glm::radians(fov), aspect_ratio, near_z, far_z);
        }
        matrixes.matrix_inv_proj = glm::inverse(matrixes.matrix_proj);
    }

    First_Person_Camera::First_Person_Camera(float rotation_speed, float move_speed, float movement_delta)
        : rotation_speed(rotation_speed)
        , movement_speed(move_speed)
        , movement_delta(movement_delta)
    {
        target_yaw = yaw;
        target_pitch = pitch;
        target_movement = position;
        mouse_sensitivity = 1.0f;
    }

    auto First_Person_Camera::update(platform::InputEvent* input, math::float2 window_size, float delta_time) -> void
    {
        Camera::update();

        if (input->is_mouse_dragging(Mouse::right)) {
            target_yaw += (input->mouse_position.x - input->pre_mouse_position.x) * mouse_sensitivity * delta_time;
            target_pitch -= (input->mouse_position.y - input->pre_mouse_position.y) * mouse_sensitivity * delta_time;
        }

        auto camera_movement = math::float3{0.0f};

        if (input->is_key_down(Keyboard::a)) {
            camera_movement -= movement_delta * right;
        }
        else if (input->is_key_down(Keyboard::d)) {
            camera_movement += movement_delta * right;
        }

        if (input->is_key_down(Keyboard::w)) {
            camera_movement += movement_delta * forward;
        }
        else if (input->is_key_down(Keyboard::s)) {
            camera_movement -= movement_delta * forward;
        }

        if (input->is_key_down(Keyboard::q)) {
            camera_movement += movement_delta * up;
        }
        else if (input->is_key_down(Keyboard::e)) {
            camera_movement -= movement_delta * up;
        }

        auto tween_speed = rotation_speed * delta_time;
        Camera::rotate((target_pitch - pitch) * tween_speed, (target_yaw - yaw) * tween_speed);

        target_movement += camera_movement;
        auto tween_move_speed = movement_speed * delta_time;
        position = math::lerp_float3(position, target_movement, 0.9f, tween_move_speed);
    }
}
