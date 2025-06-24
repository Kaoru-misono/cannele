#include "log_system.hpp"

namespace cannele
{
    inline namespace
    {
        ANSIInitializer initializer{};
    }

    LogSystem::LogSystem()
    {
        loggers["Cannele"] = std::make_unique<Logger>();
        loggers["Cannele"]->set_config(true, false, false, true);
    }

    auto LogSystem::instance() -> LogSystem*
    {
        static LogSystem instance{};
        return &instance;
    }

    auto LogSystem::register_logger(std::string const& name) -> Logger*
    {
        std::lock_guard<std::mutex> lock(mutex);
        loggers[name] = std::make_unique<Logger>();
        return loggers[name].get();
    }

    auto LogSystem::set_level(std::string const& name, ELogLevel level) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (loggers.contains(name)) {
            loggers[name]->set_level(level);
        } else {
            CNE_WARN("Logger {} not found", name);
        }
    }

    auto LogSystem::set_color(std::string const& name, ELogLevel level, uint16_t color) -> void
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (loggers.contains(name)) {

        }
    }

    auto LogSystem::get_logger(const std::string& name) -> Logger*
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (loggers.contains(name)) {
            return loggers[name].get();
        }
        return nullptr;
    }
}
