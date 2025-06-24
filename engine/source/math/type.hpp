#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace cannele::math
{
    using bool1 = bool;
    using bool2 = glm::bvec2;
    using bool3 = glm::bvec3;
    using bool4 = glm::bvec4;

    using float1 = float;
    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;

    using int1 = int;
    using int2 = glm::ivec2;
    using int3 = glm::ivec3;
    using int4 = glm::ivec4;

    using uint  = uint32_t;
    using uint1 = uint32_t;
    using uint2 = glm::u32vec2;
    using uint3 = glm::u32vec3;
    using uint4 = glm::u32vec4;

    using ushort4 = glm::u16vec4;

    using float2x2 = glm::mat2;
    using float3x3 = glm::mat3;
    using float4x4 = glm::mat4;

    using quaternion = glm::quat;

    static constexpr auto pi     = glm::pi<float>();
    static constexpr auto two_pi = glm::two_pi<float>();
}
