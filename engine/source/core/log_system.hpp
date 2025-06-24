#pragma once

#include "logger.hpp"

#include <memory>
#include <unordered_map>
#include <mutex>

namespace cannele
{
    struct LogSystem final
    {
        LogSystem();

        static auto instance() -> LogSystem*;

        auto register_logger(std::string const& name) -> Logger*;
        auto set_level(std::string const& name, ELogLevel level) -> void;
        auto set_color(std::string const& name, ELogLevel level, uint16_t color) -> void;
        auto get_logger(const std::string& name) -> Logger*;

    private:

        std::mutex mutex{};
        std::unordered_map<std::string, std::unique_ptr<Logger>> loggers{};
    };

#define CNE_TRACE(fmt, ...)   ::cannele::LogSystem::instance()->get_logger("Cannele")->log(::cannele::ELogLevel::trace, fmt, ##__VA_ARGS__)
#define CNE_INFO(fmt, ...)    ::cannele::LogSystem::instance()->get_logger("Cannele")->log(::cannele::ELogLevel::info, fmt, ##__VA_ARGS__)
#define CNE_WARN(fmt, ...)    ::cannele::LogSystem::instance()->get_logger("Cannele")->log(::cannele::ELogLevel::warning, fmt, ##__VA_ARGS__)
#define CNE_ERROR(fmt, ...)   ::cannele::LogSystem::instance()->get_logger("Cannele")->log(::cannele::ELogLevel::error, fmt, ##__VA_ARGS__)
#define CNE_CRITICAL(fmt, ...) ::cannele::LogSystem::instance()->get_logger("Cannele")->log(::cannele::ELogLevel::exception, fmt, ##__VA_ARGS__)

    template <typename... Args>
    auto print(std::format_string<Args...> fmt, Args&&... args) -> void
    {
        auto logger = LogSystem::instance()->get_logger("Cannele");
        logger->log_stream(std::vformat(fmt.get(), std::make_format_args(args...)));
    }
}
