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

    auto VulkanPipelineManager::create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo const* info) -> std::shared_ptr<VulkanGraphicsPipeline>
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

    auto VulkanPipelineManager::create_mesh_pipeline(std::string_view name, MeshPipelineCreateInfo const* info) -> std::shared_ptr<VulkanMeshPipeline>
    {
        auto hash = XXH64(info, sizeof(MeshPipelineCreateInfo), 0);

        std::lock_guard<std::mutex> lock(mutex);

        auto it = mesh_pipelines.find(hash);

        if (it == mesh_pipelines.end()) {
            it = mesh_pipelines.emplace(hash, std::make_shared<VulkanMeshPipeline>(parent, info)).first;

            set_resource_name(parent->device, VK_OBJECT_TYPE_PIPELINE, (uint64_t) it->second->pipeline, name);
        }

        return it->second;
    }

    auto VulkanPipelineManager::create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo const* info) -> std::shared_ptr<VulkanComputePipeline>
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

    auto VulkanPipelineManager::create_shader_module(ShaderModuleCreateInfo const* info) -> VulkanShaderModule*
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

        auto max_resource_descriptors = 0u;
        for (auto& binding: bindings) {
            binding.count = std::clamp(binding.count, 1u, binding.limit);
            if (binding.type != VK_DESCRIPTOR_TYPE_SAMPLER) {
                max_resource_descriptors += binding.count;
            }
        }

        auto descriptor_sizes = std::vector<size_t>{
            descriptor_buffer_properties->samplerDescriptorSize,
            descriptor_buffer_properties->sampledImageDescriptorSize,
            descriptor_buffer_properties->storageImageDescriptorSize,
            descriptor_buffer_properties->uniformBufferDescriptorSize,
            descriptor_buffer_properties->storageBufferDescriptorSize,
        };
        stride = *std::max_element(descriptor_sizes.begin(), descriptor_sizes.end());

        auto create_descriptor_heap = [&](VkDescriptorSetLayoutCreateInfo* layout_info, uint32_t max_descriptors, bool is_sampler) -> std::unique_ptr<DescriptorHeap> {
            auto descriptor_heap = std::make_unique<DescriptorHeap>();
            descriptor_heap->max_descriptors = max_descriptors;

            auto result_layout_create = vkCreateDescriptorSetLayout(device->device, layout_info, parent->allocation_callbacks, &descriptor_heap->descriptor_set_layout);
            CNE_ASSERT_WITH(result_layout_create == VK_SUCCESS, std::format("Failed to create descriptor set layout: {}", vk_error_to_string(result_layout_create)));
            set_resource_name(
                device->device,
                VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                (uint64_t) descriptor_heap->descriptor_set_layout,
                is_sampler ? "Bindless sampler descriptor set layout" : "Bindless resource descriptor set layout"
            );

            auto set_layout_size = 0zu;
            vkGetDescriptorSetLayoutSizeEXT(parent->device, descriptor_heap->descriptor_set_layout, &set_layout_size);
            CNE_TRACE("{} size per {} descriptor", set_layout_size / max_descriptors, is_sampler ? "sampler" : "resource");
            set_layout_size = aligned_size(set_layout_size, descriptor_buffer_properties->descriptorBufferOffsetAlignment);

            auto buffer_ci = VkBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            buffer_ci.size        = set_layout_size;
            buffer_ci.usage       = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | (is_sampler ? VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT : VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT);
            buffer_ci.flags       = 0;
            buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            auto allocation_create_info = VmaAllocationCreateInfo{};
            allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO;
            allocation_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            auto allocation_info = VmaAllocationInfo{};
            auto result = vmaCreateBuffer(
                parent->allocator,
                &buffer_ci, &allocation_create_info,
                &descriptor_heap->descriptor_buffer, &descriptor_heap->descriptor_buffer_allocation, &allocation_info
            );
            CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create buffer: {}", vk_error_to_string(result)));
            set_resource_name(
                parent->device,
                VK_OBJECT_TYPE_BUFFER,
                (uint64_t) descriptor_heap->descriptor_buffer,
                is_sampler ? "Bindless sampler descriptor buffer" : "Bindless resource descriptor buffer"
            );

            descriptor_heap->descriptor_buffer_mapped_ptr = allocation_info.pMappedData;
            CNE_TRACE("Bindless descriptor buffer mapped at: {0}", descriptor_heap->descriptor_buffer_mapped_ptr);
            auto device_address_info = VkBufferDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, descriptor_heap->descriptor_buffer};
            descriptor_heap->descriptor_buffer_address = vkGetBufferDeviceAddress(parent->device, &device_address_info);
            CNE_TRACE("Bindless descriptor buffer address: {0}", descriptor_heap->descriptor_buffer_address);

            return descriptor_heap;
        };

        auto binding_flag = VkDescriptorBindingFlags{VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
        auto binding_flags_ci = VkDescriptorSetLayoutBindingFlagsCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        binding_flags_ci.bindingCount  = 1;
        binding_flags_ci.pBindingFlags = &binding_flag;

        auto mutable_types = std::vector<VkDescriptorType>{
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        };
        auto mutable_descriptor_type_list = VkMutableDescriptorTypeListEXT{
            .descriptorTypeCount = (uint32_t) mutable_types.size(),
            .pDescriptorTypes    = mutable_types.data()
        };
        auto mutable_descriptor_type_ci = VkMutableDescriptorTypeCreateInfoEXT{VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT};
        mutable_descriptor_type_ci.mutableDescriptorTypeListCount  = 1;
        mutable_descriptor_type_ci.pMutableDescriptorTypeLists     = &mutable_descriptor_type_list;
        mutable_descriptor_type_ci.pNext                           = &binding_flags_ci;

        auto layout_binding = VkDescriptorSetLayoutBinding{
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
            .descriptorCount = max_resource_descriptors,
            .stageFlags      = VK_SHADER_STAGE_ALL,
        };
        auto set_layout_ci = VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        set_layout_ci.bindingCount = 1;
        set_layout_ci.pBindings    = &layout_binding;
        set_layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        set_layout_ci.pNext        = &mutable_descriptor_type_ci;

        resource_heap = create_descriptor_heap(&set_layout_ci, max_resource_descriptors, false);

        auto max_sampler_descriptors = 32;
        layout_binding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        layout_binding.descriptorCount = max_sampler_descriptors;
        set_layout_ci.pNext = &binding_flags_ci;
        sampler_heap = create_descriptor_heap(&set_layout_ci, max_sampler_descriptors, true);
    }

    VulkanBindlessManager::~VulkanBindlessManager()
    {
        vkDestroyDescriptorSetLayout(parent->device, resource_heap->descriptor_set_layout, parent->allocation_callbacks);
        vkDestroyDescriptorSetLayout(parent->device, sampler_heap->descriptor_set_layout, parent->allocation_callbacks);
        vmaDestroyBuffer(parent->allocator, resource_heap->descriptor_buffer, resource_heap->descriptor_buffer_allocation);
        vmaDestroyBuffer(parent->allocator, sampler_heap->descriptor_buffer, sampler_heap->descriptor_buffer_allocation);
    }

    auto VulkanBindlessManager::register_sampler(VkSampler sampler) -> BindlessIndex
    {
        auto bindless_index = sampler_heap->request_index();

        auto get_info = VkDescriptorGetInfoEXT{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};
        get_info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        get_info.data.pSampler = &sampler;

        auto binding = &bindings[(size_t) EDescriptorType::sampler];
        auto descriptor_address = (std::byte*) sampler_heap->descriptor_buffer_mapped_ptr + binding->descriptor_size * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_buffer(EDescriptorType type, VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize range) -> BindlessIndex
    {
        auto bindless_index = resource_heap->request_index();

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
        auto descriptor_address = (std::byte*) resource_heap->descriptor_buffer_mapped_ptr + stride * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::register_texture(EDescriptorType type, VkImageView image_view, VkImageLayout layout) -> BindlessIndex
    {
        auto bindless_index = resource_heap->request_index();

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
        auto descriptor_address = (std::byte*) resource_heap->descriptor_buffer_mapped_ptr + stride * bindless_index;
        vkGetDescriptorEXT(parent->device, &get_info, binding->descriptor_size, descriptor_address);

        return bindless_index;
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        resource_heap->free_index(index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanTexture* fallback_texture) -> void
    {
        // TODO: fall back

        resource_heap->free_index(index);
    }

    auto VulkanBindlessManager::free_UAV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        resource_heap->free_index(index);
    }

    auto VulkanBindlessManager::free_SRV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void
    {
        // TODO: fall back

        resource_heap->free_index(index);
    }

    auto VulkanBindlessManager::DescriptorHeap::request_index() -> BindlessIndex
    {
        std::lock_guard<std::mutex> lock(mutex);

        auto index = 0u;

        if (free_indices.empty()) {
            index = next_usable_index++;

            if (next_usable_index > max_descriptors) {
                CNE_ERROR("Too much bindless resources requested.");
            }
        } else {
            index = free_indices.front();
            free_indices.pop();
        }

        return index;
    }

    auto VulkanBindlessManager::DescriptorHeap::free_index(BindlessIndex index) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);

        free_indices.push(index);
    }
}
