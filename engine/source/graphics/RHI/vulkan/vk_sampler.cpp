#include "vk_RHI.hpp"
#include "vk_tool.hpp"

namespace cannele::inline graphics::rhi::vk
{

    auto VulkanDevice::create_sampler(std::string_view name, SamplerCreateInfo* info) -> SamplerHandle
    {
        auto hash = XXH64(info, sizeof(SamplerCreateInfo), 0);

        auto it = samplers.find(hash);
        if (it != samplers.end()) {
            return it->second;
        }

        it = samplers.emplace(hash, std::make_shared<VulkanSampler>(this, info)).first;

        set_resource_name(device, VK_OBJECT_TYPE_SAMPLER, (uint64_t) it->second->sampler, name);

        return it->second;
    }

    VulkanSampler::VulkanSampler(VulkanDevice* device, SamplerCreateInfo* info)
        : VulkanDeviceChild<VulkanSampler>(device)
        , info(*info)
    {
        auto sampler_info = VkSamplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler_info.minFilter               = static_cast<VkFilter>(info->filter_min);
        sampler_info.magFilter               = static_cast<VkFilter>(info->filter_mag);
        sampler_info.mipmapMode              = static_cast<VkSamplerMipmapMode>(info->filter_mip);
        sampler_info.addressModeU            = static_cast<VkSamplerAddressMode>(info->address_u);
        sampler_info.addressModeV            = static_cast<VkSamplerAddressMode>(info->address_v);
        sampler_info.addressModeW            = static_cast<VkSamplerAddressMode>(info->address_w);
        sampler_info.mipLodBias              = info->mip_bias;
        sampler_info.anisotropyEnable        = info->anisotropy > 0.0f;
        sampler_info.maxAnisotropy           = info->anisotropy;
        sampler_info.compareEnable           = (info->compare_operation != ECompareOperation::never ? VK_TRUE : VK_FALSE);
        sampler_info.compareOp               = convert_to_vk_compare_op(info->compare_operation);
        sampler_info.minLod                  = info->min_mip_level;
        sampler_info.maxLod                  = info->max_mip_level;
        sampler_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        auto result = vkCreateSampler(device->device, &sampler_info, nullptr, &sampler);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create sampler: {}", vk_error_to_string(result)));

        bindless_idx = parent->bindless_manager->register_sampler(sampler);
    }

    VulkanSampler::~VulkanSampler()
    {
        vkDestroySampler(parent->device, sampler, nullptr);
    }

    auto VulkanSampler::bindless_index() -> uint32_t
    {
        return bindless_idx;
    }
}
