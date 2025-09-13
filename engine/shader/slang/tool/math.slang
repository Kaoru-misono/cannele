#pragma once

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

// Quad schedule style, fake pixel shader dispatch style.
// Input-> [0, 63]
//
// Output:
//  00 01 08 09 10 11 18 19
//  02 03 0a 0b 12 13 1a 1b
//  04 05 0c 0d 14 15 1c 1d
//  06 07 0e 0f 16 17 1e 1f
//  20 21 28 29 30 31 38 39
//  22 23 2a 2b 32 33 3a 3b
//  24 25 2c 2d 34 35 3c 3d
//  26 27 2e 2f 36 37 3e 3f
uint2 remap8x8(uint tid)
{
    return uint2((((tid >> 2) & 0x7) & 0xFFFE) | (tid & 0x1), ((tid >> 1) & 0x3) | (((tid >> 3) & 0x7) & 0xFFFC));
}

// Quad schedule style, fake pixel shader dispatch style.
// Input-> [0, 255]
//
// Output:
//  00 01 08 09 10 11 18 19 40 41 48 49 50 51 58 59
//  02 03 0a 0b 12 13 1a 1b 42 43 4a 4b 52 53 5a 5b
//  04 05 0c 0d 14 15 1c 1d 44 45 4c 4d 54 55 5c 5d
//  06 07 0e 0f 16 17 1e 1f 46 47 4e 4f 56 57 5e 5f
//  20 21 28 29 30 31 38 39 60 61 68 69 70 71 78 79
//  22 23 2a 2b 32 33 3a 3b 62 63 6a 6b 72 73 7a 7b
//  24 25 2c 2d 34 35 3c 3d 64 65 6c 6d 74 75 7c 7d
//  26 27 2e 2f 36 37 3e 3f 66 67 6e 6f 76 77 7e 7f
//  80 81 88 89 90 91 98 99 c0 c1 c8 c9 d0 d1 d8 d9
//  82 83 8a 8b 92 93 9a 9b c2 c3 ca cb d2 d3 da db
//  84 85 8c 8d 94 95 9c 9d c4 c5 cc cd d4 d5 dc dd
//  86 87 8e 8f 96 97 9e 9f c6 c7 ce cf d6 d7 de df
//  a0 a1 a8 a9 b0 b1 b8 b9 e0 e1 e8 e9 f0 f1 f8 f9
//  a2 a3 aa ab b2 b3 ba bb e2 e3 ea eb f2 f3 fa fb
//  a4 a5 ac ad b4 b5 bc bd e4 e5 ec ed f4 f5 fc fd
//  a6 a7 ae af b6 b7 be bf e6 e7 ee ef f6 f7 fe ff
uint2 remap16x16(uint tid)
{
    uint2 xy;
    uint2 basic = remap8x8(tid % 64);

    // offset x:   0-63  is 0
    //           64-127  is 8
    //           128-191 is 0
    //           192-255 is 8
    xy.x = basic.x + 8 * ((tid >> 6) % 2);

    // offset y:   0-63  is 0
    //           64-127  is 0
    //           128-191 is 8
    //           192-255 is 8
    xy.y = basic.y + 8 * ((tid >> 7));
    return xy;
}
