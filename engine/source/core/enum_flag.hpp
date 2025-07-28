#pragma once

#include <type_traits>
#include <initializer_list>

template <typename T>
concept enum_type = std::is_enum_v<T> && !std::is_convertible_v<T, std::underlying_type_t<T>>;

// The Enum_Flags struct is a utility to handle bitwise operations on enum types.
template <enum_type Enum>
struct EnumFlags final {
    using Enum_Type = Enum;
    using Value = typename std::underlying_type_t<Enum>;

    constexpr EnumFlags() = default;
    constexpr EnumFlags(Enum in_enum);
    constexpr EnumFlags(std::initializer_list<Enum> enums);

    constexpr auto operator ~ () -> EnumFlags;

    constexpr auto operator == (EnumFlags flags) -> bool;

    explicit constexpr operator bool ();

    constexpr auto is_null() -> bool;

    [[nodiscard]] constexpr auto any(EnumFlags flags) -> bool;

    [[nodiscard]] constexpr auto all(EnumFlags flags) -> bool;

    [[nodiscard]] constexpr auto none(EnumFlags flags) -> bool;

    constexpr auto set(EnumFlags in_flags) -> void;
    constexpr auto unset(EnumFlags in_flags) -> void;

    constexpr auto operator |= (EnumFlags in_flags) -> EnumFlags&;
    constexpr auto operator &= (EnumFlags in_flags) -> EnumFlags&;

    mutable Value value{};
};

template <enum_type Enum> constexpr auto operator == (EnumFlags<Enum> a, EnumFlags<Enum> b) -> bool;
template <enum_type Enum> constexpr auto operator != (EnumFlags<Enum> a, EnumFlags<Enum> b) -> bool;

template <enum_type Enum> constexpr auto operator & (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>;
template <enum_type Enum> constexpr auto operator | (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>;
template <enum_type Enum> constexpr auto operator ^ (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>;

template <enum_type Enum>
inline constexpr EnumFlags<Enum>::EnumFlags(Enum in_enum): value(1 << (Value) in_enum) {}

template <enum_type Enum>
inline constexpr EnumFlags<Enum>::EnumFlags(std::initializer_list<Enum> in_enum): value(0)
{
    for (auto item: in_enum) {
        value |= 1 << (Value) item;
    }
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::operator ~ () -> EnumFlags
{
    value = ~value;
    return *this;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::operator == (EnumFlags flags) -> bool
{
    return value == flags.value;
}

template <enum_type Enum>
inline constexpr EnumFlags<Enum>::operator bool ()
{
    return (bool) value;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::is_null() -> bool
{
    return value == 0;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::any(EnumFlags<Enum> flags) -> bool
{
    return ((value & flags.value) != 0);
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::all(EnumFlags<Enum> flags) -> bool
{
    return ((value & flags.value) == value);
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::none(EnumFlags<Enum> flags) -> bool
{
    return ((value & flags.value) == 0);
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::set(EnumFlags<Enum> flags) -> void
{
    value |= flags.value;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::unset(EnumFlags<Enum> flags) -> void
{
    value &= ~flags.value;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::operator |= (EnumFlags<Enum> flags) -> EnumFlags&
{
    value |= flags.value;

    return *this;
}

template <enum_type Enum>
inline constexpr auto EnumFlags<Enum>::operator &= (EnumFlags<Enum> flags) -> EnumFlags&
{
    value &= flags.value;

    return *this;
}

template <enum_type Enum>
inline constexpr auto operator == (EnumFlags<Enum> a, EnumFlags<Enum> b) -> bool
{
    return a.value == b.value;
}

template <enum_type Enum>
inline constexpr auto operator != (EnumFlags<Enum> a, EnumFlags<Enum> b) -> bool
{
    return !(a == b);
}

template <enum_type Enum>
inline constexpr auto operator & (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>
{
    return (a &= b);
}

template <enum_type Enum>
inline constexpr auto operator | (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>
{
    return (a |= b);
}

template <enum_type Enum>
inline constexpr auto operator ^ (EnumFlags<Enum> a, EnumFlags<Enum> b) -> EnumFlags<Enum>
{
    return (a ^= b);
}

template <typename T>
struct flag_type {
    using type = std::underlying_type_t<T>;
};

// This macro defines bitwise operators for an enum type that already is an flag.
#define ENUM_STRUCT_FLAGS(Flags_Enum) \
    inline constexpr auto operator |= (Flags_Enum& lhs, Flags_Enum rhs) noexcept -> Flags_Enum& { using underlying_type = std::underlying_type_t<Flags_Enum>; return lhs = (Flags_Enum) (underlying_type(lhs) | underlying_type(rhs)); } \
    inline constexpr auto operator &= (Flags_Enum& lhs, Flags_Enum rhs) noexcept -> Flags_Enum& { using underlying_type = std::underlying_type_t<Flags_Enum>; return lhs = (Flags_Enum) (underlying_type(lhs) & underlying_type(rhs)); } \
    inline constexpr auto operator ^= (Flags_Enum& lhs, Flags_Enum rhs) noexcept -> Flags_Enum& { using underlying_type = std::underlying_type_t<Flags_Enum>; return lhs = (Flags_Enum) (underlying_type(lhs) ^ underlying_type(rhs));} \
    inline constexpr auto operator |  (Flags_Enum lhs, Flags_Enum rhs) noexcept -> Flags_Enum { using underlying_type = std::underlying_type_t<Flags_Enum>; return (Flags_Enum) (underlying_type(lhs) | underlying_type(rhs)); } \
    inline constexpr auto operator &  (Flags_Enum lhs, Flags_Enum rhs) noexcept -> Flags_Enum { using underlying_type = std::underlying_type_t<Flags_Enum>; return (Flags_Enum) (underlying_type(lhs) & underlying_type(rhs)); } \
    inline constexpr auto operator ^  (Flags_Enum lhs, Flags_Enum rhs) noexcept -> Flags_Enum { using underlying_type = std::underlying_type_t<Flags_Enum>; return (Flags_Enum) (underlying_type(lhs) ^ underlying_type(rhs)); } \
    inline constexpr auto operator !  (Flags_Enum e) noexcept -> bool { using underlying_type = std::underlying_type_t<Flags_Enum>; return !underlying_type(e); } \
    inline constexpr auto operator ~  (Flags_Enum e) noexcept -> Flags_Enum { using underlying_type = std::underlying_type_t<Flags_Enum>; return (Flags_Enum) ~underlying_type(e); } \
    static_assert(std::is_unsigned_v<std::underlying_type_t<Flags_Enum>>, "Enum must use unsigned underlying type.");


template <enum_type Flags_Enum>
constexpr auto enum_has_all_flags(Flags_Enum flags, Flags_Enum contains) -> bool
{
    using underlying_type = std::underlying_type_t<Flags_Enum>;
    return (underlying_type(flags) & underlying_type(contains)) == underlying_type(contains);
}

template <enum_type Flags_Enum>
constexpr auto enum_has_any_flags(Flags_Enum flags, Flags_Enum contains) -> bool
{
    using underlying_type = std::underlying_type_t<Flags_Enum>;
    return (underlying_type(flags) & underlying_type(contains)) != 0;
}

template <enum_type Flags_Enum>
constexpr auto enum_add_flags(Flags_Enum& flags, Flags_Enum flags_to_add) -> void
{
    using underlying_type = std::underlying_type_t<Flags_Enum>;
    flags = (Flags_Enum) (underlying_type(flags) | underlying_type(flags_to_add));
}

template <enum_type Flags_Enum>
constexpr auto enum_remove_flags(Flags_Enum& flags, Flags_Enum flags_to_remove) -> void
{
    using underlying_type = std::underlying_type_t<Flags_Enum>;
    flags = (Flags_Enum) (underlying_type(flags) & ~underlying_type(flags_to_remove));
}
