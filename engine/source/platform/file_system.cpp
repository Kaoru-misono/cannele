#include "file_system.hpp"

#include <core/log_system.hpp>
#include <core/string_tool.hpp>

#include <fstream>

namespace cannele::inline platform::file
{
    inline namespace
    {
        using Path = std::filesystem::path;

        auto bad_path_to_good_path(std::string_view path) -> std::string
        {
            auto result = std::filesystem::absolute(path).lexically_normal().string();
            std::replace(result.begin(), result.end(), '\\', '/');
            return result;
        }

        auto engine_path() -> std::string
        {
            auto exe_dir = std::filesystem::current_path().string();
            auto engine_path_dir = exe_dir + "/engine_path.txt";
            std::string engine_path{};
            {
                auto file = File{engine_path_dir};
                engine_path = file.slurp();
            }
            if (!std::filesystem::exists(engine_path)) {
                CNE_CRITICAL("general", "File engine_path.txt is broken.");
            }
            return std::filesystem::path(engine_path).string();
        }
    }

    File::File(std::string_view p_path): path(bad_path_to_good_path(p_path)) {}

    File::operator bool ()
    {
        return !path.empty() && std::filesystem::exists(path);
    }

    auto File::name() -> std::string
    {
        return path.filename().string();
    }

    auto File::stem() -> std::string
    {
        return path.stem().string();
    }

    auto File::extension() -> std::string
    {
        return path.extension().string();
    }

    auto File::directory() -> std::string
    {
        return path.parent_path().string();
    }

    auto File::absolute_path() -> std::string
    {
        return path.string();
    }

    auto File::last_mutated_time() -> FileTimePoint
    {
        return std::filesystem::last_write_time(path);
    }

    auto File::slurp() -> std::string
    {
        auto fin = std::ifstream{path, std::ios::ate};
        if (!fin) {
            CNE_ERROR("Reading file {} is failed.", name());
            return {};
        }
        auto length = fin.tellg();
        fin.seekg(0);
        auto result = std::string(length, {});
        fin.read(result.data(), length);
        return result;
    }

    auto File::slurp_as_binary() -> std::vector<std::byte>
    {
        auto fin = std::ifstream{path, std::ios::ate | std::ios::binary};
        if (!fin) {
            CNE_ERROR("Reading file {} is failed.", name());
            return {};
        }
        auto length = fin.tellg();
        fin.seekg(0);
        auto result = std::vector<std::byte>(length, {});
        fin.read(reinterpret_cast<char*>(result.data()), length);
        return result;
    }

    auto File::spurt(std::string_view content) -> bool
    {
        auto fout = std::ofstream{path};
        if (!fout) {
            CNE_ERROR("Writing file {} is failed.", name());
            return false;
        }
        fout.write(content.data(), content.size());
        return true;
    }

    auto File::spurt_as_binary(std::span<std::byte> content) -> bool
    {
        auto fout = std::ofstream{path, std::ios::binary | std::ios::trunc};
        if (!fout) {
            CNE_ERROR("Writing file {} is failed.", name());
            return false;
        }
        fout.write(reinterpret_cast<char*>(content.data()), content.size());
        return true;
    }

    WorkingDirectory::WorkingDirectory(std::string_view in_root): root(bad_path_to_good_path(in_root)) {}


    auto WorkingDirectory::file_exists(std::string_view path, File* opt_file) -> bool
    {
        auto parts = split(path, '/');
        if (parts.empty()) return false;

        auto current = this;

        auto real_path = root / path;
        for (auto i = 0zu; i < parts.size() - 1; i++) {
            auto it = current->sub_directories.find(parts[i]);
            if (it == current->sub_directories.end()) {
                auto dir_path = current->root / parts[i];
                if (!std::filesystem::is_directory(dir_path)) {
                    return false;
                }
                it = current->sub_directories.emplace(parts[i], WorkingDirectory{dir_path.string()}).first;
            }
            current = &it->second;
        }

        auto file_path = current->root / parts.back();
        if (std::filesystem::is_regular_file(file_path)) {
            if (opt_file) *opt_file = File{file_path.string()};
            return true;
        }

        return false;
    }

    auto WorkingDirectory::get_file(std::string_view path) -> File
    {
        auto real_path = root / path;
        auto file = File{};
        if (!file_exists(path, &file)) {
            CNE_WARN("File of path does not exist, empty File has been returned.");
        }
        return file;
    }

    auto WorkingDirectory::create_file(std::string_view path) -> File
    {
        auto parts = split(path, '/');
        if (parts.empty()) {
            CNE_ERROR("Empty path for file creation");
            return {};
        }

        auto current = this;

        for (size_t i = 0; i < parts.size() - 1; ++i) {
            auto it = current->sub_directories.find(parts[i]);
            if (it == current->sub_directories.end()) {
                auto dir_path = current->root / parts[i];
                std::filesystem::create_directories(dir_path);
                it = current->sub_directories.emplace(
                    parts[i], WorkingDirectory{dir_path.string()}).first;
            }
            current = &it->second;
        }

        auto file_path = current->root / parts.back();
        if (std::filesystem::exists(file_path)) {
            CNE_ERROR("File: {} already exists", file_path.string());
            return get_file(path);
        }

        auto fout = std::ofstream{file_path};
        return File{file_path.string()};
    }

    auto WorkingDirectory::remove_file(std::string_view path) -> bool
    {
        auto file = File{};
        if (!file_exists(path, &file)) return false;
        return std::filesystem::remove(file.absolute_path());
    }

    auto WorkingDirectory::directory_name() -> std::string
    {
        return bad_path_to_good_path(root.string());
    }

    auto WorkingDirectory::find_directory(std::string_view path) -> WorkingDirectory*
    {
        auto parts = split(path, '/');
        auto current = this;

        for (auto& part : parts) {
            auto it = current->sub_directories.find(part);
            if (it == current->sub_directories.end()) {
                auto dir_path = current->root / part;
                if (!std::filesystem::is_directory(dir_path)) {
                    return nullptr;
                }
                it = current->sub_directories.emplace(part, WorkingDirectory{dir_path.string()}).first;
            }
            current = &it->second;
        }
        return current;
    }

    FileSystem::FileSystem()
    {
        register_working_directory("runtime", WorkingDirectory{runtime_path()});
        register_working_directory("engine", WorkingDirectory{engine_path()});
        register_working_directory("shader", WorkingDirectory{engine_path() + "/shader"});
    }

    auto FileSystem::register_working_directory(std::string_view name, WorkingDirectory directory) -> bool
    {
        auto it = working_directorys.emplace(name, std::move(directory));
        return it.second;
    }

    auto FileSystem::unregister_working_directory(std::string_view name) -> bool
    {
        auto it = working_directorys.find(std::string(name));
        if (it == working_directorys.end()) return false;
        working_directorys.erase(it);
        return true;
    }

    auto FileSystem::file_exists(std::string_view path) -> bool
    {
        if (std::filesystem::exists(path)) {
            return true;
        }

        // Search from all registered working directories.
        for (auto& [name, dir]: working_directorys) {
            if (path.starts_with(name)) {
                return dir.file_exists(path.substr(name.size() + 1));
            }
            auto root = dir.root.string();
            if (path.starts_with(root)) {
                return dir.file_exists(path.substr(root.size() + 1));
            }
        }
        return false;
    }

    auto FileSystem::get_file(std::string_view path) -> File
    {
        // Search from all registered working directories.
        for (auto& [name, dir]: working_directorys) {
            if (path.starts_with(name)) {
                return dir.get_file(path.substr(name.size() + 1));
            }
            auto root = dir.root.string();
            if (path.starts_with(root)) {
                return dir.get_file(path.substr(root.size() + 1));
            }
        }

        // Not found, check if exist.
        if (std::filesystem::exists(path)) {
            return File{path};
        }

        return {};
    }

    auto FileSystem::create_file(std::string_view path) -> File
    {
        for (auto& [name, dir]: working_directorys) {
            if (path.starts_with(name)) {
                return dir.create_file(path.substr(name.size() + 1));
            }
            auto root = dir.root.string();
            if (path.starts_with(root)) {
                return dir.create_file(path.substr(root.size() + 1));
            }
        }

        // Not found, check if exist.
        if (std::filesystem::exists(path)) {
            return get_file(path);
        } else {
            auto fs_path = std::filesystem::path{path};
            if (auto parent = fs_path.parent_path(); !parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            if (std::ofstream(fs_path).is_open()) {
                return get_file(path);
            }
        }

        // Create failed, return empty file.
        return {};
    }

    auto FileSystem::remove_file(std::string_view path) -> bool
    {
        auto slash_pos = path.find_first_of('/');
        if (slash_pos == std::string_view::npos) {
            for (auto& [_, dir]: working_directorys) {
                if (dir.remove_file(path)) return true;
            }
        }
        auto prefix_name = std::string(path.substr(0, slash_pos));
        auto it = working_directorys.find(prefix_name);
        if (it != working_directorys.end()) return it->second.remove_file(path.substr(slash_pos +1));
        return false;
    }

    auto FileSystem::try_current() -> FileSystem*
    {
        static auto file_system = FileSystem{};

        return &file_system;
    }

    auto FileSystem::runtime_path() -> std::string
    {
        return std::filesystem::current_path().string();
    }

    auto FileSystem::find_or_create_file(std::string_view path) -> File
    {
        auto file = get_file(path);
        if (!file) {
            file = create_file(path);
        }

        return file;
    }

    auto FileSystem::get_directory(std::string_view path) -> WorkingDirectory*
    {
        // First, split path by '/' and push them to stack.
        // Then, pop stack and find it, if it exists, pass stack to it and find continue.
        // Until the stack is empty, if it is not found, return nullptr, otherwise, return it.

        auto parts = split(path, '/');
        if (parts.empty()) return nullptr;

        auto wd_it = working_directorys.find(parts[0]);
        if (wd_it == working_directorys.end()) return nullptr;

        if (parts.size() == 1) return &wd_it->second;

        auto sub_path = std::string{};
        for (size_t i = 1; i < parts.size(); ++i) {
            sub_path += parts[i];
            if (i < parts.size() - 1) sub_path += '/';
        }

        return wd_it->second.find_directory(sub_path);
    }
}
