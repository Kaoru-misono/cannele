#include "slang_reflection_print.hpp"

#include <print>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        int indentation = 0;

        auto print_indentation() -> void
        {
            for (auto i = 1; i < indentation; i++) {
                std::print("  ");
            }
        }

        auto begin_object() -> void { indentation++; }
        auto end_object()   -> void { indentation--; }
        auto begin_array()  -> void { indentation++; }
        auto end_array()    -> void { indentation--; }

        struct ScopedObject final
        {
            ScopedObject() { begin_object(); }
            ~ScopedObject() { end_object(); }
        };

        #define SCOPED_OBJECT() ScopedObject scoped_object_##__COUNTER__

        #define WITH_ARRAY() for (auto i = (begin_array(), 1); i; i = (end_array(), 0))

        auto new_line() -> void
        {
            std::print("\n");
            print_indentation();
        }

        bool after_array_element = true;

        auto element() -> void
        {
            new_line();
            std::print("- ");
            after_array_element = true;
        }

        auto key(std::string_view key) -> void
        {
            if (!after_array_element) {
                new_line();
            }
            after_array_element = false;

            std::print("{}: ", key);
        }

        auto print_quoted_string(std::string_view view) -> void
        {
            if (view.empty()) {
                std::print("null");
            } else {
                std::print("\"{}\"", view);
            }
        }

        auto print_comment(std::string_view comment) -> void
        {
            std::print("# {}", comment);
        }

        template <typename T>
        auto print_value(T const& value) -> void
        {
            std::print("{} ", value);
        }

        template <typename T>
        auto key_value(std::string_view key, T const& value) -> void
        {
            if (!after_array_element) {
                new_line();
            }
            after_array_element = false;

            std::print("{}: {}", key, value);
        }

        auto to_string(slang::TypeReflection::Kind kind) -> std::string
        {
            switch (kind) {
            #define CASE(ENUM) \
                case slang::TypeReflection::Kind::ENUM: return #ENUM;

                CASE(None);
                CASE(Struct);
                CASE(Array);
                CASE(Matrix);
                CASE(Vector);
                CASE(Scalar);
                CASE(ConstantBuffer);
                CASE(Resource);
                CASE(SamplerState);
                CASE(TextureBuffer);
                CASE(ShaderStorageBuffer);
                CASE(ParameterBlock);
                CASE(GenericTypeParameter);
                CASE(Interface);
                CASE(OutputStream);
                CASE(Specialized);
                CASE(Feedback);
                CASE(Pointer);
                CASE(DynamicResource);

            #undef CASE

                default: return "Unexpected enum";
            }
        }

        auto to_string(slang::TypeReflection::ScalarType scalarType) -> std::string
        {
            switch (scalarType)
            {
            #define CASE(ENUM)                    \
                case slang::TypeReflection::ENUM: return #ENUM;

                CASE(None);
                CASE(Void);
                CASE(Bool);
                CASE(Int32);
                CASE(UInt32);
                CASE(Int64);
                CASE(UInt64);
                CASE(Float16);
                CASE(Float32);
                CASE(Float64);
                CASE(Int8);
                CASE(UInt8);
                CASE(Int16);
                CASE(UInt16);
            #undef CASE

                default: return "Unhandled scalar type";
            }
        }

        auto to_string(SlangResourceShape shape) -> std::string
        {
            auto result = std::string{"base: "};
            auto baseShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;
            switch (baseShape) {
            #define CASE(ENUM) case SLANG_##ENUM: result += #ENUM; break

                CASE(TEXTURE_1D);
                CASE(TEXTURE_2D);
                CASE(TEXTURE_3D);
                CASE(TEXTURE_CUBE);
                CASE(TEXTURE_BUFFER);
                CASE(STRUCTURED_BUFFER);
                CASE(BYTE_ADDRESS_BUFFER);
                CASE(RESOURCE_UNKNOWN);
                CASE(ACCELERATION_STRUCTURE);
                CASE(TEXTURE_SUBPASS);

            #undef CASE

                default: result += "# unexpected enumerant"; break;
            }

            #define CASE(TAG) \
                do { \
                    if (shape & SLANG_TEXTURE_##TAG##_FLAG) { \
                        result += #TAG; \
                        result += ": true"; \
                    }                                       \
                } while (0)

                CASE(FEEDBACK);
                CASE(SHADOW);
                CASE(ARRAY);
                CASE(MULTISAMPLE);

            #undef CASE

            return result;
        }

        auto to_string(SlangResourceAccess access) -> std::string
        {
            switch (access)
            {
            #define CASE(TAG)                     \
                case SLANG_RESOURCE_ACCESS_##TAG: return #TAG

                CASE(NONE);
                CASE(READ);
                CASE(READ_WRITE);
                CASE(RASTER_ORDERED);
                CASE(APPEND);
                CASE(CONSUME);
                CASE(WRITE);
                CASE(FEEDBACK);

            #undef CASE

            default: return "# unexpected enumerant";
            }
        }

        auto to_string(slang::ParameterCategory layout_unit) -> std::string
        {
            switch (layout_unit)
            {
            #define CASE(ENUM, DESCRIPTION) \
                case slang::ParameterCategory::ENUM: \
                    return DESCRIPTION;

                CASE(ConstantBuffer, "constant buffer slots");
                CASE(ShaderResource, "texture slots");
                CASE(UnorderedAccess, "uav slots");
                CASE(VaryingInput, "varying input slots");
                CASE(VaryingOutput, "varying output slots");
                CASE(SamplerState, "sampler slots");
                CASE(Uniform, "bytes");
                CASE(DescriptorTableSlot, "bindings");
                CASE(SpecializationConstant, "specialization constant ids");
                CASE(PushConstantBuffer, "push-constant buffers");
                CASE(RegisterSpace, "register space offset for a variable");
                CASE(GenericResource, "generic resources");
                CASE(RayPayload, "ray payloads");
                CASE(HitAttributes, "hit attributes");
                CASE(CallablePayload, "callable payloads");
                CASE(ShaderRecord, "shader records");
                CASE(ExistentialTypeParam, "existential type parameters");
                CASE(ExistentialObjectParam, "existential object parameters");
                CASE(SubElementRegisterSpace, "register spaces / descriptor sets");
                CASE(InputAttachmentIndex, "subpass input attachments");
                CASE(MetalArgumentBufferElement, "Metal argument buffer elements");
                CASE(MetalAttribute, "Metal attributes");
                CASE(MetalPayload, "Metal payloads");

            #undef CASE

                default: return "Unexpected enum";
            }
        }

        auto to_string(SlangStage stage) -> std::string
        {
            switch (stage) {
            #define CASE(ENUM) case SLANG_STAGE_##ENUM: return #ENUM;

                CASE(NONE);
                CASE(VERTEX);
                CASE(HULL);
                CASE(DOMAIN);
                CASE(GEOMETRY);
                CASE(FRAGMENT);
                CASE(COMPUTE);
                CASE(RAY_GENERATION);
                CASE(INTERSECTION);
                CASE(ANY_HIT);
                CASE(CLOSEST_HIT);
                CASE(MISS);
                CASE(CALLABLE);
                CASE(MESH);
                CASE(AMPLIFICATION);
                CASE(DISPATCH);

            #undef CASE

                default: return "Unexpected enum";
            }
        }

        auto calculate_cumulative_offset(
            slang::ParameterCategory layout_unit,
            AccessPath access_path
        ) -> CumulativeOffset
        {
            auto result = CumulativeOffset{};

            switch (layout_unit) {
                case slang::ParameterCategory::Uniform: {
                    for (auto node = access_path.leaf; node != access_path.deepest_constant_bufer; node = node->outer) {
                        result.value += node->variable_layout->getOffset(layout_unit);
                    }
                    break;
                }
                case slang::ParameterCategory::ConstantBuffer:
                case slang::ParameterCategory::ShaderResource:
                case slang::ParameterCategory::UnorderedAccess:
                case slang::ParameterCategory::SamplerState:
                case slang::ParameterCategory::DescriptorTableSlot: {
                    for (auto node = access_path.leaf; node != access_path.deepest_parameter_block; node = node->outer) {
                        result.value += node->variable_layout->getOffset(layout_unit);
                        result.space += node->variable_layout->getBindingSpace(layout_unit);
                    }
                    for (auto node = access_path.deepest_parameter_block; node != nullptr; node = node->outer) {
                        result.space += node->variable_layout->getOffset(
                            slang::ParameterCategory::SubElementRegisterSpace);
                    }
                    break;
                }
                default: {
                    for (auto node = access_path.leaf; node != nullptr; node = node->outer) {
                        result.value += node->variable_layout->getOffset(layout_unit);
                    }
                    break;
                }
            }

            return result;
        }
    }

    // ### Variable
    auto print_variable(slang::VariableReflection* variable) -> void
    {
        SCOPED_OBJECT();

        auto name = variable->getName();
        auto type = variable->getType();

        std::print("name: \"{}\" ", name ? name : "(null)");
        key("type");
        print_type(type);

        auto value = int64_t{};
        if (SLANG_SUCCEEDED(variable->getDefaultValueInt(&value))) {
            key("value");
            std::print("{}", value);
        }
    }

    // ### Types
    auto print_type(slang::TypeReflection* type) -> void
    {
        SCOPED_OBJECT();

        auto name = type->getName();
        auto kind = type->getKind();

        std::print("name: \"{}\" ", name ? name : "(null)");
        key("kind");
        std::print("{}", to_string(kind));

        print_common_type_info(type);

        switch (kind) {
            case slang::TypeReflection::Kind::Struct: {
                key("fields");
                auto field_count = type->getFieldCount();

                WITH_ARRAY();
                for (auto f = 0; f < field_count; f++) {
                    element();
                    auto field = type->getFieldByIndex(f);
                    print_variable(field);
                }
                break;
            }
            case slang::TypeReflection::Kind::Array:
            case slang::TypeReflection::Kind::Vector:
            case slang::TypeReflection::Kind::Matrix: {
                key("element type:");
                print_type(type->getElementType());
                break;
            }
            case slang::TypeReflection::Kind::Resource: {
                key("result type:");
                print_type(type->getResourceResultType());
                break;
            }
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
            case slang::TypeReflection::Kind::TextureBuffer:
            case slang::TypeReflection::Kind::ShaderStorageBuffer: {
                key("element type");
                print_type(type->getElementType());
                break;
            }
            default: break;
        }
    }

    auto print_possibly_unbounded(size_t value) -> void
    {
        if (value == ~size_t(0)) {
            std::print("unbounded");
        } else {
            std::print("{} ", value);
        }
    }

    auto print_common_type_info(slang::TypeReflection* type) -> void
    {
        switch (type->getKind()) {
            case slang::TypeReflection::Kind::Scalar: {
                key("scalar type");
                std::print("{} ", to_string(type->getScalarType()));
                break;
            }
            case slang::TypeReflection::Kind::Array: {
                key("element count");
                // print_possibly_unbounded(type->getElementCount());
                break;
            }
            case slang::TypeReflection::Kind::Vector: {
                key("element count");
                // std::print("{} ", type->getElementCount());
                break;
            }
            case slang::TypeReflection::Kind::Matrix: {
                key("row count");
                std::print("{} ", type->getRowCount());
                key("column count");
                std::print("{} ", type->getColumnCount());
                break;
            }
            case slang::TypeReflection::Kind::Resource: {
                key("shape");
                std::print("{}", to_string(type->getResourceShape()));
                key("access");
                std::print("{}", to_string(type->getResourceAccess()));
                break;
            }
            default: break;
        }
    }

    // ### Variable Layouts
    auto print_variable_layout(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void
    {
        SCOPED_OBJECT();

        auto name = variable_layout->getName();

        std::print("name: \"{}\" ", name ? name : "(null)");

        print_offsets(variable_layout, access_path);
        print_varying_parameter_info(variable_layout);

        ExtendedAccessPath variable_path(access_path, variable_layout);

        key("type layout");
        print_type_layout(variable_layout->getTypeLayout(), variable_path);
    }

    auto print_varying_parameter_info(slang::VariableLayoutReflection* variable_layout) -> void
    {
        if (auto semantic_name = variable_layout->getSemanticName()) {
            key("semantic");
            SCOPED_OBJECT();
            std::print("name: \"{}\" ", semantic_name ? semantic_name : "(null)");
            std::print("index: {} ", variable_layout->getSemanticIndex());
        }
    }

    auto print_relative_offsets(slang::VariableLayoutReflection* variable_layout) -> void
    {
        key("relative");
        auto used_layout_unit_count = variable_layout->getCategoryCount();
        WITH_ARRAY();
        for (auto i = 0; i < used_layout_unit_count; ++i) {
            element();
            auto layout_unit = variable_layout->getCategoryByIndex(i);
            print_offset(variable_layout, layout_unit);
        }
    }

    auto print_offset(slang::ParameterCategory layout_unit, size_t offset, size_t space) -> void
    {
        SCOPED_OBJECT();

        key_value("offset", offset);
        key_value("unit", to_string(layout_unit));

        switch (layout_unit) {
            case slang::ParameterCategory::ConstantBuffer:
            case slang::ParameterCategory::ShaderResource:
            case slang::ParameterCategory::UnorderedAccess:
            case slang::ParameterCategory::SamplerState:
            case slang::ParameterCategory::DescriptorTableSlot: {
                key_value("space", space);
                break;
            }
            default: break;
        }
    }

    auto print_offset(slang::VariableLayoutReflection* variable_layout, slang::ParameterCategory layout_unit) -> void
    {
        print_offset(layout_unit, variable_layout->getOffset(layout_unit), variable_layout->getBindingSpace(layout_unit));
    }

    auto calculate_cumulative_offset(
        slang::VariableLayoutReflection* variable_layout,
        slang::ParameterCategory layout_unit,
        AccessPath access_path
    ) -> CumulativeOffset
    {
        auto result = calculate_cumulative_offset(layout_unit, access_path);
        result.value += variable_layout->getOffset(layout_unit);
        result.space += variable_layout->getBindingSpace(layout_unit);

        return result;
    }

    auto print_offsets(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void
    {
        key("offset");
        {
            SCOPED_OBJECT();
            print_relative_offsets(variable_layout);
            if (access_path.valid) {
                print_cumulative_offsets(variable_layout, access_path);
            }
        }

        if (access_path.valid) {
            print_stage_usage(variable_layout, access_path);
        }
    }

    auto print_cumulative_offset(slang::VariableLayoutReflection* variable_layout, slang::ParameterCategory layout_unit, AccessPath access_path) -> void
    {
        auto cumulative_offset = calculate_cumulative_offset(variable_layout, layout_unit, access_path);
        print_offset(layout_unit, cumulative_offset.value, cumulative_offset.space);
    }

    auto print_cumulative_offsets(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void
    {
        key("cumulative");

        auto used_layout_unit_count = variable_layout->getCategoryCount();
        WITH_ARRAY();
        for (auto i = 0; i < used_layout_unit_count; ++i) {
            element();
            auto layout_unit = variable_layout->getCategoryByIndex(i);
            print_cumulative_offset(variable_layout, layout_unit, access_path);
        }
    }

    // ### Type Layouts
    auto print_type_layout(slang::TypeLayoutReflection* type_layout, AccessPath access_path) -> void
    {
        SCOPED_OBJECT();

        auto name = type_layout->getName();
        auto kind = type_layout->getKind();

        key_value("name", std::format("\"{}\" ", name ? name : "(null)"));
        key_value("kind", to_string(kind));
        print_common_type_info(type_layout->getType());

        print_sizes(type_layout);
        print_kind_specific_info(type_layout, access_path);
    }

    auto print_sizes(slang::TypeLayoutReflection* type_layout) -> void
    {
        auto used_layout_unit_count = type_layout->getCategoryCount();
        if (used_layout_unit_count > 0) {
            key("sizes");
        }
        WITH_ARRAY()
        for (auto i = 0; i < used_layout_unit_count; ++i) {
            element();
            auto layout_unit = type_layout->getCategoryByIndex(i);
            print_size(type_layout, layout_unit);
        }

        if (type_layout->getSize() != 0) {
            key_value("alignment in bytes", type_layout->getAlignment());
            key_value("stride in bytes", type_layout->getStride());
        }
    }

    auto print_size(slang::TypeLayoutReflection* type_layout, slang::ParameterCategory layout_unit) -> void
    {
        auto size = type_layout->getSize(layout_unit);
        print_size(layout_unit, size);
    }

    auto print_size(slang::ParameterCategory layout_unit, size_t size) -> void
    {
        SCOPED_OBJECT();
        key("value");
        print_possibly_unbounded(size);
        key_value("unit", to_string(layout_unit));
    }

    // ### Kind-Specific Info
    auto print_kind_specific_info(slang::TypeLayoutReflection* type_layout, AccessPath access_path) -> void
    {
        switch (type_layout->getKind()) {
            case slang::TypeReflection::Kind::Struct: {
                key("fields");
                auto field_count = type_layout->getFieldCount();
                WITH_ARRAY();
                for (auto f = 0; f < field_count; f++) {
                    element();
                    auto field = type_layout->getFieldByIndex(f);
                    print_variable_layout(field, access_path);
                }
                break;
            }

            case slang::TypeReflection::Kind::Array:
            case slang::TypeReflection::Kind::Vector:
            case slang::TypeReflection::Kind::Matrix: {
                key("element type layout");
                print_type_layout(type_layout->getElementTypeLayout(), AccessPath());
                break;
            }

            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ParameterBlock:
            case slang::TypeReflection::Kind::TextureBuffer:
            case slang::TypeReflection::Kind::ShaderStorageBuffer: {
                auto container_layout = type_layout->getContainerVarLayout();
                auto element_layout = type_layout->getElementVarLayout();
                key("container");
                {
                    SCOPED_OBJECT();
                    print_offsets(container_layout, access_path);
                }
                key("content");
                {
                    SCOPED_OBJECT();

                    print_offsets(element_layout, access_path);

                    key("type layout");
                    print_type_layout(element_layout->getTypeLayout(), ExtendedAccessPath(access_path, element_layout));
                }
                break;
            }

            case slang::TypeReflection::Kind::Resource: {
                auto shape = type_layout->getResourceShape();
                if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER) {
                    key("element type layout");
                    print_type_layout(type_layout->getElementTypeLayout(), access_path);
                } else {
                    key("result type");
                    print_type(type_layout->getResourceResultType());
                }
                break;
            }

            default: break;
        }
    }

    // ### Stage Usage
    auto print_stage_usage(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void
    {

    }

    auto print_scope(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void
    {
        auto scope_offsets = ExtendedAccessPath{access_path, variable_layout};

        auto scope_type_layout = variable_layout->getTypeLayout();
        switch (scope_type_layout->getKind()) {
            case slang::TypeReflection::Kind::Struct: {
                key("parameters");
                for (auto f = 0; f < scope_type_layout->getFieldCount(); f++) {
                    element();
                    auto field = scope_type_layout->getFieldByIndex(f);
                    print_variable_layout(field, scope_offsets);
                }
            }
            case slang::TypeReflection::Kind::ConstantBuffer: {
                key("automatically-introduced constant buffer");
                {
                    SCOPED_OBJECT();
                    print_offsets(scope_type_layout->getContainerVarLayout(), scope_offsets);
                }

                print_scope(scope_type_layout->getElementVarLayout(), scope_offsets);
                break;
            }
            case slang::TypeReflection::Kind::ParameterBlock: {
                key("automatically-introduced parameter block");
                {
                    SCOPED_OBJECT();
                    print_offsets(scope_type_layout->getContainerVarLayout(), scope_offsets);
                }

                print_scope(scope_type_layout->getElementVarLayout(), scope_offsets);
                break;
            }
            default: {
                key("variable layout");
                print_variable_layout(variable_layout, access_path);
            }
        }
    }

    auto print_entry_point_layout(slang::EntryPointLayout* entry_point_layout, AccessPath access_path) -> void
    {
        SCOPED_OBJECT();

        key_value("stage", to_string(entry_point_layout->getStage()));
        key_value("entry point", entry_point_layout->getName());
        print_scope(entry_point_layout->getVarLayout(), access_path);

        auto result_variable_layout = entry_point_layout->getResultVarLayout();
        if (result_variable_layout->getTypeLayout()->getKind() != slang::TypeReflection::Kind::None) {
            key("result");
            print_variable_layout(result_variable_layout, access_path);
        }
    }

    auto print_program_layout(slang::ProgramLayout* program_layout) -> void
    {
        SCOPED_OBJECT();
        auto root_offsets = AccessPath{};
        root_offsets.valid = true;
        key("global scope");
        {
            SCOPED_OBJECT();
            print_scope(program_layout->getGlobalParamsVarLayout(), root_offsets);
        }

        key("entry points");
        auto entry_point_count = program_layout->getEntryPointCount();
        WITH_ARRAY()
        for (auto i = 0zu; i < entry_point_count; i++) {
            element();
            print_entry_point_layout(program_layout->getEntryPointByIndex(i), root_offsets);
        }
        new_line();
    }
}
