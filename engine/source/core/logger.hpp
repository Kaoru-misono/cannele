#pragma once

#include "enum_flag.hpp"

#include <mutex>
#include <ostream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#endif

namespace cannele
{
    struct ANSIInitializer
    {
        ANSIInitializer()
        {
#ifdef _WIN32
            HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dw_mode = 0;
            GetConsoleMode(output_handle, &dw_mode);
            dw_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(output_handle, dw_mode);
#endif
        }
    };

    struct LockedStream final
    {
        LockedStream(std::ostream& in_os): os(in_os) {}

        template <typename T>
        auto operator << (T&& arg) -> LockedStream&
        {
            std::lock_guard<std::mutex> lock(mutex);
            os << std::forward<T>(arg);
            return *this;
        }

        using Stream_Manipulator = std::ostream& (*) (std::ostream&);
        auto operator << (Stream_Manipulator manipulator) -> LockedStream&
        {
            std::lock_guard<std::mutex> lock(mutex);
            os << manipulator;
            return *this;
        }

        auto operator << (std::string& basic_string) -> LockedStream&
        {
            std::lock_guard<std::mutex> lock(mutex);
            os << basic_string;
            return *this;
        }

        std::mutex mutex{};
        std::ostream& os;
    };

    struct Style final
    {
        enum struct EColor: uint8_t
        {
            black,
            red,
            green,
            yellow,
            blue,
            magenta,
            cyan,
            white,
            not_set,
        };

        enum struct EFormat: uint8_t
        {
            not_set   = 0,
            bold      = 1 << 0,
            underline = 1 << 1,
            inverse   = 1 << 2,
            delete_line    = 1 << 3,
        };


        auto str() const -> std::string
        {
            auto ss = std::stringstream{};
            ss << "\033[";
            if (front_color == EColor::not_set && back_color == EColor::not_set) {
                ss << "0m";
                return ss.str();
            }

            bool need_sep = false;

            auto add_format = [&](EFormat in_fomrat, char const* c) -> void {
                if ((uint8_t) in_fomrat & (uint8_t) format) {
                    ss << (need_sep ? ";" : "") << c;
                    need_sep = true;
                }
            };

            add_format(EFormat::bold, "1");
            add_format(EFormat::underline, "4");
            add_format(EFormat::inverse, "7");
            add_format(EFormat::delete_line, "9");

            if (front_color != EColor::not_set) {
                ss << (need_sep ? ";" : "") << (((uint8_t) format & (uint8_t) EFormat::delete_line) ? 90 : 30) + (uint8_t) front_color;
                need_sep = true;
            }
            if (back_color != EColor::not_set) {
                ss << (need_sep ? ";" : "") << (((uint8_t) format & (uint8_t) EFormat::delete_line) ? 100 : 40) + (uint8_t) back_color;
                need_sep = true;
            }
            ss << "m";
            return ss.str();
        }

        inline static auto clear() -> std::string { return "\033[0m"; }

        friend auto operator << (std::ostream& os, Style const& style) -> std::ostream&
        {
            os << clear() << style.str();
            return os;
        }

        EColor front_color = EColor::not_set;
        EColor back_color = EColor::not_set;
        EFormat format = EFormat::not_set;
    };
    ENUM_STRUCT_FLAGS(Style::EFormat);

    template <typename Duration>
    auto format_time(std::chrono::time_point<std::chrono::system_clock> tp) -> std::string
    {
        using namespace std::chrono;

            auto sys_tp = time_point_cast<system_clock::duration>(tp);
            auto dp = floor<days>(sys_tp);
            auto tt = sys_tp - dp;
            auto ms = duration_cast<Duration>(tt);

            auto t = system_clock::to_time_t(sys_tp);
            auto tm = std::tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif

            auto ss =  std::stringstream{};
            ss << std::put_time(&tm, "%F %T.")
               << std::setfill('0') << std::setw(3) << ms.count() % 1000;

            return ss.str();
    }

    enum struct ELogLevel: uint8_t
    {
        info,
        trace,
        warning,
        error,
        exception,
    };

    struct Logger final
    {
        Logger(std::ostream& os = std::cout);

        template <typename... Args>
        auto log(ELogLevel in_level, std::format_string<Args...> fmt, Args&&... args) -> void;

        template <typename... Stream>
        auto log_stream(Stream&&... args) -> void;

        auto set_level(ELogLevel in_level) -> void;
        auto set_style(ELogLevel in_level, Style::EColor front_color, Style::EColor back_color, Style::EFormat format) -> void;
        auto set_config(bool in_enable_color, bool in_show_time, bool in_show_thread_id, bool in_show_level_prefix) -> void;

        static auto level_string(ELogLevel level) -> std::string;

    private:

        LockedStream output{std::cout};
        ELogLevel level{ELogLevel::info};
        std::unordered_map<ELogLevel, Style> level_styles{};
        bool enable_color{true};
        bool show_time{true};
        bool show_thread_id{true};
        bool show_level_prefix{true};
    };

    template <typename... Args>
    auto Logger::log(ELogLevel in_level, std::format_string<Args...> fmt, Args&&... args) -> void
    {
        if (in_level < level) return;

        auto ss = std::stringstream{};
        if (enable_color) {
            ss << level_styles[in_level];
        }
        if (show_time) {
            ss << '[' << format_time<std::chrono::milliseconds>(
                std::chrono::system_clock::now()) << "] ";
        }
        if (show_thread_id) {
            ss << "[T:" << std::this_thread::get_id() << "] ";
        }
        if (show_level_prefix) {
            ss << '[' << level_string(in_level) << "] ";
        }
        ss << std::vformat(fmt.get(), std::make_format_args(args...));
        ss << Style::clear();;
        // ss << " \033[2m(" << __FILE__ << ':' << __LINE__ << ")\033[0m";
        output << ss.str() << std::endl;
    }

    template <typename... Stream>
    auto Logger::log_stream(Stream&&... args) -> void
    {
        auto ss = std::stringstream{};

        auto output_without_style = [&ss](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (!std::is_same_v<T, Style>) {
                ss << std::forward<decltype(arg)>(arg);
            }
        };

        if (enable_color) {
            (ss << ... <<  std::forward<Stream>(args));
        } else {
            (output_without_style(std::forward<Stream>(args)), ...);
        }
        ss << Style::clear();
        output << ss.str() << std::endl;
    }
}
