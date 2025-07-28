#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
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

    using double1 = double;
    using double2 = glm::dvec2;
    using double3 = glm::dvec3;
    using double4 = glm::dvec4;

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

    using double2x2 = glm::dmat2;
    using double3x3 = glm::dmat3;
    using double4x4 = glm::dmat4;

    using quaternion = glm::quat;

    static constexpr auto pi     = glm::pi<float>();
    static constexpr auto two_pi = glm::two_pi<float>();
}

// For HLSL-compatible type.
using uint = uint32_t;
using uint2 = cannele::math::uint2;
using uint3 = cannele::math::uint3;
using uint4 = cannele::math::uint4;

using int2 = cannele::math::int2;
using int3 = cannele::math::int3;
using int4 = cannele::math::int4;

using float2 = cannele::math::float2;
using float3 = cannele::math::float3;
using float4 = cannele::math::float4;

using double2 = cannele::math::double2;
using double3 = cannele::math::double3;
using double4 = cannele::math::double4;

using bool2 = cannele::math::bool2;
using bool3 = cannele::math::bool3;
using bool4 = cannele::math::bool4;

using float2x2 = cannele::math::float2x2;
using float3x3 = cannele::math::float3x3;
using float4x4 = cannele::math::float4x4;

using double2x2 = cannele::math::double2x2;
using double3x3 = cannele::math::double3x3;
using double4x4 = cannele::math::double2x2;
