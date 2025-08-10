#ifndef MATERIAL_HLSLI
#define MATERIAL_HLSLI

struct GBufferInfo
{
    float3 position;
    float3 color;
    float3 emissive;

    float4 base_color;
    float3 vertex_normal;
    float3 pixel_normal;

    float2 motion_vector;

    float material_AO;
    float roughness;
    float metallic;
};

void load_gltf_metallic_roughness_pbr_material(
    in GLTFMaterialGPUData material_info,
    in TriangleMiscInfo triangle_info,
    in uint2 dispatch_pos,
    in float2 work_texel_size,
    out GBufferinfo gbuffer
)
{
    float2 screen_uv = (dispatch_pos + 0.5) * work_texel_size;

    Barycentrics barycentrics = calculate_triangle_barycentrics(
        screen_to_ndc(screen_uv),
        triangle_info.position_clip_space[0],
        triangle_info.position_clip_space[1],
        triangle_info.position_clip_space[2],
        work_texel_size
    );

    float3 barycentric = barycentrics.interpolation;
    float3 ddx = barycentrics.ddx;
    float3 ddy = barycentrics.ddy;

    float2 mesh_uv     = triangle_info.uv[0] * barycentric.x + triangle_info.uv[1] * barycentric.y + triangle_info.uv[2] * barycentric.z;
    float2 mesh_uv_ddx = triangle_info.uv[0] * ddx.x         + triangle_info.uv[1] * ddx.y         + triangle_info.uv[2] * ddx.z;
    float2 mesh_uv_ddy = triangle_info.uv[0] * ddy.x         + triangle_info.uv[1] * ddy.y         + triangle_info.uv[2] * ddy.z;

    float3 mesh_pos_clip_space = triangle_info.position_clip_space_no_jitter[0] * barycentric.x + triangle_info.position_clip_space_no_jitter[1] * barycentric.y + triangle_info.position_clip_space_no_jitter[1] * barycentric.z;
    float3 mesh_pos_clip_space_last_frame = triangle_info.position_clip_space_no_jitter_last_frame[0] * barycentric.x + triangle_info.position_clip_space_no_jitter_last_frame[1] * barycentric.y + triangle_info.position_clip_space_no_jitter_last_frame[1] * barycentric.z;

    gbuffer.motion_vector = (mesh_pos_clip_space_last_frame.xy / mesh_pos_clip_space_last_frame.z - mesh_pos_clip_space.xy / mesh_pos_clip_space.z) * float2(0.5, -0.5);

    gbuffer.position = triangle_info.position[0] * barycentric.x + triangle_info.position[1] * barycentric.y + triangle_info.position[2] * barycentric.z;

    float4 base_color;
    Texture2D<float4> base_color_texture = T_BINDLESS(Texture2D, float4, material_info.base_color_texture);
    SamplerState base_color_sampler = BINDLESS(SamplerState, material_info.base_color_sampler);
    base_color = base_color_texture.SampleGrad(base_color_sampler, mesh_uv, mesh_uv_ddx, mesh_uv_ddy) * material_info.base_color_factor;
    // TODO: base_color.xyz = mul(sRGB_2_AP1, base_color.xyz);
    gbuffer.base_color = base_color;

    float3 emissive_color;
    Texture2D<float4> emissive_texture = T_BINDLESS(Texture2D, float4, material_info.emissive_texture);
    SamplerState emissive_sampler = BINDLESS(SamplerState, material_info.emissive_sampler);
    emissive_color = emissive_texture.SampleGrad(emissive_sampler, mesh_uv, mesh_uv_ddx, mesh_uv_ddy).xyz * material_info.emissive_factor;
    gbuffer.color = 0.0;
    gbuffer.emissive_color = emissive_color;

    // Vertex normal.
    // PERFORMANCE: normalize can remove here because we do barycentric lerp instead of scanline raster.
    float3 vert_normal = (triangleInfo.normal[0] * barycentric.x + triangleInfo.normal[1] * barycentric.y + triangleInfo.normal[2] * barycentric.z);

    float3 normal;

    bool has_normal_texture = material_info.normal_texture != -1;

    [branch]
    if (has_normal_texture) {
        float3 tangent = triangle_info.tangent[0] * barycentric.x + triangle_info.tangent[1] * barycentric.y + triangle_info.tangent[2] * barycentric.z;
        float3 bitangent = triangle_info.bitangent[0] * barycentric.x + triangle_info.bitangent[1] * barycentric.y + triangle_info.bitangent[2] * barycentric.z;

        float3x3 TBN = float3x3(tangent, bitangent, vert_normal);

        Texture2D<float2> normal_texture = T_BINDLESS(Texture2D, float2, material_info.normal_texture);
        SamplerState normal_sampler = BINDLESS(SamplerState, material_info.normal_sampler);

        float3 xyz;
        xyz.xy = normal_texture.SampleGrad(normal_sampler, mesh_uv, mesh_uv_ddx, mesh_uv_ddy) * 2.0 - 1.0;
        xyz.z = sqrt(1.0 - dot(xyz.xy, xyz.xy));

        xyz.xy *= material_info.normal_factor_scale;

        normal = mul(TBN, normalize(xyz));
    } else {
        normal = vert_normal;
    }
    gbuffer.vertex_normal = vert_normal;
    gbuffer.pixel_normal = normal;

    bool has_ao_roughness_metallic_texture = material_info.metallic_roughness_texture != -1;
    if (has_ao_roughness_metallic_texture) {
        Texture2D<float4> metallic_roughness_texture = TBindless(Texture2D, float4, materialInfo.metallic_roughness_texture);
        SamplerState metallic_roughness_sampler      = Bindless(SamplerState, materialInfo.metallicRoughnessSampler);

        float4 metallic_roughness_data = metallic_roughness_texture.SampleGrad(metallic_roughness_sampler, meshUv, meshUv_ddx, meshUv_ddy);

        // NOTE: Some DCC store sqrt distribution roughness when export to keep higher precision when near zero. (called perceptual roughness).
        //       BTW, we can't control the artist behavior (all media assets download from sketchfab), so, just set roughness is roughness.

        // The metallic-roughness texture. The metalness values are sampled from the B channel. The roughness values are sampled from the G channel.
        // These values **MUST** be encoded with a linear transfer function.
        // If other channels are present (R or A), they **MUST** be ignored for metallic-roughness calculations. When undefined, the texture **MUST** be sampled as having `1.0` in G and B components.

        gbuffer.roughness = metallic_roughness_data.g;
        gbuffer.metallic = metallic_roughness_data.b;
        gbuffer.material_ao = material_info.has_occlusion ? material_info.occlusion_texture_strength * metallic_roughness_data.r : 1.0;
    } else {
        gbuffer.roughness = material_info.roughness_factor;
        gbuffer.metallic = get_gallback_metallic(material_info.metallic_factor);
        gbuffer.material_ao = 1.0;
    }
}

#endif // !MATERIAL_HLSLI
