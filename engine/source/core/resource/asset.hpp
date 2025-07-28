#pragma once

#include "../idiom.hpp"
#include "../exclusive.hpp"
#include "../hash.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <any>

namespace cannele::inline core::resource
{
    struct IAsset;

    template <typename T>
    concept has_metadata = requires { T::metadata; };

    template <typename T>
    concept is_asset = std::is_base_of_v<IAsset, T>;

    template <typename AssetType>
    concept asset_type = has_metadata<AssetType> && is_asset<AssetType>;

    struct AssetMetadata final
    {
        std::string name{};

        std::string save_suffix{};

        std::string support_extension{};

        std::function<auto (std::any config) -> bool> importer{};
    };

    struct IAsset: std::enable_shared_from_this<IAsset>
    {
        CNE_INTERFACE(IAsset);

        explicit IAsset() = default;

        virtual auto name() -> std::string_view = 0;
        virtual auto save(std::string_view path) -> bool = 0;
        virtual auto load(std::string_view path) -> std::unique_ptr<IAsset> = 0;
    };

    struct AssetManager final: ThreadExclusive<AssetManager>
    {
        CNE_MOVE_ONLY(AssetManager);

        using AssetID = size_t;

        std::unordered_map<std::string, AssetMetadata const*> registered_type{};
        std::unordered_map<std::string, AssetID> typed_assets{};
        std::unordered_map<AssetID, std::unique_ptr<IAsset>> assets{};

        AssetManager() = default;

        auto register_asset(AssetMetadata const* metadata) -> void;

        template <asset_type T>
        auto create_asset(std::string_view path, std::unique_ptr<T> asset = {}) -> T*
        {
            if (!asset) {
                asset = std::make_unique<T>();
                asset->load(path);
            }

            auto new_asset = asset.get();

            assets.emplace(core::hash(path), std::move(asset));

            return new_asset;
        }
    };
}
