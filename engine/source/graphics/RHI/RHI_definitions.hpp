#pragma once

#include <core/enum_flag.hpp>

#include <cstdint>
#include <string>

namespace cannele::inline graphics::rhi
{
    enum struct EFormat: uint8_t
    {
        undefined,
        r8_uint,
        r8_sint,
        r8_unorm,
        r8_snorm,
        rg8_uint,
        rg8_sint,
        rg8_unorm,
        rg8_snorm,
        r16_uint,
        r16_sint,
        r16_unorm,
        r16_snorm,
        r16_float,
        bgra4_unorm,
        b5g6r5_unorm,
        b5g5r5a1_unorm,
        rgba8_uint,
        rgba8_sint,
        rgba8_unorm, // We don't support srgb8_unorm, use this for srgb8_unorm.
        rgba8_snorm,
        bgra8_unorm,
        sbgra8_unorm,
        r10g10b10a2_unorm,
        r11g11b10_float,
        rg16_uint,
        rg16_sint,
        rg16_unorm,
        rg16_snorm,
        rg16_float,
        r32_uint,
        r32_sint,
        r32_float,
        rgba16_uint,
        rgba16_sint,
        rgba16_float,
        rgba16_unorm,
        rgba16_snorm,
        rg32_uint,
        rg32_sint,
        rg32_float,
        rgb32_uint,
        rgb32_sint,
        rgb32_float,
        rgba32_uint,
        rgba32_sint,
        rgba32_float,

        d16_unorm,
        d24_unorm_s8_uint,
        d32_sfloat,
        d32_sfloat_s8_uint,

        bc1_unorm,
        bc1_unorm_srgb,
        bc2_unorm,
        bc2_unorm_srgb,
        bc3_unorm,
        bc3_unorm_srgb,
        bc4_unorm,
        bc4_snorm,
        bc5_unorm,
        bc5_snorm,
        bc6h_ufloat,
        bc6h_sfloat,
        bc7_unorm,
        bc7_unorm_srgb,

        last,

        index_uint16 = r16_uint,
        index_uint32 = r32_uint,
    };

    enum struct EFormatKind: uint8_t
    {
        integer,
        normalized,
        float_t,
        depth_stencil,

        last,
    };

    struct FormatInfo final
    {
        char const* name{};
        EFormat format{};
        uint8_t bytes_per_block{};
        uint8_t blocks{};
        uint8_t channels{};
        uint8_t is_signed{};
        EFormatKind kind{};
    };

    auto get_format_info(EFormat format) -> FormatInfo const*;

    enum struct EResourceStates: uint32_t
    {
        unknown = 0,
        unused                    = 1 << 0,

        // Read
        present                   = 1 << 1,
        vertex_attribute_read     = 1 << 2,
        index_read                = 1 << 3,
        uniform_read              = 1 << 4,
        storage_read              = 1 << 5,
        sampled_texture           = 1 << 6,
        transfer_src              = 1 << 7,
        depth_stencil_read        = 1 << 8,
        indirect_command_read     = 1 << 9,

        // Read-write
        storage_write             = 1 << 10,
        color_attachment          = 1 << 11,
        transfer_dst              = 1 << 12,
        depth_stencil_attachment  = 1 << 13,

        SRV_access = uniform_read | sampled_texture | storage_read,
        UAV_access = storage_write,

        read_only  = present | vertex_attribute_read | index_read | SRV_access | transfer_src | depth_stencil_read | indirect_command_read,
        readable   = read_only | UAV_access,
        write_only = color_attachment | transfer_dst | depth_stencil_attachment,
        writable   = write_only | UAV_access,
    };
    ENUM_STRUCT_FLAGS(EResourceStates);

    inline auto to_string(EResourceStates access) -> std::string
    {
        auto result = std::string{};
        if (enum_has_any_flags(access, EResourceStates::vertex_attribute_read)) {
            result += "(VERTEX_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::index_read)) {
            result += "(INDEX_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::indirect_command_read)) {
            result += "(INDIRECT_COMMAND_READ) ";
        }
        if (enum_has_any_flags(access, EResourceStates::uniform_read)) {
            result += "(UNIFORM_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::storage_write)) {
            result += "(STORAGE_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::transfer_src)) {
            result += "(TRANSFER_SRC) ";
        }
        if (enum_has_any_flags(access, EResourceStates::transfer_dst)) {
            result += "(TRANSFER_DST) ";
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_read)) {
            result += "(DEPTH_STENCIL_READ) ";
        }
        if (enum_has_any_flags(access, EResourceStates::depth_stencil_attachment)) {
            result += "(DEPTH_STENCIL_ATTACHMENT) ";
        }
        if (enum_has_any_flags(access, EResourceStates::sampled_texture)) {
            result += "(SAMPLED_TEXTURE) ";
        }
        if (enum_has_any_flags(access, EResourceStates::color_attachment)) {
            result += "(COLOR_ATTACHMENT) ";
        }
        if (enum_has_any_flags(access, EResourceStates::present)) {
            result += "(PRESENT) ";
        }
        if (result.empty()) {
            result = "(UNKNOWN) ";
        }
        return result;
    }

    inline auto is_readable(EResourceStates state) -> bool
    {
        return enum_has_any_flags(state, EResourceStates::readable);
    }

    inline auto is_writable(EResourceStates state) -> bool
    {
        return enum_has_any_flags(state, EResourceStates::writable);
    }

    enum struct EPipelineStage : uint64_t
    {
        none                             = 0llu,
        top_of_pipe                      = 0x00000001llu,
        draw_indirect                    = 0x00000002llu,
        vertex_input                     = 0x00000004llu,
        vertex_shader                    = 0x00000008llu,
        tessellation_control_shader      = 0x00000010llu,
        tessellation_evaluation_shader   = 0x00000020llu,
        geometry_shader                  = 0x00000040llu,
        fragment_shader                  = 0x00000080llu,
        early_fragment_tests             = 0x00000100llu,
        late_fragment_tests              = 0x00000200llu,
        color_attachment_output          = 0x00000400llu,
        compute_shader                   = 0x00000800llu,
        all_transfer                     = 0x00001000llu,
        transfer                         = 0x00002000llu,
        bottom_of_pipe                   = 0x00004000llu,
        host                             = 0x00008000llu,
        all_graphics                     = 0x00010000llu,
        all_commands                     = 0x00020000llu,
        command_preprocess               = 0x00040000llu,
        conditional_rendering            = 0x00080000llu,
        task_shader                      = 0x00100000llu,
        mesh_shader                      = 0x00200000llu,
        ray_tracing_shader               = 0x00400000llu,
        fragment_shading_rate_attachment = 0x00800000llu,
        fragment_density_process         = 0x01000000llu,
        transform_feedback               = 0x02000000llu,
        acceleration_structure_build     = 0x04000000llu,
        video_decode                     = 0x08000000llu,
        video_encode                     = 0x100000000llu,
        copy                             = 0x200000000llu,
        resolve                          = 0x400000000llu,
        blit                             = 0x800000000llu,
        clear                            = 0x1000000000llu,
        index_input                      = 0x2000000000llu,
        vertex_attribute_input           = 0x4000000000llu,
        pre_rasterization_shaders        = 0x8000000000llu,
    };
    ENUM_STRUCT_FLAGS(EPipelineStage);

    enum struct EDescriptorType: uint8_t
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

    enum struct EResourceAction: uint8_t
    {
        none             = 0,
        load             = 1 << 0,
        clear            = 1 << 1,
        generate_mipmaps = 1 << 2,
    };
    ENUM_STRUCT_FLAGS(EResourceAction);

    enum struct ESamplerFilter: uint8_t
    {
        nearest,
        linear,
    };

    enum struct ESamplerAddressMode: uint8_t
    {
        repeat,
        mirror_repeat,
        clamp_to_edge,
        clamp_to_border,
        mirror_clamp_to_edge,
    };

    enum struct ECompareOperation: uint8_t
    {
        never,
        less,
        equal,
        less_or_equal,
        greater,
        not_equal,
        greater_or_equal,
        always,
    };

    enum struct EBlendOperation: uint8_t
    {
        add,
        subtract,
        min,
        max,
        reverse_subtract,
    };

    enum struct EBlendFactor: uint8_t
    {
        zero,
        one,
        src_color,
        one_minus_src_color,
        src_alpha,
        one_minus_src_alpha,
        dst_color,
        one_minus_dst_color,
        dst_alpha,
        one_minus_dst_alpha,
        src_alpha_saturate,
        constant_color,
        one_minus_constant_color,
        constant_alpha,
        one_minus_constant_alpha,
    };

    enum struct EColorWriteMask: uint8_t
    {
        none = 0,
        r = 1 << 0,
        g = 1 << 1,
        b = 1 << 2,
        a = 1 << 3,
        rgb = r | g | b,
        rgba = r | g | b | a,
        rg = r | g,
        ba = b | a,
    };
    ENUM_STRUCT_FLAGS(EColorWriteMask);

    enum struct ERasterizerTopologyType: uint8_t
    {
        point_list,
        line_list,
        line_strip,
        triangle_list,
        triangle_strip,
    };

    enum struct ERasterizerFrontFace: uint8_t
    {
        counter_clockwise,
        clockwise,
    };

    enum struct ERasterizerFillMode: uint8_t
    {
        point,
        line,
        solid,
    };

    enum struct ERasterizerCullMode: uint8_t
    {
        none,
        front,
        back,
    };

    enum struct EStencilOperation: uint8_t
    {
        keep,
        zero,
        replace,
        increment_and_clamp,
        decrement_and_clamp,
        invert,
        increment_and_wrap,
        decrement_and_wrap,
    };

    enum struct ELoadOp: uint8_t
    {
        no_action,
        load,
        clear,
    };

    enum struct EStoreOp: uint8_t
    {
        no_action,
        store,
    };

    enum struct EVertexInputRate: uint8_t
    {
        vertex,
        instance,
    };

    enum struct EQueueType: uint8_t
    {
        graphics,
        compute,
        transfer,

        last,

        ignore, // For barrier
    };

    enum struct EPresentMode: uint8_t
    {
        immediate,
        mailbox,
        fifo,

        last,
    };

    enum struct EColorSpace: uint8_t
    {
        srgb_nonliner,
        display_p3_nonliner,
        extent_srgb_liner,
        display_p3_liner,
        dci_p3_nonliner,
        bt709_liner,
        bt709_nonliner,
        bt2020_liner,
        hdr10_st2084,
        dolbyvision,
        hdr10_hlg,
        adobergb_liner,
        adobergb_nonliner,
        pass_through,
        extended_srgb_nonliner,
        display_native_amd,

        last,
    };
}
