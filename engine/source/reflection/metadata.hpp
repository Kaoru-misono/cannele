#pragma once

#include <string>
#include <unordered_map>
#include <any>
#include <functional>
#include <typeindex>

namespace cannele::inline reflection
{
    struct Metadata
    {
        using Content = std::unordered_map<std::string, std::any>;

        auto set_metadata(std::string_view name, std::any value) -> void;
        // Get metadata of a specific type, return {} if not found
        auto get_metadata(std::string_view name) -> std::any;

        template <typename T>
        auto get_metadata(std::string_view name, T default_value) -> std::decay<T>;

    private:

        Content metadatas{};
    };

    using deref_function = auto (std::any) -> std::any;
    using deref_assign_function = auto (std::any, std::any) -> void;

    using array_index_func = auto (std::any, int) -> std::any;
    using array_size_func = auto (std::any) -> size_t;
    using array_resize_func = auto (std::any, size_t) -> void;

    struct TypeEntry final: Metadata
    {
        std::type_index type;

        deref_function* copy{};
        deref_function* move{};
        deref_assign_function* assign{};
    };

    using FunctionType = std::function<auto (std::initializer_list<std::any>) -> std::any>;

    struct FunctionEntry final: Metadata
    {
        FunctionType call_fn{};
        std::type_index return_type;
        std::vector<std::type_index> parameters_types{};

        FunctionEntry(
            FunctionType in_call_func,
            std::type_index in_return_type,
            std::vector<std::type_index> in_parameters_types
        )
            : call_fn(std::move(in_call_func))
            , return_type(in_return_type)
            , parameters_types(std::move(in_parameters_types))
        {}

        auto call(std::initializer_list<std::any> args) -> std::any { return call_fn(args); }
        auto call() -> std::any { if (parameters_types.size() != 0) return {}; return call({}); }
        template <typename... Args>
        auto call(Args&&... args) -> std::any { return call({ std::forward<Args>(args)... }); }
    };

    struct ConstantEntry final: Metadata
    {
        std::any value{};

        ConstantEntry(std::any in_value): value(std::move(in_value)) {}
    };

    using Getter_Function_Type = std::function<auto () -> std::any>;
    using Setter_Function_Type = std::function<auto (std::any) -> void>;

    struct VariableEntry final: Metadata
    {
        std::type_index type;

        Getter_Function_Type get{};
        Setter_Function_Type set{};

        VariableEntry(
            std::type_index in_type,
            Getter_Function_Type in_get_func,
            Setter_Function_Type in_set_func
        )
            : type(in_type)
            , get(std::move(in_get_func))
            , set(std::move(in_set_func))
        {}
    };

    using Member_Getter_Function_Type = std::function<auto (std::any) -> std::any>;
    using Member_Setter_Function_Type = std::function<auto (std::any, std::any) -> void>;

    struct MemberVariableEntry final: Metadata
    {
        std::type_index type;

        Member_Getter_Function_Type get{};
        Member_Setter_Function_Type set{};

        MemberVariableEntry(
            std::type_index in_type,
            Member_Getter_Function_Type in_get_func,
            Member_Setter_Function_Type in_set_func
        )
            : type(in_type)
            , get(std::move(in_get_func))
            , set(std::move(in_set_func))
        {}
    };

    using ToParentFunctionType = std::function<auto (std::any) -> std::any>;

    struct StructEntry final: Metadata
    {
        std::vector<std::type_index> parents{};
        std::unordered_map<std::string, MemberVariableEntry*> scoped_variables{};
        std::unordered_map<std::string, FunctionEntry*> scoped_functions{};
    };

    using ToCommonType = auto (std::any) -> std::uint64_t;

    struct EnumEntry final: Metadata
    {
        std::unordered_map<std::string, ConstantEntry*> scoped_constants;
        std::unordered_map<std::uint64_t, std::string_view> enumerator_to_type;
        ToCommonType* to_common{};

        EnumEntry(ToCommonType* in_to_common)
            : to_common(in_to_common)
        {}
    };
}

namespace cannele::inline reflection
{
    inline auto Metadata::set_metadata(std::string_view name, std::any value) -> void
    {
        metadatas[std::string{name}] = value;
    }

    inline auto Metadata::get_metadata(std::string_view name) -> std::any
    {
        auto it = metadatas.find(std::string{name});
        if (it == metadatas.end()) return {};
        return it->second;
    }

    template <typename T>
    inline auto Metadata::get_metadata(std::string_view name, T default_value) -> std::decay<T>
    {
        auto it = metadatas.find(std::string{name});
        if (it == metadatas.end()) return default_value;
        return std::any_cast<T>(it->second);
    }
}
