#pragma once

#include "../RHI_resource.hpp"

#include <core/assert.hpp>
#include <core/macro.hpp>

#include <volk.h>
#include <type_traits>
#include <span>

namespace cannele::inline graphics::rhi::vk
{
    template <typename T>
    concept structure_has_sType_and_pNext = requires (T t) { t.sType; t.pNext; };

    template <typename T>
    concept Vk_Structure = !std::is_pointer_v<T> && structure_has_sType_and_pNext<T>;

    template <Vk_Structure T, typename sType>
    inline auto empty_vk_structure(T& structure, sType type) -> void
    {
        structure.sType = type;
        structure.pNext = nullptr;
    }

    struct VkStructureHeader final
    {
        VkStructureType sType;
        VkStructureHeader* pNext;
    };

    // structure is head of this chain, and next is what you want to connect.
    inline auto connect_to_next(auto* structure, auto* next) -> void
    {
        auto p = (VkStructureHeader*) structure;
        while (p->pNext) { p = (VkStructureHeader*) p->pNext; }
        p->pNext = (VkStructureHeader*) next;
    }

    inline auto check_device_extension_support(std::span<VkExtensionProperties> properties, char const* ext_name) -> bool
    {
        return std::find_if(
            properties.begin(), properties.end(),
            [ext_name] (VkExtensionProperties const& prop) {
                return strcmp(prop.extensionName, ext_name) == 0;
            }
        ) != properties.end();
    }

    inline auto vk_error_to_string(int error_code) -> char const*
    {
        switch (error_code) {
            case VK_SUCCESS:                                   return "VK_SUCCESS";
            case VK_ERROR_OUT_OF_HOST_MEMORY:                  return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:                return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED:               return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST:                         return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED:                   return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT:                   return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT:               return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT:                 return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER:                 return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS:                    return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED:                return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL:                     return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_OUT_OF_POOL_MEMORY:                  return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE:             return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:      return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            default:                                           return "UNKNOWN_VK_ERROR";
        }
    }

    inline auto set_resource_name(VkDevice device, VkObjectType object_type, uint64_t handle, std::string_view name) -> void
    {
        auto name_info = VkDebugUtilsObjectNameInfoEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        name_info.objectType = object_type;
        name_info.objectHandle = handle;
        name_info.pObjectName = name.data();
        vkSetDebugUtilsObjectNameEXT(device, &name_info);
    }

    auto convert_to_vk_format(EFormat format) -> VkFormat;
    auto convert_to_format(VkFormat vk_format) -> EFormat;

    inline auto is_color_format(VkFormat format) -> bool
    {
        return (true
            && format != VK_FORMAT_D16_UNORM
            && format != VK_FORMAT_D16_UNORM_S8_UINT
            && format != VK_FORMAT_D24_UNORM_S8_UINT
            && format != VK_FORMAT_D32_SFLOAT
            && format != VK_FORMAT_X8_D24_UNORM_PACK32
            && format != VK_FORMAT_D32_SFLOAT_S8_UINT
        );
    }

    inline auto is_depth_stencil_format(VkFormat format) -> bool
    {
        return !is_color_format(format);
    }

    inline auto is_depth_only_format(VkFormat format) -> bool
    {
        return (false
            || format == VK_FORMAT_D16_UNORM
            || format == VK_FORMAT_D32_SFLOAT
            || format == VK_FORMAT_X8_D24_UNORM_PACK32
        );
    }

    inline auto aspect_flag_from_format(VkFormat format) -> VkImageAspectFlags
    {
        return is_color_format(format) ? VK_IMAGE_ASPECT_COLOR_BIT : is_depth_only_format(format) ? VK_IMAGE_ASPECT_DEPTH_BIT : (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    }

    inline auto convert_to_vk_primitive_topology(ERasterizerTopologyType type) -> VkPrimitiveTopology
    {
        switch (type) {
            case ERasterizerTopologyType::point_list:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case ERasterizerTopologyType::line_list:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case ERasterizerTopologyType::line_strip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            case ERasterizerTopologyType::triangle_list:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case ERasterizerTopologyType::triangle_strip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_cull_mode(ERasterizerCullMode mode) -> VkCullModeFlags
    {
        switch (mode) {
            case ERasterizerCullMode::none:  return VK_CULL_MODE_NONE;
            case ERasterizerCullMode::front: return VK_CULL_MODE_FRONT_BIT;
            case ERasterizerCullMode::back:  return VK_CULL_MODE_BACK_BIT;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_polygen_mode(ERasterizerFillMode mode) -> VkPolygonMode
    {
        switch (mode) {
            case ERasterizerFillMode::point: return VK_POLYGON_MODE_POINT;
            case ERasterizerFillMode::line:  return VK_POLYGON_MODE_LINE;
            case ERasterizerFillMode::solid: return VK_POLYGON_MODE_FILL;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_compare_op(ECompareOperation op) -> VkCompareOp
    {
        switch (op) {
            case ECompareOperation::never:            return VK_COMPARE_OP_NEVER;
            case ECompareOperation::less:             return VK_COMPARE_OP_LESS;
            case ECompareOperation::equal:            return VK_COMPARE_OP_EQUAL;
            case ECompareOperation::less_or_equal:    return VK_COMPARE_OP_LESS_OR_EQUAL;
            case ECompareOperation::greater:          return VK_COMPARE_OP_GREATER;
            case ECompareOperation::not_equal:        return VK_COMPARE_OP_NOT_EQUAL;
            case ECompareOperation::greater_or_equal: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case ECompareOperation::always:           return VK_COMPARE_OP_ALWAYS;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_stencil_op(EStencilOperation op) -> VkStencilOp
    {
        switch (op) {
            case EStencilOperation::keep:                return VK_STENCIL_OP_KEEP;
            case EStencilOperation::zero:                return VK_STENCIL_OP_ZERO;
            case EStencilOperation::replace:             return VK_STENCIL_OP_REPLACE;
            case EStencilOperation::increment_and_clamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case EStencilOperation::decrement_and_clamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case EStencilOperation::invert:              return VK_STENCIL_OP_INVERT;
            case EStencilOperation::increment_and_wrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case EStencilOperation::decrement_and_wrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_blend_factor(EBlendFactor factor) -> VkBlendFactor
    {
        switch (factor) {
            case EBlendFactor::zero:                     return VK_BLEND_FACTOR_ZERO;
            case EBlendFactor::one:                      return VK_BLEND_FACTOR_ONE;
            case EBlendFactor::src_color:                return VK_BLEND_FACTOR_SRC_COLOR;
            case EBlendFactor::one_minus_src_color:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case EBlendFactor::src_alpha:                return VK_BLEND_FACTOR_SRC_ALPHA;
            case EBlendFactor::one_minus_src_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case EBlendFactor::dst_color:                return VK_BLEND_FACTOR_DST_COLOR;
            case EBlendFactor::one_minus_dst_color:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case EBlendFactor::dst_alpha:                return VK_BLEND_FACTOR_DST_ALPHA;
            case EBlendFactor::one_minus_dst_alpha:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case EBlendFactor::src_alpha_saturate:       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case EBlendFactor::constant_color:           return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case EBlendFactor::one_minus_constant_color: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case EBlendFactor::constant_alpha:           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case EBlendFactor::one_minus_constant_alpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_blend_op(EBlendOperation op) -> VkBlendOp
    {
        switch (op) {
            case EBlendOperation::add:              return VK_BLEND_OP_ADD;
            case EBlendOperation::subtract:         return VK_BLEND_OP_SUBTRACT;
            case EBlendOperation::reverse_subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case EBlendOperation::min:              return VK_BLEND_OP_MIN;
            case EBlendOperation::max:              return VK_BLEND_OP_MAX;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_attribute_format(EVertexAttributeFormat format) -> VkFormat
    {
        switch (format) {
            case EVertexAttributeFormat::sfloat32:   return VK_FORMAT_R32_SFLOAT;
            case EVertexAttributeFormat::sfloat32x2: return VK_FORMAT_R32G32_SFLOAT;
            case EVertexAttributeFormat::sfloat32x3: return VK_FORMAT_R32G32B32_SFLOAT;
            case EVertexAttributeFormat::sfloat32x4: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case EVertexAttributeFormat::uint8:      return VK_FORMAT_R8_UINT;
            case EVertexAttributeFormat::uint8x4:    return VK_FORMAT_R8G8B8A8_UINT;
            case EVertexAttributeFormat::unorm8:     return VK_FORMAT_R8_UNORM;
            case EVertexAttributeFormat::unorm8x2:   return VK_FORMAT_R8G8_UNORM;
            case EVertexAttributeFormat::unorm8x3:   return VK_FORMAT_R8G8B8_UNORM;
            case EVertexAttributeFormat::unorm8x4:   return VK_FORMAT_R8G8B8A8_UNORM;
            case EVertexAttributeFormat::unorm16:    return VK_FORMAT_R16_UNORM;
            case EVertexAttributeFormat::unorm16x2:  return VK_FORMAT_R16G16_UNORM;
            case EVertexAttributeFormat::unorm16x3:  return VK_FORMAT_R16G16B16_UNORM;
            case EVertexAttributeFormat::unorm16x4:  return VK_FORMAT_R16G16B16A16_UNORM;
            case EVertexAttributeFormat::unorm32:    return VK_FORMAT_R32_UINT;
            case EVertexAttributeFormat::unorm32x2:  return VK_FORMAT_R32G32_UINT;
            case EVertexAttributeFormat::unorm32x3:  return VK_FORMAT_R32G32B32_UINT;
            case EVertexAttributeFormat::unorm32x4:  return VK_FORMAT_R32G32B32A32_UINT;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_access_type(EResourceStates access) -> VkAccessFlags2
    {
        auto result = VkAccessFlags2{};
        if (enum_has_any_flags(access, EResourceStates::vertex_buffer)) {
            result |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::index_buffer)) {
            result |= VK_ACCESS_2_INDEX_READ_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::indirect_command_read)) {
            result |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::SRV_access)) {
            result |= VK_ACCESS_2_SHADER_READ_BIT;
            if (enum_has_any_flags(access, EResourceStates::uniform_buffer)) {
                result |= VK_ACCESS_2_UNIFORM_READ_BIT;
            }
        }
        if (enum_has_any_flags(access, EResourceStates::UAV_access)) {
            result |= VK_ACCESS_2_SHADER_WRITE_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::transfer_src)) {
            result |= VK_ACCESS_2_TRANSFER_READ_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::transfer_dst)) {
            result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_read)) {
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_attachment)) {
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::color_attachment)) {
            result |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        }

        return result;
    }

    inline auto pipeline_stage_from_access(EResourceStates access) -> VkPipelineStageFlags2
    {
        auto result = VkPipelineStageFlags2{};
        if (enum_has_any_flags(access, EResourceStates::vertex_buffer)) {
            result |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::index_buffer)) {
            result |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::indirect_command_read)) {
            result |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::SRV_access | EResourceStates::UAV_access)) {
            result |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::transfer_src | EResourceStates::transfer_dst)) {
            result |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_read | EResourceStates::depth_stencil_attachment)) {
            result |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::color_attachment)) {
            result |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (enum_has_any_flags(access, EResourceStates::present)) {
            result |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }

        return result;
    }

    inline auto convert_to_vk_shader_stage(ShaderStageFlags stage) -> VkShaderStageFlags
    {
        auto result = VkShaderStageFlags{};
        if (stage.any(EShaderStage::vertex)) {
            result |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (stage.any(EShaderStage::tessellation_control)) {
            result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        if (stage.any(EShaderStage::tessellation_evaluation)) {
            result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        if (stage.any(EShaderStage::geometry)) {
            result |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        if (stage.any(EShaderStage::fragment)) {
            result |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (stage.any(EShaderStage::compute)) {
            result |= VK_SHADER_STAGE_COMPUTE_BIT;
        }

        return result;
    }

    inline auto convert_to_vk_descriptor_type(EDescriptorType type) -> VkDescriptorType
    {
        switch (type) {
            using enum EDescriptorType;
            case EDescriptorType::uniform_buffer:  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case EDescriptorType::storage_buffer:  return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case EDescriptorType::texture:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case EDescriptorType::storage_texture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case EDescriptorType::sampler:         return VK_DESCRIPTOR_TYPE_SAMPLER;
            case EDescriptorType::texture_sampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_present_mode(EPresentMode mode) -> VkPresentModeKHR
    {
        switch (mode) {
            case EPresentMode::fifo:      return VK_PRESENT_MODE_FIFO_KHR;
            case EPresentMode::immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
            case EPresentMode::mailbox:   return VK_PRESENT_MODE_MAILBOX_KHR;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto convert_to_vk_color_space(EColorSpace space) -> VkColorSpaceKHR
    {
        switch (space) {
            case EColorSpace::srgb_nonliner:          return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            case EColorSpace::display_p3_nonliner:    return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
            case EColorSpace::extent_srgb_liner:      return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            case EColorSpace::display_p3_liner:       return VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT;
            case EColorSpace::dci_p3_nonliner:        return VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT;
            case EColorSpace::bt709_liner:            return VK_COLOR_SPACE_BT709_LINEAR_EXT;
            case EColorSpace::bt709_nonliner:         return VK_COLOR_SPACE_BT709_NONLINEAR_EXT;
            case EColorSpace::bt2020_liner:           return VK_COLOR_SPACE_BT2020_LINEAR_EXT;
            case EColorSpace::hdr10_st2084:           return VK_COLOR_SPACE_HDR10_ST2084_EXT;
            case EColorSpace::dolbyvision:            return VK_COLOR_SPACE_DOLBYVISION_EXT;
            case EColorSpace::hdr10_hlg:              return VK_COLOR_SPACE_HDR10_HLG_EXT;
            case EColorSpace::adobergb_liner:         return VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT;
            case EColorSpace::adobergb_nonliner:      return VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT;
            case EColorSpace::pass_through:           return VK_COLOR_SPACE_PASS_THROUGH_EXT;
            case EColorSpace::extended_srgb_nonliner: return VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT;
            case EColorSpace::display_native_amd:     return VK_COLOR_SPACE_DISPLAY_NATIVE_AMD;
            default: CNE_UNREACHABLE();
        }
    }

    inline auto default_buffer_barrier() -> VkBufferMemoryBarrier2
    {
        auto barrier = VkBufferMemoryBarrier2{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
        barrier.srcStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        barrier.srcAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
        barrier.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        barrier.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer              = VK_NULL_HANDLE;
        barrier.offset              = 0;
        barrier.size                = VK_WHOLE_SIZE;

        return barrier;
    }

    inline auto default_image_barrier() -> VkImageMemoryBarrier2
    {
        auto barrier = VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask                = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        barrier.srcAccessMask               = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
        barrier.dstStageMask                = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
        barrier.dstAccessMask               = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;
        barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.image                       = VK_NULL_HANDLE;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        return barrier;
    }

    inline auto convert_to_vk_buffer_usage(EBufferUsage usage) -> VkBufferUsageFlags
    {
        auto result = VkBufferUsageFlags{};
        // TODO: Check.
        result |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        if (enum_has_any_flags(usage, EBufferUsage::transfer_src)) {
            result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::transfer_dst)) {
            result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::vertex)) {
            result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::index)) {
            result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::uniform)) {
            result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::storage)) {
            result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (enum_has_any_flags(usage, EBufferUsage::indirect)) {
            result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        }

        return result;
    }

    inline auto image_type_from_dimension(ETextureDimension dim) -> VkImageType
    {
        switch (dim) {
            case ETextureDimension::tex_2d: return VK_IMAGE_TYPE_2D;
            case ETextureDimension::tex_3d: return VK_IMAGE_TYPE_3D;
            case ETextureDimension::tex_cube: return VK_IMAGE_TYPE_2D;
            case ETextureDimension::tex_2d_array: return VK_IMAGE_TYPE_2D;
            case ETextureDimension::tex_cube_array: return VK_IMAGE_TYPE_2D;
        }

        return VK_IMAGE_TYPE_MAX_ENUM;
    }

    inline auto image_create_flags_from_dimension(ETextureDimension dim) -> VkImageCreateFlags
    {
        auto flags = VkImageCreateFlags{};
        if (dim == ETextureDimension::tex_cube || dim == ETextureDimension::tex_cube_array) {
            flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        return flags;
    }

    inline auto image_view_type_from_dimension(ETextureDimension dim) -> VkImageViewType
    {
        switch (dim) {
            case ETextureDimension::tex_2d: return VK_IMAGE_VIEW_TYPE_2D;
            case ETextureDimension::tex_3d: return VK_IMAGE_VIEW_TYPE_3D;
            case ETextureDimension::tex_cube: return VK_IMAGE_VIEW_TYPE_CUBE;
            case ETextureDimension::tex_2d_array: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ETextureDimension::tex_cube_array: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default: return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    inline auto image_layout_from_access(EResourceStates access, bool is_depth_stencil_attachment) -> VkImageLayout
    {
        if (enum_has_any_flags(access, EResourceStates::color_attachment)) {
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_attachment)) {
            return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_read)) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        if (enum_has_any_flags(access, EResourceStates::SRV_access)) {
            if (is_depth_stencil_attachment) {
                return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            }
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        if (enum_has_any_flags(access, EResourceStates::UAV_access)) {
            return VK_IMAGE_LAYOUT_GENERAL;
        }
        if (enum_has_any_flags(access, EResourceStates::present)) {
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        switch (access) {
            case EResourceStates::unknown:      return VK_IMAGE_LAYOUT_UNDEFINED;
            case EResourceStates::transfer_src: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case EResourceStates::transfer_dst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    inline auto convert_to_vk_image_usage(ETextureUsage usage) -> VkImageUsageFlags
    {
        // All image support transfer
        VkImageUsageFlags result = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (enum_has_any_flags(usage, ETextureUsage::sampled)) {
            result |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (enum_has_any_flags(usage, ETextureUsage::storage)) {
            result |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (enum_has_any_flags(usage, ETextureUsage::color_attachment)) {
            result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (enum_has_any_flags(usage, ETextureUsage::depth_stencil_attachment)) {
            result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if (enum_has_any_flags(usage, ETextureUsage::transfer_src)) {
            result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (enum_has_any_flags(usage, ETextureUsage::transfer_dst)) {
            result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        return result;
    }

    inline auto convert_to_vk_load_op(ELoadOp load) -> VkAttachmentLoadOp
    {
        switch (load) {
            case ELoadOp::no_action: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            case ELoadOp::clear:     return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case ELoadOp::load:      return VK_ATTACHMENT_LOAD_OP_LOAD;
            default: return VK_ATTACHMENT_LOAD_OP_LOAD;
        }
    };

    inline auto convert_to_vk_store_op(EStoreOp store) -> VkAttachmentStoreOp
    {
        switch (store) {
            case EStoreOp::no_action: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            case EStoreOp::store:     return VK_ATTACHMENT_STORE_OP_STORE;
            default: return VK_ATTACHMENT_STORE_OP_STORE;
        }
    };

#define CHECK_VK_RESULT(X) CNE_CHECK((X) == VK_SUCCESS, CNE_ERROR)
#define check(X) if (!(X)) { CNE_ASSERT_WITH(false, "Check {3} failed in function '{1}', line: {0}, file: '{2}'", __LINE__, __FUNCTION__, __FILE__, #X); }
}
