#include "vk_RHI.hpp"
#include "vk_tool.hpp"

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        static auto const dynamic_states = std::vector<VkDynamicState>{
            VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,          // VkPipelineVertexInputStateCreateInfo
            VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT, // VkPipelineMultisampleStateCreateInfo::rasterizationSamples
            VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,    // VkPipelineRasterizationStateCreateInfo::depthClampEnable
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,          // VkPipelineRasterizationStateCreateInfo::polygonMode
            VK_DYNAMIC_STATE_CULL_MODE,                 // VkPipelineRasterizationStateCreateInfo::cullMode
            VK_DYNAMIC_STATE_FRONT_FACE,                // VkPipelineRasterizationStateCreateInfo::frontFace
            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,         // VkPipelineRasterizationStateCreateInfo::depthBiasEnable
            VK_DYNAMIC_STATE_DEPTH_BIAS,                // VkPipelineRasterizationStateCreateInfo::depthBias
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,       // VkPipelineViewportStateCreateInfo::viewportCount, pViewports
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,        // VkPipelineViewportStateCreateInfo::scissorCount, pScissors
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,         // VkPipelineDepthStencilStateCreateInfo::depthTestEnable
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,        // VkPipelineDepthStencilStateCreateInfo::depthWriteEnable
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,          // VkPipelineDepthStencilStateCreateInfo::depthCompareOp
            VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,  // VkPipelineDepthStencilStateCreateInfo::depthBoundsTestEnable
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,       // VkPipelineDepthStencilStateCreateInfo::stencilTestEnable
            VK_DYNAMIC_STATE_STENCIL_OP,                // VkPipelineDepthStencilStateCreateInfo::front, VkPipelineDepthStencilStateCreateInfo::back
            VK_DYNAMIC_STATE_DEPTH_BOUNDS,              // VkPipelineDepthStencilStateCreateInfo::minDepthBounds, VkPipelineDepthStencilStateCreateInfo::maxDepthBounds
            VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
            VK_DYNAMIC_STATE_LOGIC_OP_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,    // VkPipelineColorBlendStateCreateInfo::logicOpEnable
            VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,  // VkPipelineColorBlendStateCreateInfo::logicOp
            VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,      // VkPipelineColorBlendStateCreateInfo::colorWriteMask
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,           // VkPipelineColorBlendStateCreateInfo::blendConstants
        };
    }

    auto VulkanDevice::create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo* info) -> GraphicsPipelineHandle
    {
        return pipeline_manager->create_graphics_pipeline(name, info);
    }

    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VulkanDevice* device, GraphicsPipelineCreateInfo* info)
        : VulkanDeviceChild<VulkanGraphicsPipeline>(device)
    {
        auto shader_stage_create_infos = std::vector<VkPipelineShaderStageCreateInfo>{};
        shader_stage_create_infos.reserve(4);
        auto shader_stage_flags = VkShaderStageFlags{};
        auto push_constant_size = 0u;
        auto vulkan_vs_module = assert_ref_count_cast<VulkanShaderModule>(info->vs);
        {
            auto shader_stage_ci = &shader_stage_create_infos.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            shader_stage_ci->stage  = vulkan_vs_module->stage;
            shader_stage_ci->module = vulkan_vs_module->shader_module;
            shader_stage_ci->pName  = vulkan_vs_module->entry_point.c_str();

            push_constant_size = std::max(push_constant_size, vulkan_vs_module->push_constant_size);
            shader_stage_flags |= vulkan_vs_module->stage;
        }
        auto vulkan_fs_module = assert_ref_count_cast<VulkanShaderModule>(info->fs);
        {
            auto shader_stage_ci = &shader_stage_create_infos.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            shader_stage_ci->stage  = vulkan_fs_module->stage;
            shader_stage_ci->module = vulkan_fs_module->shader_module;
            shader_stage_ci->pName  = vulkan_fs_module->entry_point.c_str();

            push_constant_size = std::max(push_constant_size, vulkan_fs_module->push_constant_size);
            shader_stage_flags |= vulkan_fs_module->stage;
        }

        // TODO: We encode the primitiveRestartEnable to false, implement it later.
        auto input_assembly_state_ci = VkPipelineInputAssemblyStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly_state_ci.topology               = convert_to_vk_primitive_topology(info->topology);
        input_assembly_state_ci.primitiveRestartEnable = VK_FALSE;

        auto rasterization_ci = VkPipelineRasterizationStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization_ci.depthClampEnable        = VK_FALSE;
        rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_ci.lineWidth               = 1.0f;

        // TODO: Enable samples.
        auto multisample_state_ci = VkPipelineMultisampleStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample_state_ci.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_ci.sampleShadingEnable   = VK_FALSE;
        multisample_state_ci.minSampleShading      = 0.0f;
        multisample_state_ci.pSampleMask           = nullptr;
        multisample_state_ci.alphaToCoverageEnable = VK_FALSE;
        multisample_state_ci.alphaToOneEnable      = VK_FALSE;

        auto dynamic_state_ci = VkPipelineDynamicStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic_state_ci.dynamicStateCount = (uint32_t) dynamic_states.size();
        dynamic_state_ci.pDynamicStates    = dynamic_states.data();

        auto vk_color_formats = std::vector<VkFormat>{};
        vk_color_formats.resize(info->render_target_info.color_formats.size());
        for (auto i = 0zu; i < vk_color_formats.size(); i++) {
            vk_color_formats[i] = convert_to_vk_format(info->render_target_info.color_formats[i]);
        }
        auto depth_stencil_format = convert_to_vk_format(info->render_target_info.depth_stencil_format);

        auto pipeline_rendering_ci = VkPipelineRenderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        pipeline_rendering_ci.viewMask                = 0;
        pipeline_rendering_ci.colorAttachmentCount    = (uint32_t) vk_color_formats.size();
        pipeline_rendering_ci.pColorAttachmentFormats = vk_color_formats.data();
        pipeline_rendering_ci.depthAttachmentFormat   = depth_stencil_format;
        pipeline_rendering_ci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        // We only use the bindless descriptor set now.
        auto bindless_manager = parent->bindless_manager.get();
        auto descriptor_set_layouts = std::vector<VkDescriptorSetLayout>{
            bindless_manager->resource_heap->descriptor_set_layout,
            bindless_manager->sampler_heap->descriptor_set_layout
        };
        auto push_constants = std::vector<VkPushConstantRange>{};
        if (push_constant_size > 0) {
            push_constants.emplace_back(shader_stage_flags, 0, push_constant_size);
        }

        pipeline_layout = parent->layout_manager->create_pipeline_layout(descriptor_set_layouts, push_constants);
        auto pipeline_ci = VkGraphicsPipelineCreateInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_ci.pNext               = &pipeline_rendering_ci;
        pipeline_ci.flags               = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        pipeline_ci.stageCount          = (uint32_t) shader_stage_create_infos.size();
        pipeline_ci.pStages             = shader_stage_create_infos.data();
        pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
        pipeline_ci.pRasterizationState = &rasterization_ci;
        pipeline_ci.pMultisampleState   = &multisample_state_ci;
        pipeline_ci.pDynamicState       = &dynamic_state_ci;
        pipeline_ci.layout              = pipeline_layout;

        auto result = vkCreateGraphicsPipelines(parent->device, VK_NULL_HANDLE, 1, &pipeline_ci, parent->allocation_callbacks, &pipeline);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create graphics pipeline: {}", vk_error_to_string(result)));
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        if (pipeline) {
            vkDestroyPipeline(parent->device, pipeline, parent->allocation_callbacks);
        }
    }

    auto VulkanDevice::create_mesh_pipeline(std::string_view name, MeshPipelineCreateInfo* info) -> MeshPipelineHandle
    {
        return pipeline_manager->create_mesh_pipeline(name, info);
    }

    VulkanMeshPipeline::VulkanMeshPipeline(VulkanDevice* device, MeshPipelineCreateInfo* info)
        : VulkanDeviceChild<VulkanMeshPipeline>(device)
    {
        auto shader_stage_create_infos = std::vector<VkPipelineShaderStageCreateInfo>{};
        shader_stage_create_infos.reserve(4);
        auto shader_stage_flags = VkShaderStageFlags{};
        auto push_constant_size = 0u;
        if (info->as) {
            auto vulkan_as_module = assert_ref_count_cast<VulkanShaderModule>(info->as);
            auto shader_stage_ci = &shader_stage_create_infos.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            shader_stage_ci->stage  = vulkan_as_module->stage;
            shader_stage_ci->module = vulkan_as_module->shader_module;
            shader_stage_ci->pName  = vulkan_as_module->entry_point.c_str();

            push_constant_size = std::max(push_constant_size, vulkan_as_module->push_constant_size);
            shader_stage_flags |= vulkan_as_module->stage;
        }
        auto vulkan_ms_module = assert_ref_count_cast<VulkanShaderModule>(info->ms);
        {
            auto shader_stage_ci = &shader_stage_create_infos.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            shader_stage_ci->stage  = vulkan_ms_module->stage;
            shader_stage_ci->module = vulkan_ms_module->shader_module;
            shader_stage_ci->pName  = vulkan_ms_module->entry_point.c_str();

            push_constant_size = std::max(push_constant_size, vulkan_ms_module->push_constant_size);
            shader_stage_flags |= vulkan_ms_module->stage;
        }
        auto vulkan_fs_module = assert_ref_count_cast<VulkanShaderModule>(info->fs);
        {
            auto shader_stage_ci = &shader_stage_create_infos.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            shader_stage_ci->stage  = vulkan_fs_module->stage;
            shader_stage_ci->module = vulkan_fs_module->shader_module;
            shader_stage_ci->pName  = vulkan_fs_module->entry_point.c_str();

            push_constant_size = std::max(push_constant_size, vulkan_fs_module->push_constant_size);
            shader_stage_flags |= vulkan_fs_module->stage;
        }

        // TODO: We encode the primitiveRestartEnable to false, implement it later.
        auto input_assembly_state_ci = VkPipelineInputAssemblyStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly_state_ci.topology               = convert_to_vk_primitive_topology(info->topology);
        input_assembly_state_ci.primitiveRestartEnable = VK_FALSE;

        auto rasterization_ci = VkPipelineRasterizationStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization_ci.depthClampEnable        = VK_FALSE;
        rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
        rasterization_ci.lineWidth               = 1.0f;

        // TODO: Enable samples.
        auto multisample_state_ci = VkPipelineMultisampleStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample_state_ci.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_ci.sampleShadingEnable   = VK_FALSE;
        multisample_state_ci.minSampleShading      = 0.0f;
        multisample_state_ci.pSampleMask           = nullptr;
        multisample_state_ci.alphaToCoverageEnable = VK_FALSE;
        multisample_state_ci.alphaToOneEnable      = VK_FALSE;

        auto dynamic_state_ci = VkPipelineDynamicStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        auto state_span = std::span{dynamic_states}.subspan(1);
        dynamic_state_ci.dynamicStateCount = (uint32_t) state_span.size();
        dynamic_state_ci.pDynamicStates    = state_span.data();

        auto vk_color_formats = std::vector<VkFormat>{};
        vk_color_formats.resize(info->render_target_info.color_formats.size());
        for (auto i = 0zu; i < vk_color_formats.size(); i++) {
            vk_color_formats[i] = convert_to_vk_format(info->render_target_info.color_formats[i]);
        }
        auto depth_stencil_format = convert_to_vk_format(info->render_target_info.depth_stencil_format);

        auto pipeline_rendering_ci = VkPipelineRenderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        pipeline_rendering_ci.viewMask                = 0;
        pipeline_rendering_ci.colorAttachmentCount    = (uint32_t) vk_color_formats.size();
        pipeline_rendering_ci.pColorAttachmentFormats = vk_color_formats.data();
        pipeline_rendering_ci.depthAttachmentFormat   = depth_stencil_format;
        pipeline_rendering_ci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

        // We only use the bindless descriptor set now.
        auto bindless_manager = parent->bindless_manager.get();
        auto descriptor_set_layouts = std::vector<VkDescriptorSetLayout>{
            bindless_manager->resource_heap->descriptor_set_layout,
            bindless_manager->sampler_heap->descriptor_set_layout
        };
        auto push_constants = std::vector<VkPushConstantRange>{};
        if (push_constant_size > 0) {
            push_constants.emplace_back(shader_stage_flags, 0, push_constant_size);
        }

        pipeline_layout = parent->layout_manager->create_pipeline_layout(descriptor_set_layouts, push_constants);
        auto pipeline_ci = VkGraphicsPipelineCreateInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipeline_ci.pNext               = &pipeline_rendering_ci;
        pipeline_ci.flags               = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        pipeline_ci.stageCount          = (uint32_t) shader_stage_create_infos.size();
        pipeline_ci.pStages             = shader_stage_create_infos.data();
        pipeline_ci.pInputAssemblyState = &input_assembly_state_ci;
        pipeline_ci.pRasterizationState = &rasterization_ci;
        pipeline_ci.pMultisampleState   = &multisample_state_ci;
        pipeline_ci.pDynamicState       = &dynamic_state_ci;
        pipeline_ci.layout              = pipeline_layout;

        auto result = vkCreateGraphicsPipelines(parent->device, VK_NULL_HANDLE, 1, &pipeline_ci, parent->allocation_callbacks, &pipeline);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create mesh pipeline: {}", vk_error_to_string(result)));
    }

    VulkanMeshPipeline::~VulkanMeshPipeline()
    {
        if (pipeline) {
            vkDestroyPipeline(parent->device, pipeline, parent->allocation_callbacks);
        }
    }

    auto VulkanDevice::create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo* info) -> ComputePipelineHandle
    {
        return pipeline_manager->create_compute_pipeline(name, info);
    }

    VulkanComputePipeline::VulkanComputePipeline(VulkanDevice* device, ComputePipelineCreateInfo* info)
        : VulkanDeviceChild<VulkanComputePipeline>(device)
    {
        auto vulkan_shader = assert_ref_count_cast<VulkanShaderModule>(info->compute_shader);
        auto shader_stage_ci = VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        shader_stage_ci.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        shader_stage_ci.module = vulkan_shader->shader_module;
        shader_stage_ci.pName  = vulkan_shader->entry_point.c_str();

        // We only use the bindless descriptor set now.
        // FIXME:
        auto bindless_manager = parent->bindless_manager.get();
        auto descriptor_set_layouts = std::vector<VkDescriptorSetLayout>{
            bindless_manager->resource_heap->descriptor_set_layout,
            bindless_manager->sampler_heap->descriptor_set_layout
        };
        auto push_constants = std::vector<VkPushConstantRange>{};
        if (vulkan_shader->push_constant_size > 0) {
            push_constants.emplace_back(VK_SHADER_STAGE_COMPUTE_BIT, 0, vulkan_shader->push_constant_size);
        }

        pipeline_layout = parent->layout_manager->create_pipeline_layout(descriptor_set_layouts, push_constants);
        auto pipeline_ci = VkComputePipelineCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline_ci.flags  = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        pipeline_ci.stage  = shader_stage_ci;
        pipeline_ci.layout = pipeline_layout;

        auto result = vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_ci, parent->allocation_callbacks, &pipeline);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create compute pipeline: {}", vk_error_to_string(result)));
    }

    VulkanComputePipeline::~VulkanComputePipeline()
    {
        if (pipeline) {
            vkDestroyPipeline(parent->device, pipeline, parent->allocation_callbacks);
        }
    }
}
