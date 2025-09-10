#pragma once

#include "metadata.hpp"

#include <core/traits.hpp>
#include <core/log_system.hpp>

#include <memory>

namespace cannele::inline reflection
{
    namespace details
    {

    }

    struct Registration final
    {
        std::vector<std::unique_ptr<FunctionEntry>> functions;
        std::vector<std::unique_ptr<ConstantEntry>> constants;
        std::vector<std::unique_ptr<VariableEntry>> variables;
        std::vector<std::unique_ptr<MemberVariableEntry>> member_variables;
        std::vector<std::unique_ptr<EnumEntry>> enums;
        std::vector<std::unique_ptr<StructEntry>> structs;

        std::unordered_map<std::type_index, deref_function*> to_type_copys;
        std::unordered_map<std::type_index, deref_function*> to_type_moves;
        std::unordered_map<std::type_index, deref_assign_function*> to_type_assigns;

        std::unordered_map<std::string, FunctionEntry*> to_functions;
        std::unordered_map<std::string, ConstantEntry*> to_constants;
        std::unordered_map<std::string, VariableEntry*> to_variables;

        std::unordered_map<std::type_index, EnumEntry*> to_enums;
        std::unordered_map<std::string, std::type_index> enum_name_to_type;
        std::unordered_map<std::type_index, std::string> enum_type_to_name;

        std::unordered_map<std::type_index, StructEntry*> to_structs;
        std::unordered_map<std::string, std::type_index> struct_name_to_type;
        std::unordered_map<std::type_index, std::string> struct_type_to_name;

        static auto instance() -> Registration*
        {
            static Registration instance{};
            return &instance;
        }

        auto register_type_impl(
            std::type_index type,
            deref_function* copy,
            deref_function* move,
            deref_assign_function* assign
        ) -> void;

        auto register_function_impl(
            std::string_view function_name,
            FunctionType function_call,
            std::type_index return_type,
            std::vector<std::type_index> parameters
        ) -> void;

        auto register_member_function_impl(
            std::type_index struct_type,
            std::string_view function_name,
            FunctionType function_call,
            std::type_index return_type,
            std::vector<std::type_index> parameters
        ) -> void;

        auto register_constant_impl(std::string_view name, std::any value) -> void;

        auto register_variable_impl(
            std::type_index type,
            std::string_view name,
            Getter_Function_Type get,
            Setter_Function_Type set
        ) -> void;

        auto register_member_variable_impl(
            std::type_index struct_type,
            std::type_index type,
            std::string_view name,
            Member_Getter_Function_Type get,
            Member_Setter_Function_Type set
        ) -> void;

        auto register_enum_impl(
            std::type_index type,
            std::string_view name,
            ToCommonType* to_common
        ) -> void;

        auto register_enum_item_impl(std::string_view item_name, std::any item) -> void;

        auto register_struct_impl(std::type_index type, std::string_view name) -> void;

        auto register_struct_parent_impl(std::type_index child, std::type_index parent) -> void;
    };

    template <typename T>
    auto dref_copy(std::any value) -> std::any
    {
        if constexpr (is_copyable<T>) {
            return std::any_cast<T>(value);
        } else {
            throw std::runtime_error(std::format("Type {} is not copyable.", typeid(T).name()));
        }
    }

    template <typename T>
    auto dref_move(std::any value) -> std::any
    {
        if constexpr (is_movable<T>) {
            return std::move(std::any_cast<T>(value));
        } else {
            throw std::runtime_error(std::format("Type {} is not movable.", typeid(T).name()));
        }
    }

    template <typename T>
    auto dref_assign(std::any ref, std::any value) -> void
    {
        if constexpr (!std::is_const_v<T> && is_forwardable<T>) {
            *std::any_cast<T>(&ref) = std::move(std::any_cast<T>(value));
        } else {
            throw std::runtime_error(std::format("Type {} is not assignable.", typeid(T).name()));
        }
    }

    template <typename T>
    auto register_type_and_pointer() -> void
    {
        Registration::instance()->register_type_impl(
            typeid(T),
            &dref_copy<T>,
            &dref_move<T>,
            &dref_assign<T>
        );

        Registration::instance()->register_type_impl(
            typeid(T*),
            &dref_copy<T>,
            &dref_move<T>,
            &dref_assign<T>
        );
    }

    template <typename T>
    auto register_abstract_type_and_pointer() -> void
    {
        Registration::instance()->register_type_impl(
            typeid(T),
            nullptr,
            nullptr,
            nullptr
        );

        Registration::instance()->register_type_impl(
            typeid(T*),
            nullptr,
            nullptr,
            nullptr
        );
    }

    template <typename T>
    auto register_type() -> void
    {
        if constexpr (std::is_abstract_v<T>) {
            register_abstract_type_and_pointer<T>();
        } else {
            register_type_and_pointer<T>();
        }
    }

    template <typename Derived, typename Base>
    auto parent(std::any derived) -> std::any
    {
        if (derived.type() == typeid(Derived)) {
            auto self_pointer = std::any_cast<Derived>(&derived);
            return static_cast<Base*>(self_pointer);
        }
        else if (derived.type() == typeid(Derived*)) {
            auto self_pointer = *std::any_cast<Derived*>(&derived);
            return static_cast<Base*>(self_pointer);
        }
        else {
            throw std::runtime_error("Invalid type.");
        }
    }

    template <typename Return, typename... Args, std::size_t... I>
    auto function(Return (*func)(Args...), std::any args[], std::index_sequence<I...>) -> std::any
    {
        if constexpr (std::is_void_v<Return>) {
            func(std::any_cast<Args>(args[I])...);
            return {};
        } else {
            return func(std::any_cast<Args>(args[I])...);
        }
    }

    template <typename Return, typename T, typename... Args, std::size_t... I>
    auto member_function(Return (T::* func)(Args...), T* obj, std::any args[], std::index_sequence<I...>) -> std::any
    {
        if constexpr (std::is_void_v<Return>) {
            (obj->*func)(std::any_cast<Args>(args[I])...);
            return {};
        } else {
            return (obj->*func)(std::any_cast<Args>(args[I])...);
        }
    }

    template <typename Return, typename... Args>
    auto dynamic_call(Return (*func)(Args...), std::any args[]) -> std::any
    {
        return function(func, args, std::index_sequence_for<Args...>{});
    }

    template <typename Return, typename T, typename... Args>
    auto dynamic_call(Return (T::* func)(Args...), T* obj, std::any args[]) -> std::any
    {
        return member_function(func, obj, args, std::index_sequence_for<Args...>{});
    }

    template <typename Return, typename... Args>
    auto register_function(std::string_view name, Return (*func)(Args...)) -> void
    {
        Registration::instance()->register_function_impl(
            name,
            [func] (std::initializer_list<std::any> args) -> std::any {
                auto array = args.size() > 0 ? const_cast<std::any*>(args.begin()) : nullptr;
                return dynamic_call(func, array);
            },
            typeid(Return),
            { typeid(Args)... }
        );
    }

    template <typename Struct, typename Return, typename... Args>
    auto register_member_function(std::string_view name, Return (Struct::* func)(Args...)) -> void
    {
        Registration::instance()->register_member_function_impl(
            typeid(Struct),
            name,
            [func] (std::initializer_list<std::any> args) -> std::any {
                auto obj = std::any_cast<Struct*>(*args.begin());
                auto array = args.size() > 1 ? const_cast<std::any*>(args.begin() + 1) : nullptr;
                return dynamic_call(func, obj,  array);
            },
            typeid(Return),
            { typeid(Struct*), typeid(Args)... }
        );
    }

    template <typename Constant>
    auto register_constant(std::string_view name, Constant value) -> void
    {
        if constexpr (std::is_enum_v<Constant>) {
            using underlying_type = std::underlying_type_t<Constant>;
            Registration::instance()->register_constant_impl(name, (underlying_type) value);
        } else {
            Registration::instance()->register_constant_impl(name, value);
        }
    }

    template <typename Variable>
    auto register_variable(std::string_view name, Variable* var) -> void
    {
        Registration::instance()->register_variable_impl(
            typeid(Variable),
            name,
            [var] () -> std::any {
                if constexpr (is_copyable<Variable>) {
                    return {*var};
                } else {
                    throw std::runtime_error(std::format("Variable of {} cannot be copied.", typeid(Variable).name()));
                }
            },
            [var] (std::any value) {
                if constexpr (is_forwardable<Variable>) {
                    *var = std::move(std::any_cast<Variable>(value));
                } else {
                    throw std::runtime_error(std::format("Variable of {} cannot be moved.", typeid(Variable).name()));
                }
            }
        );
    }

    template <typename Struct, typename Variable>
    auto register_variable(std::string_view name, Variable Struct::* var) -> void
    {
        Registration::instance()->register_member_variable_impl(
            typeid(Struct),
            typeid(Variable),
            name,
            [var] (std::any self) -> std::any {
                if constexpr (is_copyable<Variable>) {
                    auto self_pointer = std::any_cast<Struct*>(self);
                    return {&(self_pointer->*var)};
                } else {
                    throw std::runtime_error(std::format("Variable of {} cannot be copied.", typeid(Variable).name()));
                }
            },
            [var] (std::any self, std::any value) {
                if constexpr (is_forwardable<Variable>) {
                    auto self_pointer = std::any_cast<Struct*>(self);
                    self_pointer->*var = std::move(std::any_cast<Variable>(value));
                } else {
                    throw std::runtime_error(std::format("Variable of {} cannot be moved.", typeid(Variable).name()));
                }
            }
        );
    }

    template <typename Enum>
    auto enum_to_common_uint64(std::any value) -> uint64_t
    {
        return static_cast<uint64_t>(std::any_cast<Enum>(value));
    }

    template <typename Enum>
    auto register_enum(std::string_view name) -> void
    {
        Registration::instance()->register_enum_impl(typeid(Enum), name, &enum_to_common_uint64<Enum>);
    }

    template <typename Enum>
    auto register_enum_item(std::string_view name, Enum item) -> void
    {
        Registration::instance()->register_enum_item_impl(name, std::move(item));
    }

    template <typename Struct>
    auto register_struct(std::string_view name) -> void
    {
        Registration::instance()->register_struct_impl(typeid(Struct), name);
        register_type<Struct>();
    }

    template <typename Derived, typename Base>
    auto register_struct_parent() -> void
    {
        Registration::instance()->register_struct_parent_impl(typeid(Base), typeid(Base));
    }

    template <typename T, typename Struct>
    auto get_struct_member(Struct* object, std::string_view scoped_name) -> T*
    {
        auto registry = Registration::instance();
        CNE_ASSERT_WITH(registry->to_structs.contains(typeid(Struct)), "Trying to get non exist Struct");

        auto struct_entry = registry->to_structs.at(typeid(Struct));
        return std::any_cast<T*>(struct_entry->scoped_variables.at(scoped_name.data())->get(object));
    }

    template <typename Struct>
    auto get_struct_member_function_entry(Struct* object, std::string_view scoped_name) -> FunctionEntry*
    {
        auto registry = Registration::instance();
        CNE_ASSERT_WITH(registry->to_structs.contains(typeid(Struct)), "Trying to get non exist Struct");

        auto struct_entry = registry->to_structs.at(typeid(Struct));
        return struct_entry->scoped_functions.at(scoped_name.data());
    }
}

#define REFLECTION_REGISTER_CONSTANT(X) \
    ::cannele::reflection::register_constant(#X, X)

#define REFLECTION_REGISTER_VARIABLE(SCOPE, X) \
    ::cannele::reflection::register_variable(#SCOPE "::" #X, & SCOPE :: X)

#define REFLECTION_REGISTER_FUNCTION(SCOPE, X) \
    ::cannele::reflection::register_function(#SCOPE "::" #X, & SCOPE :: X)

#define REFLECTION_REGISTER_FUNCTION_OVERLOAD(SCOPE, X, NAME, ...) \
    ::cannele::reflection::register_function(#SCOPE "::" #NAME, (auto (*) __VA_ARGS__) & SCOPE::X)

#define REFLECTION_REGISTER_MEMBER_FUNCTION(SCOPE, X) \
    ::cannele::reflection::register_member_function(#SCOPE "::" #X, & SCOPE :: X)

#define REFLECTION_REGISTER_MEMBER_FUNCTION_OVERLOAD(SCOPE, X, NAME, ...) \
    ::cannele::reflection::register_member_function(#SCOPE "::" #NAME, (auto (SCOPE::*) __VA_ARGS__) & SCOPE::X)

#define REFLECTION_REGISTER_MEMBER_VARIABLE(SCOPE, X) \
    ::cannele::reflection::register_variable(#SCOPE "::" #X, & SCOPE :: X)

#define REFLECTION_REGISTER_STRUCT(X) \
    ::cannele::reflection::register_struct<X>(#X)

#define REFLECTION_REGISTER_STRUCT_PARENT(X, ...) \
    ::cannele::reflection::register_struct_parent<X, __VA_ARGS__>()

#define STRINGIFY(X) STRINGIFY_IMPL(X)
#define STRINGIFY_IMPL(X) #X

#define REFLECTION_REGISTER_ENUM() \
    ::cannele::reflection::register_enum<CURRENT_ENUM>(STRINGIFY(CURRENT_ENUM))

#define REFLECTION_REGISTER_ENUM_ITEM(ITEM) \
    ::cannele::reflection::register_enum_item(#ITEM, CURRENT_ENUM::ITEM)

