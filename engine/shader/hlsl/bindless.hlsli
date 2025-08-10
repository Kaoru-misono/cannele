#ifndef SHADER_BINDLESS_HLSLI
#define SHADER_BINDLESS_HLSLI

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
#define BINDLESS_SET 0

// NOTE: Current Spir-V still don't support ResourceDescriptorHeap.
//       So need tons of macro to support fully bindless :(

#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)
#define T_BINDLESS_TYPED_RESOURCE(Type, DataType) CONCAT(T_BINDLESS, CONCAT(Type, DataType))
#define   BINDLESS_TYPED_RESOURCE(Type) CONCAT(BINDLESS, Type)

// Declare of type.
#define T_BINDLESS_DECLARE(Type, Binding, RegisterType, DataType) [[vk::binding(Binding, 0)]] Type<DataType> T_BINDLESS_TYPED_RESOURCE(Type, DataType)[] : register(CONCAT(RegisterType, Binding), space0);
#define   BINDLESS_DECLARE(Type, Binding, RegisterType) [[vk::binding(Binding, 0)]] Type BINDLESS_TYPED_RESOURCE(Type)[] : register(CONCAT(RegisterType, Binding), space0);

/////////////////////////////////////////////////////////////////////////////////////////
// Texture area.
#define T_BINDLESS_TEXTURE_FORMAT_DECLARE(Type, Binding, RegisterType) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, float ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, float1) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, float2) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, float3) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, float4) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, uint  ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, uint1 ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, uint2 ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, uint3 ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, uint4 ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, int   ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, int1  ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, int2  ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, int3  ) \
    T_BINDLESS_DECLARE(Type, Binding, RegisterType, int4  )

T_BINDLESS_TEXTURE_FORMAT_DECLARE(Texture2D,   SAMPLED_TEXTURE_BINDING, t)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(Texture3D,   SAMPLED_TEXTURE_BINDING, t)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(TextureCube, SAMPLED_TEXTURE_BINDING, t)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(RWTexture2D, STORAGE_TEXTURE_BINDING, u)
T_BINDLESS_TEXTURE_FORMAT_DECLARE(RWTexture3D, STORAGE_TEXTURE_BINDING, u)

#undef T_BINDLESS_TEXTURE_FORMAT_DECLARE

#define T_BINDLESS_CONSTATNT_BUFFER_DECLARE(Type) \
    T_BINDLESS_DECLARE(ConstantBuffer, UNIFORM_BUFFER_BINDING, b, Type)

// ByteAddressBuffer don't care type.
BINDLESS_DECLARE(ByteAddressBuffer, STORAGE_BUFFER_BINDING, t)
BINDLESS_DECLARE(RWByteAddressBuffer, STORAGE_BUFFER_BINDING, u)

// SamplerState don't care type.
BINDLESS_DECLARE(SamplerState, SAMPLER_BINDING, s)
BINDLESS_DECLARE(SamplerComparisonState, SAMPLER_BINDING, s)

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

#define BYTE_ADDRESS_BUFFER_TYPE_LOAD(Type, BufferId, ElementId) BYTE_ADDRESS_BINDLESS(BufferId).TYPE_LOAD(Type, ElementId)
#define BYTE_ADDRESS_BUFFER_TYPE_STORE(Type, BufferId, ElementId, Value) RWBYTE_ADDRESS_BINDLESS(BufferId).TYPE_STORE(Type, ElementId, Value)
#define RW_BYTE_ADDRESS_BUFFER_TYPE_LOAD(Type, BufferId, ElementId) RWBYTE_ADDRESS_BINDLESS(BufferId).TYPE_LOAD(Type, ElementId)

#define  STORE_RW_TEXTURE_2D_DECLARE(Type) \
    void store_RWTexture2D_##Type(uint id, uint2 pos, Type v) { RWTexture2D<Type> rw = T_BINDLESS(RWTexture2D, Type, id); rw[pos] = v; }

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
