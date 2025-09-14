#pragma once

#include <slang.h>

namespace cannele::inline graphics::rhi
{
    struct CumulativeOffset
    {
        size_t value = 0;
        size_t space = 0;
    };

    // ### Access Paths

    struct AccessPathNode
    {
        slang::VariableLayoutReflection* variable_layout = nullptr;
        AccessPathNode* outer = nullptr;
    };

    struct AccessPath
    {
        AccessPath() {}

        bool valid = false;
        AccessPathNode* deepest_constant_bufer = nullptr;
        AccessPathNode* deepest_parameter_block = nullptr;
        AccessPathNode* leaf = nullptr;
    };

    struct ExtendedAccessPath : AccessPath
    {
        ExtendedAccessPath(AccessPath const& base, slang::VariableLayoutReflection* variable_layout)
            : AccessPath(base)
        {
            if (!valid)
                return;

            element.variable_layout = variable_layout;
            element.outer = leaf;

            leaf = &element;
        }

        AccessPathNode element;
    };

    auto print_variable(slang::VariableReflection* variable) -> void;
    auto print_type(slang::TypeReflection* type) -> void;
    auto print_possibly_unbounded(size_t value) -> void;
    auto print_common_type_info(slang::TypeReflection* type) -> void;
    auto print_variable_layout(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void;
    auto print_varying_parameter_info(slang::VariableLayoutReflection* variable_layout) -> void;
    auto print_relative_offsets(slang::VariableLayoutReflection* variable_layout) -> void;
    auto print_offset(slang::VariableLayoutReflection* variable_layout, slang::ParameterCategory layout_unit) -> void;
    auto print_offsets(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void;
    auto print_cumulative_offsets(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void;
    auto print_type_layout(slang::TypeLayoutReflection* type_layout, AccessPath access_path) -> void;
    auto print_sizes(slang::TypeLayoutReflection* type_layout) -> void;
    auto print_size(slang::TypeLayoutReflection* type_layout, slang::ParameterCategory layout_unit) -> void;
    auto print_size(slang::ParameterCategory layout_unit, size_t size) -> void;
    auto print_kind_specific_info(slang::TypeLayoutReflection* type_layout, AccessPath access_path) -> void;
    auto print_stage_usage(slang::VariableLayoutReflection* variable_layout, AccessPath access_path) -> void;
    auto print_program_layout(slang::ProgramLayout* program_layout) -> void;
}
