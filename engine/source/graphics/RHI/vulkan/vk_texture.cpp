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

    auto VulkanDevice::create_texture(std::string_view name, TextureCreateInfo* info) -> TextureHandle
    {
        auto hash = XXH64(info, sizeof(TextureCreateInfo), 0);
        auto texture = texture_pool->create<VulkanTexture>(hash, this, info);

        TRACE_POOLED_TEXTURE("Create", name, info->extent.x * info->extent.y * info->depth * 4);

        // Set deleter for pool texture:
        texture->deleter = [pool = texture_pool.get()](VulkanTexture* resource) {
            pool->resource_delete(pool, resource);
            TRACE_POOLED_TEXTURE("Release", resource->name, resource->info.extent.x * resource->info.extent.y * resource->info.depth * 4);
        };

        set_resource_name(device, VK_OBJECT_TYPE_IMAGE, (uint64_t) texture->image, name);
        texture->name = name;

        return texture;
    }

    VulkanTexture::VulkanTexture(VulkanDevice* device, TextureCreateInfo* in_info)
        : VulkanDeviceChild<VulkanTexture>(device)
        , info(std::move(*in_info))
    {
        auto image_info = VkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        image_info.flags                 = image_create_flags_from_dimension(info.dimension);
        image_info.imageType             = image_type_from_dimension(info.dimension);
        image_info.format                = convert_to_vk_format(info.format);
        image_info.extent                = VkExtent3D{(uint32_t) info.extent.x, (uint32_t) info.extent.y, (uint32_t) info.depth};
        image_info.mipLevels             = info.num_mips;
        image_info.arrayLayers           = info.num_layers;
        image_info.samples               = VK_SAMPLE_COUNT_1_BIT; // TODO:
        image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage                 = convert_to_vk_image_usage(info.usage);
        image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 0;
        image_info.pQueueFamilyIndices   = nullptr;
        image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

        auto allocation_info = VmaAllocationCreateInfo{};
        allocation_info.flags = 0;
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        auto result = vmaCreateImage(device->allocator, &image_info, &allocation_info, &image, &allocation, nullptr);
        CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create image: {}", vk_error_to_string(result)));

        default_view_info = image_view_create_info(0, 0);

        tracker.texture = this;
    }

    VulkanTexture::VulkanTexture(VulkanDevice* device, TextureCreateInfo* in_info, VkImage in_image)
        : VulkanDeviceChild<VulkanTexture>(device), image(in_image)
        , info(std::move(*in_info))
    {
        default_view_info = image_view_create_info(0, 0);

        tracker.texture = this;
    }

    VulkanTexture::~VulkanTexture()
    {
        for (auto& [_, view] : image_views) {
            vkDestroyImageView(parent->device, view.image_view, parent->allocation_callbacks);
            if (auto& bindless = parent->bindless_manager) {
                bindless->free_index(view.type, view.bindless_index);
            }
        }

        if (allocation) {
            CNE_TRACE("free texture: {}", name);
            vmaDestroyImage(parent->allocator, image, allocation);
        }
    }

    auto VulkanTexture::bindless_index() -> uint32_t
    {
        auto view = default_view();
        if (view->bindless_index == k_invalid_bindless_index) {
            parent->bindless_manager->register_texture_view(view);
        }

        return view->bindless_index;
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

    auto VulkanTexture::texture_view(VkImageViewCreateInfo const* in_info) -> VulkanTextureView*
    {
        auto hash = XXH64(in_info, sizeof(VkImageViewCreateInfo), 0);

        auto it = image_views.find(hash);

        if (it == image_views.end()) {
            auto texture_view = VulkanTextureView{};
            auto result = vkCreateImageView(parent->device, in_info, parent->allocation_callbacks, &texture_view.image_view);
            CNE_ASSERT_WITH(result == VK_SUCCESS, std::format("Failed to create image view: {}", vk_error_to_string(result)));

            it = image_views.emplace(hash, texture_view).first;
        }

        return &it->second;
    }

    auto VulkanTexture::default_view() -> VulkanTextureView*
    {
        return texture_view(&default_view_info);
    }

    auto VulkanTexture::image_view_create_info(uint32_t mip_level, uint32_t array_layer) -> VkImageViewCreateInfo
    {
        auto format = convert_to_vk_format(info.format);
        auto view_ci = VkImageViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view_ci.image            = image;
        view_ci.viewType         = image_view_type();
        view_ci.format           = format;
        view_ci.subresourceRange = VkImageSubresourceRange{aspect_flag_from_format(format), mip_level, info.num_mips, array_layer, info.num_layers};

        return view_ci;
    }
}
