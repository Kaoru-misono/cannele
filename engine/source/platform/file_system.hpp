#pragma once

#include <vector>
#include <string>
#include <span>
#include <map>
#include <filesystem>

namespace cannele::inline platform::file
{
    using Path = std::filesystem::path;
    using FileTimePoint = std::filesystem::file_time_type;

    struct File final
    {
    private:

        Path path{};

    public:

        File() = default;
        File(std::string_view path);

        virtual explicit operator bool ();
        auto operator == (File const& other) const -> bool = default;

        virtual auto name() -> std::string;
        virtual auto stem() -> std::string;
        virtual auto extension() -> std::string;
        virtual auto directory() -> std::string;
        virtual auto absolute_path() -> std::string;
        virtual auto last_mutated_time() -> FileTimePoint;

        virtual auto slurp() -> std::string;
        virtual auto slurp_as_binary() -> std::vector<std::byte>;

        virtual auto spurt(std::string_view content) -> bool;
        virtual auto spurt_as_binary(std::span<std::byte> content) -> bool;
    };

    struct WorkingDirectory final
    {
    private:

        Path root{};
        std::map<std::string, File> files{};
        std::map<std::string, WorkingDirectory> sub_directories{};

        friend struct FileSystem;

    public:

        WorkingDirectory() = default;
        WorkingDirectory(std::string_view in_root);

        virtual auto file_exists(std::string_view path, File* opt_file = nullptr) -> bool;
        virtual auto get_file(std::string_view path) -> File;
        virtual auto create_file(std::string_view path) -> File;
        virtual auto remove_file(std::string_view path) -> bool;
        virtual auto directory_name() -> std::string;
        virtual auto find_directory(std::string_view path) -> WorkingDirectory*;
    };

    struct FileSystem
    {
    private:

        std::map<std::string, WorkingDirectory> working_directorys{};

    public:

        FileSystem();

        static auto try_current() -> FileSystem*;

        auto register_working_directory(std::string_view name, WorkingDirectory directory) -> bool;
        auto unregister_working_directory(std::string_view name) -> bool;

        auto file_exists(std::string_view path) -> bool;
        auto get_file(std::string_view path) -> File;
        auto create_file(std::string_view path) -> File;
        auto remove_file(std::string_view path) -> bool;
        auto runtime_path() -> std::string;
        auto find_or_create_file(std::string_view path) -> File;
        auto get_directory(std::string_view path) -> WorkingDirectory*;
    };
}
