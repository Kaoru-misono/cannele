#include "RHI_resource.hpp"
#include "RHI.hpp"
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
    auto BufferRange::adapt_to_buffer(BufferCreateInfo* info) -> void
    {
        size_bytes = std::min(size_bytes, info->size_bytes);
        CNE_ASSERT_WITH(offset_bytes + size_bytes <= info->size_bytes, "Buffer range out of bound.");
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

    auto TextureSubresourceSet::contain_all_resources(TextureCreateInfo const* info) -> bool
    {
        if (base_mip_level > 0u || base_mip_level + num_mip_levels < info->num_mips) return false;

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                if (base_array_layer > 0u || base_array_layer + num_array_layers < info->num_layers) return false;
            }
            default: return true;
        }
    }

    auto TextureSubresourceSet::adapt_to_texture(TextureCreateInfo const* info, bool signle_mip_level) -> void
    {
        auto result = TextureSubresourceSet{};
        result.base_mip_level = base_mip_level;

        if (signle_mip_level) {
            num_mip_levels = 1;
        } else {
            auto max_mip_levels = std::min(base_mip_level + num_mip_levels, info->num_mips);
            num_mip_levels = std::max(0u, max_mip_levels - base_mip_level);
        }

        switch (info->dimension) {
            case ETextureDimension::tex_2d_array:
            case ETextureDimension::tex_cube_array: {
                auto max_array_layers = std::min(base_array_layer + num_array_layers, info->num_layers);
                num_array_layers = std::max(0u, max_array_layers - base_array_layer);
                break;
            }
            default: {
                base_array_layer = 0;
                num_array_layers = 1;
                break;
            }
        }
    }
}
