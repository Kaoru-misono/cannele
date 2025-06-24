#pragma once

#include "type.hpp"

namespace cannele::math
{
    struct Transform final
    {
        math::float3 scaling{};
        math::quaternion rotation{};
        math::float3 translation{};

        auto matrix() const -> math::float4x4
        {
            auto trans_mat = glm::translate(math::float4x4{1.0f}, translation);
            auto scale_mat = glm::scale(math::float4x4{1.0f}, scaling);
            auto rot_mat = glm::mat4_cast(rotation);
            return trans_mat * rot_mat * scale_mat;
        }

        static auto from_matrix(math::float4x4 const& matrix) -> Transform
        {
            auto trans = Transform{};
            trans.translation = matrix[3];
            trans.rotation = matrix;
            trans.scaling.x = glm::length(trans.rotation[0]);
            if (trans.scaling.x != 0.0f) { trans.rotation[0] /= trans.scaling.x; }
            trans.scaling.y = glm::length(trans.rotation[1]);
            if (trans.scaling.y != 0.0f) { trans.rotation[1] /= trans.scaling.y; }
            trans.scaling.z = glm::length(trans.rotation[2]);
            if (trans.scaling.z != 0.0f) { trans.rotation[2] /= trans.scaling.z; }
            return trans;
        }

        auto operator * (Transform const& other) const -> Transform
        {
            return from_matrix(matrix() * other.matrix());
        }
    };
}
