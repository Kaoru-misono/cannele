#pragma once

#include <math/type.hpp>
#include <platform/input/input.hpp>

namespace cannele::inline scene
{
    struct Camera_Matrix final
    {
        math::float4x4 matrix_view{};
        math::float4x4 matrix_inv_view{};
        math::float4x4 matrix_proj{};
        math::float4x4 matrix_inv_proj{};
        math::float4x4 matrix_proj_view{};
        math::float4x4 matrix_inv_proj_view{};
    };

    struct Camera
    {
        enum struct Projection: uint8_t
        {
            perspective,
            orthogonal,
        };

        Camera();
        virtual ~Camera() = default;

        auto set_orthogonal(float in_near_z, float in_far_z, float in_width, float in_height, float in_zoom) -> void;
        auto set_perspective(float in_near_z, float in_far_z, float in_fov, float in_aspect_ratio) -> void;

        auto set_near_far_plane(float in_near_z, float in_far_z) -> void;
        auto set_position(math::float3 const& in_position) -> void;
        auto set_lookat_position(math::float3 lookat_point) -> void;

        // Orthogonal.
        auto set_viewport(float in_width, float in_height) -> void;
        auto set_zoom(float in_zoom) -> void;

        // Perspective.
        auto set_fov(float in_fov) -> void;
        auto set_aspect_ratio(float in_aspect_ratio) -> void;

        auto rotate(float delta_pitch, float delta_yaw) -> void;

        virtual auto update(float delta_time) -> void = 0;

        auto matrix() -> Camera_Matrix const* { return &matrixes; }

        float near_z{0.1f};
        float far_z{10000.0f};

        float fov{60.0f};
        float aspect_ratio{1.0f};

    protected:

        math::float3 position{0.0f};
        math::float3 forward{0.0f};
        math::float3 right{0.0f};
        math::float3 up{0.0f};

        float pitch{0.0f};
        float yaw{0.0f};

        static constexpr float constraint_pitch{math::pi / 2.0f - 0.01f};

        float zoom{0.0f};
        float viewport_width{0.0f};
        float viewport_height{0.0f};

        Projection mode{Projection::perspective};

        Camera_Matrix matrixes{};

        bool is_view_dirty{true};
        bool is_projection_dirty{true};

        auto reset_state() -> void;
        auto direction_to_pitch_yaw(math::float3 direction) -> math::float2;
        auto update_view() -> void;
        auto update_projection() -> void;
    };

    struct First_Person_Camera: Camera
    {
        First_Person_Camera(float in_rotation_speed = 10.0f, float in_move_speed = 10.0f, float in_movement_delta = 1.0f);
        ~First_Person_Camera() = default;

        auto update(float delta_time) -> void override;

        float movement_speed{};
        float rotation_speed{};

    private:

        float mouse_sensitivity{};
        float movement_delta{};

        float target_yaw{};
        float target_pitch{};
        math::float3 target_movement{};
    };
}
