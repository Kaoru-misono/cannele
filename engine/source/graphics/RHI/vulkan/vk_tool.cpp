#include "vk_tool.hpp"

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        struct FormatMap final
        {
            EFormat format{};
            VkFormat vk_format{};
        };

        static constexpr auto format_map = std::array<FormatMap, (size_t) EFormat::last>{
            {
                {EFormat::undefined,          VK_FORMAT_UNDEFINED},
                {EFormat::r8_uint,            VK_FORMAT_R8_UINT},
                {EFormat::r8_sint,            VK_FORMAT_R8_SINT},
                {EFormat::r8_unorm,           VK_FORMAT_R8_UNORM},
                {EFormat::r8_snorm,           VK_FORMAT_R8_SNORM},
                {EFormat::rg8_uint,           VK_FORMAT_R8G8_UINT},
                {EFormat::rg8_sint,           VK_FORMAT_R8G8_SINT},
                {EFormat::rg8_unorm,          VK_FORMAT_R8G8_UNORM},
                {EFormat::rg8_snorm,          VK_FORMAT_R8G8_SNORM},
                {EFormat::r16_uint,           VK_FORMAT_R16_UINT},
                {EFormat::r16_sint,           VK_FORMAT_R16_SINT},
                {EFormat::r16_unorm,          VK_FORMAT_R16_UNORM},
                {EFormat::r16_snorm,          VK_FORMAT_R16_SNORM},
                {EFormat::r16_float,          VK_FORMAT_R16_SFLOAT},
                {EFormat::bgra4_unorm,        VK_FORMAT_B4G4R4A4_UNORM_PACK16},
                {EFormat::b5g6r5_unorm,       VK_FORMAT_B5G6R5_UNORM_PACK16},
                {EFormat::b5g5r5a1_unorm,     VK_FORMAT_B5G5R5A1_UNORM_PACK16},
                {EFormat::rgba8_uint,         VK_FORMAT_R8G8B8A8_UINT},
                {EFormat::rgba8_sint,         VK_FORMAT_R8G8B8A8_SINT},
                {EFormat::rgba8_unorm,        VK_FORMAT_R8G8B8A8_UNORM},
                {EFormat::rgba8_snorm,        VK_FORMAT_R8G8B8A8_SNORM},
                {EFormat::bgra8_unorm,        VK_FORMAT_B8G8R8A8_UNORM},
                {EFormat::sbgra8_unorm,       VK_FORMAT_B8G8R8A8_SRGB},
                {EFormat::r10g10b10a2_unorm,  VK_FORMAT_A2B10G10R10_UNORM_PACK32},
                {EFormat::r11g11b10_float,    VK_FORMAT_B10G11R11_UFLOAT_PACK32},
                {EFormat::rg16_uint,          VK_FORMAT_R16G16_UINT},
                {EFormat::rg16_sint,          VK_FORMAT_R16G16_SINT},
                {EFormat::rg16_unorm,         VK_FORMAT_R16G16_UNORM},
                {EFormat::rg16_snorm,         VK_FORMAT_R16G16_SNORM},
                {EFormat::rg16_float,         VK_FORMAT_R16G16_SFLOAT},
                {EFormat::r32_uint,           VK_FORMAT_R32_UINT},
                {EFormat::r32_sint,           VK_FORMAT_R32_SINT},
                {EFormat::r32_float,          VK_FORMAT_R32_SFLOAT},
                {EFormat::rgba16_uint,        VK_FORMAT_R16G16B16A16_UINT},
                {EFormat::rgba16_sint,        VK_FORMAT_R16G16B16A16_SINT},
                {EFormat::rgba16_float,       VK_FORMAT_R16G16B16A16_SFLOAT},
                {EFormat::rgba16_unorm,       VK_FORMAT_R16G16B16A16_UNORM},
                {EFormat::rgba16_snorm,       VK_FORMAT_R16G16B16A16_SNORM},
                {EFormat::rg32_uint,          VK_FORMAT_R32G32_UINT},
                {EFormat::rg32_sint,          VK_FORMAT_R32G32_SINT},
                {EFormat::rg32_float,         VK_FORMAT_R32G32_SFLOAT},
                {EFormat::rgb32_uint,         VK_FORMAT_R32G32B32_UINT},
                {EFormat::rgb32_sint,         VK_FORMAT_R32G32B32_SINT},
                {EFormat::rgb32_float,        VK_FORMAT_R32G32B32_SFLOAT},
                {EFormat::rgba32_uint,        VK_FORMAT_R32G32B32A32_UINT},
                {EFormat::rgba32_sint,        VK_FORMAT_R32G32B32A32_SINT},
                {EFormat::rgba32_float,       VK_FORMAT_R32G32B32A32_SFLOAT},
                {EFormat::d16_unorm,          VK_FORMAT_D16_UNORM},
                {EFormat::d24_unorm_s8_uint,  VK_FORMAT_D24_UNORM_S8_UINT},
                {EFormat::d32_sfloat,         VK_FORMAT_D32_SFLOAT},
                {EFormat::d32_sfloat_s8_uint, VK_FORMAT_D32_SFLOAT_S8_UINT},
                {EFormat::bc1_unorm,          VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
                {EFormat::bc1_unorm_srgb,     VK_FORMAT_BC1_RGBA_SRGB_BLOCK},
                {EFormat::bc2_unorm,          VK_FORMAT_BC2_UNORM_BLOCK},
                {EFormat::bc2_unorm_srgb,     VK_FORMAT_BC2_SRGB_BLOCK},
                {EFormat::bc3_unorm,          VK_FORMAT_BC3_UNORM_BLOCK},
                {EFormat::bc3_unorm_srgb,     VK_FORMAT_BC3_SRGB_BLOCK},
                {EFormat::bc4_unorm,          VK_FORMAT_BC4_UNORM_BLOCK},
                {EFormat::bc4_snorm,          VK_FORMAT_BC4_SNORM_BLOCK},
                {EFormat::bc5_unorm,          VK_FORMAT_BC5_UNORM_BLOCK},
                {EFormat::bc5_snorm,          VK_FORMAT_BC5_SNORM_BLOCK},
                {EFormat::bc6h_ufloat,        VK_FORMAT_BC6H_UFLOAT_BLOCK},
                {EFormat::bc6h_sfloat,        VK_FORMAT_BC6H_SFLOAT_BLOCK},
                {EFormat::bc7_unorm,          VK_FORMAT_BC7_UNORM_BLOCK},
                {EFormat::bc7_unorm_srgb,     VK_FORMAT_BC7_SRGB_BLOCK},
            }
        };
    }

    auto convert_to_vk_format(EFormat format) -> VkFormat
    {
        return format_map[(uint8_t) format].vk_format;
    }

    auto convert_to_format(VkFormat vk_format) -> EFormat
    {
        switch (vk_format) {
            case VK_FORMAT_UNDEFINED:                return EFormat::undefined;
            case VK_FORMAT_R8_UINT:                  return EFormat::r8_uint;
            case VK_FORMAT_R8_SINT:                  return EFormat::r8_sint;
            case VK_FORMAT_R8_UNORM:                 return EFormat::r8_unorm;
            case VK_FORMAT_R8_SNORM:                 return EFormat::r8_snorm;
            case VK_FORMAT_R8G8_UINT:                return EFormat::rg8_uint;
            case VK_FORMAT_R8G8_SINT:                return EFormat::rg8_sint;
            case VK_FORMAT_R8G8_UNORM:               return EFormat::rg8_unorm;
            case VK_FORMAT_R8G8_SNORM:               return EFormat::rg8_snorm;
            case VK_FORMAT_R16_UINT:                 return EFormat::r16_uint;
            case VK_FORMAT_R16_SINT:                 return EFormat::r16_sint;
            case VK_FORMAT_R16_UNORM:                return EFormat::r16_unorm;
            case VK_FORMAT_R16_SNORM:                return EFormat::r16_snorm;
            case VK_FORMAT_R16_SFLOAT:               return EFormat::r16_float;
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16:    return EFormat::bgra4_unorm;
            case VK_FORMAT_B5G6R5_UNORM_PACK16:      return EFormat::b5g6r5_unorm;
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16:    return EFormat::b5g5r5a1_unorm;
            case VK_FORMAT_R8G8B8A8_UINT:            return EFormat::rgba8_uint;
            case VK_FORMAT_R8G8B8A8_SINT:            return EFormat::rgba8_sint;
            case VK_FORMAT_R8G8B8A8_UNORM:           return EFormat::rgba8_unorm;
            case VK_FORMAT_R8G8B8A8_SNORM:           return EFormat::rgba8_snorm;
            case VK_FORMAT_B8G8R8A8_UNORM:           return EFormat::bgra8_unorm;
            case VK_FORMAT_B8G8R8A8_SRGB:            return EFormat::sbgra8_unorm;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return EFormat::r10g10b10a2_unorm;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:  return EFormat::r11g11b10_float;
            case VK_FORMAT_R16G16_UINT:              return EFormat::rg16_uint;
            case VK_FORMAT_R16G16_SINT:              return EFormat::rg16_sint;
            case VK_FORMAT_R16G16_UNORM:             return EFormat::rg16_unorm;
            case VK_FORMAT_R16G16_SNORM:             return EFormat::rg16_snorm;
            case VK_FORMAT_R16G16_SFLOAT:            return EFormat::rg16_float;
            case VK_FORMAT_R32_UINT:                 return EFormat::r32_uint;
            case VK_FORMAT_R32_SINT:                 return EFormat::r32_sint;
            case VK_FORMAT_R32_SFLOAT:               return EFormat::r32_float;
            case VK_FORMAT_R16G16B16A16_UINT:        return EFormat::rgba16_uint;
            case VK_FORMAT_R16G16B16A16_SINT:        return EFormat::rgba16_sint;
            case VK_FORMAT_R16G16B16A16_SFLOAT:      return EFormat::rgba16_float;
            case VK_FORMAT_R16G16B16A16_UNORM:       return EFormat::rgba16_unorm;
            case VK_FORMAT_R16G16B16A16_SNORM:       return EFormat::rgba16_snorm;
            case VK_FORMAT_R32G32_UINT:              return EFormat::rg32_uint;
            case VK_FORMAT_R32G32_SINT:              return EFormat::rg32_sint;
            case VK_FORMAT_R32G32_SFLOAT:            return EFormat::rg32_float;
            case VK_FORMAT_R32G32B32_UINT:           return EFormat::rgb32_uint;
            case VK_FORMAT_R32G32B32_SINT:           return EFormat::rgb32_sint;
            case VK_FORMAT_R32G32B32_SFLOAT:         return EFormat::rgb32_float;
            case VK_FORMAT_R32G32B32A32_UINT:        return EFormat::rgba32_uint;
            case VK_FORMAT_R32G32B32A32_SINT:        return EFormat::rgba32_sint;
            case VK_FORMAT_R32G32B32A32_SFLOAT:      return EFormat::rgba32_float;
            case VK_FORMAT_D16_UNORM:                return EFormat::d16_unorm;
            case VK_FORMAT_D24_UNORM_S8_UINT:        return EFormat::d24_unorm_s8_uint;
            case VK_FORMAT_D32_SFLOAT:               return EFormat::d32_sfloat;
            case VK_FORMAT_D32_SFLOAT_S8_UINT:       return EFormat::d32_sfloat_s8_uint;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:     return EFormat::bc1_unorm;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:      return EFormat::bc1_unorm_srgb;
            case VK_FORMAT_BC2_UNORM_BLOCK:          return EFormat::bc2_unorm;
            case VK_FORMAT_BC2_SRGB_BLOCK:           return EFormat::bc2_unorm_srgb;
            case VK_FORMAT_BC3_UNORM_BLOCK:          return EFormat::bc3_unorm;
            case VK_FORMAT_BC3_SRGB_BLOCK:           return EFormat::bc3_unorm_srgb;
            case VK_FORMAT_BC4_UNORM_BLOCK:          return EFormat::bc4_unorm;
            case VK_FORMAT_BC4_SNORM_BLOCK:          return EFormat::bc4_snorm;
            case VK_FORMAT_BC5_UNORM_BLOCK:          return EFormat::bc5_unorm;
            case VK_FORMAT_BC5_SNORM_BLOCK:          return EFormat::bc5_snorm;
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:        return EFormat::bc6h_ufloat;
            case VK_FORMAT_BC6H_SFLOAT_BLOCK:        return EFormat::bc6h_sfloat;
            case VK_FORMAT_BC7_UNORM_BLOCK:          return EFormat::bc7_unorm;
            case VK_FORMAT_BC7_SRGB_BLOCK:           return EFormat::bc7_unorm_srgb;
            default: return EFormat::undefined;
        }
    };
}
