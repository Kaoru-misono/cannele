#pragma once

#include <vector>
#include <string>

namespace cannele
{
    auto split(std::string_view str, char delimiter) -> std::vector<std::string>;

    auto shrink_to_discard_prefix(std::string_view input, std::string_view prefix) -> std::string;

    auto bad_path_to_good_path(std::string_view path) -> std::string;
}
