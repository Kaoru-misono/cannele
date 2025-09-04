#include "shader_factory.hpp"
#include "../RHI.hpp"

#include <core/string_tool.hpp>
#include <graphics/RHI/RHI.hpp>

#include <slang-com-ptr.h>
#include <ranges>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        static constexpr auto hlsl_shader_path = "shader/hlsl";

        using namespace Slang;
        using namespace slang;

        auto get_message(ComPtr<IBlob> blob) -> std::string_view
        {
            if (!blob) return {};

            auto message_view = std::string_view{reinterpret_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize()};

            return message_view;
        }

    }

    ShaderFactory::ShaderFactory(IDevice* device)
        : device(device)
    {
        CNE_ASSERT_WITH(createGlobalSession(global_session.writeRef()) == SLANG_OK, "Failed to create slang global session");

        auto search_path = register_shader_directory_module();

        auto file_system = file::FileSystem::try_current();
        for (auto path: search_path) {
            CNE_TRACE("Shader search path: {}", path);
        }

        auto search_paths_c_str = std::views::transform(search_path, [](auto const& path) { return path.c_str(); }) | std::ranges::to<std::vector>();

        auto target_dsec = TargetDesc{};
        target_dsec.format = SLANG_SPIRV;
        target_dsec.flags = 0;

        auto target_descs = std::vector<TargetDesc>{};
        target_dsec.profile = global_session->findProfile("spirv_1_5 + spvGroupNonUniformBallot + spvGroupNonUniformArithmetic");
        target_descs.emplace_back(target_dsec);

        auto preprocessor_macros = PreprocessorMacroDesc{.name = "SLANG_SCOPE", .value = "1"};
        auto compiler_options = std::vector<CompilerOptionEntry>{
            {CompilerOptionName::BindlessSpaceIndex, CompilerOptionValue{.intValue0 = 0}},
        };

        auto session_desc = SessionDesc{};
        session_desc.targetCount              = target_descs.size();
        session_desc.targets                  = target_descs.data();
        session_desc.compilerOptionEntryCount = 0;
        session_desc.defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
        session_desc.searchPathCount          = search_paths_c_str.size();
        session_desc.searchPaths              = search_paths_c_str.data();
        session_desc.preprocessorMacroCount   = 1;
        session_desc.preprocessorMacros       = &preprocessor_macros;
        session_desc.compilerOptionEntryCount = compiler_options.size();
        session_desc.compilerOptionEntries    = compiler_options.data();

        auto session = ComPtr<ISession>{};
        auto session_create_result = global_session->createSession(session_desc, session.writeRef());
        CNE_ASSERT_WITH(session_create_result == SLANG_OK, "Failed to create slang session");

        sessions.emplace(0, session);

        compile_module_and_composition(default_session_key);
    }
    ShaderFactory::~ShaderFactory()
    {}

    auto ShaderFactory::get_shader_impl(size_t composition_hash) -> ShaderModuleHandle
    {
        auto register_table = ShaderRegisterTable::instance();
        auto composition = &register_table->compositions.at(composition_hash);
        auto entry_index = register_table->modules.at(composition->module_name).entry_points[composition->entry_point];

        auto hash = core::hash(default_session_key, composition->module_name);

        auto rhi_module_hash = core::hash(hash, composition->entry_point, (uint8_t) composition->stage);

        auto it = rhi_modules.find(rhi_module_hash);
        if (it != rhi_modules.end()) return it->second;

        auto composed_program = slang_composed_programs.at(hash);
        auto spirv = ComPtr<IBlob>{};
        auto diagnostics_blob = ComPtr<IBlob>{};
        auto compile_result = composed_program->getEntryPointCode(
            entry_index, 0, spirv.writeRef(), diagnostics_blob.writeRef()
        );
        if (diagnostics_blob) {
            CNE_WARN("{}", get_message(diagnostics_blob));
        }
        CNE_ASSERT_WITH(compile_result == SLANG_OK, std::format("Failed to compile shader: {}", get_message(diagnostics_blob)));

        auto create_info = ShaderModuleCreateInfo{
            .name = composition->name,
            .file_name = register_table->modules.at(composition->module_name).file_path,
            .entry = composition->entry_point,
            .stage = composition->stage,
            .code = std::span{
                reinterpret_cast<const std::byte*>(spirv->getBufferPointer()),
                spirv->getBufferSize()
            } | std::ranges::to<std::vector>()
        };

        it = rhi_modules.emplace(rhi_module_hash, device->create_shader_module(composition->name, &create_info)).first;

        return it->second;
    }

    auto ShaderFactory::register_shader_directory_module() -> std::vector<std::string>
    {
        auto search_path = std::vector<std::string>{};
        auto shader_library_dir = file::FileSystem::try_current()->get_directory("shader/slang");
        search_path.emplace_back(shader_library_dir->directory_name());
        auto register_table = ShaderRegisterTable::instance();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(shader_library_dir->directory_name())) {
            if (entry.is_directory()) {
                search_path.emplace_back(entry.path().string());
            }
            else if (entry.is_regular_file()) {
                auto module_name = entry.path().stem().string();
                auto path = bad_path_to_good_path(entry.path().string());
                register_table->register_module(module_name, path);
            }
        }

        for (auto& [name, composition]: register_table->compositions) {
            if (register_table->modules.contains(composition.module_name)) {
                auto module = &register_table->modules.at(composition.module_name);
                module->entry_points.emplace(composition.entry_point, module->entry_points.size());
                for (auto& required_module : composition.required_modules) {
                    module->dependencies.emplace(required_module);
                }
                module->need_to_load = true;
            } else {
                CNE_ERROR("Module {} not found for composition {}", composition.module_name,composition.name);
            }
        }

        return search_path;
    }

    auto ShaderFactory::compile_module_and_composition(SessionKey session_key) -> void
    {
        auto register_table = ShaderRegisterTable::instance();

        for (auto& [name, module]: register_table->modules) {
            for (auto& dependency_name: module.dependencies) {
                load_shader_module(&register_table->modules.at(dependency_name), session_key);
            }

            load_shader_module(&module, session_key);
        }

        for (auto& [name, composition]: register_table->compositions) {
            create_composed_program(&composition, session_key);
        }
    }

    auto ShaderFactory::load_shader_module(ShaderModule* shader_module, SessionKey session_key) -> void
    {
        if (!shader_module->need_to_load) return;

        auto hash = core::hash(session_key, shader_module->name);

        if (slang_modules.contains(hash)) return;

        auto session = sessions[session_key];

        auto diagnostics_blob = ComPtr<IBlob>{};
        auto module = session->loadModule(shader_module->file_path.c_str(), diagnostics_blob.writeRef());
        if (diagnostics_blob) {
            CNE_WARN("{}", get_message(diagnostics_blob));
        }
        CNE_ASSERT_WITH(module, std::format("Failed to load slang module: {}", get_message(diagnostics_blob)));

        slang_modules.emplace(hash, module);
        CNE_TRACE("Loaded shader module {} to hash {}", shader_module->name, hash);
    }

    auto ShaderFactory::create_composed_program(ShaderComposition* shader_composition, SessionKey session_key) -> void
    {
        auto module_hash = core::hash(session_key, shader_composition->module_name);

        if (slang_composed_programs.contains(module_hash)) return;

        auto session = sessions[session_key];

        CNE_ASSERT_WITH(slang_modules.contains(module_hash), std::format("Slang module {} not found for composition {}", shader_composition->module_name, shader_composition->name));

        auto module = &ShaderRegisterTable::instance()->modules.at(shader_composition->module_name);
        auto component_types = std::vector<IComponentType*>{};
        for (auto& depenency_module: shader_composition->required_modules) {
            auto dependency_hash = core::hash(session_key, depenency_module);
            component_types.emplace_back(slang_modules.at(dependency_hash));
        }
        component_types.emplace_back(slang_modules.at(module_hash));
        auto entry_index = 0;
        for (auto& [name, index]: module->entry_points) {
            auto entry_point = ComPtr<IEntryPoint>{};
            slang_modules.at(module_hash)->findEntryPointByName(name.c_str(), entry_point.writeRef());
            CNE_ASSERT_WITH(entry_point, std::format("Failed to find slang entry point: {}", name));
            component_types.emplace_back(entry_point);
            // Fix index to composed order.
            index = entry_index++;
        }

        auto diagnostics_blob = Slang::ComPtr<slang::IBlob>{};
        auto composed_program = Slang::ComPtr<slang::IComponentType>{};
        auto compose_result = session->createCompositeComponentType(
            component_types.data(), component_types.size(),
            composed_program.writeRef(), diagnostics_blob.writeRef()
        );
        if (diagnostics_blob) {
            CNE_WARN("{}", get_message(diagnostics_blob));
        }
        CNE_ASSERT_WITH(compose_result == SLANG_OK, std::format("Failed to compose program: {}", get_message(diagnostics_blob)));
        auto linked_program = Slang::ComPtr<slang::IComponentType>{};
        auto link_result = composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
        if (diagnostics_blob) {
            CNE_WARN("{}", get_message(diagnostics_blob));
        }
        CNE_ASSERT_WITH(link_result == SLANG_OK, std::format("Failed to link program: {}", get_message(diagnostics_blob)));

        slang_composed_programs.emplace(module_hash, std::move(linked_program));
        CNE_TRACE("Created composed program {} to hash {}, module: {}", shader_composition->name, module_hash, shader_composition->module_name);
    }
}
