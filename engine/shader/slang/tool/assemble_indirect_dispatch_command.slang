import bindless;

[shader("compute")]
[numthreads(1, 1, 1)]
void main_assemble_indirect_dispatch_cmd_cs(
    uniform StructuredBufferHandle<uint> count_buffer,
    uniform RWStructuredBufferHandle<uint3> cmds_buffer,
    uniform uint group_size,
    uniform uint offset
)
{
    uint total_meshlet_count = count_buffer[0];

    uint3 indirect_cmd;
    indirect_cmd.x = (total_meshlet_count + group_size - 1) / group_size;
    indirect_cmd.y = 1;
    indirect_cmd.z = 1;

    cmds_buffer[offset] = indirect_cmd;
}
