#pragma once

#include <core/resource/asset.hpp>
#include <graphics/RHI/resource.hpp>
#include <graphics/resource/texture_asset.hpp>

#include <mesh.slang.hpp>
#include <scene.slang.hpp>

namespace cannele::inline scene::resource
{
    enum struct EBlendMode: uint8_t
    {
        opaque,
        alpha_test,
        alpha_blend,
    };

    struct GLTFMaterialAssetImportConfig final
    {
        std::string_view path{};

        std::vector<std::byte> data{};

        bool is_srgb{false};

        bool generate_mipmaps{false};
    };

    struct GLTFMaterialAsset final: core::resource::IAsset
    {
        using AssetMetadata = core::resource::AssetMetadata;
        using TextureAsset = graphics::resource::TextureAsset;

        std::string path{};

        static const AssetMetadata metadata;

        struct TextureInfo final
        {
            TextureAsset* texture{};
            int32_t texcoord{0};
            rhi::SamplerCreateInfo sampler{};
        };

        math::float4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
        TextureInfo base_color_texture{};

        float metallic_factor{1.0f};
        float roughness_factor{1.0f};
        TextureInfo metallic_roughness_texture{};

        math::float3 emissive_factor{0.0f, 0.0f, 0.0f};
        TextureInfo emissive_texture{};

        EBlendMode blend_mode{EBlendMode::opaque};

        float alpha_cutoff{0.5f};
        bool double_sided{false};

        float normal_texture_scale{1.0f};
        TextureInfo normal_texture{};
        bool exist_occlusion{false};
        TextureInfo occlusion_texture{};
        float occlusion_texture_strength{1.0f};

        auto name() -> std::string_view override { return {}; }

        auto save(std::string_view path) -> bool override { return false; }

        auto load(std::string_view path) -> std::unique_ptr<IAsset> override { return nullptr; }
    };

    struct GLTFPrimitive final
    {
        std::string name{};

        GLTFMaterialAsset* material{};

        uint32_t meshlet_offset{0};
        uint32_t lod_0_meshlet_count{0};
        uint32_t bvh_node_count{0};
        uint32_t meshlet_group_count{0};

        uint32_t bvh_node_offset{0};
        uint32_t meshlet_group_offset{0};
        uint32_t meshlet_group_indices_offset{0};

        uint32_t vertex_offset{0};
        uint32_t vertex_count{0};

        uint32_t lod_0_indices_offset{0};
        uint32_t lod_0_indices_count{0};

        bool has_color{false};
        bool has_smooth_normal{false};
        bool has_texcoord_1{false};

        uint32_t color_offset{0};
        uint32_t smooth_normal_offset{0};
        uint32_t texcoord_1_offset{0};

        math::float3 position_min{};
        math::float3 position_max{};
        math::float3 pos_center{};

        auto gpu_buffer() -> GltfPrimitiveInfo
        {
            return GltfPrimitiveInfo{
                .position_min = position_min,
                .vertex_offset = vertex_offset,
                .position_max = position_max,
                .vertex_count = vertex_count,
                .position_average = pos_center,
                .pad = 0,
                .meshlet_offset = meshlet_offset,
                .color_0_offset = color_offset,
                .smooth_normal_offset = smooth_normal_offset,
                .texcoord_1_offset = texcoord_1_offset,
                .bvh_node_offset = bvh_node_offset,
                .meshlet_group_offset = meshlet_group_offset,
                .meshlet_group_indices_offset = meshlet_group_indices_offset,
                .meshlet_group_count = meshlet_group_count,
                .lod_0_indices_offset = lod_0_indices_offset,
                .lod_0_indices_count = lod_0_indices_count,
                .lod_0_meshlet_count = lod_0_meshlet_count,
            };
        }
    };

    struct GLTFMesh final
    {
        std::string name{};

        std::vector<GLTFPrimitive> primitives{};
    };

    struct GLTFNode final
    {
        std::string name{};

        int32_t mesh{-1};
        math::float4x4 local_matrix{1.0f};
        std::vector<int32_t> childrens{};
    };

    struct GLTFScene final
    {
        std::string name{};

        std::vector<int32_t> nodes{};
    };

    struct GLTFData final
    {
        std::vector<GLTFMeshlet> meshlets{};
        std::vector<uint32_t> meshlet_datas{}; // Store all triangle indices of meshlets
        std::vector<GLTFBVHNode> bvh_nodes{};
        std::vector<GLTFMeshletGroup> meshlet_groups{};
        std::vector<uint32_t> meshlet_group_indices{};

        std::vector<uint32_t> lod_0_indices{};

        std::vector<math::float3> positions{};
        std::vector<math::float3> normals{};
        std::vector<math::float2> texcoords_0{};
        std::vector<math::float4> tangents{};

        std::vector<math::float2> texcoords_1{};
        std::vector<math::float4> colors{};
        std::vector<math::float3> smooth_normals{};
    };

    struct GLTFGpuData final: rhi::IUploadResource
    {
        rhi::BufferHandle lod_0_indices{};
        rhi::BufferHandle positions{};
        rhi::BufferHandle normals{};
        rhi::BufferHandle texcoords_0{};
        rhi::BufferHandle tangents{};
        rhi::BufferHandle meshlets{};
        rhi::BufferHandle meshlet_data{};
        rhi::BufferHandle bvh_nodes{};
        rhi::BufferHandle meshlet_groups{};
        rhi::BufferHandle meshlet_group_indices{};

        rhi::BufferHandle texcoords_1{};
        rhi::BufferHandle colors{};
        rhi::BufferHandle smooth_normals{};

        auto primitive_data_buffers() -> GltfPrimitiveDataBuffers;
    };

    struct GLTFAssetImportConfig final
    {
        std::string import_path{};
        std::string store_path{};

        bool generate_smooth_normals{false};
        float meshlet_cone_weight{0.7f};
    };

    struct GLTFAsset: core::resource::IAsset
    {
        using AssetMetadata = core::resource::AssetMetadata;

        std::string path{};

        int32_t default_scene{-1};

        std::vector<GLTFNode> nodes{};
        std::vector<GLTFMesh> meshes{};
        std::vector<GLTFScene> scenes{};
        GLTFData data{};
        GLTFGpuData gpu_data{};

        static const AssetMetadata metadata;

        static auto import_from_config(GLTFAssetImportConfig* config) -> GLTFAsset*;

        auto name() -> std::string_view override { return {}; }
        auto save(std::string_view path) -> bool override { return false; }
        auto load(std::string_view path) -> std::unique_ptr<IAsset> override { return nullptr; }
    };
}
