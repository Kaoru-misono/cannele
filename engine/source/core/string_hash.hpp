#pragma once

#include <string>
#include <unordered_map>

struct StringHash final
{
    using is_transparent = void;

    auto operator()(std::string_view value) const -> size_t
    {
        return std::hash<std::string_view>{}(value);
    }

    auto operator()(std::string const& value) const -> size_t
    {
        return std::hash<std::string>{}(value);
    }

    auto operator()(char const* value) const -> size_t
    {
        return std::hash<std::string_view>{}(value);
    }
};

struct StringEqual final
{
    using is_transparent = void;

    auto operator()(std::string const& lhs, std::string const& rhs) const -> bool
    {
        return lhs == rhs;
    }

    auto operator()(std::string const& lhs, std::string_view rhs) const -> bool
    {
        return lhs == rhs;
    }

    auto operator()(std::string const& lhs, char const* rhs) const -> bool
    {
        return lhs == rhs;
    }

    auto operator()(std::string_view lhs, std::string const& rhs) const -> bool
    {
        return lhs == rhs;
    }

    auto operator()(char const* lhs, std::string const& rhs) const -> bool
    {
        return lhs == rhs;
    }
};

template <typename T>
using string_map = std::unordered_map<std::string, T, StringHash, StringEqual>;
