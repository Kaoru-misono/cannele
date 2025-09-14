#include "../definitions.hpp"

#include <array>

namespace cannele::inline graphics::rhi
{

    inline namespace
    {
        static constexpr auto format_table = std::array<FormatInfo, (size_t) EFormat::last>{
            {
                {"UNDEFINED",          EFormat::undefined,          0,  0, 0, 0, EFormatKind::last},
                {"R8_UINT",            EFormat::r8_uint,            1,  1, 1, 0, EFormatKind::integer},
                {"R8_SINT",            EFormat::r8_sint,            1,  1, 1, 1, EFormatKind::integer},
                {"R8_UNORM",           EFormat::r8_unorm,           1,  1, 1, 0, EFormatKind::normalized},
                {"R8_SNORM",           EFormat::r8_snorm,           1,  1, 1, 1, EFormatKind::normalized},
                {"RG8_UINT",           EFormat::rg8_uint,           1,  1, 2, 0, EFormatKind::integer},
                {"RG8_SINT",           EFormat::rg8_sint,           1,  1, 2, 1, EFormatKind::integer},
                {"RG8_UNORM",          EFormat::rg8_unorm,          1,  1, 2, 0, EFormatKind::normalized},
                {"RG8_SNORM",          EFormat::rg8_snorm,          1,  1, 2, 1, EFormatKind::normalized},
                {"R16_UINT",           EFormat::r16_uint,           2,  1, 1, 0, EFormatKind::integer},
                {"R16_SINT",           EFormat::r16_sint,           2,  1, 1, 1, EFormatKind::integer},
                {"R16_UNORM",          EFormat::r16_unorm,          2,  1, 1, 0, EFormatKind::normalized},
                {"R16_SNORM",          EFormat::r16_snorm,          2,  1, 1, 1, EFormatKind::normalized},
                {"R16_FLOAT",          EFormat::r16_float,          2,  1, 1, 0, EFormatKind::float_t},
                {"BGRA4_UNORM",        EFormat::bgra4_unorm,        4,  1, 4, 0, EFormatKind::normalized},
                {"B5G6R5_UNORM",       EFormat::b5g6r5_unorm,       2,  1, 3, 0, EFormatKind::normalized},
                {"B5G5R5A1_UNORM",     EFormat::b5g5r5a1_unorm,     2,  1, 4, 0, EFormatKind::normalized},
                {"RGBA8_UINT",         EFormat::rgba8_uint,         4,  1, 4, 0, EFormatKind::integer},
                {"RGBA8_SINT",         EFormat::rgba8_sint,         4,  1, 4, 1, EFormatKind::integer},
                {"RGBA8_UNORM",        EFormat::rgba8_unorm,        4,  1, 4, 0, EFormatKind::normalized},
                {"RGBA8_SNORM",        EFormat::rgba8_snorm,        4,  1, 4, 1, EFormatKind::normalized},
                {"BGRA8_UNORM",        EFormat::bgra8_unorm,        4,  1, 4, 0, EFormatKind::normalized},
                {"SBGRA8_UNORM",       EFormat::sbgra8_unorm,       4,  1, 4, 0, EFormatKind::normalized},
                {"R10G10B10A2_UNORM",  EFormat::r10g10b10a2_unorm,  4,  1, 4, 0, EFormatKind::normalized},
                {"R11G11B10_FLOAT",    EFormat::r11g11b10_float,    4,  1, 3, 0, EFormatKind::float_t},
                {"RG16_UINT",          EFormat::rg16_uint,          4,  1, 2, 0, EFormatKind::integer},
                {"RG16_SINT",          EFormat::rg16_sint,          4,  1, 2, 1, EFormatKind::integer},
                {"RG16_UNORM",         EFormat::rg16_unorm,         4,  1, 2, 0, EFormatKind::normalized},
                {"RG16_SNORM",         EFormat::rg16_snorm,         4,  1, 2, 1, EFormatKind::normalized},
                {"RG16_FLOAT",         EFormat::rg16_float,         4,  1, 2, 0, EFormatKind::float_t},
                {"R32_UINT",           EFormat::r32_uint,           4,  1, 1, 0, EFormatKind::integer},
                {"R32_SINT",           EFormat::r32_sint,           4,  1, 1, 1, EFormatKind::integer},
                {"R32_FLOAT",          EFormat::r32_float,          4,  1, 1, 0, EFormatKind::float_t},
                {"RGBA16_UINT",        EFormat::rgba16_uint,        8,  1, 4, 0, EFormatKind::integer},
                {"RGBA16_SINT",        EFormat::rgba16_sint,        8,  1, 4, 1, EFormatKind::integer},
                {"RGBA16_FLOAT",       EFormat::rgba16_float,       8,  1, 4, 0, EFormatKind::float_t},
                {"RGBA16_UNORM",       EFormat::rgba16_unorm,       8,  1, 4, 0, EFormatKind::normalized},
                {"RGBA16_SNORM",       EFormat::rgba16_snorm,       8,  1, 4, 1, EFormatKind::normalized},
                {"RG32_UINT",          EFormat::rg32_uint,          8,  1, 2, 0, EFormatKind::integer},
                {"RG32_SINT",          EFormat::rg32_sint,          8,  1, 2, 1, EFormatKind::integer},
                {"RG32_FLOAT",         EFormat::rg32_float,         8,  1, 2, 0, EFormatKind::float_t},
                {"RGB32_UINT",         EFormat::rgb32_uint,         12, 1, 3, 0, EFormatKind::integer},
                {"RGB32_SINT",         EFormat::rgb32_sint,         12, 1, 3, 1, EFormatKind::integer},
                {"RGB32_FLOAT",        EFormat::rgb32_float,        12, 1, 3, 0, EFormatKind::float_t},
                {"RGBA32_UINT",        EFormat::rgba32_uint,        16, 1, 4, 0, EFormatKind::integer},
                {"RGBA32_SINT",        EFormat::rgba32_sint,        16, 1, 4, 1, EFormatKind::integer},
                {"RGBA32_FLOAT",       EFormat::rgba32_float,       16, 1, 4, 0, EFormatKind::float_t},
                {"D16_UNORM",          EFormat::d16_unorm,          2,  1, 1, 0, EFormatKind::depth_stencil},
                {"D24_UNORM_S8_UINT",  EFormat::d24_unorm_s8_uint,  4,  1, 2, 0, EFormatKind::depth_stencil},
                {"D32_SFLOAT",         EFormat::d32_sfloat,         4,  1, 1, 1, EFormatKind::depth_stencil},
                {"D32_SFLOAT_S8_UINT", EFormat::d32_sfloat_s8_uint, 8,  1, 2, 1, EFormatKind::depth_stencil},
                {"BC1_UNORM",          EFormat::bc1_unorm,          8,  4, 4, 0, EFormatKind::normalized},
                {"BC1_UNORM_SRGB",     EFormat::bc1_unorm_srgb,     8,  4, 4, 0, EFormatKind::normalized},
                {"BC1_UNORM",          EFormat::bc2_unorm,          16, 4, 4, 0, EFormatKind::normalized},
                {"BC2_UNORM_SRGB",     EFormat::bc2_unorm_srgb,     16, 4, 4, 0, EFormatKind::normalized},
                {"BC3_UNORM",          EFormat::bc3_unorm,          16, 4, 4, 0, EFormatKind::normalized},
                {"BC3_UNORM_SRGB",     EFormat::bc3_unorm_srgb,     16, 4, 4, 0, EFormatKind::normalized},
                {"BC4_UNORM",          EFormat::bc4_unorm,          8,  4, 1, 0, EFormatKind::normalized},
                {"BC4_SNORM",          EFormat::bc4_snorm,          8,  4, 1, 1, EFormatKind::normalized},
                {"BC5_UNORM",          EFormat::bc5_unorm,          16, 4, 2, 0, EFormatKind::normalized},
                {"BC5_SNORM",          EFormat::bc5_snorm,          16, 4, 2, 1, EFormatKind::normalized},
                {"BC6H_UFLOAT",        EFormat::bc6h_ufloat,        16, 4, 3, 0, EFormatKind::float_t},
                {"BC6H_SFLOAT",        EFormat::bc6h_sfloat,        16, 4, 3, 1, EFormatKind::float_t},
                {"BC7_UNORM",          EFormat::bc7_unorm,          16, 4, 4, 0, EFormatKind::normalized},
                {"BC7_UNORM_SRGB",     EFormat::bc7_unorm_srgb,     16, 4, 4, 0, EFormatKind::normalized},
            }
        };
    }


    auto get_format_info(EFormat format) -> FormatInfo const*
    {
        return &format_table[(uint8_t) format];
    }
}
