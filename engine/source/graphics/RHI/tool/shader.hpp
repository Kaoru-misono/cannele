#pragma once

#include <core/idiom.hpp>
#include <core/assert.hpp>
#include <graphics/RHI/RHI_resource.hpp>

#include <slang-com-ptr.h>
#include <slang.h>
#include <unordered_set>

namespace cannele::inline graphics::rhi
{
    struct Shader
    {
        CNE_INTERFACE(Shader);
    };

    struct ShaderModule
    {
        std::string name{};
        std::string file_path{};
        std::unordered_set<std::string> dependencies{};
        std::unordered_map<std::string, uint32_t> entry_points{};
        size_t source_hash{};
        bool need_to_load{};
    };

    struct ShaderComposition
    {
        std::string name{};
        std::string module_name{};
        std::string entry_point{};
        EShaderStage stage{};

        std::vector<std::string> required_modules{};
        size_t hash{};

        auto update_hash() -> void
        {
            hash = 0;
            for (const auto& module : required_modules) {
                hash = core::hash_combine(hash, module);
            }
            hash = core::hash(name, hash, entry_point, (uint8_t) stage);
        }
    };

    struct ShaderRegisterTable final
    {
        std::unordered_map<std::string, ShaderModule> modules{};
        // Use struct hash code to identify the shader composition.
        std::unordered_map<size_t, ShaderComposition> compositions{};

        static auto instance() -> ShaderRegisterTable*
        {
            static ShaderRegisterTable instance;
            return &instance;
        }

        auto register_module(std::string_view name, std::string_view path) -> void
        {
            CNE_ASSERT(!modules.contains(name.data()));
            modules.emplace(name, ShaderModule{
                .name = std::string{name},
                .file_path = std::string{path},
            });
        }
    };

    struct ModuleRegistry
    {
        ModuleRegistry(std::string_view name, std::string_view path, std::initializer_list<std::string> dependencies = {})
        {
            auto register_table = ShaderRegisterTable::instance();
            auto modules = &register_table->modules;
            CNE_ASSERT(!modules->contains(name.data()));
            auto module = &modules->emplace(name, ShaderModule{
                .name = std::string{name},
                .file_path = std::string{path},
                .dependencies = dependencies
            }).first->second;
        }
    };

#define CONCAT_IMPL(X, Y) X##Y
#define CONCAT(X, Y) CONCAT_IMPL(X, Y)

#define REGISTER_SHADER_MODULE(MODULE_NAME, PATH, ...) \
    cannele::rhi::ModuleRegistry CONCAT(registered_module_, __COUNTER__){MODULE_NAME, PATH, {__VA_ARGS__}}

    template <typename CompositionType>
    struct CompositionRegistry
    {
        CompositionRegistry(std::string_view composition_name, std::string_view module_name, std::string_view entry_point, EShaderStage stage, std::initializer_list<std::string> required_modules = {})
        {
            auto register_table = ShaderRegisterTable::instance();
            auto compositions = &register_table->compositions;
            auto key = typeid(CompositionType).hash_code();
            CNE_ASSERT(!compositions->contains(key));
            auto composition = &compositions->emplace(key, ShaderComposition{
                .name = std::string{composition_name},
                .module_name = std::string{module_name},
                .entry_point = std::string{entry_point},
                .stage = stage,
                .required_modules = required_modules
            }).first->second;
            composition->update_hash();
        }
    };

#define REGISTER_SHADER_COMPOSITION(COMPOSITION_SHADER_TYPE, MODULE_NAME, ENTRY, STAGE, ...) \
    struct COMPOSITION_SHADER_TYPE: cannele::rhi::Shader {}; \
    cannele::rhi::CompositionRegistry<COMPOSITION_SHADER_TYPE> CONCAT(registered_composition_, __COUNTER__){#COMPOSITION_SHADER_TYPE, MODULE_NAME, ENTRY, STAGE, {__VA_ARGS__}}

}

