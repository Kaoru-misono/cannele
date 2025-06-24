#include "logger.hpp"

namespace cannele
{
    Logger::Logger(std::ostream& os): output(os)
    {
        level_styles = {
            {ELogLevel::info,      {Style::EColor::green}},
            {ELogLevel::trace,     {Style::EColor::cyan}},
            {ELogLevel::warning,   {Style::EColor::yellow, Style::EColor::not_set}},
            {ELogLevel::error,     {Style::EColor::red,    Style::EColor::not_set, Style::EFormat::underline}},
            {ELogLevel::exception, {Style::EColor::white,  Style::EColor::red,     Style::EFormat::bold}},
        };
    }

    auto Logger::set_level(ELogLevel in_level) -> void
    {
        level = in_level;
    }

    auto Logger::set_style(ELogLevel in_level, Style::EColor front_color, Style::EColor back_color, Style::EFormat format) -> void
    {
        level_styles[in_level] = {front_color, back_color, format};
    }

    auto Logger::set_config(bool in_enable_color, bool in_show_time, bool in_show_thread_id, bool in_show_level_prefix) -> void
    {
        enable_color = in_enable_color;
        show_time = in_show_time;
        show_thread_id = in_show_thread_id;
        show_level_prefix = in_show_level_prefix;
    }

    auto Logger::level_string(ELogLevel level) -> std::string
    {
        using enum ELogLevel;
        switch (level) {
            case info:      return "INFO";
            case trace:     return "TRACE";
            case warning:   return "WARNING";
            case error:     return "ERROR";
            case exception: return "EXCEPTION";
            default:        return "NOTSET";
        }
    }
}
