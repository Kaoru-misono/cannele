#include "vk_RHI.hpp"
#include "vk_tool.hpp"

#include <xxhash.h>

namespace cannele::inline graphics::rhi::vk
{
    inline namespace
    {
        auto image_usage_to_string(VkImageUsageFlags usage) -> std::string
        {
            auto result = std::string{};

            if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) result += "sampled, ";
            if (usage & VK_IMAGE_USAGE_STORAGE_BIT) result += "storage, ";
            if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) result += "color_attachment, ";
            if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) result += "depth_stencil_attachment, ";
            if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) result += "transfer_src, ";
            if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) result += "transfer_dst, ";

            return result;
        }
    }

#if ENABLE_POOL_TRACE
    #define TRACE_POOLED_TEXTURE(CREATE_OR_RELEASE, NAME, SIZE) \
        CNE_TRACE(CREATE_OR_RELEASE " Texture: \"{}\" with size {} KB", NAME, SIZE / 1024);
#else
    #define TRACE_POOLED_TEXTURE(CREATE_OR_RELEASE, NAME, SIZE)
#endif

    auto VulkanDevice::create_texture(std::string_view name, TextureCreateInfo const* info) -> TextureHandle
    {
        auto hash = XXH64(info, sizeof(TextureCreateInfo), 0);
        auto texture = texture_pool->create<VulkanTexture>(name, hash, this, info);

        TRACE_POOLED_TEXTURE("Create", name, info->extent.width * info->extent.height * info->extent.depth * 4);

        set_resource_name(device, VK_OBJECT_TYPE_IMAGE, (uint64_t) texture->image, name);
        texture->name = name;

        return texture;
    }

    VulkanTexture::VulkanTexture(VulkanDevice* device, TextureCreateInfo const* in_info)
        : RHITexture(device)
        , info(*in_info)
    {
        format = to_vk_format(info.format);
        auto image_info = VkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image_info.flags                 = image_create_flags_from_dimension(info.dimension);
        image_info.imageType             = image_type_from_dimension(info.dimension);
        image_info.format                = format;
        image_info.extent                = VkExtent3D{info.extent.width, info.extent.height, info.extent.depth};
        image_info.mipLevels             = info.mip_count;
        image_info.arrayLayers           = info.layer_count;
        image_info.samples               = VK_SAMPLE_COUNT_1_BIT; // TODO:
        image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage                 = to_vk_image_usage(info.usage);
        image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 0;
        image_info.pQueueFamilyIndices   = nullptr;
        image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

        auto allocation_info = VmaAllocationCreateInfo{};
        allocation_info.flags = 0;
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        CHECK_VK_RESULT(vmaCreateImage(device->allocator, &image_info, &allocation_info, &image, &allocation, nullptr));

        auto whole_subresources = resolve_subresource_rage(k_all_subresources);
        default_view_ = subresource_view(whole_subresources);
    }

    VulkanTexture::VulkanTexture(VulkanDevice* device, TextureCreateInfo const* in_info, VkImage in_image)
        : RHITexture(device), image(in_image)
        , info(*in_info)
    {
        format = to_vk_format(info.format);

        auto whole_subresources = resolve_subresource_rage(k_all_subresources);
        default_view_ = subresource_view(whole_subresources);
    }

    VulkanTexture::~VulkanTexture()
    {
        auto parent = get_device<VulkanDevice>();
        for (auto& [_, view] : texture_subresource_views) {
            vkDestroyImageView(parent->device, view.image_view, parent->allocation_callbacks);
        }

        for (auto& [_, view] : texture_subresource_views) {
            if (auto& bindless = parent->bindless_manager) {
                if (view.bindless_index[0] != k_invalid_bindless_index) {
                    bindless->resource_heap->free_index(view.bindless_index[0]);
                }
                if (view.bindless_index[1] != k_invalid_bindless_index) {
                    bindless->resource_heap->free_index(view.bindless_index[1]);
                }
            }
        }

        if (allocation) {
            vmaDestroyImage(parent->allocator, image, allocation);
        }
    }

    auto VulkanTexture::view(TextureSubresourceRange const& subresources) -> RHITextureView*
    {
        if (subresources == k_all_subresources) {
            return default_view_;
        }

        auto resolved = resolve_subresource_rage(subresources);

        return subresource_view(resolved);
    }

    auto VulkanTexture::image_view_type() -> VkImageViewType
    {
        switch (info.dimension) {
            case ETextureDimension::tex_2d:         return VK_IMAGE_VIEW_TYPE_2D;
            case ETextureDimension::tex_3d:         return VK_IMAGE_VIEW_TYPE_3D;
            case ETextureDimension::tex_cube:       return VK_IMAGE_VIEW_TYPE_CUBE;
            case ETextureDimension::tex_2d_array:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ETextureDimension::tex_cube_array: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            default: CNE_UNREACHABLE();
        }
    }

    auto VulkanTexture::subresource_view(TextureSubresourceRange const& subresources) -> VulkanTextureView*
    {
        auto view_key = XXH64(&subresources, sizeof(TextureSubresourceRange), 0);

        auto it = texture_subresource_views.find(view_key);

        if (it != texture_subresource_views.end()) {
            return &it->second;
        }

        auto create_info = VkImageViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        create_info.image            = image;
        create_info.viewType         = image_view_type();
        create_info.format           = format;
        create_info.subresourceRange = VkImageSubresourceRange{
            aspect_flag_from_format(format),
            subresources.mip_level,
            subresources.mip_count,
            subresources.mip_level,
            subresources.layer_count,
        };
        create_info.components = VkComponentMapping{
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };

        auto subresource_view = VulkanTextureView{};
        subresource_view.texture_ = this;
        subresource_view.range_ = subresources;

        auto parent = get_device<VulkanDevice>();
        CHECK_VK_RESULT(vkCreateImageView(parent->device, &create_info, parent->allocation_callbacks, &subresource_view.image_view));
        subresource_view.image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        it = texture_subresource_views.emplace(view_key, subresource_view).first;

        return &it->second;
    }

    auto VulkanTextureView::descriptor_handle(EDescriptorType type) -> math::uint2
    {
        auto parent = texture_->get_device<VulkanDevice>();
        auto index = type == EDescriptorType::sampled_texture ? 0 : 1;
        // TODO: Check usage contain type required.
        if (bindless_index[index] != k_invalid_bindless_index) {
            return {bindless_index[index], 0};
        } else {
            auto image_layout = type == EDescriptorType::sampled_texture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
            bindless_index[index] = parent->bindless_manager->register_texture(EDescriptorType::sampled_texture, image_view, image_layout);
            return {bindless_index[index], 0};
        }
    }
}
