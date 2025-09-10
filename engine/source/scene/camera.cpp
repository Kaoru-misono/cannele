#include "camera.hpp"

#include <math/tool.hpp>
#include <imgui.h>

namespace cannele::inline scene
{
    Camera::Camera()
    {}

    auto Camera::set_orthogonal(float near_z, float far_z, float width, float height, float zoom) -> void
    {
        mode = Projection::orthogonal;
        this->z_near = near_z;
        this->z_far = far_z;
        viewport_width = width;
        viewport_height = height;
        this->zoom = zoom;
        reset_state();

        is_projection_dirty = true;
    }

    auto Camera::set_perspective(float near_z, float far_z, float fov, float aspect_ratio) -> void
    {
        mode = Projection::perspective;
        this->z_near = near_z;
        this->z_far = far_z;
        this->fov = fov;
        this->aspect_ratio = aspect_ratio;
        reset_state();

        is_projection_dirty = true;
    }

    auto Camera::set_near_far_plane(float near_z, float far_z) -> void
    {
        this->z_near = near_z;
        this->z_far = far_z;

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
            matrixes.matrix_proj = glm::ortho(zoom * -viewport_width / 2.0f, zoom * viewport_width / 2.0f, zoom * -viewport_height / 2.0f, zoom * viewport_height / 2.0f, z_near, z_far);
        }
        else if (mode == Projection::perspective) {
            matrixes.matrix_proj = glm::infinitePerspective(glm::radians(fov), aspect_ratio, z_near);
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

    auto First_Person_Camera::update(float delta_time) -> void
    {
        if (!is_view_dirty && !is_projection_dirty) return;

        if (is_view_dirty) update_view();
        if (is_projection_dirty) update_projection();

        matrixes.matrix_proj_view = matrixes.matrix_proj * matrixes.matrix_view;
        matrixes.matrix_inv_proj_view = glm::inverse(matrixes.matrix_proj_view);

        auto io = &ImGui::GetIO();

        auto key_data = ImGui::IsKeyDown(ImGuiKey_A);
        auto key_a = io->KeysData[ImGuiKey_A - ImGuiKey_NamedKey_BEGIN];
        ImGui::IsMouseDragging(ImGuiMouseButton_Right);

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            target_yaw += (io->MousePos.x - io->MousePosPrev.x) * mouse_sensitivity * delta_time;
            target_pitch -= (io->MousePos.y - io->MousePosPrev.y) * mouse_sensitivity * delta_time;
        }

        auto camera_movement = math::float3{0.0f};

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera_movement -= movement_delta * right;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera_movement += movement_delta * right;
        }

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera_movement += movement_delta * forward;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera_movement -= movement_delta * forward;
        }

        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            camera_movement += movement_delta * up;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_E)) {
            camera_movement -= movement_delta * up;
        }

        auto tween_speed = rotation_speed * delta_time;
        Camera::rotate((target_pitch - pitch) * tween_speed, (target_yaw - yaw) * tween_speed);

        target_movement += camera_movement;
        auto tween_move_speed = movement_speed * delta_time;
        position = math::lerp_float3(position, target_movement, 0.9f, tween_move_speed);
    }
}
