struct MsToPs
{
    float4 position: SV_POSITION;
    float4 color: COLOR;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void main_mesh_draw_ms(
    in uint gtid: SV_GroupThreadID,
    in uint gid: SV_GroupID,
    out vertices MsToPs verts[3],
    out indices uint3 prims[1]
)
{
    SetMeshOutputCounts(3, 1);

    if (gtid < 3) {
        verts[gtid].position = float4(
            (gtid == 0) ? -0.5 : ((gtid == 1) ? 0.5 : 0.0),
            (gtid == 0) ? -0.5 : ((gtid == 1) ? -0.5 : 0.5),
            0.0, 1.0
        );
        verts[gtid].color = float4(
            (gtid == 0) ? 1.0 : 0.0,
            (gtid == 1) ? 1.0 : 0.0,
            (gtid == 2) ? 1.0 : 0.0,
            1.0
        );
    }

    if (gtid == 0) {
        prims[0] = uint3(0, 1, 2);
    }
}

[shader("fragment")]
float4 main_mesh_draw_fs(MsToPs input): SV_TARGET
{
    return input.color;
}
