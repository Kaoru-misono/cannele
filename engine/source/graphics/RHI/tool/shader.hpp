#pragma once

#include "shader_permutation.hpp"

#include <core/idiom.hpp>
#include <graphics/RHI/RHI_resource.hpp>

namespace cannele::inline graphics::rhi
{
    struct Shader
    {
        CNE_INTERFACE(Shader);

        virtual auto modify_compilation_environment(ShaderCompileEnvironment* environment, int32_t permutation_id) -> void = 0;
        virtual auto compile_permutation(int32_t permutation_id) -> bool = 0;
    };

    struct GlobalShaderRegisterTable final
    {
        // ShaderTypeKey is the hash of the shader type.
        using ShaderTypeKey = size_t;

        using CompileInfoKey = size_t;

        std::unordered_map<ShaderTypeKey, std::unique_ptr<ShaderCompileInfo>> shader_infos{};
        std::unordered_map<CompileInfoKey, ShaderPermutationCompileBatched> compile_batches{};

        static auto instance() -> GlobalShaderRegisterTable*
        {
            static GlobalShaderRegisterTable instance;
            return &instance;
        }
    };

    template <typename ShaderType>
    struct ShaderRegistry
    {
        ShaderRegistry(std::string_view path, std::string_view entry, EShaderStage stage)
        {
            auto register_table = GlobalShaderRegisterTable::instance();
            CNE_ASSERT(register_table);

            auto&shader_info = register_table->shader_infos[typeid(ShaderType).hash_code()];
            CNE_ASSERT(!shader_info);

            shader_info = std::make_unique<ShaderCompileInfo>(path, entry, stage);

            // Fetch all permutations and generate compile instructions.
            if constexpr (requires { typename ShaderType::Permutation; }) {
                using Permutation = ShaderType::Permutation;
                auto permutation_count = Permutation::permutation_count;

                auto compile_batch = &register_table->compile_batches.emplace(shader_info->base_hash, shader_info.get()).first->second;
                for (auto i = 0u; i < permutation_count; i++) {
                    compile_batch->add<ShaderType, Permutation>(Permutation::from_id(i));
                }
            } else {
                auto compile_batch = &register_table->compile_batches.emplace(shader_info->base_hash, shader_info.get()).first->second;
                compile_batch->add_default<ShaderType>();
            }
        }
    };

#define DECLARE_DEFAULT_SHADER(NAME) \
    struct NAME final: cannele::rhi::Shader { \
        using Parent = cannele::rhi::Shader; \
        auto modify_compilation_environment(ShaderCompileEnvironment* environment, int32_t permutation_id) -> void override {} \
        auto compile_permutation(int32_t permutation_id) -> bool override { return true; } \
    }
}

