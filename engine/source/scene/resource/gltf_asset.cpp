#include "gltf_asset.hpp"

#include <core/assert.hpp>
#include <core/enum_flag.hpp>
#include <platform/file_system.hpp>
#include <platform/engine.hpp>
#include <graphics/resource/texture_asset.hpp>
#include <scene/resource/gltf_asset.hpp>
#include <scene/resource/nanite.hpp>

#include <stb/stb_image_resize2.h>
#include <tiny_gltf.h>
#include <map>
#include <set>
#include <ranges>

namespace cannele::inline scene::resource
{
    inline namespace
    {
        using namespace graphics::resource;
        using namespace core::resource;
        using namespace scene::resource;

        enum struct EGLTFExtension: uint8_t
        {
            KHR_lights_punctual,
            KHR_texture_transform,
            KHR_materials_specular,
            KHR_materials_unlit,
            KHR_materials_anisotropy,
            KHR_materials_ior,
            KHR_materials_volume,
            KHR_materials_transmission,
            KHR_texture_basisu,
            KHR_materials_clearcoat,
            KHR_materials_sheen,

            last,
        };

        // See https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos.
        static constexpr auto GLTFExtensionMap = std::array<std::string_view, (size_t) EGLTFExtension::last>{
            "KHR_lights_punctual",
            "KHR_texture_transform",
            "KHR_materials_specular",
            "KHR_materials_unlit",
            "KHR_materials_anisotropy",
            "KHR_materials_ior",
            "KHR_materials_volume",
            "KHR_materials_transmission",
            "KHR_texture_basisu",
            "KHR_materials_clearcoat",
            "KHR_materials_sheen",
        };

        // Return char const* because of key is std::string.
        auto get_extension(EGLTFExtension value) -> char const*
        {
            CNE_ASSERT(value < EGLTFExtension::last);
            return GLTFExtensionMap[(size_t) value].data();
        }

        auto get_texture_id(tinygltf::Value const* value, std::string const& name) -> int32_t
        {
            if (value->Has(name)) {
                return value->Get(name).Get("index").Get<int32_t>();
            }

            return -1;
        }

        enum struct EColorChannel: uint8_t
        {
            r,
            g,
            b,
            a,
        };

        static constexpr auto r    = EnumFlags<EColorChannel>{EColorChannel::r};
        static constexpr auto g    = EnumFlags<EColorChannel>{EColorChannel::g};
        static constexpr auto b    = EnumFlags<EColorChannel>{EColorChannel::b};
        static constexpr auto a    = EnumFlags<EColorChannel>{EColorChannel::a};
        static constexpr auto rgb  = EnumFlags<EColorChannel>{EColorChannel::r, EColorChannel::g, EColorChannel::b};
        static constexpr auto rgba = EnumFlags<EColorChannel>{EColorChannel::r, EColorChannel::g, EColorChannel::b, EColorChannel::a};

        auto get_format(EnumFlags<EColorChannel> channels, bool compressed, bool is_16_bit) -> rhi::EFormat
        {
            if (is_16_bit) {
                CNE_ASSERT_WITH(false, "16-bit textures are not supported");
            }

            // TODO: Process compression.
            if (channels.all(rgb) || channels.all(rgba)) {
                return rhi::EFormat::rgba8_unorm;
            }
            else if (channels.all(r | g)) {
                return rhi::EFormat::rg8_unorm;
            }
            else if (channels.all(r) || channels.all(g) || channels.all(b) || channels.all(a)) {
                return rhi::EFormat::r8_unorm;
            }

            CNE_UNREACHABLE();
        };
    }

    auto const GLTFMaterialAsset::metadata = AssetMetadata{
        .name = "GLTFMaterial",
        .save_suffix = ".gltf_material_asset",
    };

    auto const GLTFAsset::metadata = AssetMetadata{
        .name = "GLTF",
        .save_suffix = ".gltf_asset",
        .support_extension = ".glb,.gltf",
        .importer = [](std::any config) -> bool {
            if (config.type() == typeid(GLTFAssetImportConfig)) {
                auto gltf_config = std::any_cast<GLTFAssetImportConfig>(&config);
                return GLTFAsset::import_from_config(gltf_config);
            }

            return false;
        }
    };

    auto GLTFAsset::import_from_config(GLTFAssetImportConfig* config) -> GLTFAsset*
    {
        auto import_file = file::FileSystem::try_current()->get_file(config->import_path);

        if (!import_file) return nullptr;

        auto model = tinygltf::Model{};
        {
            auto tiny_gltf = tinygltf::TinyGLTF{};
            auto warning = std::string{};
            auto error = std::string{};

            auto success = false;
            if (import_file.extension() == ".gltf") {
                success = tiny_gltf.LoadASCIIFromFile(&model, &error, &warning, import_file.absolute_path());
            }
            else if (import_file.extension() == ".glb") {
                success = tiny_gltf.LoadBinaryFromFile(&model, &error, &warning, import_file.absolute_path());
            }
            if (!success) return nullptr;

            if (!warning.empty()) {
                CNE_WARN("File: {} import warning: {}", import_file.name(), warning);
            }
            if (!error.empty()) {
                CNE_ERROR("File: {} import error: {}", import_file.name(), error);
            }
        }

        std::ranges::for_each(model.extensionsRequired, [](auto const& extension) {
            if (!std::ranges::contains(GLTFExtensionMap, extension)) {
                CNE_ERROR("Unsupported gltf extension: {}", extension);
            }
        });

        // Load all images.
        auto imported_images = std::unordered_map<int32_t, TextureAsset*>{};
        auto srgb_images_map = std::set<int32_t>{};
        auto alpha_cutoff_images = std::map<int32_t, float>{};
        auto const invalid_image_index = -1;
        auto images_channel_map = std::map<int32_t, EnumFlags<EColorChannel>>{};

        for (auto& material: model.materials) {
            if (auto index = material.pbrMetallicRoughness.baseColorTexture.index; index != invalid_image_index) {
                auto base_color_source = model.textures.at(index).source;

                srgb_images_map.emplace(base_color_source);

                if (material.alphaMode != "OPAQUE") {
                    alpha_cutoff_images[base_color_source] = material.alphaCutoff;
                }

                images_channel_map[base_color_source] = rgba;
            }

            if (auto index = material.emissiveTexture.index; index != invalid_image_index) {
                auto emissive_source = model.textures.at(index).source;

                srgb_images_map.emplace(emissive_source);

                images_channel_map[emissive_source] = rgb;
            }

            if (auto index = material.normalTexture.index; index != invalid_image_index) {
                auto normal_source = model.textures.at(index).source;

                images_channel_map[normal_source] = r | g;
            }

            if (auto index = material.occlusionTexture.index; index != invalid_image_index) {
                auto occlusion_source = model.textures.at(index).source;

                images_channel_map[occlusion_source] = r;
            }

            if (auto index = material.pbrMetallicRoughness.metallicRoughnessTexture.index; index != invalid_image_index) {
                auto metallic_roughness_source = model.textures.at(index).source;

                images_channel_map[metallic_roughness_source] = g | b;

                if (material.occlusionTexture.index != invalid_image_index) {
                    images_channel_map[metallic_roughness_source] |= r;
                }

            }

            auto extension_name = get_extension(EGLTFExtension::KHR_materials_specular);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto specular_color_source = model.textures.at(get_texture_id(extension, "specularColorTexture")).source;
                auto specular_source       = model.textures.at(get_texture_id(extension, "specularTexture")).source;

                srgb_images_map.emplace(specular_color_source);

                images_channel_map[specular_color_source] = rgb;
                images_channel_map[specular_source] = a;

                if (specular_color_source != specular_source) {
                    CNE_WARN("Specular color texture and specular texture are not the same.");
                }
            }

            extension_name = get_extension(EGLTFExtension::KHR_materials_anisotropy);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto anisotropy_source = model.textures.at(get_texture_id(extension, "anisotropyTexture")).source;

                // Red and green channels represent the anisotropy direction in [-1, 1] tangent, bitangent space, to be rotated by anisotropyRotation.
				// The blue channel contains strength as [0, 1] to be multiplied by anisotropyStrength.
                images_channel_map[anisotropy_source] = rgb;
            }

            extension_name = get_extension(EGLTFExtension::KHR_materials_clearcoat);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto clearcoat_index           = get_texture_id(extension, "clearcoatTexture");
                auto clearcoat_roughness_index = get_texture_id(extension, "clearcoatRoughnessTexture");
                auto clearcoat_normal_index    = get_texture_id(extension, "clearcoatNormalTexture");

                auto clearcoat_source           = model.textures.at(clearcoat_index).source;
                auto clearcoat_roughness_source = model.textures.at(clearcoat_roughness_index).source;
                auto clearcoat_normal_source    = model.textures.at(clearcoat_normal_index).source;

                images_channel_map[clearcoat_source] = r;
                images_channel_map[clearcoat_roughness_source] = g;
                images_channel_map[clearcoat_normal_source] = r | g;
            }

            extension_name = get_extension(EGLTFExtension::KHR_materials_sheen);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto sheen_color_index     = get_texture_id(extension, "sheenColorTexture");
                auto sheen_roughness_index = get_texture_id(extension, "sheenRoughnessTexture");

                auto sheen_color_source = model.textures.at(sheen_color_index).source;
                auto sheen_roughness_source = model.textures.at(sheen_roughness_index).source;

                srgb_images_map.emplace(sheen_color_source);

                images_channel_map[sheen_color_source] = rgb;
                images_channel_map[sheen_roughness_source] = a;
            }

            extension_name = get_extension(EGLTFExtension::KHR_materials_transmission);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto transmission_source = model.textures.at(get_texture_id(extension, "transmissionTexture")).source;

                images_channel_map[transmission_source] = r;
            }

            extension_name = get_extension(EGLTFExtension::KHR_materials_volume);
            if (material.extensions.contains(extension_name)) {
                auto const extension = &material.extensions[extension_name];

                auto thickness_source = model.textures.at(get_texture_id(extension, "thicknessTexture")).source;

                images_channel_map[thickness_source] = g;
            }
        }

        // TODO: Parallelize.
        for (auto image_index = 0zu; image_index < model.images.size(); image_index++) {
            auto const image = &model.images[image_index];

            auto is_srgb = srgb_images_map.contains(image_index);

            auto has_alpha = alpha_cutoff_images.contains(image_index);

            auto name = image->name;
            if (name.empty()) {
                name = std::format("texture_{}_{}", image->uri, image_index);
            }

            if (!images_channel_map.contains(image_index)) {
                CNE_ERROR("Texture '{}' doesn't have any channel info, skipped.", name);
                continue;
            }

            // TODO: Set the path to real save path.
            auto import_config = TextureAssetImportConfig{};
            import_config.path    = image->uri;
            import_config.data    = std::as_bytes(std::span{image->image}) | std::ranges::to<std::vector<std::byte>>();
            import_config.extent  = math::uint3{image->width, image->height, 1};
            import_config.format  = get_format(images_channel_map[image_index], false, false);
            import_config.is_srgb = is_srgb;

            auto texture_asset = TextureAsset::import_from_config(&import_config);
            if (!texture_asset) {
                CNE_ERROR("Failed to import texture '{}'.", name);
                return nullptr;
            }

            imported_images[image_index] = texture_asset;
        }

        // Load all materials.
        auto imported_materials = std::unordered_map<uint32_t, GLTFMaterialAsset*>{};
        auto asset_manager = AssetManager::current();
        for (auto material_index = 0zu; material_index < model.materials.size(); material_index++) {
            auto const material = &model.materials[material_index];

            auto name = material->name.empty() ? std::format("Material_{}", material_index) : material->name;

            auto material_asset = std::make_unique<GLTFMaterialAsset>();

            auto sampler_from_gltf = [](tinygltf::Sampler const* sampler) -> rhi::SamplerCreateInfo {
                auto sampler_info = rhi::SamplerCreateInfo{};
                sampler_info.address_u =
                    (sampler->wrapS == TINYGLTF_TEXTURE_WRAP_REPEAT) ? rhi::ESamplerAddressMode::repeat :
                    (sampler->wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) ? rhi::ESamplerAddressMode::clamp_to_edge :
                    rhi::ESamplerAddressMode::mirror_repeat;
                sampler_info.address_v =
                    (sampler->wrapT == TINYGLTF_TEXTURE_WRAP_REPEAT) ? rhi::ESamplerAddressMode::repeat :
                    (sampler->wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) ? rhi::ESamplerAddressMode::clamp_to_edge :
                    rhi::ESamplerAddressMode::mirror_repeat;
                switch (sampler->minFilter) {
                    case TINYGLTF_TEXTURE_FILTER_NEAREST: {
                        sampler_info.filter_min = rhi::ESamplerFilter::nearest;
                        break;
                    }
                    case TINYGLTF_TEXTURE_FILTER_LINEAR: {
                        sampler_info.filter_min = rhi::ESamplerFilter::linear;
                        break;
                    }
                    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: {
                        sampler_info.filter_min = rhi::ESamplerFilter::nearest;
                        sampler_info.filter_mip = rhi::ESamplerFilter::nearest;
                        break;
                    }
                    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: {
                        sampler_info.filter_min = rhi::ESamplerFilter::linear;
                        sampler_info.filter_mip = rhi::ESamplerFilter::nearest;
                        break;
                    }
                    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: {
                        sampler_info.filter_min = rhi::ESamplerFilter::nearest;
                        sampler_info.filter_mip = rhi::ESamplerFilter::linear;
                        break;
                    }
                    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: {
                        sampler_info.filter_min = rhi::ESamplerFilter::linear;
                        sampler_info.filter_mip = rhi::ESamplerFilter::linear;
                        break;
                    }
                    default: break;
                }
                sampler_info.filter_mag = (sampler->magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST) ? rhi::ESamplerFilter::nearest : rhi::ESamplerFilter::linear;

                return sampler_info;
            };

            auto set_texture_info = [&](GLTFMaterialAsset::TextureInfo* texture_info, auto const* from) {
                if (from->index == invalid_image_index) return;

                auto texture = &model.textures.at(from->index);
                auto sampler = &model.samplers.at(texture->sampler);

                texture_info->texture  = imported_images.at(from->index);
                texture_info->texcoord = from->texCoord;
                texture_info->sampler  = sampler_from_gltf(sampler);
            };

            auto pbr_mr = &material->pbrMetallicRoughness;

            material_asset->base_color_factor = {
                pbr_mr->baseColorFactor[0],
                pbr_mr->baseColorFactor[1],
                pbr_mr->baseColorFactor[2],
                pbr_mr->baseColorFactor[3]
            };
            set_texture_info(&material_asset->base_color_texture, &pbr_mr->baseColorTexture);

            material_asset->metallic_factor  = pbr_mr->metallicFactor;
            material_asset->roughness_factor = pbr_mr->roughnessFactor;
            set_texture_info(&material_asset->metallic_roughness_texture, &pbr_mr->metallicRoughnessTexture);

            material_asset->emissive_factor = {
                material->emissiveFactor[0],
                material->emissiveFactor[1],
                material->emissiveFactor[2]
            };
            set_texture_info(&material_asset->emissive_texture, &material->emissiveTexture);

            if (material->alphaMode == "OPAQUE") {
                material_asset->blend_mode = EBlendMode::opaque;
            }
            else if (material->alphaMode == "MASK") {
                material_asset->blend_mode = EBlendMode::alpha_test;
            }
            else {
                material_asset->blend_mode = EBlendMode::alpha_blend;
            }

            material_asset->alpha_cutoff = material->alphaCutoff;
            material_asset->double_sided = material->doubleSided;

            material_asset->normal_texture_scale = material->normalTexture.scale;
            set_texture_info(&material_asset->normal_texture, &material->normalTexture);

            material_asset->exist_occlusion            = material->occlusionTexture.index != invalid_image_index;
            material_asset->occlusion_texture_strength = material->occlusionTexture.strength;
            set_texture_info(&material_asset->occlusion_texture, &material->occlusionTexture);

            auto store_path = std::format("assets/materials/{}", material_index);
            material_asset->path = store_path;
            imported_materials[material_index] = asset_manager->create_asset<GLTFMaterialAsset>(store_path, std::move(material_asset));
        }

        auto gltf_asset = std::make_unique<GLTFAsset>();
        auto gltf_store_path = std::format("assets/gltf/{}", config->import_path);
        gltf_asset->path = gltf_store_path; // FIXME: path is not correct

        auto& gltf_data = gltf_asset->data;
        {
            gltf_asset->default_scene = model.defaultScene;

            gltf_asset->scenes.reserve(model.scenes.size());
            std::ranges::transform(model.scenes, std::back_inserter(gltf_asset->scenes), [&](tinygltf::Scene const& scene) -> GLTFScene {
                return GLTFScene{
                    .name = scene.name,
                    .nodes = scene.nodes,
                };
            });

            gltf_asset->nodes.reserve(model.nodes.size());
            std::ranges::transform(model.nodes, std::back_inserter(gltf_asset->nodes), [&](tinygltf::Node const& node) -> GLTFNode {
                auto result = GLTFNode{
                    .name      = node.name,
                    .mesh      = node.mesh,
                    .childrens = node.children,
                };

                if (node.translation.size() == 3) {
                    auto translation = math::float3{node.translation[0], node.translation[1], node.translation[2]};
                    result.local_matrix = glm::translate(result.local_matrix, translation);
                }
                if (node.rotation.size() == 4) {
                    auto rotation = math::quaternion(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
                    result.local_matrix *= glm::mat4_cast(rotation);
                }
                if (node.scale.size() == 3) {
                    auto scale = math::float3{node.scale[0], node.scale[1], node.scale[2]};
                    result.local_matrix = glm::scale(result.local_matrix, scale);
                }

                if (node.matrix.size() == 16) {
                    auto float_mat = node.matrix | std::ranges::to<std::vector<float>>();
                    result.local_matrix = glm::make_mat4x4(float_mat.data());
                }

                return result;
            });

            auto cached_primitive = std::unordered_map<size_t, GLTFPrimitive>{};
            gltf_asset->meshes.reserve(model.meshes.size());
            std::ranges::transform(model.meshes, std::back_inserter(gltf_asset->meshes), [&](tinygltf::Mesh const& mesh) -> GLTFMesh {
                auto result = GLTFMesh{};
                result.name = mesh.name;

                result.primitives.reserve(mesh.primitives.size());
                for (auto& primitive: mesh.primitives) {
                    if (primitive.mode != 4) {
                        CNE_ERROR("Current mesh {} does not have triangle mesh, skipped.", mesh.name);
                        continue;
                    }

                    auto target_primitive = &result.primitives.emplace_back();
                    auto has_cache = false;
                    auto key = 0zu;
                    for (auto& attribute: primitive.attributes) {
                        key = core::hash_combine(key, core::hash(attribute.first, attribute.second));
                    }
                    if (auto it = cached_primitive.find(key); it != cached_primitive.end()) {
                        has_cache = true;
                        *target_primitive = it->second;
                    }

                    target_primitive->name = mesh.name;
                    target_primitive->material = primitive.material > -1 ? imported_materials[primitive.material] : nullptr;

                    if (!has_cache) {
                        auto vertices = std::vector<Vertex>{};
                        auto indices  = std::vector<uint32_t>{};
                        if (!primitive.attributes.contains("POSITION")) {
                            CNE_ERROR("Current mesh {} does not have POSITION attribute, skipped.", mesh.name);
                            continue;
                        }

                        // Indices.
                        if (primitive.indices > -1) {
                            auto const accessor    = &model.accessors[primitive.indices];
                            auto const buffer_view = &model.bufferViews[accessor->bufferView];

                            auto get_buffer_data = [&]<typename T>() {
                                auto const buffer = reinterpret_cast<T const*>(&model.buffers[buffer_view->buffer].data[accessor->byteOffset + buffer_view->byteOffset]);
                                return std::span{buffer, buffer + accessor->count};
                            };

                            switch (accessor->componentType) {
                                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                                    indices = get_buffer_data.operator()<uint32_t>() | std::ranges::to<std::vector>();
                                    break;
                                }
                                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                                    indices = get_buffer_data.operator()<uint16_t>() | std::ranges::to<std::vector<uint32_t>>();
                                    break;
                                }
                                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                                    indices = get_buffer_data.operator()<uint8_t>() | std::ranges::to<std::vector<uint32_t>>();
                                    break;
                                }
                                default: {
                                    CNE_ERROR("Current mesh {} has unsupported index type, skipped.", mesh.name);
                                    continue;
                                }
                            }
                        } else {
                            CNE_INFO("Current mesh {} has no indices, use triangle order indexing." , mesh.name);

                            auto const accessor = &model.accessors[primitive.attributes.at("POSITION")];

                            indices = std::views::iota(0u, accessor->count) | std::ranges::to<std::vector>();
                        }

                        // Position.
                        auto mesh_pos_min = math::float3{std::numeric_limits<float>::max()};
                        auto mesh_pos_max = math::float3{std::numeric_limits<float>::min()};
                        auto mesh_pos_avg = math::float3{0.0f};
                        {
                            auto const accessor    = &model.accessors[primitive.attributes.at("POSITION")];
                            auto const buffer_view = &model.bufferViews[accessor->bufferView];

                            auto buffer = reinterpret_cast<float const*>(&model.buffers[buffer_view->buffer].data[accessor->byteOffset + buffer_view->byteOffset]);

                            vertices.reserve(accessor->count);
                            auto pos_accumulate = math::double3{0.0};
                            for (auto index: std::views::iota(0u, accessor->count)) {
                                auto position = math::double3{buffer[0], buffer[1], buffer[2]};
                                pos_accumulate += position;
                                vertices.emplace_back(math::float3{position});
                                buffer += 3;

                                mesh_pos_min = glm::min(mesh_pos_min, vertices.back().position);
                                mesh_pos_max = glm::max(mesh_pos_max, vertices.back().position);
                            }

                            mesh_pos_avg = math::float3{pos_accumulate / (double) accessor->count};
                        }

                        // Normal.
                        if (primitive.attributes.contains("NORMAL")) {
                            auto const accessor    = &model.accessors[primitive.attributes.at("NORMAL")];
                            auto const buffer_view = &model.bufferViews[accessor->bufferView];

                            auto buffer = reinterpret_cast<float const*>(&model.buffers[buffer_view->buffer].data[accessor->byteOffset + buffer_view->byteOffset]);

                            for (auto index: std::views::iota(0u, accessor->count)) {
                                vertices[index].normal = math::float3{buffer[0], buffer[1], buffer[2]};
                                buffer += 3;
                            }
                        } else {
                            for (auto i: std::views::iota(0u, indices.size())) {
                                auto idx_0 = indices[i + 0];
                                auto idx_1 = indices[i + 1];
                                auto idx_2 = indices[i + 2];

                                auto const& pos_0 = vertices[idx_0].position;
                                auto const& pos_1 = vertices[idx_1].position;
                                auto const& pos_2 = vertices[idx_2].position;

                                auto const v_1 = glm::normalize(pos_2 - pos_0);
                                auto const v_2 = glm::normalize(pos_1 - pos_0);
                                auto const normal = glm::normalize(glm::cross(v_1, v_2));

                                vertices[idx_0].normal = normal;
                                vertices[idx_1].normal = normal;
                                vertices[idx_2].normal = normal;
                            }
                        }

                        // Texcoord_0.
                        auto const has_texcoord_0 = primitive.attributes.contains("TEXCOORD_0");
                        if (has_texcoord_0) {
                            auto const accessor    = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
                            auto const buffer_view = &model.bufferViews[accessor->bufferView];

                            auto buffer = reinterpret_cast<float const*>(&model.buffers[buffer_view->buffer].data[accessor->byteOffset + buffer_view->byteOffset]);

                            for (auto index: std::views::iota(0u, accessor->count)) {
                                vertices[index].uv_0 = math::float2{buffer[0], buffer[1]};
                                buffer += 2;
                            }
                        } else {
                            for (auto index: std::views::iota(0u, vertices.size())) {
                                vertices[index].uv_0 = math::float2{0.0f, 0.0f};
                            }
                        }

                        // Tangent.
                        if (has_texcoord_0) {
                            if (primitive.attributes.contains("TANGENT")) {
                                auto const accessor    = &model.accessors[primitive.attributes.at("TANGENT")];
                                auto const buffer_view = &model.bufferViews[accessor->bufferView];

                                auto buffer = reinterpret_cast<float const*>(&model.buffers[buffer_view->buffer].data[accessor->byteOffset + buffer_view->byteOffset]);

                                for (auto index: std::views::iota(0u, accessor->count)) {
                                    vertices[index].tangent = math::float4{buffer[0], buffer[1], buffer[2], buffer[3]};
                                    buffer += 4;
                                }
                            } else {
                                // TODO: generate tangents.
                                CNE_ERROR("Tangent not found in {}", mesh.name);

                            }
                        } else {
                            for (auto index: std::views::iota(0u, vertices.size())) {
                                vertices[index].tangent = math::float4{1.0f, 0.0f, 0.0f, 1.0f};
                            }
                        }

                        // Smooth normals.
                        if (config->generate_smooth_normals) {
                            auto new_normals = std::vector<math::float3>{vertices.size()};

                            for (auto index = 0zu; index < indices.size(); index += 3) {
                                auto idx_0 = indices[index + 0];
                                auto idx_1 = indices[index + 1];
                                auto idx_2 = indices[index + 2];

                                auto const& n_0 = vertices[idx_0].normal;
                                auto const& n_1 = vertices[idx_1].normal;
                                auto const& n_2 = vertices[idx_2].normal;

                                new_normals[idx_0] += n_0;
                                new_normals[idx_1] += n_1;
                                new_normals[idx_2] += n_2;
                            }

                            for (auto& normal: new_normals) {
                                normal = glm::normalize(normal);
                            }

                            for (auto index: std::views::iota(0u, vertices.size())) {
                                vertices[index].smooth_normal = new_normals[index];
                            }
                        }

                        //TODO: Optional attributes. Texcoord1 and color

                        target_primitive->vertex_offset                = gltf_data.positions.size();
                        target_primitive->lod_0_indices_offset         = gltf_data.lod_0_indices.size();

                        CNE_ASSERT(target_primitive->vertex_offset == gltf_data.normals.size());
                        CNE_ASSERT(target_primitive->vertex_offset == gltf_data.texcoords_0.size());
                        CNE_ASSERT(target_primitive->vertex_offset == gltf_data.tangents.size());

                        target_primitive->color_offset                 = gltf_data.colors.size();
                        target_primitive->smooth_normal_offset         = gltf_data.smooth_normals.size();
                        target_primitive->texcoord_1_offset            = gltf_data.smooth_normals.size();

                        target_primitive->meshlet_offset               = gltf_data.meshlets.size();
                        target_primitive->meshlet_group_offset         = gltf_data.meshlet_groups.size();
                        target_primitive->meshlet_group_indices_offset = gltf_data.meshlet_group_indices.size();
                        target_primitive->bvh_node_offset              = gltf_data.bvh_nodes.size();
                        target_primitive->meshlet_group_count          = gltf_data.meshlet_groups.size(); // ??

                        target_primitive->pos_min                      = mesh_pos_min;
                        target_primitive->pos_max                      = mesh_pos_max;
                        target_primitive->pos_center                   = mesh_pos_avg;

                        auto nanite_builder = NaniteBuilder{};
                        nanite_builder.indices = std::move(indices);
                        nanite_builder.vertices = std::move(vertices);
                        nanite_builder.cone_weight = 0.7f;
                        {
                            auto container = nanite_builder.build();
                            target_primitive->bvh_node_count = container.bvh_nodes[0].bvh_node_count;
                            target_primitive->meshlet_group_count = container.meshlet_groups.size();

                            gltf_data.meshlet_groups.append_range(container.meshlet_groups);
                            gltf_data.meshlet_group_indices.append_range(container.meshlet_group_indices);
                            gltf_data.bvh_nodes.append_range(container.bvh_nodes);

                            for (auto& meshlet: container.meshlets) {
                                auto const data_offset = gltf_data.meshlet_datas.size();

                                for (auto i: std::views::iota(0u, meshlet.vertex_count)) {
                                    gltf_data.meshlet_datas.emplace_back(container.vertices[meshlet.vertex_offset + i]);
                                }

                                for (auto i: std::views::iota(0u, meshlet.triangle_count)) {
                                    auto id_0 = container.triangles[meshlet.triangle_offset + i * 3 + 0];
                                    auto id_1 = container.triangles[meshlet.triangle_offset + i * 3 + 1];
                                    auto id_2 = container.triangles[meshlet.triangle_offset + i * 3 + 2];

                                    auto idx = (uint32_t) id_0;
                                    idx |= (uint32_t) (id_1 << 8);
                                    idx |= (uint32_t) (id_2 << 16);

                                    gltf_data.meshlet_datas.emplace_back(idx);
                                }

                                gltf_data.meshlets.emplace_back(meshlet.to_gltf_meshlet(data_offset));
                            }

                            target_primitive->lod_0_meshlet_count = std::ranges::count_if(container.meshlets, [](Meshlet const& meshlet) -> bool {
                                return meshlet.lod == 0;
                            });
                        }

                        target_primitive->vertex_count = nanite_builder.vertices.size();
                        target_primitive->lod_0_indices_count = nanite_builder.indices.size();

                        // TODO:
                        target_primitive->has_color = false;
                        target_primitive->has_smooth_normal = false;
                        target_primitive->has_texcoord_1 = false;

                        // Fill lod0 indices. (Used for voxelize, ray tracing or sdf generation, etc.)
                        gltf_data.lod_0_indices.append_range(nanite_builder.indices);

                        for (auto& vertex: nanite_builder.vertices) {
                            gltf_data.positions.emplace_back(vertex.position);
                            gltf_data.normals.emplace_back(vertex.normal);
                            gltf_data.texcoords_0.emplace_back(vertex.uv_0);
                            gltf_data.tangents.emplace_back(vertex.tangent);

                            if (target_primitive->has_color) {
                                gltf_data.colors.emplace_back(vertex.color);
                            }
                            if (target_primitive->has_smooth_normal) {
                                gltf_data.smooth_normals.emplace_back(vertex.smooth_normal);
                            }
                            if (target_primitive->has_texcoord_1) {
                                gltf_data.texcoords_1.emplace_back(vertex.uv_1);
                            }
                        }

                        cached_primitive[key] = *target_primitive;
                    }
                }

                return result;
            });
        }

        auto asset_ptr = asset_manager->create_asset(gltf_store_path, std::move(gltf_asset));
        auto device = platform::Engine::current()->device.get();
        auto async_uploader = device->async_uploader();

        using namespace rhi;
        async_uploader->add_task(
            [asset_ptr, device] (this auto&, RHICommandList* cmd_list) {
                auto gpu_data = &asset_ptr->gpu_data;
                auto buffer_info = BufferCreateInfo{};
                buffer_info.type       = EBufferType::gpu_only;

                auto index_buffer_data = std::as_writable_bytes(std::span(asset_ptr->data.lod_0_indices));
                buffer_info.size_bytes = index_buffer_data.size();
                buffer_info.stride     = sizeof(uint32_t);
                buffer_info.usage      = EBufferUsage::index | EBufferUsage::transfer_dst;
                gpu_data->lod_0_indices_buffer = device->create_buffer(std::format("{}_lod_0_indices", asset_ptr->path), &buffer_info);
                cmd_list->write_buffer(gpu_data->lod_0_indices_buffer, index_buffer_data, 0);

                buffer_info.usage = EBufferUsage::vertex | EBufferUsage::transfer_dst;

                auto position_buffer_data = std::as_writable_bytes(std::span(asset_ptr->data.positions));
                buffer_info.size_bytes = position_buffer_data.size();
                buffer_info.stride     = sizeof(math::float3);
                gpu_data->positions_buffer = device->create_buffer(std::format("{}_positions", asset_ptr->path), &buffer_info);
                cmd_list->write_buffer(gpu_data->positions_buffer, position_buffer_data, 0);

                auto normal_buffer_data = std::as_writable_bytes(std::span(asset_ptr->data.normals));
                buffer_info.size_bytes = normal_buffer_data.size();
                buffer_info.stride     = sizeof(math::float3);
                gpu_data->normals_buffer = device->create_buffer(std::format("{}_normals", asset_ptr->path), &buffer_info);
                cmd_list->write_buffer(gpu_data->normals_buffer, normal_buffer_data, 0);

                auto texcoord_0_buffer_data = std::as_writable_bytes(std::span(asset_ptr->data.texcoords_0));
                buffer_info.size_bytes = texcoord_0_buffer_data.size();
                buffer_info.stride     = sizeof(math::float2);
                gpu_data->texcoords_0_buffer = device->create_buffer(std::format("{}_texcoords_0", asset_ptr->path), &buffer_info);
                cmd_list->write_buffer(gpu_data->texcoords_0_buffer, texcoord_0_buffer_data, 0);

                auto tangent_buffer_data = std::as_writable_bytes(std::span(asset_ptr->data.tangents));
                buffer_info.size_bytes = tangent_buffer_data.size();
                buffer_info.stride     = sizeof(math::float4);
                gpu_data->tangents_buffer = device->create_buffer(std::format("{}_tangents", asset_ptr->path), &buffer_info);
                cmd_list->write_buffer(gpu_data->tangents_buffer, tangent_buffer_data, 0);

                cmd_list->set_buffer_state(gpu_data->lod_0_indices_buffer, EResourceStates::index_buffer);
                cmd_list->set_buffer_state(gpu_data->positions_buffer, EResourceStates::vertex_buffer);
                cmd_list->set_buffer_state(gpu_data->normals_buffer, EResourceStates::vertex_buffer);
                cmd_list->set_buffer_state(gpu_data->texcoords_0_buffer, EResourceStates::vertex_buffer);
                cmd_list->set_buffer_state(gpu_data->tangents_buffer, EResourceStates::vertex_buffer);
                cmd_list->commit_barriers(EQueueType::transfer, EQueueType::graphics);

            },
            [asset_ptr] () { asset_ptr->gpu_data.uploading = false; }
        );



        return asset_ptr;
    }
}
