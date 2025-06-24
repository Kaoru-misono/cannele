#include "token.hpp"

#include <vector>
#include <memory>
#include <cctype>
#include <stdexcept>
// #include <fmt/format.h>

namespace cannele
{
    auto Tokenizer::step_forward(size_t stride) -> char const*
    {
        if (touch_the_end()) return current;

        auto previous = current;

        while (stride > 0 && !touch_the_end()) {
            if (*current++ == '\n') {
                line++;
                line_starts.emplace_back(current);
                column = 1;
            } else {
                column++;
            }
            stride--;
        }

        return previous;
    }

    auto Tokenizer::step_backward(size_t stride) -> char const*
    {
        if (current == start) return current;

        auto previous = current;

        while (stride > 0 && current > start) {
            if (*--current == '\n') {
                if (line > 1) {
                    line--;
                }
                auto line_start = line_starts[line - 1]; // Line 1 -> line_starts[0].
                column = current - line_start + 1;
            } else {
                column--;
            }
            stride--;
        }

        return previous;
    }

    auto Tokenizer::step_to_next(char c, bool move_start) -> std::string_view
    {
        while (!touch_the_end() && *current != c) {
            step_forward();
        }

        auto result = std::string_view{start, current};
        if (move_start) {
            move_start_to_current();
        }

        return result;
    }

    auto Tokenizer::step_to_next_line() -> void
    {
        auto has_line_continuation = false;

        do {
            has_line_continuation = false;

            while (!touch_the_end() && peek() != '\n' && peek() != '\r') {
                step_forward();
            }

            step_forward();

            if (peek() == '\n') {
                step_forward();
            }

            if (current > start + 1 && peek(-2) == '\\') {
                has_line_continuation = true;
            }
        }
        while (!touch_the_end() && has_line_continuation);
    }

    auto Tokenizer::peek(int offset) -> char
    {
        if (current + offset > source.data() + source.size()) return '\0';

        return *(current + offset);
    }

    auto Tokenizer::match(char c, bool step) -> bool
    {
        if (!touch_the_end() && *current != c) return false;
        if (step) {
            step_forward();
        }

        return true;
    }

    auto Tokenizer::touch_the_end() -> bool
    {
        if (*current == '\0') return true;

        return current == source.data() + source.size();
    }
}
