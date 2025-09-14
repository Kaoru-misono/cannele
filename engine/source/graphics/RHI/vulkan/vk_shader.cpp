#include "vk_shader.hpp"
#include "vk_RHI.hpp"
#include "vk_tool.hpp"
#include "../tool/slang_reflection_print.hpp"

#include <spirv_cross/spirv_hlsl.hpp>

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        auto to_vk_shader_stage(SlangStage stage) -> VkShaderStageFlagBits
        {
            switch (stage) {
                case SLANG_STAGE_VERTEX:                  return VK_SHADER_STAGE_VERTEX_BIT;
                case SLANG_STAGE_GEOMETRY:                return VK_SHADER_STAGE_GEOMETRY_BIT;
                case SLANG_STAGE_FRAGMENT:                return VK_SHADER_STAGE_FRAGMENT_BIT;
                case SLANG_STAGE_COMPUTE:                 return VK_SHADER_STAGE_COMPUTE_BIT;
                case SLANG_STAGE_AMPLIFICATION:           return VK_SHADER_STAGE_TASK_BIT_EXT;
                case SLANG_STAGE_MESH:                    return VK_SHADER_STAGE_MESH_BIT_EXT;
                default: CNE_UNREACHABLE();
            }
        }

        auto to_vk_descriptor_type(slang::BindingType type) -> VkDescriptorType
        {
            switch (type) {
                case slang::BindingType::Sampler:                         return VK_DESCRIPTOR_TYPE_SAMPLER;
                case slang::BindingType::CombinedTextureSampler:          return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                case slang::BindingType::Texture:                         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case slang::BindingType::MutableTexture:                  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                case slang::BindingType::TypedBuffer:                     return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                case slang::BindingType::MutableTypedBuffer:              return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                case slang::BindingType::RawBuffer:                       return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                case slang::BindingType::InputRenderTarget:               return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                case slang::BindingType::InlineUniformData:               return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
                case slang::BindingType::RayTracingAccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                case slang::BindingType::ConstantBuffer:                  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case slang::BindingType::PushConstant:
                default:                                                  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            }
        }

        #define DEBUG_PRINT_PROGRAM_LAYOUT 0
    }

    auto VulkanDevice::create_shader_module(std::string_view name, ShaderModuleCreateInfo const* info) -> ShaderModuleHandle
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto shader = std::make_shared<VulkanShaderModule>(this, info);

        set_resource_name(device, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t) shader->shader_module, name);
        shader->name = name;

        return shader;
    }

    auto VulkanDevice::create_shader_program(ShaderProgramCreateInfo const* info) -> std::shared_ptr<RHIShaderProgram>
    {
        return std::make_shared<VulkanShaderProgram>(this, *info);
    }

    auto VulkanDevice::create_shader_object_layout(slang::ISession* session, slang::TypeLayoutReflection* type_layout) -> std::shared_ptr<ShaderObjectLayout>
    {
        return VulkanShaderObjectLayout::create_element_layout(this, session, type_layout);
    }

    VulkanShaderModule::VulkanShaderModule(VulkanDevice* device, ShaderModuleCreateInfo const* info)
        : RHIShaderModule(device)
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
        auto parent = get_device<VulkanDevice>();
        if (shader_module) {
            vkDestroyShaderModule(parent->device, shader_module, nullptr);
        }
    }

    auto VulkanShaderModule::recreate(std::span<std::byte const> code) -> void
    {
        auto parent = get_device<VulkanDevice>();
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

        auto parent = get_device<VulkanDevice>();
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

    VulkanShaderProgram::VulkanShaderProgram(VulkanDevice* device, ShaderProgramCreateInfo const& info)
        : RHIShaderProgram(device, info)
    {
        root_layout = VulkanShaderObjectLayout::create_root_layout(device, linked_program, linked_program->getLayout());
    }

    VulkanShaderProgram::~VulkanShaderProgram()
    {
        auto device = get_device<VulkanDevice>();
        for (auto& module: modules) {
            if (module.shader_module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device->device, module.shader_module, device->allocation_callbacks);
            }
        }
    }

    auto VulkanShaderProgram::create_shader_module(slang::EntryPointReflection* entry_point_info, std::span<std::byte const> code) -> bool
    {
        auto parent = get_device<VulkanDevice>();

        auto module = &modules.emplace_back();
        auto stage_create_info = &shader_stages.emplace_back();

        module->code = code;
        module->entry_point_name = entry_point_info->getNameOverride();

        auto create_info = VkShaderModuleCreateInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        create_info.codeSize = module->code.size();
        create_info.pCode    = (uint32_t*) module->code.data();
        CHECK_VK_RESULT(vkCreateShaderModule(parent->device, &create_info, parent->allocation_callbacks, &module->shader_module));

        stage_create_info->sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_create_info->stage  = to_vk_shader_stage(entry_point_info->getStage());
        stage_create_info->module = module->shader_module;
        stage_create_info->pName  = "main";

        return true;
    }

    auto VulkanShaderProgram::root_shader_object_layout() const -> ShaderObjectLayout*
    {
        return root_layout.get();
    }

    VulkanShaderObjectLayout::VulkanShaderObjectLayout(VulkanDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout)
        : ShaderObjectLayout(device, session, element_type_layout)
    {}

    VulkanShaderObjectLayout::~VulkanShaderObjectLayout()
    {
        auto parent = get_device<VulkanDevice>();
        for (auto& descriptor_set_info: owned_descriptor_sets) {
            vkDestroyDescriptorSetLayout(parent->device, descriptor_set_info.set_layout, parent->allocation_callbacks);
        }
    }

    auto VulkanShaderObjectLayout::create_element_layout(VulkanDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout) -> std::unique_ptr<VulkanShaderObjectLayout>
    {
        auto layout = std::make_unique<VulkanShaderObjectLayout>(device, session, element_type_layout);

        layout->set_element_type_layout(element_type_layout);

        auto need_ordinary_data_buffer = element_type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM) != 0;
        auto ordinary_data_buffer_count = need_ordinary_data_buffer ? 1 : 0;

        auto container_offset = BindingOffset{};
        auto element_offset = BindingOffset{};
        element_offset.binding = ordinary_data_buffer_count;

        auto primary_descriptor_count = ordinary_data_buffer_count + element_type_layout->getSize(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT);
        element_offset.pending.binding = primary_descriptor_count;

        layout->add_descriptor_range_as_constant_buffer(element_type_layout, container_offset, element_offset, VK_SHADER_STAGE_ALL_GRAPHICS);

        layout->create_vk_descriptor_set_layout();

        return layout;
    }

    auto VulkanShaderObjectLayout::create_entry_point_layout(VulkanDevice* device, slang::ISession* session, slang::EntryPointReflection* entry_point_reflection) -> std::unique_ptr<VulkanEntryPointLayout>
    {
        auto layout = std::make_unique<VulkanEntryPointLayout>(device, session);

        layout->slang_entry_point_layout = entry_point_reflection;
        layout->stage_flag = to_vk_shader_stage(entry_point_reflection->getStage());
        layout->set_element_type_layout(entry_point_reflection->getTypeLayout());

        return layout;
    }

    auto VulkanShaderObjectLayout::create_root_layout(VulkanDevice *device, slang::IComponentType *program, slang::ProgramLayout *program_layout) -> std::unique_ptr<VulkanRootShaderObjectLayout>
    {
        #if DEBUG_PRINT_PROGRAM_LAYOUT == 1
            print_program_layout(program_layout);
        #endif

        auto layout = std::make_unique<VulkanRootShaderObjectLayout>(device, program, program_layout);

        layout->add_global_params(program_layout->getGlobalParamsVarLayout());

        auto entry_point_count = program_layout->getEntryPointCount();
        for (auto i = 0zu; i < entry_point_count; i++) {
            auto slang_entry_point = program_layout->getEntryPointByIndex(i);

            layout->add_entry_point(slang_entry_point);
        }

        if (program->getSpecializationParamCount() > 0) return layout;

        // Bindless descriptor sets always use space 0 and 1, 0 for resources, 1 for samplers.
        layout->descriptor_set_layouts.emplace_back(device->bindless_manager->resource_heap->descriptor_set_layout);
        layout->descriptor_set_layouts.emplace_back(device->bindless_manager->sampler_heap->descriptor_set_layout);

        layout->add_all_descriptor_sets();
        layout->add_all_push_constant_ranges();

        // Create pipeline layout.
        auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout_create_info.setLayoutCount = layout->descriptor_set_layouts.size();
        pipeline_layout_create_info.pSetLayouts    = layout->descriptor_set_layouts.data();
        if (!layout->all_push_constant_ranges.empty()) {
            auto total_push_constant_size = 0u;
            for (auto& range: layout->all_push_constant_ranges) {
                total_push_constant_size = std::max(total_push_constant_size, range.offset + range.size);
            }
            if (total_push_constant_size > device->physical_device_properties.properties2.properties.limits.maxPushConstantsSize) {
                CNE_ERROR("Push constants too large, max size is {} bytes", device->physical_device_properties.properties2.properties.limits.maxPushConstantsSize);
            }
            pipeline_layout_create_info.pushConstantRangeCount = layout->all_push_constant_ranges.size();
            pipeline_layout_create_info.pPushConstantRanges    = layout->all_push_constant_ranges.data();
        }
        CHECK_VK_RESULT(vkCreatePipelineLayout(device->device, &pipeline_layout_create_info, device->allocation_callbacks, &layout->pipeline_layout));

        return layout;
    }

    auto VulkanShaderObjectLayout::find_or_add_descriptor_set(uint32_t set_index) -> uint32_t
    {
        if (set_index == 0 || set_index == 1) return -1;

        auto it = set_to_index_map.find(set_index);
        if (it != set_to_index_map.end()) {
            return it->second;
        }

        auto index = owned_descriptor_sets.size();
        auto info = &owned_descriptor_sets.emplace_back();
        info->set_index = set_index;

        set_to_index_map[set_index] = index;

        return index;
    }

    auto VulkanShaderObjectLayout::add_descriptor_range_as_value(slang::TypeLayoutReflection* type_layout, BindingOffset const& offset, VkShaderStageFlags stage) -> void
    {
        if (!type_layout) return;

        auto descriptor_set_count = type_layout->getDescriptorSetCount();
        for (auto i = 0u; i < descriptor_set_count; i++) {
            auto descriptor_range_count = type_layout->getDescriptorSetDescriptorRangeCount(i);
            if (descriptor_range_count == 0) continue;

            auto descriptor_set_index = find_or_add_descriptor_set(offset.set + type_layout->getDescriptorSetSpaceOffset(i));
            // TODO:
        }

        auto binding_range_count = type_layout->getBindingRangeCount();
        for (auto binding_range_index = 0u; binding_range_index < binding_range_count; binding_range_index++) {
            auto binding_range_type = type_layout->getBindingRangeType(binding_range_index);
            switch (binding_range_type) {
                case slang::BindingType::ParameterBlock:
                case slang::BindingType::ConstantBuffer:
                case slang::BindingType::ExistentialValue:
                case slang::BindingType::PushConstant: {
                    continue;
                }
                case slang::BindingType::VaryingInput:
                case slang::BindingType::VaryingOutput: {
                    continue;
                }
                default: break;
            }

            auto descriptor_range_count = type_layout->getBindingRangeDescriptorRangeCount(binding_range_index);
            if (descriptor_range_count == 0) continue;

            auto slang_descriptor_set_index = type_layout->getBindingRangeDescriptorSetIndex(binding_range_index);
            auto descriptor_set_index = find_or_add_descriptor_set(
                offset.set + type_layout->getDescriptorSetSpaceOffset(slang_descriptor_set_index)
            );
            if (descriptor_set_index == -1) continue;

            auto descriptor_set_info = &owned_descriptor_sets[descriptor_set_index];

            auto first_descriptor_range_index = type_layout->getBindingRangeFirstDescriptorRangeIndex(binding_range_index);
            for (auto j = 0u; j < descriptor_range_count; j++) {
                auto descriptor_range_index = first_descriptor_range_index + j;
                auto slang_descriptor_type = type_layout->getDescriptorSetDescriptorRangeType(slang_descriptor_set_index, descriptor_range_index);

                switch (slang_descriptor_type) {
                    case slang::BindingType::ExistentialValue:
                    case slang::BindingType::InlineUniformData:
                    case slang::BindingType::PushConstant: {
                        continue;
                    }
                    default: break;
                }

                auto descriptor_type = to_vk_descriptor_type(slang_descriptor_type);
                auto layout_binding = &descriptor_set_info->bindings.emplace_back();;
                layout_binding->binding = (
                    offset.binding +
                    type_layout->getDescriptorSetDescriptorRangeIndexOffset(slang_descriptor_set_index, descriptor_range_index)
                );
                layout_binding->descriptorCount = type_layout->getDescriptorSetDescriptorRangeDescriptorCount(
                    slang_descriptor_set_index,
                    descriptor_range_index
                );
                layout_binding->descriptorType = descriptor_type;
                layout_binding->stageFlags = VK_SHADER_STAGE_ALL;
            }
        }

        auto sub_object_range_count = type_layout->getSubObjectRangeCount();
        for (auto sub_object_range_index = 0u; sub_object_range_index < sub_object_range_count; sub_object_range_index++) {
            auto binding_range_index = type_layout->getSubObjectRangeBindingRangeIndex(sub_object_range_index);
            auto binding_type = type_layout->getBindingRangeType(binding_range_index);

            auto sub_object_type_layout = type_layout->getBindingRangeLeafTypeLayout(binding_range_index);
            CNE_ASSERT(sub_object_type_layout);

            auto sub_object_range_offset = offset;
            sub_object_range_offset += BindingOffset{type_layout->getSubObjectRangeOffset(sub_object_range_index)};

            switch (binding_type) {
                case slang::BindingType::ExistentialValue: {
                    if (auto pending_type_layout = sub_object_type_layout->getPendingDataTypeLayout()) {
                        auto pending_offset = BindingOffset{sub_object_range_offset};
                        add_descriptor_range_as_value(pending_type_layout, pending_offset, stage);
                    }
                    break;
                }
                case slang::BindingType::ConstantBuffer: {
                    CNE_ASSERT(sub_object_type_layout);

                    auto container_var_layout = sub_object_type_layout->getContainerVarLayout();
                    CNE_ASSERT(container_var_layout);

                    auto element_var_layout = sub_object_type_layout->getElementVarLayout();
                    CNE_ASSERT(element_var_layout);

                    auto element_type_layout = element_var_layout->getTypeLayout();
                    CNE_ASSERT(element_type_layout);

                    auto container_offset = sub_object_range_offset;
                    container_offset += BindingOffset{sub_object_type_layout->getContainerVarLayout()};

                    auto element_offset = sub_object_range_offset;
                    element_offset += BindingOffset{element_var_layout};

                    add_descriptor_range_as_constant_buffer(element_type_layout, container_offset, element_offset, stage);
                }
                case slang::BindingType::PushConstant: {
                    CNE_ASSERT(sub_object_type_layout);

                    auto container_var_layout = sub_object_type_layout->getContainerVarLayout();
                    CNE_ASSERT(container_var_layout);

                    auto element_var_layout = sub_object_type_layout->getElementVarLayout();
                    CNE_ASSERT(element_var_layout);

                    auto element_type_layout = element_var_layout->getTypeLayout();
                    CNE_ASSERT(element_type_layout);

                    auto container_offset = sub_object_range_offset;
                    container_offset += BindingOffset{sub_object_type_layout->getContainerVarLayout()};

                    auto element_offset = sub_object_range_offset;
                    element_offset += BindingOffset{element_var_layout};

                    element_type_layout->getElementTypeLayout();
                    add_descriptor_range_as_push_constant(element_type_layout, container_offset, element_offset, stage);
                }
                case slang::BindingType::ParameterBlock:
                default: break;
            }
        }
    }

    auto VulkanShaderObjectLayout::add_descriptor_range_as_constant_buffer(
        slang::TypeLayoutReflection* type_layout,
        BindingOffset const& container_offset,
        BindingOffset const& element_offset,
        VkShaderStageFlags stage
    ) -> void
    {
        if (type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM) != 0) {
            auto descriptor_set_index = find_or_add_descriptor_set(container_offset.set);
            if (descriptor_set_index == -1) return;
            auto descriptor_set_info = &owned_descriptor_sets[descriptor_set_index];

            auto layout_binding = &descriptor_set_info->bindings.emplace_back();
            layout_binding->binding         = container_offset.binding;
            layout_binding->descriptorCount = 1;
            layout_binding->descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_binding->stageFlags      = stage;
        }

        add_descriptor_range_as_value(type_layout, element_offset, stage);
    }

    auto VulkanShaderObjectLayout::add_descriptor_range_as_push_constant(
        slang::TypeLayoutReflection* type_layout,
        BindingOffset const& container_offset,
        BindingOffset const& element_offset,
        VkShaderStageFlags stage
    ) -> void
    {
        auto ordinary_data_size = type_layout->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
        if (ordinary_data_size != 0) {
            auto push_constant_range_index = container_offset.push_constant_range;

            auto push_constant_range = VkPushConstantRange{};
            push_constant_range.size       = ordinary_data_size;
            push_constant_range.stageFlags = stage;

            if (push_constant_ranges.size() <= push_constant_range_index) {
                push_constant_ranges.resize(push_constant_range_index + 1);
            }

            push_constant_ranges[push_constant_range_index] = push_constant_range;

            auto field_count = type_layout->getFieldCount();
            for (auto i = 0u; i < field_count; i++) {
                auto field_layout = type_layout->getFieldByIndex(i);

                auto unit_count = field_layout->getCategoryCount();
                for (auto j = 0u; j < unit_count; j++) {
                    auto unit = field_layout->getCategoryByIndex(j);
                }
            }
        }

        add_descriptor_range_as_value(type_layout, element_offset, stage);
    }

    auto VulkanShaderObjectLayout::add_binding_ranges(slang::TypeLayoutReflection* type_layout) -> void
    {
        auto parent = get_device<VulkanDevice>();

        auto binding_range_count = type_layout->getBindingRangeCount();
        for (auto i = 0u; i < binding_range_count; i++) {
            auto slang_binding_type = type_layout->getBindingRangeType(i);
            auto count = type_layout->getBindingRangeBindingCount(i);
            if (count < 0) continue;
            auto slang_leaf_type_layout = type_layout->getBindingRangeLeafTypeLayout(i);

            auto slot_index = 0u;
            auto sub_object_index = 0u;

            switch (slang_binding_type) {
                case slang::BindingType::ConstantBuffer:
                case slang::BindingType::ParameterBlock:
                case slang::BindingType::ExistentialValue: {
                    sub_object_index = sub_object_count_;
                    sub_object_count_ += count;
                    break;
                }
                case slang::BindingType::RawBuffer:
                case slang::BindingType::MutableRawBuffer: {
                    if (slang_leaf_type_layout->getType()->getElementType() != nullptr) {
                        sub_object_index = sub_object_count_;
                        sub_object_count_ += count;
                    }
                    slot_index = slot_count_;
                    slot_count_ += count;
                    break;
                }
                case slang::BindingType::Sampler: {
                    slot_index = slot_count_;
                    slot_count_ += count;
                    total_binding_count += 1;
                    break;
                }
                case slang::BindingType::VaryingInput:
                case slang::BindingType::VaryingOutput: {
                    break;
                }
                default: {
                    slot_index = slot_count_;
                    slot_count_ += count;
                    total_binding_count += 1;
                    break;
                }
            }

            auto binding_range_info = &binding_ranges.emplace_back();
            binding_range_info->binding_type      = slang_binding_type;
            binding_range_info->count             = count;
            binding_range_info->slot_index        = slot_index;
            binding_range_info->sub_object_index  = sub_object_index;
            binding_range_info->is_speciallizable = type_layout->isBindingRangeSpecializable(i);

            if (type_layout->getBindingRangeDescriptorRangeCount(i) != 0) {
                auto descriptor_set_index = type_layout->getBindingRangeDescriptorSetIndex(i);
                auto descriptor_range_index = type_layout->getBindingRangeFirstDescriptorRangeIndex(i);

                auto set = type_layout->getDescriptorSetSpaceOffset(descriptor_set_index);
                auto binding_offset = type_layout->getDescriptorSetDescriptorRangeIndexOffset(descriptor_set_index, descriptor_range_index);

                binding_range_info->set_offset     = set;
                binding_range_info->binding_offset = binding_offset;
            }
        }

        auto sub_object_range_count = type_layout->getSubObjectRangeCount();
        for (auto i = 0u; i < sub_object_range_count; i++) {
            auto binding_range_index = type_layout->getSubObjectRangeBindingRangeIndex(i);
            auto binding_range_info = &binding_ranges[binding_range_index];
            auto slang_binding_type = type_layout->getBindingRangeType(binding_range_index);
            auto slang_leaf_type_layout = type_layout->getBindingRangeLeafTypeLayout(binding_range_index);

            auto sub_object_layout = std::unique_ptr<VulkanShaderObjectLayout>{};
            switch (slang_binding_type) {
                case slang::BindingType::ExistentialValue: {
                    if (auto pending_type_layout = slang_leaf_type_layout->getPendingDataTypeLayout()) {
                        sub_object_layout = create_element_layout(parent, session, pending_type_layout);
                    }
                    break;
                }
                default: {
                    auto var_layout = slang_leaf_type_layout->getElementVarLayout();
                    auto sub_type_layout = var_layout->getTypeLayout();
                    sub_object_layout = create_element_layout(parent, session, sub_type_layout);
                }
            }

            auto sub_object_range = &sub_object_ranges.emplace_back();
            sub_object_range->binding_range_index = binding_range_index;
            sub_object_range->layout = std::move(sub_object_layout);

            sub_object_range->offset = SubObjectRangeOffset{type_layout->getSubObjectRangeOffset(i)};
            sub_object_range->stride = SubObjectRangeStride{slang_leaf_type_layout};
            sub_object_range->pending_ordinary_data_offset = sub_object_range->offset.pending_ordinary_data;
            sub_object_range->pending_ordinary_data_stride = sub_object_range->stride.pending_ordinary_data;

            auto sub_object_layout_ptr = sub_object_range->layout.get();
            switch (slang_binding_type) {
                case slang::BindingType::ParameterBlock: {
                    child_descriptor_set_count += sub_object_layout_ptr->total_descriptor_set_count();
                    child_push_constant_range_count += sub_object_layout_ptr->total_push_constant_range_count();
                    break;
                }
                case slang::BindingType::ConstantBuffer: {
                    child_descriptor_set_count += sub_object_layout_ptr->child_descriptor_set_count;
                    total_binding_count += sub_object_layout_ptr->total_binding_count;
                    child_push_constant_range_count += sub_object_layout_ptr->total_push_constant_range_count();
                    break;
                }
                case slang::BindingType::ExistentialValue: {
                    if (sub_object_layout_ptr) {
                        child_descriptor_set_count += sub_object_layout_ptr->child_descriptor_set_count;
                        total_binding_count += sub_object_layout_ptr->total_binding_count;
                        child_push_constant_range_count += sub_object_layout_ptr->total_push_constant_range_count();

                        auto ordirary_data_end = sub_object_range->offset.pending_ordinary_data +
                            binding_range_info->count * sub_object_range->stride.pending_ordinary_data;
                        if (ordirary_data_end > total_ordinary_data_size) {
                            total_ordinary_data_size = ordirary_data_end;
                        }
                    }
                    break;
                }
                default: break;
            }
        }
    }

    auto VulkanShaderObjectLayout::set_element_type_layout(slang::TypeLayoutReflection* type_layout) -> bool
    {
        auto [layout, object_type] = unwrap_parameter_groups(type_layout);
        type_layout = layout;
        this->element_type_layout = type_layout;
        this->container_type = object_type;

        total_ordinary_data_size = type_layout->getSize();

        add_binding_ranges(type_layout);

        return true;
    }

    VulkanRootShaderObjectLayout::VulkanRootShaderObjectLayout(VulkanDevice* device, slang::IComponentType* program, slang::ProgramLayout* program_layout)
        : VulkanShaderObjectLayout(device, program->getSession(), nullptr)
        , program(program)
        , program_layout(program_layout)
    {}

    VulkanRootShaderObjectLayout::~VulkanRootShaderObjectLayout()
    {}

    auto VulkanRootShaderObjectLayout::add_global_params(slang::VariableLayoutReflection* global_layout) -> void
    {
        set_element_type_layout(global_layout->getTypeLayout());

        auto offset = BindingOffset{global_layout};

        add_descriptor_range_as_value(global_layout->getTypeLayout(), offset, VK_SHADER_STAGE_ALL);

        pending_data_offset = offset.pending;
    }

    auto VulkanRootShaderObjectLayout::add_entry_point(slang::EntryPointReflection* entry_point_layout) -> void
    {
        auto parent = get_device<VulkanDevice>();

        auto entry_point = &entry_point_infos.emplace_back();

        entry_point->layout = create_entry_point_layout(parent, session, entry_point_layout);
        entry_point->offset = BindingOffset{entry_point_layout->getVarLayout()};
        entry_point->offset.pending += pending_data_offset;

        add_descriptor_range_as_value(entry_point_layout->getTypeLayout(), entry_point->offset, entry_point->layout->stage_flag);

        child_descriptor_set_count += entry_point->layout->total_descriptor_set_count();
    }

    auto VulkanRootShaderObjectLayout::add_all_descriptor_sets() -> bool
    {
        add_all_descriptor_sets_recursive(this);

        for (auto& [entry_point, offset]: entry_point_infos) {
            add_all_descriptor_sets_recursive(entry_point.get());
        }

        return true;
    }

    auto VulkanRootShaderObjectLayout::add_all_descriptor_sets_recursive(VulkanShaderObjectLayout* layout) -> bool
    {
        for (auto& descriptor_set_info: layout->owned_descriptor_sets) {
            descriptor_set_layouts.emplace_back(descriptor_set_info.set_layout);
        }

        add_child_descriptor_set_recursive(layout);

        return true;
    }

    auto VulkanRootShaderObjectLayout::add_child_descriptor_set_recursive(VulkanShaderObjectLayout* layout) -> bool
    {
        for (auto& sub_object: layout->sub_object_ranges) {
            auto const& binding_range = layout->binding_ranges[sub_object.binding_range_index];
            switch (binding_range.binding_type) {
                case slang::BindingType::ParameterBlock: {
                    add_all_descriptor_sets_recursive(sub_object.layout.get());
                    break;
                }
                default: {
                    if (auto sub_object_layout = sub_object.layout.get()) {
                        add_child_descriptor_set_recursive(sub_object_layout);
                    }
                    break;
                }
            }
        }

        return true;
    }

    auto VulkanRootShaderObjectLayout::add_all_push_constant_ranges() -> bool
    {
        add_all_push_constant_ranges_recursive(this);

        for (auto& [entry_point, offset]: entry_point_infos) {
            add_child_push_constant_range_recursive(entry_point.get());
        }

        return true;
    }

    auto VulkanRootShaderObjectLayout::add_all_push_constant_ranges_recursive(VulkanShaderObjectLayout* layout) -> bool
    {
        for (auto& push_constant_range: layout->push_constant_ranges) {
            push_constant_range.offset = total_push_constant_size;
            total_push_constant_size += push_constant_range.size;

            all_push_constant_ranges.emplace_back(push_constant_range);
        }

        add_child_push_constant_range_recursive(layout);

        return true;
    }

    auto VulkanRootShaderObjectLayout::add_child_push_constant_range_recursive(VulkanShaderObjectLayout* layout) -> bool
    {
        for (auto& sub_object: layout->sub_object_ranges) {
            if (auto sub_object_layout = sub_object.layout.get()) {
                add_child_push_constant_range_recursive(sub_object_layout);
            }
        }

        return true;
    }

    inline namespace
    {
        auto write_texture_state(BindingDataBuilder* builder, VulkanTextureView* texture_view, EResourceStates state) -> void
        {
            auto binding_data = builder->binding_data;
        }
    }

    auto BindingDataBuilder::bind_as_root(RootShaderObject* root_object, VulkanRootShaderObjectLayout* specialized_layout) -> VulkanBindingData*
    {
        binding_data = allocator->allocate<VulkanBindingData>();
        binding_cache->binding_data.emplace_back(binding_data);

        binding_data->pipeline_layout = specialized_layout->pipeline_layout;
        binding_data->ranges = specialized_layout->all_push_constant_ranges;
        binding_data->push_constant_datas = allocator->allocate_array<std::span<std::byte>>(binding_data->ranges.size());

        auto offset = BindingOffset{};
        offset.pending = specialized_layout->pending_data_offset;

        bind_ordinary_data_buffer_if_needed(root_object, offset, specialized_layout);
        bind_as_value(root_object, offset, specialized_layout);

        for (auto i = 0u; i < specialized_layout->entry_point_infos.size(); i++) {
            auto entry_point = root_object->entry_points[i];
            auto const entry_point_info = &specialized_layout->entry_point_infos[i];
            auto const entry_point_layout = entry_point_info->layout.get();

            bind_as_push_constants(entry_point.get(), entry_point_info->offset, entry_point_layout);
        }

        binding_data->buffer_states = allocator->allocate_array<VulkanBindingData::BufferState>(buffer_states.size());
        std::memcpy(binding_data->buffer_states.data(), buffer_states.data(), buffer_states.size() * sizeof(VulkanBindingData::BufferState));
        binding_data->texture_states = allocator->allocate_array<VulkanBindingData::TextureState>(texture_states.size());
        std::memcpy(binding_data->texture_states.data(), texture_states.data(), texture_states.size() * sizeof(VulkanBindingData::TextureState));

        return binding_data;
    }

    auto BindingDataBuilder::bind_as_push_constants(ShaderObject* shader_object, BindingOffset const& in_offset, VulkanShaderObjectLayout* specialized_layout) -> bool
    {
        auto offset = in_offset;

        if (!shader_object->data.empty()) {
            auto push_constant_range_index = offset.push_constant_range++;

            auto const push_constant_range = &binding_data->ranges[push_constant_range_index];
            //TODO: Check overlap.
            binding_data->push_constant_datas[push_constant_range_index] = allocator->allocate_array<std::byte>(push_constant_range->size);
            std::memcpy(
                binding_data->push_constant_datas[push_constant_range_index].data(),
                shader_object->data.data(),
                shader_object->data.size()
            );

            for (auto& [buffer, required_state]: shader_object->pending_buffers) {
                buffer_states.emplace_back(cast<VulkanBuffer*>(buffer), required_state);
            }

            for (auto& [texture, required_state]: shader_object->pending_texture_views) {
                texture_states.emplace_back(cast<VulkanTextureView*>(texture), required_state);
            }
        }

        return true;
    }

    auto BindingDataBuilder::bind_as_value(ShaderObject* shader_object, BindingOffset const& offset, VulkanShaderObjectLayout* specialized_layout) -> bool
    {
        for (auto& binding_range_info: specialized_layout->binding_ranges) {
            auto range_offset = offset;
            range_offset.set += binding_range_info.set_offset;
            range_offset.binding += binding_range_info.binding_offset;

            auto slot_index = binding_range_info.slot_index;
            auto count = binding_range_info.count;

            switch (binding_range_info.binding_type) {
                case slang::BindingType::CombinedTextureSampler: // We don't support combined texture sampler.
                case slang::BindingType::RayTracingAccelerationStructure: // TODO:
                case slang::BindingType::TypedBuffer:                     // TODO:
                case slang::BindingType::MutableTypedBuffer:              // TODO:
                case slang::BindingType::VaryingInput:
                case slang::BindingType::VaryingOutput:
                case slang::BindingType::PushConstant:
                case slang::BindingType::ConstantBuffer:
                case slang::BindingType::ParameterBlock:
                case slang::BindingType::ExistentialValue: {
                    break;
                }
                case slang::BindingType::Texture:
                case slang::BindingType::MutableTexture: {
                    auto required_state = (
                        binding_range_info.binding_type == slang::BindingType::Texture ?
                        EResourceStates::sampled_texture :
                        EResourceStates::storage_write
                    );
                    for (auto i = 0u; i < count; i++) {
                        auto const slot = &shader_object->resource_slots[slot_index + i];
                        auto texture_view = pointer_cast<VulkanTextureView>(slot->resource).get();
                        if (texture_view) {
                            texture_states.emplace_back(texture_view, required_state);
                        }
                    }

                    break;
                }
                case slang::BindingType::Sampler: {
                    break;
                }
                case slang::BindingType::RawBuffer:
                case slang::BindingType::MutableRawBuffer: {
                    auto required_state = (
                        binding_range_info.binding_type == slang::BindingType::RawBuffer ?
                        EResourceStates::uniform_read :
                        EResourceStates::storage_write
                    );
                    for (auto i = 0u; i < count; i++) {
                        auto const slot = &shader_object->resource_slots[slot_index + i];
                        auto buffer = pointer_cast<VulkanBuffer>(slot->resource).get();
                        if (buffer) {
                            buffer_states.emplace_back(buffer, required_state);
                        }
                    }

                    break;
                }
                default: CNE_UNREACHABLE();
            }
        }

        for (auto& sub_object_range: specialized_layout->sub_object_ranges) {
            auto const binding_range_info = &specialized_layout->binding_ranges[sub_object_range.binding_range_index];
            auto count = binding_range_info->count;
            auto sub_object_index = binding_range_info->sub_object_index;
            auto sub_object_layout = sub_object_range.layout.get();

            auto range_offset = offset;
            range_offset += sub_object_range.offset;

            auto range_stride = sub_object_range.stride;

            switch (binding_range_info->binding_type) {
                case slang::BindingType::ConstantBuffer: {
                    CNE_ASSERT_WITH(false, "Constant buffer is not supported.");
                    break;
                }
                case slang::BindingType::ParameterBlock: {
                    auto obj_offset = range_offset;
                    for (auto i = 0u; i < count; i++) {
                        auto sub_object = shader_object->objects[sub_object_index + i];

                        // TODO:
                    }

                    break;
                }
                case slang::BindingType::ExistentialValue: {
                    if (sub_object_layout) {
                        auto object_offset = range_offset.pending;
                        auto object_stride = range_stride.pending;
                        for (auto i = 0u; i < count; i++) {
                            auto sub_object = shader_object->objects[sub_object_index + i];
                            bind_as_value(sub_object.get(), BindingOffset{object_offset}, sub_object_layout);
                            object_offset += object_stride;
                        }
                    }

                    break;
                }
                case slang::BindingType::RawBuffer:
                case slang::BindingType::MutableRawBuffer: {
                    break;
                }
                case slang::BindingType::PushConstant: {
                    bind_as_push_constants(shader_object, range_offset, sub_object_layout);

                    break;
                }
                default: CNE_UNREACHABLE();
            }
        }

        return true;
    }

    auto BindingDataBuilder::bind_ordinary_data_buffer_if_needed(ShaderObject* shader_object, BindingOffset& offset, VulkanShaderObjectLayout* specialized_layout) -> bool
    {
        // TODO:
        return true;
    }
}
