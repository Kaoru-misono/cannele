#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <spirv_cross/spirv_hlsl.hpp>

namespace cannele::inline graphics::rhi::vk
{
    auto VulkanDevice::create_shader_module(std::string_view name, ShaderModuleCreateInfo const* info) -> ShaderModuleHandle
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto shader = std::make_shared<VulkanShaderModule>(this, info);

        set_resource_name(device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t) shader->shader_module, name);
        shader->name = name;

        return shader;
    }

    VulkanShaderModule::VulkanShaderModule(VulkanDevice* device, ShaderModuleCreateInfo const* info)
        : VulkanDeviceChild<VulkanShaderModule>(device)
    {
        switch (info->stage) {
            case EShaderStage::vertex:                  stage = VK_SHADER_STAGE_VERTEX_BIT; break;
            case EShaderStage::tessellation_control:    stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; break;
            case EShaderStage::tessellation_evaluation: stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; break;
            case EShaderStage::geometry:                stage = VK_SHADER_STAGE_GEOMETRY_BIT; break;
            case EShaderStage::fragment:                stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
            case EShaderStage::compute:                 stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
            case EShaderStage::task:                    stage = VK_SHADER_STAGE_TASK_BIT_EXT; break;
            case EShaderStage::mesh:                    stage = VK_SHADER_STAGE_MESH_BIT_EXT; break;
            default: CNE_UNREACHABLE();
        }
        create_module(info->code);
    }

    VulkanShaderModule::~VulkanShaderModule()
    {
        if (shader_module) {
            vkDestroyShaderModule(parent->device, shader_module, nullptr);
        }
    }

    auto VulkanShaderModule::recreate(std::span<std::byte const> code) -> void
    {
        vkDestroyShaderModule(parent->device, shader_module, parent->allocation_callbacks);

        create_module(code);
    }

    auto VulkanShaderModule::entry() -> std::string_view
    {
        return entry_point;
    }

    auto VulkanShaderModule::create_module(std::span<std::byte const> code) -> void
    {
        auto shader_module_ci = VkShaderModuleCreateInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader_module_ci.codeSize = code.size();
        shader_module_ci.pCode    = (uint32_t*) code.data();

        auto result = vkCreateShaderModule(parent->device, &shader_module_ci, parent->allocation_callbacks, &shader_module);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create shader module: {}", vk_error_to_string(result)));

        // Reflection:
        auto compiler = spirv_cross::CompilerHLSL((uint32_t*) code.data(), code.size() / sizeof(uint32_t));

        auto resources = compiler.get_shader_resources();
        if (!resources.push_constant_buffers.empty()) {
            auto push_constant_buffer = resources.push_constant_buffers[0];
            auto spirv_type = compiler.get_type(push_constant_buffer.type_id);
            push_constant_size = compiler.get_declared_struct_size(spirv_type);
        }

        entry_point = compiler.get_entry_points_and_stages()[0].name;
    }
}
