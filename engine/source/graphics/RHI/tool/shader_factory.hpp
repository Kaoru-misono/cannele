#pragma once

#include "shader.hpp"

#include <core/task_scheduler.hpp>

#include <unordered_map>
#include <mutex>

namespace cannele::inline graphics::rhi
{
    struct IDevice;

    struct ShaderFactory final
    {
        // ShaderKey is the hash of the shader file name.
        using ShaderKey = size_t;
        // ShaderTypeKey is the hash of the shader type.
        using ShaderTypeKey = size_t;

        using CompileInfoKey = size_t;

        using ShaderModules = std::unordered_map<size_t, ShaderModuleHandle>;

    private:

        std::unordered_map<ShaderKey, ShaderModules> shader_modules{};
        std::mutex mutex{};
        IDevice* device{};

        template <typename ShaderRegistryType>
        friend struct ShaderRegistry;

    public:

        ShaderFactory(IDevice* device);
        ~ShaderFactory();

        template <typename ShaderType, typename PermutationType = int32_t>
        auto get_shader(PermutationType permutation = 0) -> ShaderModuleHandle
        {
            auto global_register_table = GlobalShaderRegisterTable::instance();

            auto shader_info = global_register_table->shader_infos.at(typeid(ShaderType).hash_code()).get();

            auto shader_key = shader_info->key;
            auto hash = shader_info->base_hash;
            if constexpr (std::is_same_v<PermutationType, int32_t>) {
                hash = hash_combine(hash, permutation);
            } else {
                hash = hash_combine(hash, permutation.id());
            }

            return shader_modules.at(shader_key).at(hash);
        }

    private:

        auto recompile() -> void {} // TODO:
        auto add_shader_compile_task(ShaderCompileInfo* shader_info, ShaderPermutationCompileBatched* compile_batch) -> void;
        std::vector<std::unique_ptr<TaskSet>> tasks{};
    };

#define REGISTER_SHADER(SHADER_TYPE, PATH, ENTRY, STAGE) \
    ::cannele::rhi::ShaderRegistry<SHADER_TYPE> registered_shader_##SHADER_TYPE {PATH, ENTRY, STAGE}

#define DECLARE_DEFAULT_SHADER_AND_REGISTER(SHADER_TYPE, PATH, ENTRY, STAGE) \
    DECLARE_DEFAULT_SHADER(SHADER_TYPE); \
    REGISTER_SHADER(SHADER_TYPE, PATH, ENTRY, STAGE)
}

