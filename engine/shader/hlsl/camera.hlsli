#ifndef CAMERA_HLSLI
#define CAMERA_HLSLI

#include "bindless.hlsli"
#include "../hpp/view_data.hlsl.hpp"

#define LOAD_CAMERA_VIEW(Index) BATL(PerFrameCameraView, Index, 0)

#endif // !CAMERA_HLSLI
