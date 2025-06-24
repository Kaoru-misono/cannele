#pragma once

#include <string_view>
#include <vector>

namespace cannele
{
    struct Tokenizer
    {
        size_t line{1};
        size_t column{1};
        std::string_view source{};
        std::vector<char const*> line_starts; // For column number calculation
        char const* start{};
        char const* current{};

        Tokenizer(std::string_view code): source{code}, start(code.data()), current(code.data()), line_starts{code.data()} {}
        virtual ~Tokenizer() = default;

        // Step forward stride character, return the character of 'current' before step.
        virtual auto step_forward(size_t stride = 1) -> char const*;
        // Step backward stride character, return the character of 'current' before step.
        virtual auto step_backward(size_t stride = 1) -> char const*;
        // Step to the next character that matches the given character.
        virtual auto step_to_next(char c, bool move_start = false) -> std::string_view;
        // Step to the next line, 'current' will be moved to the start of the next line.
        virtual auto step_to_next_line() -> void;
        // Return the character at the given offset of the 'current'.
        virtual auto peek(int offset = 0) -> char;
        // Return true if the 'current' matches the given character and step forward.
        virtual auto match(char c, bool step = true) -> bool;
        // Return true if the 'current' is the end of the source or '\0'.
        virtual auto touch_the_end() -> bool;

        auto move_start_to_current() -> void { start = current; }
    };
}
