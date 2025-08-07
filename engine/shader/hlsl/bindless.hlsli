#ifndef SHADER_BINDLESS_HLSLI
#define SHADER_BINDLESS_HLSLI

#include "../hpp/binding.hlsl.hpp"

// Blog: https://www.lei.chat/posts/hlsl-for-vulkan-resources/
// Type alias table between HLSL and GLSL.
/*
    HLSL Type	            DirectX Descriptor Type	 Vulkan Descriptor Type	 GLSL Type
    SamplerState	        Sampler	                 Sampler	             uniform sampler*
    SamplerComparisonState	Sampler	                 Sampler	             uniform sampler*Shadow
    Buffer	                SRV	                     Uniform Texel Buffer	 uniform samplerBuffer
    RWBuffer	            UAV	                     Storage Texel Buffer	 uniform imageBuffer
    Texture*	            SRV	                     Sampled Image	         uniform texture*
    RWTexture*	            UAV	                     Storage Image	         uniform image*
    cbuffer	                CBV	                     Uniform Buffer      	 uniform { ... }
    ConstantBuffer	        CBV	                     Uniform Buffer	         uniform { ... }
    tbuffer	                CBV	                     Storage Buffer
    TextureBuffer	        CBV	                     Storage Buffer
    StructuredBuffer	    SRV	                     Storage Buffer	         buffer { ... }
    RWStructuredBuffer	    UAV	                     Storage Buffer	         buffer { ... }
    ByteAddressBuffer	    SRV	                     Storage Buffer
    RWByteAddressBuffer	    UAV	                     Storage Buffer
    AppendStructuredBuffer	UAV	                     Storage Buffer
    ConsumeStructuredBuffer	UAV	                     Storage Buffer
*/

#define STORAGE_BUFFER_BINDING 0
#define UNIFORM_BUFFER_BINDING 1
#define SAMPLED_TEXTURE_BINDING 2
#define STORAGE_TEXTURE_BINDING 3
#define SAMPLER_BINDING 4

// NOTE: Current Spir-V still don't support ResourceDescriptorHeap.
//       So need tons of macro to support fully bindless :(
#define T_BINDLESS_TYPED_RESOURCE(Type, DataType) T_BINDLESS##Type##DataType
#define   BINDLESS_TYPED_RESOURCE(Type) BINDLESS##Type

// Declare of type.
#define T_BINDLESS_DECLARE(Type, Binding, DataType) [[vk::binding(Binding, 0)]] Type<DataType> T_BINDLESS_TYPED_RESOURCE(Type, DataType)[];
#define   BINDLESS_DECLARE(Type, Binding) [[vk::binding(Binding, 0)]] Type BINDLESS_TYPED_RESOURCE(Type)[];

/////////////////////////////////////////////////////////////////////////////////////////
// Texture area.
#define T_BINDLESS_TEXTURE_FORMAT_DECLARE(Type, Binding) \
    T_BINDLESS_DECLARE(Type, Binding, float ) \
    T_BINDLESS_DECLARE(Type, Binding, float1) \
    T_BINDLESS_DECLARE(Type, Binding, float2) \
    T_BINDLESS_DECLARE(Type, Binding, float3) \
    T_BINDLESS_DECLARE(Type, Binding, float4) \
    T_BINDLESS_DECLARE(Type, Binding, uint  ) \
    T_BINDLESS_DECLARE(Type, Binding, uint1 ) \
    T_BINDLESS_DECLARE(Type, Binding, uint2 ) \
    T_BINDLESS_DECLARE(Type, Binding, uint3 ) \
    T_BINDLESS_DECLARE(Type, Binding, uint4 ) \
    T_BINDLESS_DECLARE(Type, Binding, int   ) \
    T_BINDLESS_DECLARE(Type, Binding, int1  ) \
    T_BINDLESS_DECLARE(Type, Binding, int2  ) \
    T_BINDLESS_DECLARE(Type, Binding, int3  ) \
    T_BINDLESS_DECLARE(Type, Binding, int4  )

T_BINDLESS_TEXTURE_FORMAT_DECLARE(Texture2D,   SAMPLED_TEXTURE_BINDING)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(Texture3D,   SAMPLED_TEXTURE_BINDING)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(TextureCube, SAMPLED_TEXTURE_BINDING)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(RWTexture2D, STORAGE_TEXTURE_BINDING)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(RWTexture3D, STORAGE_TEXTURE_BINDING)

#undef T_BINDLESS_TEXTURE_FORMAT_DECLARE

#define T_BINDLESS_CONSTATNT_BUFFER_DECLARE(Type) \
    T_BINDLESS_DECLARE(ConstantBuffer, UNIFORM_BUFFER_BINDING, Type)

// ByteAddressBuffer don't care type.
BINDLESS_DECLARE(ByteAddressBuffer, STORAGE_BUFFER_BINDING)
BINDLESS_DECLARE(RWByteAddressBuffer, STORAGE_BUFFER_BINDING)

// SamplerState don't care type.
BINDLESS_DECLARE(SamplerState, SAMPLER_BINDING)
BINDLESS_DECLARE(SamplerComparisonState, SAMPLER_BINDING)

// Helper macro to load all template type.
#define T_BINDLESS(Type, DataType, Index) T_BINDLESS_TYPED_RESOURCE(Type, DataType)[NonUniformResourceIndex(Index)]
#define  BINDLESS(Type, Index) BINDLESS_TYPED_RESOURCE(Type)[NonUniformResourceIndex(Index)]

#define BYTE_ADDRESS_BINDLESS(Index) BINDLESS(ByteAddressBuffer, Index)
#define RWBYTE_ADDRESS_BINDLESS(Index) BINDLESS(RWByteAddressBuffer, Index)


// Usage:
//
// T_BINDLESS(ConstantBuffer, ...)
// BYTE_ADDRESS_BINDLESS()

#define TYPE_LOAD(Type, ElementId) Load<Type>((ElementId) * sizeof(Type))
#define TYPE_STORE(Type, ElementId, Value) Store<Type>((ElementId) * sizeof(Type), Value)

#define BATL(Type, BufferId, ElementId) BYTE_ADDRESS_BINDLESS(BufferId).TYPE_LOAD(Type, ElementId)
#define BATS(Type, BufferId, ElementId, Value) RWBYTE_ADDRESS_BINDLESS(BufferId).TYPE_STORE(Type, ElementId, Value)
#define RWBATL(Type, BufferId, ElementId) RWBYTE_ADDRESS_BINDLESS(BufferId).TYPE_LOAD(Type, ElementId)

#define  STORE_RW_TEXTURE_2D_DECLARE(Type) \
    void storeRWTexture2D_##Type(uint id, uint2 pos, Type v) { RWTexture2D<Type> rw = T_BINDLESS(RWTexture2D, Type, id); rw[pos] = v; }

STORE_RW_TEXTURE_2D_DECLARE(float4)
STORE_RW_TEXTURE_2D_DECLARE(float3)
STORE_RW_TEXTURE_2D_DECLARE(float2)
STORE_RW_TEXTURE_2D_DECLARE(float1)

STORE_RW_TEXTURE_2D_DECLARE(uint4)
STORE_RW_TEXTURE_2D_DECLARE(uint3)
STORE_RW_TEXTURE_2D_DECLARE(uint2)
STORE_RW_TEXTURE_2D_DECLARE(uint1)

#undef STORE_RW_TEXTURE_2D_DECLARE

#define  LOAD_RW_TEXTURE_2D_DECLARE(Type) \
    Type loadRWTexture2D_##Type(uint id, uint2 pos) { RWTexture2D<Type> rw = T_BINDLESS(RWTexture2D, Type, id); return rw[pos]; }

LOAD_RW_TEXTURE_2D_DECLARE(float4)
LOAD_RW_TEXTURE_2D_DECLARE(float3)
LOAD_RW_TEXTURE_2D_DECLARE(float2)
LOAD_RW_TEXTURE_2D_DECLARE(float1)

LOAD_RW_TEXTURE_2D_DECLARE(uint4)
LOAD_RW_TEXTURE_2D_DECLARE(uint3)
LOAD_RW_TEXTURE_2D_DECLARE(uint2)
LOAD_RW_TEXTURE_2D_DECLARE(uint1)

#undef LOAD_RW_TEXTURE_2D_DECLARE

#define LOAD_TEXTURE_2D_DECLARE(Type) \
    Type loadTexture2D_##Type(uint id, uint2 pos) { Texture2D<Type> r = T_BINDLESS(Texture2D, Type, id); return r[pos];  }

LOAD_TEXTURE_2D_DECLARE(float4)
LOAD_TEXTURE_2D_DECLARE(float3)
LOAD_TEXTURE_2D_DECLARE(float2)
LOAD_TEXTURE_2D_DECLARE(float1)
LOAD_TEXTURE_2D_DECLARE(uint1)

#undef LOAD_TEXTURE_2D_DECLARE

#define SAMPLE_TEXTURE_2D_DECLARE(Type) \
    Type sampleTexture2D_##Type(uint id, float2 uv, SamplerState s, int lod = 0) { Texture2D<Type> r = T_BINDLESS(Texture2D, Type, id); return r.SampleLevel(s, uv, lod);  }

SAMPLE_TEXTURE_2D_DECLARE(float4)
SAMPLE_TEXTURE_2D_DECLARE(float3)
SAMPLE_TEXTURE_2D_DECLARE(float2)
SAMPLE_TEXTURE_2D_DECLARE(float1)

#undef SAMPLE_TEXTURE_2D_DECLARE

#endif // !SHADER_BINDLESS_HLSLI
