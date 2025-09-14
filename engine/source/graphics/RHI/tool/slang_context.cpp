#include "slang_context.hpp"

#include <core/assert.hpp>

namespace cannele::inline graphics::rhi
{
    auto SlangContext::initialize(SlangDesc const& desc, SlangCompileTarget target, std::string_view profile_name) -> bool
    {
        if (desc.global_session) {
            global_session = desc.global_session;
        } else {
            CNE_ASSERT_WITH(slang::createGlobalSession(global_session.writeRef()) == SLANG_OK, "Failed to create slang global session");
        }

        auto compile_options = std::move(desc.compiler_options);

        auto target_desc = slang::TargetDesc{};
        target_desc.format            = target;
        target_desc.profile           = global_session->findProfile(profile_name.data());
        target_desc.floatingPointMode = desc.floating_point_mode;
        target_desc.lineDirectiveMode = desc.line_directive_mode;
        target_desc.flags             = desc.target_flags;

        auto session_desc = slang::SessionDesc{};
        session_desc.defaultMatrixLayoutMode  = desc.matrix_layout_mode;
        session_desc.searchPathCount          = desc.search_paths.size();
        session_desc.searchPaths              = desc.search_paths.data();
        session_desc.preprocessorMacroCount   = desc.preprocessor_macros.size();
        session_desc.preprocessorMacros       = desc.preprocessor_macros.data();
        session_desc.compilerOptionEntryCount = compile_options.size();
        session_desc.compilerOptionEntries    = compile_options.data();
        session_desc.targetCount               = 1;
        session_desc.targets                   = &target_desc;

        CNE_ASSERT_WITH(global_session->createSession(session_desc, session.writeRef()) == SLANG_OK, "Failed to create slang session");

        return true;
    }

}
