#include "nanite_rendering.hpp"

#include <graphics/RHI/RHI.hpp>
#include <nanite.slang.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        REGISTER_SHADER_COMPOSITION(NaniteInstanceCullingCS, "nanite_instance_culling", "main_instance_culling_cs", EShaderStage::compute);
        REGISTER_SHADER_COMPOSITION(NaniteRenderMS, "nanite_render", "main_nanite_mesh_pass_ms", EShaderStage::mesh);
        REGISTER_SHADER_COMPOSITION(NaniteRenderFS, "nanite_render", "main_nanite_visibility_buffer_pass_fs", EShaderStage::fragment);
        REGISTER_SHADER_COMPOSITION(NaniteVisualizeFS, "nanite_visualization", "main_nanite_visualize_fs", EShaderStage::fragment);
    }

    auto instance_culling(rhi::CommandListHandle command_list, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>
    {
        using namespace rhi;

        auto device = command_list->device();

        auto buffer_info = BufferCreateInfo{
            .size_bytes = sizeof(uint),
            .type = EBufferType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
        };
        auto cluster_group_count_buffer = device->create_buffer("Cluster Group Count Buffer", &buffer_info);

        auto cluster_group_count = context->asset->data.meshlet_groups.size();
        auto buffer_info2 = BufferCreateInfo{
            .size_bytes = sizeof(uint2) * cluster_group_count,
            .type = EBufferType::gpu_only,
            .usage = EBufferUsage::storage | EBufferUsage::transfer_dst,
        };
        auto cluster_group_id_buffer = device->create_buffer("Cluster Group ID Buffer", &buffer_info2);

        command_list->clear_buffer_uint(cluster_group_count_buffer);
        auto push_constant_data = std::vector<std::byte>{sizeof(InstanceCullingPushConstant), std::byte(0)};
        auto push_constant = reinterpret_cast<InstanceCullingPushConstant*>(push_constant_data.data());
        push_constant->frame_view_buffer = context->frame_view_buffer->descriptor_handle();
        push_constant->cluster_group_count_buffer = cluster_group_count_buffer->descriptor_handle();
        push_constant->cluster_group_id_buffer = cluster_group_id_buffer->descriptor_handle();

        auto compute_pipeline_info = ComputePipelineCreateInfo{
            .compute_shader = device->shader_factory()->get_shader<NaniteInstanceCullingCS>()
        };
        auto compute_pipeline = device->create_compute_pipeline("Instance Culling Pipeline", &compute_pipeline_info);
        auto compute_state = ComputeState{
            .pipeline = compute_pipeline,
        };
        command_list->set_compute_state(&compute_state);
        command_list->push_constants(push_constant_data);
        command_list->dispatch(1, 1, 1);

        return {cluster_group_count_buffer, cluster_group_id_buffer};
    }
}
