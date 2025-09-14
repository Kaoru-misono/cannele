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

    auto to_vk_format(EFormat format) -> VkFormat
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

    static constexpr auto pipeline_stage_map = std::array<std::tuple<EPipelineStage, VkPipelineStageFlags2, const char*>, 36>{{
        {EPipelineStage::top_of_pipe,                      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,                          "TOP_OF_PIPE"                      },
        {EPipelineStage::draw_indirect,                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,                        "DRAW_INDIRECT"                    },
        {EPipelineStage::vertex_input,                     VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,                         "VERTEX_INPUT"                     },
        {EPipelineStage::vertex_shader,                    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,                        "VERTEX_SHADER"                    },
        {EPipelineStage::tessellation_control_shader,      VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,          "TESSELLATION_CONTROL_SHADER"      },
        {EPipelineStage::tessellation_evaluation_shader,   VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,       "TESSELLATION_EVALUATION_SHADER"   },
        {EPipelineStage::geometry_shader,                  VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,                      "GEOMETRY_SHADER"                  },
        {EPipelineStage::fragment_shader,                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                      "FRAGMENT_SHADER"                  },
        {EPipelineStage::early_fragment_tests,             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,                 "EARLY_FRAGMENT_TESTS"             },
        {EPipelineStage::late_fragment_tests,              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,                  "LATE_FRAGMENT_TESTS"              },
        {EPipelineStage::color_attachment_output,          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,              "COLOR_ATTACHMENT_OUTPUT"          },
        {EPipelineStage::compute_shader,                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,                       "COMPUTE_SHADER"                   },
        {EPipelineStage::all_transfer,                     VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,                         "ALL_TRANSFER"                     },
        {EPipelineStage::transfer,                         VK_PIPELINE_STAGE_2_TRANSFER_BIT,                             "TRANSFER"                         },
        {EPipelineStage::bottom_of_pipe,                   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,                       "BOTTOM_OF_PIPE"                   },
        {EPipelineStage::host,                             VK_PIPELINE_STAGE_2_HOST_BIT,                                 "HOST"                             },
        {EPipelineStage::all_graphics,                     VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,                         "ALL_GRAPHICS"                     },
        {EPipelineStage::all_commands,                     VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                         "ALL_COMMANDS"                     },
        {EPipelineStage::command_preprocess,               VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_EXT,               "COMMAND_PREPROCESS"               },
        {EPipelineStage::conditional_rendering,            VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,            "CONDITIONAL_RENDERING"            },
        {EPipelineStage::task_shader,                      VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,                      "TASK_SHADER"                      },
        {EPipelineStage::mesh_shader,                      VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,                      "MESH_SHADER"                      },
        {EPipelineStage::ray_tracing_shader,               VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,               "RAY_TRACING_SHADER"               },
        {EPipelineStage::fragment_shading_rate_attachment, VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR, "FRAGMENT_SHADING_RATE_ATTACHMENT" },
        {EPipelineStage::fragment_density_process,         VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT,         "FRAGMENT_DENSITY_PROCESS"         },
        {EPipelineStage::transform_feedback,               VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,               "TRANSFORM_FEEDBACK"               },
        {EPipelineStage::acceleration_structure_build,     VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,     "ACCELERATION_STRUCTURE_BUILD"     },
        {EPipelineStage::video_decode,                     VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,                     "VIDEO_DECODE"                     },
        {EPipelineStage::video_encode,                     VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR,                     "VIDEO_ENCODE"                     },
        {EPipelineStage::copy,                             VK_PIPELINE_STAGE_2_COPY_BIT,                                 "COPY"                             },
        {EPipelineStage::resolve,                          VK_PIPELINE_STAGE_2_RESOLVE_BIT,                              "RESOLVE"                          },
        {EPipelineStage::blit,                             VK_PIPELINE_STAGE_2_BLIT_BIT,                                 "BLIT"                             },
        {EPipelineStage::clear,                            VK_PIPELINE_STAGE_2_CLEAR_BIT,                                "CLEAR"                            },
        {EPipelineStage::index_input,                      VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,                          "INDEX_INPUT"                      },
        {EPipelineStage::vertex_attribute_input,           VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,               "VERTEX_ATTRIBUTE_INPUT"           },
        {EPipelineStage::pre_rasterization_shaders,        VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,            "PRE_RASTERIZATION_SHADERS"        },
    }};

    auto to_vk_pipeline_stage(EPipelineStage stages) -> VkPipelineStageFlags2
    {
        auto result = VkPipelineStageFlags2{};

        for (auto& [stage, vk_stage, _]: pipeline_stage_map) {
            if (enum_has_any_flags(stages, stage)) {
                result |= vk_stage;
            }
        }

        return result;
    }

    auto to_string(VkPipelineStageFlags2 flags) -> std::string
    {
        auto result = std::string{};

        for (auto& [_, vk_stage, name]: pipeline_stage_map) {
            if (flags & vk_stage) {
                result += name;
                result += " | ";
            }
        }

        if (result.empty()) {
            return "NONE";
        }

        return result.substr(0, result.find_last_of(" | ") - 2);
    }

    auto to_vk_pipeline_stage(EResourceStates states) -> VkPipelineStageFlags2
    {
        switch (states) {
            using enum EResourceStates;
            case unknown: {
                return VK_PIPELINE_STAGE_2_NONE;
            }
            case vertex_attribute_read:
            case index_read: {
                return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
            }
            case uniform_read:
            case UAV_access: {
                return VkPipelineStageFlagBits2{VK_PIPELINE_STAGE_2_NONE
                    | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
                    | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT
                    | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT
                    // | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR // TODO:
                };
            }
            case SRV_access: {
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            }
            case color_attachment: {
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            case depth_stencil_read:
            case depth_stencil_attachment: {
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            }
            case indirect_command_read: {
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            }
            case transfer_src:
            case transfer_dst: {
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            }
            case present: {
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            default: return VK_PIPELINE_STAGE_2_NONE;
        }
    }

    auto to_vk_access_type(EResourceStates states) -> VkAccessFlags2
    {
        switch (states) {
            using enum EResourceStates;
            case unknown:
            case present: {
                return VK_ACCESS_2_NONE;
            }
            case vertex_attribute_read: {
                return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
            }
            case index_read: {
                return VK_ACCESS_2_INDEX_READ_BIT;
            }
            case uniform_read: {
                return VK_ACCESS_2_UNIFORM_READ_BIT;
            }
            case UAV_access: {
                return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
            }
            case SRV_access: {
                return VK_ACCESS_2_SHADER_READ_BIT;
            }
            case color_attachment: {
                return VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            }
            case depth_stencil_read: {
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            }
            case depth_stencil_attachment: {
                return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
            case indirect_command_read: {
                return VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
            }
            case transfer_src: {
                return VK_ACCESS_2_TRANSFER_READ_BIT;
            }
            case transfer_dst: {
                return VK_ACCESS_2_TRANSFER_WRITE_BIT;
            }
            default: return VK_ACCESS_2_NONE;
        }
    }
}
