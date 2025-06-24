#pragma once

#include <type_traits>
#include <concepts>

namespace cannele
{
    // Check for container, see https://en.cppreference.com/w/cpp/named_req/Container
    template <typename T>
    concept is_container = requires (T& a, T& b) {
        typename T::value_type;
        typename T::reference;
        typename T::const_reference;
        typename T::iterator;
        typename T::const_iterator;
        typename T::difference_type;
        typename T::size_type;

        a.begin();
        a.end();
        a.cbegin();
        a.cend();
        a.swap(b);
        a.size();
        a.max_size();
        a.empty();
    };

    // A Type can copy constructable and assignable.
    template <typename T>
    inline constexpr bool is_copyable = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

    // A Type can copy constructable and assignable.
    template <typename T>
    inline constexpr bool is_movable = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

    template <typename T>
    inline constexpr bool is_forwardable = requires (T a, T b) {
        T{std::move(a)};
        a = std::move(b);
    };

    template <typename T>
    inline constexpr bool is_function_pointer_v = std::is_function_v<std::remove_pointer_t<T>> || std::is_member_function_pointer_v<T>;

    template <typename T>
    concept hashable_type = requires (T t) { std::hash<std::remove_cvref_t<T>>{}(t); } || requires (T t) { { t.hash() } -> std::convertible_to<size_t>; };

    template <typename T>
    inline constexpr bool is_hashable_v = hashable_type<T>;
}
