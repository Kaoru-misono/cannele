#include "registry.hpp"

#include <core/assert.hpp>

namespace cannele::inline reflection
{
    auto Registration::register_type_impl(std::type_index type, deref_function* copy, deref_function* move, deref_assign_function* assign) -> void
    {
        CNE_ASSERT(!to_type_copys.contains(type) && !to_type_moves.contains(type) && !to_type_assigns.contains(type));
        to_type_copys.emplace(type, copy);
        to_type_moves.emplace(type, move);
        to_type_assigns.emplace(type, assign);
    }

    auto Registration::register_function_impl(std::string_view function_name, FunctionType function_call, std::type_index return_type, std::vector<std::type_index> parameters) -> void
    {
        auto function = functions.emplace_back(std::make_unique<FunctionEntry>(std::move(function_call), return_type, std::move(parameters))).get();
        to_functions.emplace(std::string{function_name}, function);
    }

    auto Registration::register_member_function_impl(std::type_index struct_type, std::string_view name, FunctionType function_call, std::type_index return_type, std::vector<std::type_index> parameters) -> void
    {
        CNE_ASSERT(to_structs.contains(struct_type));

        auto struct_entry = to_structs.at(struct_type);
        auto function_name = std::string{name};
        auto base_name = name.substr(name.find_last_of(':') + 1);

        auto function = functions.emplace_back(std::make_unique<FunctionEntry>(std::move(function_call), return_type, std::move(parameters))).get();
        struct_entry->scoped_functions.emplace(std::move(base_name), function);

        CNE_ASSERT(!to_functions.contains(function_name));
        to_functions.emplace(std::move(function_name), function);
    }

    auto Registration::register_constant_impl(std::string_view name, std::any value) -> void
    {
        CNE_ASSERT(!to_constants.contains(std::string{name}));

        auto constant_entry = constants.emplace_back(std::make_unique<ConstantEntry>(std::move(value))).get();
        to_constants.emplace(std::string{name}, constant_entry);
    }

    auto Registration::register_variable_impl(std::type_index type, std::string_view name, Getter_Function_Type get, Setter_Function_Type set) -> void
    {
        CNE_ASSERT(!to_variables.contains(std::string{name}));

        auto variable_entry = variables.emplace_back(std::make_unique<VariableEntry>(type, get, set)).get();
        to_variables.emplace(std::string{name}, variable_entry);
    }

    auto Registration::register_member_variable_impl(std::type_index struct_type, std::type_index type, std::string_view name, Member_Getter_Function_Type get, Member_Setter_Function_Type set) -> void
    {
        CNE_ASSERT(to_structs.contains(struct_type));

        auto struct_entry = to_structs.at(struct_type);
        auto var_name = std::string{name};
        auto base_name = name.substr(name.find_last_of(':') + 1);

        CNE_ASSERT(!struct_entry->scoped_variables.contains(var_name));

        auto member_variable_entry = member_variables.emplace_back(std::make_unique<MemberVariableEntry>(type, get, set)).get();
        struct_entry->scoped_variables.emplace(base_name, member_variable_entry);
    }

    auto Registration::register_enum_impl(std::type_index type, std::string_view name, ToCommonType* to_common) -> void
    {
        CNE_ASSERT(!to_enums.contains(type));

        enum_type_to_name.emplace(type, std::string{name});
        enum_name_to_type.emplace(std::string{name}, type);

        auto enum_entry = enums.emplace_back(std::make_unique<EnumEntry>(to_common)).get();
        to_enums.emplace(type, enum_entry);
    }

    auto Registration::register_enum_item_impl(std::string_view item_name, std::any item) -> void
    {
        auto enum_entry = to_enums.at(item.type());

        auto name = std::string{item_name};
        CNE_ASSERT(!enum_entry->scoped_constants.contains(name));

        auto constant_entry = constants.emplace_back(std::make_unique<ConstantEntry>(std::move(item))).get();
        auto member_name = std::string_view{
            enum_entry->scoped_constants.emplace(std::move(name), constant_entry).first->first
        };
        auto value = enum_entry->to_common(item);
        enum_entry->enumerator_to_type.emplace(value, member_name);
    }

    auto Registration::register_struct_impl(std::type_index type, std::string_view name) -> void
    {
        CNE_ASSERT(!to_structs.contains(type));

        struct_type_to_name.emplace(type, std::string{name});
        struct_name_to_type.emplace(std::string{name}, type);

        auto struct_entry = structs.emplace_back(std::make_unique<StructEntry>()).get();
        to_structs.emplace(type, struct_entry);
    }

    auto Registration::register_struct_parent_impl(std::type_index child, std::type_index parent) -> void
    {
        auto struct_entry = to_structs.at(child);

        struct_entry->parents.emplace_back(parent);
    }
}
