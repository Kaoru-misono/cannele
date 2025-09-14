#include "resource.hpp"
#include "device.hpp"
#include <core/assert.hpp>

namespace cannele::inline graphics::rhi
{
    DeviceChild::DeviceChild(IDevice* device)
        : device(device->shared_from_this())
    {
        reference.store(this->device.lock(), std::memory_order_release);
    }
    DeviceChild::~DeviceChild()
    {
        invalidate_reference();
    }

    auto depth_attachment_create_info(math::uint2 in_extent, EFormat in_format) -> TextureCreateInfo
    {
        return TextureCreateInfo{
            .dimension     = ETextureDimension::tex_2d,
            .format        = in_format,
            .usage         = ETextureUsage::depth_stencil_attachment | ETextureUsage::transfer_src,
            .extent        = in_extent,
            .initial_state = EResourceStates::depth_stencil_attachment,
        };
    }

    auto adapt_to_buffer(BufferRange* range, BufferCreateInfo const* info) -> void
    {
        range->size_bytes = std::min(range->size_bytes, info->size_bytes);
        CNE_ASSERT_WITH(range->offset_bytes + range->size_bytes <= info->size_bytes, "Buffer range out of bound.");
    }

    auto contain_all_resources(TextureSubresourceSet* subresources, TextureCreateInfo const* info) -> bool
    {
        if (subresources->base_mip_level > 0u || subresources->base_mip_level + subresources->num_mip_levels < info->num_mips) return false;

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                if (subresources->base_array_layer > 0u || subresources->base_array_layer + subresources->num_array_layers < info->num_layers) return false;
            }
            default: return true;
        }
    }

    auto adapt_to_texture(TextureSubresourceSet* subresources, TextureCreateInfo const* info, bool signle_mip_level) -> void
    {
        auto result = TextureSubresourceSet{};
        result.base_mip_level = subresources->base_mip_level;

        if (signle_mip_level) {
            subresources->num_mip_levels = 1;
        } else {
            auto max_mip_levels = std::min(subresources->base_mip_level + subresources->num_mip_levels, info->num_mips);
            subresources->num_mip_levels = std::max(0u, max_mip_levels - subresources->base_mip_level);
        }

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                auto max_array_layers = std::min(subresources->base_array_layer + subresources->num_array_layers, info->num_layers);
                subresources->num_array_layers = std::max(0u, max_array_layers - subresources->base_array_layer);
                break;
            }
            default: {
                subresources->base_array_layer = 0;
                subresources->num_array_layers = 1;
                break;
            }
        }
    }
}
