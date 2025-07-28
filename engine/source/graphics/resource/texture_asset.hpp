#pragma once

#include "../RHI/RHI_resource.hpp"
#include "../RHI/tool/async_uploader.hpp"

#include <core/resource/asset.hpp>

namespace cannele::inline graphics::resource
{
    struct TextureAssetImportConfig final
    {
        std::string_view path{};

        std::vector<std::byte> data{};

        math::uint3 extent{};

        rhi::EFormat format{rhi::EFormat::undefined};

        bool is_srgb{false};

        bool generate_mipmaps{false};
    };

    struct TextureAsset: core::resource::IAsset
    {
        using AssetMetadata = core::resource::AssetMetadata;

        std::string path{};

        // Cached texture info.
        rhi::TextureCreateInfo info{};

        rhi::UploadTexture texture{};

        rhi::EColorSpace color_space{rhi::EColorSpace::srgb_nonliner};

        bool is_srgb{false};

        float mipmap_alpha_threshold{0.0f};

        static const AssetMetadata metadata;

        static auto import_from_config(TextureAssetImportConfig* config) -> TextureAsset*;

        auto name() -> std::string_view override { return {}; }

        auto save(std::string_view path) -> bool override { return false; }

        auto load(std::string_view path) -> std::unique_ptr<IAsset> override { return nullptr; }
    };
}
