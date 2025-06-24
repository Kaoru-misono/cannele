#include "shader_compile.hpp"

#include <core/assert.hpp>

#include <format>
#include <xxhash.h>

namespace cannele::inline platform
{
    inline namespace
    {
        auto stage_to_name(EShaderStage stage) -> std::string
        {
            switch (stage) {
                case EShaderStage::vertex: {
                    return "STAGE_VERTEX";
                }
                case EShaderStage::geometry: {
                    return "STAGE_GEOMETRY";
                }
                case EShaderStage::fragment: {
                    return "STAGE_FRAGMENT";
                }
                case EShaderStage::compute: {
                    return "STAGE_COMPUTE";
                }
                case EShaderStage::tessellation_control: {
                    return "STAGE_TESSELLATION_CONTROL";
                }
                case EShaderStage::tessellation_evaluation: {
                    return "STAGE_TESSELLATION_EVALUATION";
                }
                case EShaderStage::task: {
                    return "STAGE_TASK";
                }
                case EShaderStage::mesh: {
                    return "STAGE_MESH";
                }
                default: {
                    CNE_UNREACHABLE();
                }
            }
        }
    }

    ShaderCompileInfo::ShaderCompileInfo(std::string_view path, std::string_view entry, EShaderStage stage)
        : entry_point{entry}, stage{stage}
    {
        file = file::FileSystem::try_current()->get_file(path);
        CNE_ASSERT(file);

        name = std::format("{}_{}", file.stem(), stage_to_name(stage));
        auto file_name = file.name();

        auto full_file_path = file.absolute_path();
        key = XXH64(full_file_path.data(), full_file_path.size(), 0);

        base_hash = XXH64(name.data(), name.size(), (XXH64_hash_t) stage);
        base_hash = XXH64(entry_point.data(), entry_point.size(), base_hash);
        base_hash = XXH64(file_name.data(), file_name.size(), base_hash);

        update_runtime_hash();
    }
    auto ShaderCompileInfo::update_runtime_hash() -> void
    {
        auto file_mtime = std::format("{}", file.last_mutated_time());
        runtime_hash = XXH64(file_mtime.data(), file_mtime.size(), base_hash);
    }

    auto ShaderCompileEnvironment::define(std::string_view name, std::string const& value) -> void
    {
        definitions[name.data()] = std::move(value);
    }

    auto ShaderCompileEnvironment::define(std::string_view name, bool value) -> void
    {
        definitions[name.data()] = value ? "1" : "0";
        // Following code will cause runtime stack overflow becacuse of recursion.
        // define(name, value ? "1" : "0");
    }

    auto ShaderCompileEnvironment::define(std::string_view name, int value) -> void
    {
        define(name, std::to_string(value));
    }

    auto ShaderCompileEnvironment::define(std::string_view name, uint32_t value) -> void
    {
        define(name, std::to_string(value));
    }

    auto ShaderCompileEnvironment::define(std::string_view name, float value) -> void
    {
        define(name, std::format("{:.9f}", value));
    }

    auto ShaderCompileEnvironment::add_instruction(std::string_view instruction) -> void
    {
        instructions.emplace(std::string{instruction});
    }
}
