#pragma once

#include <variant>

namespace cannele
{
    template <typename... Funcs>
    struct VisitorHelper final: Funcs...
    {
        explicit VisitorHelper(Funcs&&... funcs): Funcs(std::forward<Funcs>(funcs))... {}
        using Funcs::operator()...;
    };
    template <typename... Funcs>
    VisitorHelper(Funcs...) -> VisitorHelper<Funcs...>;

    template <typename... Ts, typename... Funcs>
    static auto match_variant(std::variant<Ts...> const& v, Funcs&&... funcs)
    {
        return std::visit(VisitorHelper{std::forward<Funcs>(funcs)...}, v);
    }
}
