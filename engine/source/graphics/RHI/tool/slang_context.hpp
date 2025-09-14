#pragma once

#include <slang.h>
#include <slang-com-ptr.h>

#include <vector>
#include <string_view>

namespace cannele::inline graphics::rhi
{
    struct SlangDesc final
    {
        slang::IGlobalSession* global_session{};

        SlangMatrixLayoutMode matrix_layout_mode{SLANG_MATRIX_LAYOUT_COLUMN_MAJOR};

        std::vector<char const*> search_paths{};

        std::vector<slang::PreprocessorMacroDesc> preprocessor_macros{};

        std::vector<slang::CompilerOptionEntry> compiler_options{};

        SlangFloatingPointMode floating_point_mode{SLANG_FLOATING_POINT_MODE_DEFAULT};
        SlangOptimizationLevel optimization_level{SLANG_OPTIMIZATION_LEVEL_DEFAULT};
        SlangTargetFlags target_flags{SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY};
        SlangLineDirectiveMode line_directive_mode{SLANG_LINE_DIRECTIVE_MODE_DEFAULT};
    };

    struct SlangContext final
    {
        using SlangGlobalSession = Slang::ComPtr<slang::IGlobalSession>;
        using SlangSession = Slang::ComPtr<slang::ISession>;
        using SlangModule = Slang::ComPtr<slang::IModule>;


        SlangGlobalSession global_session{};
        SlangSession session{};

        auto initialize(
            SlangDesc const& desc,
            SlangCompileTarget target,
            std::string_view profile_name
        ) -> bool;
    };
}
