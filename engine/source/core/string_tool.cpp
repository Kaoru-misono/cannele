#include "string_tool.hpp"

#include <core/log_system.hpp>

#include <filesystem>

namespace cannele
{
    auto split(std::string_view str, char delimiter) -> std::vector<std::string>
    {
        auto tokens = std::vector<std::string>{};
        auto start = 0zu;
        auto end = str.find(delimiter);

        while (end != std::string_view::npos) {
            if (end != start) {
                tokens.emplace_back(str.substr(start, end - start));
            }
            start = end + 1;
            end = str.find('/', start);
        }

        if (start < str.size()) {
            tokens.emplace_back(str.substr(start));
        }

        return tokens;
    }

    auto shrink_to_discard_prefix(std::string_view input, std::string_view prefix) -> std::string
    {
         if (input.starts_with(prefix)) {
            return std::string{input.substr(prefix.size())};
        }

        return std::string{input};
    }

    auto bad_path_to_good_path(std::string_view path) -> std::string
    {
        auto result = std::filesystem::absolute(path).lexically_normal().string();
        std::replace(result.begin(), result.end(), '\\', '/');

        return result;
    }
}
