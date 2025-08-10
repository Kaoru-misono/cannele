struct Barycentrics
{
    float3 interpolation;
    float3 ddx;
    float3 ddy;
};

// Unreal Engine 5 Nanite improved perspective correct barycentric coordinates and partial derivatives using screen derivatives.
Barycentrics calculate_triangle_barycentrics(float2 pixel_clip, float4 point_clip_0, float4 point_clip_1, float4 point_clip_2, float2 inv_view_size)
{
	Barycentrics barycentrics;

	const float3 rcp_w = rcp(float3(point_clip_0.w, point_clip_1.w, point_clip_2.w));
	const float3 pos_0 = point_clip_0.xyz * rcp_w.x;
	const float3 pos_1 = point_clip_1.xyz * rcp_w.y;
	const float3 pos_2 = point_clip_2.xyz * rcp_w.z;

	const float3 pos_120_x = float3(pos_1.x, pos_2.x, pos_0.x);
	const float3 pos_120_y = float3(pos_1.y, pos_2.y, pos_0.y);
	const float3 pos_201_x = float3(pos_2.x, pos_0.x, pos_1.x);
	const float3 pos_201_y = float3(pos_2.y, pos_0.y, pos_1.y);

	const float3 C_dx = pos_201_y - pos_120_y;
	const float3 C_dy = pos_120_x - pos_201_x;

	const float3 C = C_dx * (pixel_clip.x - pos_120_x) + C_dy * (pixel_clip.y - pos_120_y);	// Evaluate the 3 edge functions
	const float3 G = C * rcp_w;

	const float H = dot(C, rcp_w);
	const float rcp_H = rcp(H);

	// UVW = C * rcp_w / dot(C, rcp_w)
	barycentrics.interpolation = G * rcp_H;

	// Texture coordinate derivatives:
	// UVW = G / H where G = C * rcp_w and H = dot(C, rcp_w)
	// UVW' = (G' * H - G * H') / H^2
	// float2 TexCoordDX = UVW_dx.y * TexCoord10 + UVW_dx.z * TexCoord20;
	// float2 TexCoordDY = UVW_dy.y * TexCoord10 + UVW_dy.z * TexCoord20;
	const float3 G_dx = C_dx * rcp_w;
	const float3 G_dy = C_dy * rcp_w;

	const float H_dx = dot(C_dx, rcp_w);
	const float H_dy = dot(C_dy, rcp_w);

	barycentrics.ddx = (G_dx * H - G * H_dx) * (rcp_H * rcp_H) * ( 2.0f * inv_view_size.x);
	barycentrics.ddy = (G_dy * H - G * H_dy) * (rcp_H * rcp_H) * (-2.0f * inv_view_size.y);

	return barycentrics;
}

float2 screen_to_ndc(float2 uv)
{
    uv.x = 2.0 * (uv.x - 0.5);
    uv.y = 2.0 * (uv.y - 0.5);

    return uv;
}
