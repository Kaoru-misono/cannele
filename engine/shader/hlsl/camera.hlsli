#ifndef CAMERA_HLSLI
#define CAMERA_HLSLI

#include "bindless.hlsli"
#include "../hpp/view_data.hlsl.hpp"

T_BINDLESS_DECLARE(StructuredBuffer, STORAGE_BUFFER_BINDING, t, PerFrameCameraView);

#define LOAD_CAMERA_VIEW(Index) T_BINDLESS(StructuredBuffer, PerFrameCameraView, push_constants.camera_view_id).Load(0)

#endif // !CAMERA_HLSLI
