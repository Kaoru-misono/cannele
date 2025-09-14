#include "texture_asset.hpp"
#include "../RHI/device.hpp"

#include <platform/engine.hpp>


namespace cannele::inline graphics::resource
{
    inline namespace
    {
        using namespace core::resource;
    }

    auto const TextureAsset::metadata = core::resource::AssetMetadata{
        .name = "Texture",
        .save_suffix = ".tex_asset",
        .support_extension = ".png,.jpeg,.jpg,.tga,.exr",
        .importer = [](std::any config) -> bool {
            if (config.type() == typeid(TextureAssetImportConfig)) {
                auto texture_config = std::any_cast<TextureAssetImportConfig>(&config);
                return TextureAsset::import_from_config(texture_config);
            }

            return false;
        }
    };

    auto TextureAsset::import_from_config(TextureAssetImportConfig* config) -> TextureAsset*
    {
        auto asset_manager = AssetManager::current();

        auto texture_asset = std::make_unique<TextureAsset>();
        texture_asset->path = config->path;
        texture_asset->is_srgb = config->is_srgb;
        auto asset_ptr = asset_manager->create_asset<TextureAsset>(config->path, std::move(texture_asset));

        using namespace rhi;

        asset_ptr->info.dimension          = config->extent.depth == 1 ? ETextureDimension::tex_2d : ETextureDimension::tex_3d;
        asset_ptr->info.format             = config->format;
        asset_ptr->info.usage              = ETextureUsage::sampled | ETextureUsage::transfer_dst;
        asset_ptr->info.extent             = config->extent;
        asset_ptr->info.final_state        = EResourceStates::sampled_texture;

        // If config has data, we use it, otherwise load data from file.
        if (!config->data.empty()) {
            auto device = platform::Engine::current()->device.get();

            asset_ptr->texture.texture_handle = device->create_texture(config->path, &asset_ptr->info);
            device->async_uploader()->add_task(
                [asset_ptr, data = std::move(config->data)](this auto&, CommandEncoder* encoder) {
                    auto format_info = get_format_info(asset_ptr->info.format);
                    CNE_ASSERT_WITH(data.size() == asset_ptr->info.extent.width * asset_ptr->info.extent.height * format_info->bytes_per_block, std::format("Invalid data size, expected {} bytes but got {}", data.size(), asset_ptr->info.extent.width * asset_ptr->info.extent.height * format_info->bytes_per_block));
                    auto data_view = TextureSliceDataView{
                        data.data(),
                        asset_ptr->info.extent.width * format_info->bytes_per_block,
                        asset_ptr->info.extent.height,
                        1
                    };
                    auto texture_handle = asset_ptr->texture.texture_handle;
                    encoder->upload_texture_data(texture_handle, data_view);
                },
                [asset_ptr]() { asset_ptr->texture.uploading = false; }
            );
        } else {
            TODO:
        }

        return asset_ptr;
    }
}
