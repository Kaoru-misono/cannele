#pragma once

#include "file_system.hpp"

#include <core/enum_flag.hpp>
#include <core/idiom.hpp>

#include <vector>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace cannele::inline platform
{
    struct CompileResult final
    {
        bool success{false};
        std::string message{};

        std::vector<std::byte> shader{};
        std::vector<std::byte> pdb{};
        std::vector<std::byte> reflection{};
    };

    struct IShaderCompiler
    {
        CNE_INTERFACE(IShaderCompiler);

        virtual auto compile(std::span<std::byte> data, std::span<std::string> args) -> CompileResult = 0;
    };

    auto shader_compiler() -> IShaderCompiler*;

    enum struct EShaderStage: uint8_t
    {
        vertex,
        tessellation_control,
        tessellation_evaluation,
        geometry,
        fragment,
        compute,
        task,
        mesh,

        last,
    };
    using ShaderStageFlags = EnumFlags<EShaderStage>;

    struct ShaderCompileInfo final
    {
        std::string name{};
        mutable file::File file{};
        std::string entry_point{};
        EShaderStage stage{};

        size_t key{}; // hash of the file name.
        size_t base_hash{};
        size_t runtime_hash{};

        ShaderCompileInfo(std::string_view path, std::string_view entry, EShaderStage stage);

        auto update_runtime_hash() -> void;
    };

    using ShaderCompilePlatformArguments = std::vector<std::string>;

    struct ShaderCompileEnvironment final
    {
        using ShaderCompileDefinitions = std::unordered_map<std::string, std::string>;
        using ShaderCompilerInstructions = std::unordered_set<std::string>;

        ShaderCompileInfo const* compile_info{};
        ShaderCompileDefinitions definitions{};
        ShaderCompilerInstructions instructions{};

    public:

        explicit ShaderCompileEnvironment(ShaderCompileInfo const* compile_info)
            : compile_info{compile_info}
        {}

        auto define(std::string_view name, std::string const& value) -> void;
        auto define(std::string_view name, bool value) -> void;
        auto define(std::string_view name, int value) -> void;
        auto define(std::string_view name, uint32_t value) -> void;
        auto define(std::string_view name, float value) -> void;

        auto add_instruction(std::string_view instruction) -> void;

        auto build_args() -> ShaderCompilePlatformArguments;
    };
}
