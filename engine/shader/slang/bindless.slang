#pragma once

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

[vk::binding(0, 0)]
__DynamicResource<__DynamicResourceKind.General> resourceHandles[];

[vk::binding(0, 1)]
__DynamicResource<__DynamicResourceKind.Sampler> samplerHandles[];

export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle) where T : IOpaqueDescriptor
{
    __target_switch
    {
    case spirv:
        if (T.kind == DescriptorKind.Sampler) {
            return samplerHandles[((uint2)handle).x].asOpaqueDescriptor<T>();
        }
        else {
            return resourceHandles[((uint2)handle).x].asOpaqueDescriptor<T>();
        }
    default:
        return defaultGetDescriptorFromHandle(handle);
    }
}

#define PUSHCONSTANTS(TYPE, NAME) [[vk::push_constant]] TYPE NAME

typealias StructuredBufferHandle<T, Layout: IBufferDataLayout = DefaultDataLayout> = DescriptorHandle<StructuredBuffer<T, Layout>>;
typealias RWStructuredBufferHandle<T, Layout: IBufferDataLayout = DefaultDataLayout> = DescriptorHandle<RWStructuredBuffer<T, Layout>>;
typealias ByteAddressBufferHandle = DescriptorHandle<ByteAddressBuffer>;
typealias ConstantBufferHandle<T, Layout: IBufferDataLayout = DefaultDataLayout> = DescriptorHandle<ConstantBuffer<T, Layout>>;
typealias Texture2DHandle<T: ITexelElement = float4> = DescriptorHandle<Texture2D<T>>;
typealias Sampler2DHandle<T: ITexelElement = float4> = DescriptorHandle<Sampler2D<T>>;
typealias SamplerHandle = DescriptorHandle<SamplerState>;
typealias SamplerComparisonStateHandle = DescriptorHandle<SamplerComparisonState>;


