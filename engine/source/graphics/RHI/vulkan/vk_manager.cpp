#include "vk_tool.hpp"
#include "vk_RHI.hpp"

#include <core/hash.hpp>
#include <math/tool.hpp>

#include <xxhash.h>

namespace cannele::inline graphics::rhi::vk
{
    VulkanLayoutManager::VulkanLayoutManager(VulkanDevice* device)
        : VulkanDeviceChild<VulkanLayoutManager>(device)
    {}

    VulkanLayoutManager::~VulkanLayoutManager()
    {
        for (auto& [_, layout] : descriptor_set_layouts) {
            vkDestroyDescriptorSetLayout(parent->device, layout, nullptr);
        }

        for (auto& [_, layout] : pipeline_layouts) {
            vkDestroyPipelineLayout(parent->device, layout, nullptr);
        }
    }

    auto VulkanLayoutManager::create_descriptor_set_layout(std::span<VkDescriptorSetLayoutBinding> bindings) -> VkDescriptorSetLayout
    {
        auto descriptor_set_layout_create_info = VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor_set_layout_create_info.bindingCount = bindings.size();
        descriptor_set_layout_create_info.pBindings    = bindings.data();

        auto hash = XXH64(bindings.data(), bindings.size() * sizeof(VkDescriptorSetLayoutBinding), 0);

        auto it = descriptor_set_layouts.find(hash);

        if (it == descriptor_set_layouts.end()) {
            auto layout = VkDescriptorSetLayout{VK_NULL_HANDLE};
            auto result = vkCreateDescriptorSetLayout(parent->device, &descriptor_set_layout_create_info, nullptr, &layout);
            CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create descriptor set layout: {}", vk_error_to_string(result)));

            it = descriptor_set_layouts.emplace(hash, layout).first;
        }

        return it->second;
    }

    auto VulkanLayoutManager::create_pipeline_layout(std::span<VkDescriptorSetLayout> set_layouts, std::span<VkPushConstantRange> push_constant_ranges) -> VkPipelineLayout
    {
        auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout_create_info.setLayoutCount         = set_layouts.size();
        pipeline_layout_create_info.pSetLayouts            = set_layouts.data();
        pipeline_layout_create_info.pushConstantRangeCount = push_constant_ranges.size();
        pipeline_layout_create_info.pPushConstantRanges    = push_constant_ranges.data();

        auto hash = hash_combine(pipeline_layout_create_info.setLayoutCount, pipeline_layout_create_info.pushConstantRangeCount);
        hash = hash_combine(hash, XXH64(set_layouts.data(), set_layouts.size() * sizeof(VkDescriptorSetLayout), 0));
        hash = hash_combine(hash, XXH64(push_constant_ranges.data(), push_constant_ranges.size() * sizeof(VkPushConstantRange), 0));

        auto it = pipeline_layouts.find(hash);

        if (it == pipeline_layouts.end()) {
            auto layout = VkPipelineLayout{VK_NULL_HANDLE};
            auto result = vkCreatePipelineLayout(parent->device, &pipeline_layout_create_info, nullptr, &layout);
            CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create pipeline layout: {}", vk_error_to_string(result)));

            it = pipeline_layouts.emplace(hash, layout).first;
        }

        return it->second;
    }

    VulkanPipelineManager::VulkanPipelineManager(VulkanDevice* device)
        : VulkanDeviceChild<VulkanPipelineManager>(device)
    {}

    VulkanPipelineManager::~VulkanPipelineManager()
    {
    }

    auto VulkanPipelineManager::create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo* info) -> RefCountPtr<VulkanGraphicsPipeline>
    {
        auto hash = XXH64(info, sizeof(GraphicsPipelineCreateInfo), 0);

        std::lock_guard<std::mutex> lock(mutex);

        auto it = graphics_pipelines.find(hash);

        if (it == graphics_pipelines.end()) {
            it = graphics_pipelines.emplace(hash, std::make_shared<VulkanGraphicsPipeline>(parent, info)).first;

            set_resource_name(parent->device, VK_OBJECT_TYPE_PIPELINE, (uint64_t) it->second->pipeline, name);
        }

        return it->second;
    }

    auto VulkanPipelineManager::create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo* info) -> RefCountPtr<VulkanComputePipeline>
    {
        auto hash = XXH64(info, sizeof(ComputePipelineCreateInfo), 0);

        std::lock_guard<std::mutex> lock(mutex);

        auto it = compute_pipelines.find(hash);

        if (it == compute_pipelines.end()) {
            it = compute_pipelines.emplace(hash, std::make_shared<VulkanComputePipeline>(parent, info)).first;

            set_resource_name(parent->device, VK_OBJECT_TYPE_PIPELINE, (uint64_t) it->second->pipeline, name);
        }

        return it->second;
    }

    auto VulkanPipelineManager::create_shader_module(ShaderModuleCreateInfo* info) -> VulkanShaderModule*
    {
        auto hash = XXH64(info, sizeof(ShaderModuleCreateInfo), 0);

        std::lock_guard<std::mutex> lock(mutex);

        auto it = shader_modules.find(hash);

        if (it == shader_modules.end()) {
            // it = shader_modules.emplace(hash, device->create_shader_module(info)).first;
        }

        return it->second.get();
    }

    VulkanBindlessManager::VulkanBindlessManager(VulkanDevice* device)
        : VulkanDeviceChild<VulkanBindlessManager>(device)
    {
        // TODO: Provide limits by RHI.
        auto descriptor_indexing_properties = &device->physical_device_properties.descriptor_indexing_properties;

        bindings[(uint32_t) EDescriptorResourceType::storage_buffer ] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindStorageBuffers};
        bindings[(uint32_t) EDescriptorResourceType::uniform_buffer ] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindUniformBuffers};
        bindings[(uint32_t) EDescriptorResourceType::sampled_texture] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindSampledImages};
        bindings[(uint32_t) EDescriptorResourceType::storage_texture] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindStorageImages};
        bindings[(uint32_t) EDescriptorResourceType::sampler        ] = {VK_DESCRIPTOR_TYPE_SAMPLER,        10000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindSamplers};
        CNE_INFO("Bindless storage buffer limits: {}",  bindings[(uint32_t) EDescriptorResourceType::storage_buffer].limit);
        CNE_INFO("Bindless uniform buffer limits: {}",  bindings[(uint32_t) EDescriptorResourceType::uniform_buffer].limit);
        CNE_INFO("Bindless sampled texture limits: {}", bindings[(uint32_t) EDescriptorResourceType::sampled_texture].limit);
        CNE_INFO("Bindless storage texture limits: {}", bindings[(uint32_t) EDescriptorResourceType::storage_texture].limit);
        CNE_INFO("Bindless sampler limits: {}",         bindings[(uint32_t) EDescriptorResourceType::sampler].limit);

        for (auto& binding: bindings) {
            binding.count = std::clamp(binding.count, 1u, binding.limit);
        }

        std::fill(next_usable_index.begin(), next_usable_index.end(), 0);

        auto layout_bindings = std::vector<VkDescriptorSetLayoutBinding>{};
        auto binding_flags = std::vector<VkDescriptorBindingFlags>{};
        for (auto i = 0u; i < binding_count; i++) {
            auto layout_binding = &layout_bindings.emplace_back();
            layout_binding->binding         = i;
            layout_binding->descriptorType  = bindings[i].type;
            layout_binding->descriptorCount = bindings[i].count;
            layout_binding->stageFlags      = VK_SHADER_STAGE_ALL;

            binding_flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
        }

        auto binding_flags_ci = VkDescriptorSetLayoutBindingFlagsCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        binding_flags_ci.bindingCount  = (uint32_t) binding_flags.size();
        binding_flags_ci.pBindingFlags = binding_flags.data();

        auto set_layout_ci = VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set_layout_ci.bindingCount = (uint32_t) layout_bindings.size();
        set_layout_ci.pBindings    = layout_bindings.data();
        set_layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
        set_layout_ci.pNext        = &binding_flags_ci;

        auto result_layout_create = vkCreateDescriptorSetLayout(device->device, &set_layout_ci, nullptr, &descriptor_set_layout);
        CNE_ASSERT_WITH(result_layout_create == VK_SUCCESS, std::format("Failed to create descriptor set layout: {}", vk_error_to_string(result_layout_create)));
        set_resource_name(device->device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t) descriptor_set_layout, "Bindless descriptor set layout");

        auto pool_sizes = std::vector<VkDescriptorPoolSize>{};
        pool_sizes.reserve(binding_count);
        for (auto i = 0u; i < binding_count; i++) {
            pool_sizes.emplace_back(bindings[i].type, bindings[i].count);
        }

        auto pool_ci = VkDescriptorPoolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool_ci.poolSizeCount = (uint32_t) pool_sizes.size();
        pool_ci.pPoolSizes    = pool_sizes.data();
        pool_ci.maxSets       = 1;
        pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        auto result_pool_create = vkCreateDescriptorPool(device->device, &pool_ci, nullptr, &descriptor_pool);
        CNE_ASSERT_WITH(result_pool_create == VK_SUCCESS, std::format("Failed to create descriptor pool: {}", vk_error_to_string(result_pool_create)));
        set_resource_name(device->device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t) descriptor_pool, "Bindless descriptor pool");

        auto allocate_info = VkDescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate_info.descriptorPool     = descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts        = &descriptor_set_layout;

        auto result_allocate = vkAllocateDescriptorSets(device->device, &allocate_info, &descriptor_set);
        CNE_ASSERT_WITH(result_allocate == VK_SUCCESS, std::format("Failed to allocate descriptor set: {}", vk_error_to_string(result_allocate)));
        set_resource_name(device->device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t) descriptor_set, "Bindless descriptor set");
    }

    VulkanBindlessManager::~VulkanBindlessManager()
    {
        vkDestroyDescriptorSetLayout(parent->device, descriptor_set_layout, nullptr);
        vkDestroyDescriptorPool(parent->device, descriptor_pool, nullptr);
    }

    auto VulkanBindlessManager::register_sampler(VkSampler sampler) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorResourceType::sampler);

        auto image_info = VkDescriptorImageInfo{};
        image_info.sampler     = sampler;
        image_info.imageView   = VK_NULL_HANDLE;
        image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) EDescriptorResourceType::sampler;
        write.dstArrayElement = bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo      = &image_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_SRV(VulkanTexture* texture, uint32_t mip_level, uint32_t array_layer) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorResourceType::sampled_texture);

        auto image_view_ci = texture->image_view_create_info(mip_level, array_layer);
        auto image_view = texture->texture_view(&image_view_ci);

        auto image_info = VkDescriptorImageInfo{};
        image_info.sampler     = VK_NULL_HANDLE;
        image_info.imageView   = image_view->image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) EDescriptorResourceType::sampled_texture;
        write.dstArrayElement = bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo      = &image_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_texture_view(VulkanTextureView* view) -> void
    {
        view->bindless_index = request_index(view->type);

        auto image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        auto descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        switch (view->type) {
            case EDescriptorResourceType::sampled_texture: {
                image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;
            }
            case EDescriptorResourceType::storage_texture: {
                image_layout = VK_IMAGE_LAYOUT_GENERAL;
                descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                break;
            }
            default: CNE_UNREACHABLE();
        }

        auto image_info = VkDescriptorImageInfo{};
        image_info.sampler     = VK_NULL_HANDLE;
        image_info.imageView   = view->image_view;
        image_info.imageLayout = image_layout;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) view->type;
        write.dstArrayElement = view->bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = descriptor_type;
        write.pImageInfo      = &image_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);
    }

    auto VulkanBindlessManager::register_UAV(VulkanTexture* texture, uint32_t mip_level, uint32_t array_layer) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorResourceType::storage_texture);

        auto image_view_ci = texture->image_view_create_info(mip_level, array_layer);
        auto image_view = texture->texture_view(&image_view_ci);

        auto image_info = VkDescriptorImageInfo{};
        image_info.sampler     = VK_NULL_HANDLE;
        image_info.imageView   = image_view->image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) EDescriptorResourceType::storage_texture;
        write.dstArrayElement = bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo      = &image_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_SRV(VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize range) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorResourceType::uniform_buffer);

        auto buffer_info = VkDescriptorBufferInfo{};
        buffer_info.buffer = buffer->buffer;
        buffer_info.offset = offset;
        buffer_info.range  = range;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) EDescriptorResourceType::uniform_buffer;
        write.dstArrayElement = bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo     = &buffer_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_UAV(VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize range) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorResourceType::storage_buffer);

        auto buffer_info = VkDescriptorBufferInfo{};
        buffer_info.buffer = buffer->buffer;
        buffer_info.offset = offset;
        buffer_info.range  = range;

        auto write = VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet          = descriptor_set;
        write.dstBinding      = (uint32_t) EDescriptorResourceType::storage_buffer;
        write.dstArrayElement = bindless_index;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo     = &buffer_info;

        vkUpdateDescriptorSets(parent->device, 1, &write, 0, nullptr);

        return bindless_index;
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        free_index(EDescriptorResourceType::sampled_texture, index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        free_index(EDescriptorResourceType::storage_texture, index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        free_index(EDescriptorResourceType::storage_buffer, index);
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        free_index(EDescriptorResourceType::uniform_buffer, index);
    }

    auto VulkanBindlessManager::request_index(EDescriptorResourceType type) -> BindlessIndex
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto type_idx = (uint32_t) type;
        auto binding = &bindings[type_idx];

        auto index = 0u;

        auto free_indices = &this->free_indices[type_idx];
        if (free_indices->empty()) {
            index = next_usable_index[type_idx]++;

            if (next_usable_index[type_idx] > binding->count) {
                CNE_ERROR("Too much bindless resources requested.");
            }
        } else {
            index = free_indices->front();
            free_indices->pop();
        }

        return index;
    }

    auto VulkanBindlessManager::free_index(EDescriptorResourceType type, BindlessIndex index) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto type_idx = (uint32_t) type;
        auto free_indices = &this->free_indices[type_idx];
        free_indices->push(index);
    }
}
