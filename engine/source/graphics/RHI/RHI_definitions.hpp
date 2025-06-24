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

        // Read
        CPU_read                 = 1 << 0,
        present                  = 1 << 1,
        vertex_buffer            = 1 << 2,
        index_buffer             = 1 << 3,
        uniform_buffer           = 1 << 4,
        sampled_texture          = 1 << 5,
        transfer_src             = 1 << 6,
        depth_stencil_read       = 1 << 7,
        indirect_command_read    = 1 << 8,

        // Read-write
        storage_buffer           = 1 << 9,
        storage_texture          = 1 << 10,
        color_attachment         = 1 << 11,
        transfer_dst             = 1 << 12,
        depth_stencil_attachment = 1 << 13,

        SRV_access = uniform_buffer | sampled_texture,
        UAV_access = storage_buffer | storage_texture,

        read_only  = CPU_read | present | vertex_buffer | index_buffer | SRV_access | transfer_src | depth_stencil_read | indirect_command_read,
        readable   = read_only | UAV_access,
        write_only = color_attachment | transfer_dst | depth_stencil_attachment,
        writable   = write_only | UAV_access,
    };
    ENUM_STRUCT_FLAGS(EResourceStates);

    inline auto to_string(EResourceStates access) -> std::string
    {
        auto result = std::string{};
        if (enum_has_any_flags(access, EResourceStates::vertex_buffer)) {
            result += "(VERTEX_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::index_buffer)) {
            result += "(INDEX_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::indirect_command_read)) {
            result += "(INDIRECT_COMMAND_READ) ";
        }
        if (enum_has_any_flags(access, EResourceStates::uniform_buffer)) {
            result += "(UNIFORM_BUFFER) ";
        }
        if (enum_has_any_flags(access, EResourceStates::storage_buffer)) {
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
        if (enum_has_any_flags(access, EResourceStates::storage_texture)) {
            result += "(STORAGE_TEXTURE) ";
        }
        if (enum_has_any_flags(access, EResourceStates::color_attachment)) {
            result += "(COLOR_ATTACHMENT) ";
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

    enum struct EVertexAttributeType: uint8_t
    {
        none,
        float1,
        float2,
        float3,
        float4,
        uint,
        count,
    };

    enum struct EIndexType: uint8_t
    {
        uint16,
        uint32,
    };

    enum struct EVertexInputRate: uint8_t
    {
        vertex,
        instance,
    };

    enum struct EVertexAttributeFormat: uint8_t
    {
        sfloat32,
        sfloat32x2,
        sfloat32x3,
        sfloat32x4,
        uint8,
        uint8x4,
        unorm8,
        unorm8x2,
        unorm8x3,
        unorm8x4,
        unorm16,
        unorm16x2,
        unorm16x3,
        unorm16x4,
        unorm32,
        unorm32x2,
        unorm32x3,
        unorm32x4,
    };

    enum struct EDescriptorType: uint8_t
    {
        invalid,
        uniform_buffer,
        storage_buffer,
        texture,
        storage_texture,
        sampler,
        texture_sampler,

        last,
    };

    enum struct EQueueType: uint8_t
    {
        graphics,
        compute,
        transfer,

        last,
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
