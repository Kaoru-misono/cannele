#pragma once

#include "forward.hpp"

#include <core/inplace_vector.hpp>
#include <core/assert.hpp>

#include <slang.h>
#include <slang-com-ptr.h>
#include <unordered_set>

namespace cannele::inline graphics::rhi
{
    using ShaderProgramID = uint64_t;
    using SlangComponent = Slang::ComPtr<slang::IComponentType>;
    using SlangSession = Slang::ComPtr<slang::ISession>;
    using SlangBlob = Slang::ComPtr<slang::IBlob>;

    struct ShaderProgramCreateInfo final
    {
        SlangSession session{};
        SlangComponent root_component{};
        inplace_vector<SlangComponent> entry_points{};
    };

    struct RHIShaderProgram: IResource
    {
        using IResource::IResource;

        using SpecializationKey = uint64_t;

        ShaderProgramCreateInfo info{};

        ShaderProgramID id{};

        SlangComponent root_component{};
        std::vector<SlangComponent> entry_points{};

        SlangComponent linked_program{};
        std::vector<SlangComponent> linked_entry_points{};

        std::unordered_map<SpecializationKey, std::shared_ptr<RHIShaderProgram>> specializations{};

        bool compiled{false};
        bool has_mesh_shader{false};

        RHIShaderProgram(IDevice* device, ShaderProgramCreateInfo const& create_info);
        virtual ~RHIShaderProgram();

        auto compile_shader() -> bool;
        auto find_type_by_name(std::string_view name) const -> slang::TypeReflection*;

        auto is_specializable() const -> bool { return false; } // TODO:

        virtual auto create_shader_module(slang::EntryPointReflection* entry_point_info, std::span<std::byte const> code) -> bool { return false; }
        virtual auto root_shader_object_layout() const -> ShaderObjectLayout* = 0;
    };

    struct ShaderObjectID
    {
        uint32_t id{};
        uint32_t version{};

        auto operator <=> (ShaderObjectID const& other) const = default;
    };

    enum struct BindingType: uint8_t
    {
        undefined,
        buffer,
        texture,
        sampler,
        acceleration_structure,

        last,
    };

    enum struct ShaderObjectType: uint8_t
    {
        none,
        array,
        structured_buffer,
        parameter_block,
    };

    struct Binding
    {
        BindingType type{BindingType::undefined};

        std::shared_ptr<IResource> resource{};
        BufferRange buffer_range{};
    };

    struct BindingData {};

    struct ResourceSlot final
    {
        BindingType type{BindingType::undefined};

        std::shared_ptr<IResource> resource{};

        EFormat format{EFormat::undefined};

        explicit operator bool () const noexcept { return type != BindingType::undefined && resource; }
    };

    using ShaderComponentID = uint32_t;

    struct ShaderObjectLayout: IResource
    {
        struct BindingRangeInfo
        {
            slang::BindingType binding_type{};

            uint32_t count{};

            uint32_t slot_index{};

            uint32_t sub_object_index{};

            bool is_speciallizable{false};
        };

        struct SubObjectRangeInfo
        {
            uint32_t binding_range_index{};

            uint32_t pending_ordinary_data_offset{};
            uint32_t pending_ordinary_data_stride{};
        };

        SlangSession session{};
        // If this layout is nested element, the element type layout will be set.
        slang::TypeLayoutReflection* element_type_layout{};
        ShaderObjectType container_type{};

        ShaderComponentID component_id{};

        ShaderObjectLayout(IDevice* device, slang::ISession* session, slang::TypeLayoutReflection* element_type_layout);
        virtual ~ShaderObjectLayout();

        static auto unwrap_parameter_groups(slang::TypeLayoutReflection* type_layout) -> std::pair<slang::TypeLayoutReflection*, ShaderObjectType>
        {
            auto result = ShaderObjectType::none;

            while (true) {
                if (!type_layout->getType()) {
                    if (auto element_type_layout = type_layout->getElementTypeLayout()) {
                        type_layout = element_type_layout;
                    }
                }
                switch (type_layout->getKind()) {
                    case slang::TypeReflection::Kind::Array: {
                        CNE_ASSERT(result == ShaderObjectType::none);
                        result = ShaderObjectType::array;
                        type_layout = type_layout->getElementTypeLayout();
                        return {type_layout, result};
                    }
                    case slang::TypeReflection::Kind::Resource: {
                        if (type_layout->getResourceShape() != SLANG_STRUCTURED_BUFFER) break;
                        CNE_ASSERT(result == ShaderObjectType::none);
                        result = ShaderObjectType::structured_buffer;
                        type_layout = type_layout->getElementTypeLayout();
                        return {type_layout, result};
                    }
                    case slang::TypeReflection::Kind::ConstantBuffer:
                    case slang::TypeReflection::Kind::ParameterBlock: {
                        result = ShaderObjectType::parameter_block;
                        type_layout = type_layout->getElementTypeLayout();
                        return {type_layout, result};
                    }
                    default: return {type_layout, result};
                }
            }
        }

        virtual auto slot_count() -> uint32_t = 0;
        virtual auto sub_object_count() -> uint32_t = 0;
        virtual auto binding_range_count() -> uint32_t = 0;
        virtual auto binding_range(uint32_t index) -> BindingRangeInfo const& = 0;
        virtual auto ordinary_data_size() -> uint32_t = 0;
        virtual auto sub_object_range_count() -> uint32_t = 0;
        virtual auto sub_object_range(uint32_t index) -> SubObjectRangeInfo const& = 0;
        virtual auto sub_object_range_layout(uint32_t index) -> ShaderObjectLayout* = 0;

        virtual auto entry_point_count() -> uint32_t { return 0; }
        virtual auto entry_point_layout(uint32_t index) -> ShaderObjectLayout*;
    };

    using ShaderObjectLayoutPtr = std::unique_ptr<ShaderObjectLayout>;
    using DescriptorHandle = math::uint2;


    struct ShaderOffset final
    {
        uint32_t uniform_offset{};
        uint32_t binding_range_index{};
        uint32_t binding_array_index{};

        auto operator <=> (ShaderOffset const& other) const = default;
    };

    struct ShaderObject
    {
        ShaderObjectLayout* layout{};

        inplace_vector<ResourceSlot> resource_slots{};
        inplace_vector<std::byte, 1024> data{};
        inplace_vector<std::shared_ptr<ShaderObject>> objects{};

        inplace_vector<std::pair<RHIBuffer*, EResourceStates>> pending_buffers{};
        inplace_vector<std::pair<RHITextureView*, EResourceStates>> pending_texture_views{};

        uint32_t id{};
        uint32_t version{};

        bool allow_modification{true};

        ShaderObject() = default;
        ShaderObject(ShaderObjectLayout* layout);
        virtual ~ShaderObject();

        auto element_type_layout() -> slang::TypeLayoutReflection*;
        auto container_type() -> ShaderObjectType;
        virtual auto entry_point_count() -> uint32_t;
        virtual auto entry_point(uint32_t index) -> ShaderObject*;

        auto collect_specialization_args() -> void;

        auto write_structured_buffer(slang::TypeLayoutReflection* layout, ShaderObjectLayout* specialized_layout) -> BufferHandle;

        auto set_data(ShaderOffset const& offset, void const* data, size_t size) -> void;
        auto set_descriptor_handle(ShaderOffset const& offset, DescriptorHandle const& handle) -> void;
        auto set_bindless_buffer(ShaderOffset const& offset, RHIBuffer* buffer, EResourceStates state) -> void;
        auto set_bindless_texture(ShaderOffset const& offset, RHITextureView* texture_view, EResourceStates state) -> void;
        auto set_bindless_texture(ShaderOffset const& offset, RHITextureView* texture_view, SamplerHandle sampler, EResourceStates state) -> void;

        // TODO: Set object.
        auto set_object(ShaderOffset const& offset, std::shared_ptr<ShaderObject> object) -> void;
        auto sub_object(ShaderOffset const& offset) -> ShaderObject*;

        virtual auto track_resources(std::unordered_set<std::shared_ptr<IResource>>* resources) -> void;
    };

    using ShaderObjectHandle = std::shared_ptr<ShaderObject>;

    struct RootShaderObject final: ShaderObject
    {
        RHIShaderProgram const* program{};

        std::vector<std::shared_ptr<ShaderObject>> entry_points{};

        RootShaderObject(RHIShaderProgram const* program);
        virtual ~RootShaderObject();

        auto entry_point_count() -> uint32_t override;
        auto entry_point(uint32_t index) -> ShaderObject* override;

        auto track_resources(std::unordered_set<std::shared_ptr<IResource>>* resources) -> void override;

        auto specialized_layout() -> ShaderObjectLayout*;
    };

    using RootShaderObjectHandle = std::shared_ptr<RootShaderObject>;

    struct ShaderObjectWriter final
    {
        ShaderObject* base_object{};
        slang::TypeLayoutReflection* type_layout{};
        ShaderObjectType container_type{};
        ShaderOffset offset{};

        auto valid() const -> bool { return base_object != nullptr; }

        explicit operator bool() const { return valid(); }

        auto field_writer(std::string_view name) -> ShaderObjectWriter;

        auto element_writer(uint32_t index) -> ShaderObjectWriter;

        ShaderObjectWriter() = default;
        ShaderObjectWriter(ShaderObject* object);

        auto dereference() -> ShaderObjectWriter;

        auto set_data(void const* data, size_t size) -> void;
        auto set_object(std::shared_ptr<ShaderObject> object) -> void;
        auto set_bindless_buffer(BufferHandle buffer, EResourceStates state) -> void;
        auto set_bindless_texture(RHITextureView* texture_view, EResourceStates state) -> void;
        auto set_bindless_texture(RHITextureView* texture_view, SamplerHandle sampler, EResourceStates state) -> void;

        template <typename T>
        auto set_data(T const& value) -> void
        {
            set_data(&value, sizeof(value));
        }

        auto set_descriptor_handle(DescriptorHandle const& handle) -> void { base_object->set_descriptor_handle(offset, handle); }

        auto operator [] (std::string_view name) -> ShaderObjectWriter { return field_writer(name); }

        auto operator [] (uint32_t index) -> ShaderObjectWriter { return element_writer(index); }
    };
}
