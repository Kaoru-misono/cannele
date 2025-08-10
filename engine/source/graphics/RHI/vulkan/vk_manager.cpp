#include "vk_tool.hpp"
#include "vk_RHI.hpp"

#include <core/hash.hpp>
#include <core/aligned.hpp>
#include <math/tool.hpp>

#include <xxhash.h>
#include <ranges>

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        static constexpr auto descriptor_type_map = std::array<VkDescriptorType, (size_t) EDescriptorType::last>{
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_SAMPLER,
        };
    }

    VulkanLayoutManager::VulkanLayoutManager(VulkanDevice* device)
        : VulkanDeviceChild<VulkanLayoutManager>(device)
    {}

    VulkanLayoutManager::~VulkanLayoutManager()
    {
        for (auto& [_, layout] : descriptor_set_layouts) {
            vkDestroyDescriptorSetLayout(parent->device, layout, parent->allocation_callbacks);
        }

        for (auto& [_, layout] : pipeline_layouts) {
            vkDestroyPipelineLayout(parent->device, layout, parent->allocation_callbacks);
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
            auto result = vkCreateDescriptorSetLayout(parent->device, &descriptor_set_layout_create_info, parent->allocation_callbacks, &layout);
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
            auto result = vkCreatePipelineLayout(parent->device, &pipeline_layout_create_info, parent->allocation_callbacks, &layout);
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
        auto descriptor_buffer_properties = &device->physical_device_properties.descriptor_buffer_properties;
        bindings[(uint32_t) EDescriptorType::storage_buffer ] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindStorageBuffers, descriptor_buffer_properties->storageBufferDescriptorSize};
        bindings[(uint32_t) EDescriptorType::uniform_buffer ] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindUniformBuffers, descriptor_buffer_properties->uniformBufferDescriptorSize};
        bindings[(uint32_t) EDescriptorType::sampled_texture] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindSampledImages,  descriptor_buffer_properties->sampledImageDescriptorSize};
        bindings[(uint32_t) EDescriptorType::storage_texture] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  50000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindStorageImages,  descriptor_buffer_properties->storageBufferDescriptorSize};
        bindings[(uint32_t) EDescriptorType::sampler        ] = {VK_DESCRIPTOR_TYPE_SAMPLER,        10000u, descriptor_indexing_properties->maxDescriptorSetUpdateAfterBindSamplers,       descriptor_buffer_properties->samplerDescriptorSize};
        CNE_INFO("Bindless storage buffer limits: {}",  bindings[(uint32_t) EDescriptorType::storage_buffer].limit);
        CNE_INFO("Bindless uniform buffer limits: {}",  bindings[(uint32_t) EDescriptorType::uniform_buffer].limit);
        CNE_INFO("Bindless sampled texture limits: {}", bindings[(uint32_t) EDescriptorType::sampled_texture].limit);
        CNE_INFO("Bindless storage texture limits: {}", bindings[(uint32_t) EDescriptorType::storage_texture].limit);
        CNE_INFO("Bindless sampler limits: {}",         bindings[(uint32_t) EDescriptorType::sampler].limit);
        CNE_INFO("Descriptor Buffer Offset Alignment: {}", descriptor_buffer_properties->descriptorBufferOffsetAlignment);
        CNE_INFO("Descriptor Buffer Max Bindings: {}", descriptor_buffer_properties->maxDescriptorBufferBindings);
        CNE_INFO("Descriptor Buffer Uniform Buffer Descriptor Size: {}", descriptor_buffer_properties->uniformBufferDescriptorSize);
        CNE_INFO("Descriptor Buffer Storage Buffer Descriptor Size: {}", descriptor_buffer_properties->storageBufferDescriptorSize);
        CNE_INFO("Descriptor Buffer Storage Image Descriptor Size: {}", descriptor_buffer_properties->storageImageDescriptorSize);
        CNE_INFO("Descriptor Buffer Sampled Image Descriptor Size: {}", descriptor_buffer_properties->sampledImageDescriptorSize);
        CNE_INFO("Descriptor Buffer Sampler Descriptor Size: {}", descriptor_buffer_properties->samplerDescriptorSize);
        CNE_INFO("Descriptor Buffer Acceleration Structure Descriptor Size: {}", descriptor_buffer_properties->accelerationStructureDescriptorSize);

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

            binding_flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        }
        CNE_TRACE("{}, {}", __FILE__, __LINE__);

        auto binding_flags_ci = VkDescriptorSetLayoutBindingFlagsCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        binding_flags_ci.bindingCount  = (uint32_t) binding_flags.size();
        binding_flags_ci.pBindingFlags = binding_flags.data();

        CNE_TRACE("{}, {}", __FILE__, __LINE__);
        auto set_layout_ci = VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set_layout_ci.bindingCount = (uint32_t) layout_bindings.size();
        set_layout_ci.pBindings    = layout_bindings.data();
        set_layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        set_layout_ci.pNext        = &binding_flags_ci;

        CNE_TRACE("{}, {}", __FILE__, __LINE__);
        auto result_layout_create = vkCreateDescriptorSetLayout(device->device, &set_layout_ci, parent->allocation_callbacks, &descriptor_set_layout);
        CNE_ASSERT_WITH(result_layout_create == VK_SUCCESS, std::format("Failed to create descriptor set layout: {}", vk_error_to_string(result_layout_create)));
        set_resource_name(device->device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t) descriptor_set_layout, "Bindless descriptor set layout");

        CNE_TRACE("{}, {}", __FILE__, __LINE__);
        auto set_layout_size = 0zu;
        vkGetDescriptorSetLayoutSizeEXT(parent->device, descriptor_set_layout, &set_layout_size);

        set_layout_size = aligned_size(set_layout_size, descriptor_buffer_properties->descriptorBufferOffsetAlignment);

        for (auto i: std::views::iota(0u, bindings.size())) {
            CNE_TRACE("{}, {}", __FILE__, __LINE__);
            vkGetDescriptorSetLayoutBindingOffsetEXT(parent->device, descriptor_set_layout, i, &bindings[i].offset);
            CNE_INFO("Bindless binding {} offset: {}", i, bindings[i].offset);
        }

        auto buffer_ci = VkBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        buffer_ci.size        = set_layout_size;
        buffer_ci.usage       = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
        buffer_ci.flags       = 0;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        auto allocation_create_info = VmaAllocationCreateInfo{};
        allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        auto allocation_info = VmaAllocationInfo{};
        auto result = vmaCreateBuffer(
            parent->allocator,
            &buffer_ci, &allocation_create_info,
            &descriptor_buffer, &descriptor_buffer_allocation, &allocation_info
        );
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create buffer: {}", vk_error_to_string(result)));
        set_resource_name(parent->device, VK_OBJECT_TYPE_BUFFER, (uint64_t) descriptor_buffer, "Bindless descriptor buffer");

        descriptor_buffer_mapped_ptr = allocation_info.pMappedData;
        CNE_INFO("Bindless descriptor buffer mapped at: {}", descriptor_buffer_mapped_ptr);
        auto device_address_info = VkBufferDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, descriptor_buffer};
        descriptor_buffer_address = vkGetBufferDeviceAddress(parent->device, &device_address_info);
        CNE_INFO("Bindless descriptor buffer device address: {}", descriptor_buffer_address);
    }

    VulkanBindlessManager::~VulkanBindlessManager()
    {
        vkDestroyDescriptorSetLayout(parent->device, descriptor_set_layout, parent->allocation_callbacks);
        vmaDestroyBuffer(parent->allocator, descriptor_buffer, descriptor_buffer_allocation);
    }

    auto VulkanBindlessManager::register_sampler(VkSampler sampler) -> BindlessIndex
    {
        auto bindless_index = request_index(EDescriptorType::sampler);

        auto get_info = VkDescriptorGetInfoEXT{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        get_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        get_info.data.pSampler = &sampler;

        auto binding = &bindings[(size_t) EDescriptorType::sampler];
        auto descriptor_address = (std::byte*) descriptor_buffer_mapped_ptr + binding->offset + binding->descriptor_size * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_buffer(EDescriptorType type, VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize range) -> BindlessIndex
    {
        auto bindless_index = request_index(type);

        auto buffer_address_info = VkDescriptorAddressInfoEXT{VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT};
        buffer_address_info.address = buffer->device_address + offset;
        buffer_address_info.range   = range;
        buffer_address_info.format  = VK_FORMAT_UNDEFINED;

        auto get_info = VkDescriptorGetInfoEXT{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        get_info.type = descriptor_type_map[(size_t) type];
        switch (type) {
            case EDescriptorType::storage_buffer: {
                get_info.data.pStorageBuffer = &buffer_address_info;
                break;
            }
            case EDescriptorType::uniform_buffer: {
                get_info.data.pUniformBuffer = &buffer_address_info;
                break;
            }
            default: CNE_UNREACHABLE();
        }

        auto binding = &bindings[(size_t) type];
        auto descriptor_address = (std::byte*) descriptor_buffer_mapped_ptr + binding->offset + binding->descriptor_size * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_texture(EDescriptorType type, VkImageView image_view, VkImageLayout layout) -> BindlessIndex
    {
        auto bindless_index = request_index(type);

        auto image_info = VkDescriptorImageInfo{};
        image_info.sampler     = VK_NULL_HANDLE;
        image_info.imageView   = image_view;
        image_info.imageLayout = layout;

        auto get_info = VkDescriptorGetInfoEXT{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        get_info.type = descriptor_type_map[(size_t) type];
        switch (type) {
            case EDescriptorType::sampled_texture: {
                get_info.data.pSampledImage = &image_info;
                break;
            }
            case EDescriptorType::storage_texture: {
                get_info.data.pStorageImage = &image_info;
                break;
            }
            default: CNE_UNREACHABLE();
        }

        auto binding = &bindings[(size_t) type];
        auto descriptor_address = (std::byte*) descriptor_buffer_mapped_ptr + binding->offset + binding->descriptor_size * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        free_index(EDescriptorType::sampled_texture, index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        free_index(EDescriptorType::storage_texture, index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        free_index(EDescriptorType::storage_buffer, index);
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        free_index(EDescriptorType::uniform_buffer, index);
    }

    auto VulkanBindlessManager::request_index(EDescriptorType type) -> BindlessIndex
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

    auto VulkanBindlessManager::free_index(EDescriptorType type, BindlessIndex index) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto type_idx = (uint32_t) type;
        auto free_indices = &this->free_indices[type_idx];
        free_indices->push(index);
    }
}
