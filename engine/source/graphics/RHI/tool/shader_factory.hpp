#pragma once

#include "shader.hpp"

#include <core/task_scheduler.hpp>
#include <core/string_hash.hpp>

#include <slang.h>
#include <slang-com-ptr.h>
#include <unordered_map>
#include <mutex>

namespace cannele::inline graphics::rhi
{
    struct IDevice;

    struct ShaderFactory final
    {
        using SessionKey = size_t;
        // ModuleKey is the hash of session and module name.
        using ModuleKey = size_t;

    private:

        std::mutex mutex{};
        IDevice* device{};

        Slang::ComPtr<slang::IGlobalSession> global_session{};

        SessionKey default_session_key{0};

        std::unordered_map<SessionKey, Slang::ComPtr<slang::ISession>> sessions{};
        std::unordered_map<ModuleKey, Slang::ComPtr<slang::IModule>> slang_modules{};
        std::unordered_map<size_t, Slang::ComPtr<slang::IComponentType>> slang_composed_programs{};
        std::unordered_map<size_t, ShaderModuleHandle> rhi_modules{};

    public:

        ShaderFactory(IDevice* device);
        ~ShaderFactory();

        template <typename ShaderType>
        auto get_shader() -> ShaderModuleHandle
        {
            return get_shader_impl(typeid(ShaderType).hash_code());
        }


    private:

        auto get_shader_impl(size_t composition_hash) -> ShaderModuleHandle;
        auto recompile() -> void {} // TODO:
        auto register_shader_directory_module() -> std::vector<std::string>;
        auto compile_module_and_composition(SessionKey session_key = 0) -> void;
        auto load_shader_module(ShaderModule* shader_module, SessionKey session_key = 0) -> void;
        auto create_composed_program(ShaderComposition* shader_composition, SessionKey session_key = 0) -> void;
    };
}

