#pragma once

namespace cannele
{
    enum struct EDescriptorType
    {
        // StructuredBuffer<T>
        // RWStructuredBuffer<T>
        // ByteAddressBuffer
        // RWByteAddressBuffer
        storage_buffer,

        // ConstantBuffer<T>
        uniform_buffer,

        // Texture2D<T>
        // Texture3D<T>
        // TextureCube<T>
        sampled_texture,

        // RWTexture2D<T>
        // RWTexture3D<T>
        storage_texture,

        // SamplerState,
        // SamplerComparisonState
        sampler,

        last,
    };
}

