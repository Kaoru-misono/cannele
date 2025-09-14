#include "device.hpp"

#include <xxhash.h>
#include <ranges>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        auto get_message(slang::IBlob* blob) -> std::string_view
        {
            if (!blob) return {};

            auto message_view = std::string_view{reinterpret_cast<const char*>(blob->getBufferPointer()), blob->getBufferSize()};

            return message_view;
        }

        auto diagnose_if_need(slang::IBlob* blob) -> void
        {
            if (auto message = get_message(blob); !message.empty()) {
                if (message.contains("error")) {
                    CNE_ERROR("{}", get_message(blob));
                }
                else if (message.contains("warning")) {
                    CNE_WARN("{}", get_message(blob));
                }
            }
        }
    }

    IDevice::IDevice()
    {
        auto search_path = std::vector<std::string>{};
        auto shader_library_dir = file::FileSystem::try_current()->get_directory("shader/slang");
        search_path.emplace_back(shader_library_dir->directory_name());
        for (const auto& entry : std::filesystem::recursive_directory_iterator(shader_library_dir->directory_name())) {
            if (entry.is_directory()) {
                search_path.emplace_back(entry.path().string());
            }
        }

        auto slang_desc = SlangDesc{};
        slang_desc.search_paths = std::views::transform(search_path, [](auto const& path) { return path.c_str(); }) | std::ranges::to<std::vector>();
        slang_desc.compiler_options = std::vector<slang::CompilerOptionEntry>{
            {slang::CompilerOptionName::BindlessSpaceIndex, slang::CompilerOptionValue{.intValue0 = 0}},
        };

        slang_context.initialize(slang_desc, SLANG_SPIRV, "spirv_1_5 + spvGroupNonUniformBallot + spvGroupNonUniformArithmetic");
    }

    auto IDevice::entry_point_code_from_shader_cache(
        RHIShaderProgram* program,
        slang::IComponentType* component,
        std::string_view entry_point_name,
        uint32_t entry_point_index,
        uint32_t target_index
    ) -> std::span<std::byte const>
    {
        auto hash_blob = SlangBlob{};
        component->getEntryPointHash(entry_point_index, target_index, hash_blob.writeRef());
        auto key = XXH64(hash_blob->getBufferPointer(), hash_blob->getBufferSize(), 0);

        auto it = shader_cache.find(key);
        if (it != shader_cache.end()) {
            return it->second;
        }

        auto code = SlangBlob{};
        auto message = SlangBlob{};
        component->getEntryPointCode(entry_point_index, target_index, code.writeRef(), message.writeRef());
        if (message) {
            CNE_ERROR("{}", get_message(message));
        }

        auto cached_code = std::vector<std::byte>{
            (std::byte*) code->getBufferPointer(),
            (std::byte*) code->getBufferPointer() + code->getBufferSize()
        };

        it = shader_cache.emplace(key, std::move(cached_code)).first;

        return it->second;
    }

    auto IDevice::create_graphics_shader_program(
        std::string_view module_name,
        std::string_view vertex_entry_name,
        std::string_view fragment_entry_name
    ) -> std::shared_ptr<RHIShaderProgram>
    {
        auto hash = XXH64(module_name.data(), module_name.size(), 0);
        hash = XXH64(vertex_entry_name.data(), vertex_entry_name.size(), hash);
        hash = XXH64(fragment_entry_name.data(), fragment_entry_name.size(), hash);

        auto it = shader_programs.find(hash);
        if (it != shader_programs.end()) {
            return it->second;
        }

        auto diagnostics_blob = SlangBlob{};

        // TODO: cache module.
        auto module = slang_context.session->loadModule(module_name.data(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        auto create_info = ShaderProgramCreateInfo{};
        create_info.session = slang_context.session;

        auto vertex_entry_point = Slang::ComPtr<slang::IEntryPoint>{};
        module->findEntryPointByName(vertex_entry_name.data(), vertex_entry_point.writeRef());

        auto fragment_entry_point = Slang::ComPtr<slang::IEntryPoint>{};
        module->findEntryPointByName(fragment_entry_name.data(), fragment_entry_point.writeRef());

        auto component_types = std::vector<slang::IComponentType*>{
            module,
            vertex_entry_point,
            fragment_entry_point
        };

        auto composed_program = SlangComponent{};
        slang_context.session->createCompositeComponentType(
            component_types.data(),
            component_types.size(),
            composed_program.writeRef(),
            diagnostics_blob.writeRef()
        );
        diagnose_if_need(diagnostics_blob);

        auto linked_program = SlangComponent{};
        composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        create_info.root_component = linked_program;

        auto program = create_shader_program(&create_info);
        program->compile_shader();

        it = shader_programs.emplace(hash, std::move(program)).first;

        return it->second;
    }

    auto IDevice::create_graphics_shader_program(
        std::string_view vertex_module_name,
        std::string_view vertex_entry_name,
        std::string_view fragment_module_name,
        std::string_view fragment_entry_name
    ) -> std::shared_ptr<RHIShaderProgram>
    {
        auto hash = XXH64(vertex_module_name.data(), vertex_module_name.size(), 0);
        hash = XXH64(vertex_entry_name.data(), vertex_entry_name.size(), hash);
        hash = XXH64(fragment_module_name.data(), fragment_module_name.size(), hash);
        hash = XXH64(fragment_entry_name.data(), fragment_entry_name.size(), hash);

        auto it = shader_programs.find(hash);
        if (it != shader_programs.end()) {
            return it->second;
        }

        auto diagnostics_blob = SlangBlob{};

        // TODO: cache module.
        auto vertex_module = slang_context.session->loadModule(vertex_module_name.data(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);
        auto fragment_module = slang_context.session->loadModule(fragment_module_name.data(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        auto create_info = ShaderProgramCreateInfo{};
        create_info.session = slang_context.session;

        auto vertex_entry_point = Slang::ComPtr<slang::IEntryPoint>{};
        vertex_module->findEntryPointByName(vertex_entry_name.data(), vertex_entry_point.writeRef());

        auto fragment_entry_point = Slang::ComPtr<slang::IEntryPoint>{};
        fragment_module->findEntryPointByName(fragment_entry_name.data(), fragment_entry_point.writeRef());

        auto component_types = std::vector<slang::IComponentType*>{
            vertex_module,
            fragment_module,
            vertex_entry_point,
            fragment_entry_point
        };

        auto composed_program = SlangComponent{};
        slang_context.session->createCompositeComponentType(
            component_types.data(),
            component_types.size(),
            composed_program.writeRef(),
            diagnostics_blob.writeRef()
        );
        diagnose_if_need(diagnostics_blob);

        auto linked_program = SlangComponent{};
        composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        create_info.root_component = linked_program;

        auto program = create_shader_program(&create_info);
        program->compile_shader();

        it = shader_programs.emplace(hash, std::move(program)).first;

        return it->second;
    }

    auto IDevice::create_compute_shader_program(
            std::string_view module_name,
            std::string_view entry_point_name
    ) -> std::shared_ptr<RHIShaderProgram>
    {
        auto hash = XXH64(module_name.data(), module_name.size(), 0);
        hash = XXH64(entry_point_name.data(), entry_point_name.size(), hash);

        auto it = shader_programs.find(hash);
        if (it != shader_programs.end()) {
            return it->second;
        }

        auto diagnostics_blob = SlangBlob{};

        auto module = slang_context.session->loadModule(module_name.data(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        auto create_info = ShaderProgramCreateInfo{};
        create_info.session = slang_context.session;

        auto entry_point = Slang::ComPtr<slang::IEntryPoint>{};
        module->findEntryPointByName(entry_point_name.data(), entry_point.writeRef());

        auto component_types = std::vector<slang::IComponentType*>{
            module,
            entry_point
        };

        auto composed_program = SlangComponent{};
        slang_context.session->createCompositeComponentType(
            component_types.data(),
            component_types.size(),
            composed_program.writeRef(),
            diagnostics_blob.writeRef()
        );
        diagnose_if_need(diagnostics_blob);

        auto linked_program = SlangComponent{};
        composed_program->link(linked_program.writeRef(), diagnostics_blob.writeRef());
        diagnose_if_need(diagnostics_blob);

        create_info.root_component = linked_program;

        auto program = create_shader_program(&create_info);
        program->compile_shader();

        it = shader_programs.emplace(hash, std::move(program)).first;

        return it->second;
    }

    auto IDevice::get_shader_object_layout(slang::ISession* session, slang::TypeReflection* type, ShaderObjectType container) -> std::shared_ptr<ShaderObjectLayout>
    {
        switch (container) {
            case ShaderObjectType::structured_buffer: {
                type = session->getContainerType(type, slang::ContainerType::StructuredBuffer);
                break;
            }
            case ShaderObjectType::array: {
                type = session->getContainerType(type, slang::ContainerType::UnsizedArray);
                break;
            }
            default: break;
        }

        auto type_layout = session->getTypeLayout(type);

        return get_shader_object_layout(session, type_layout);
    }

    auto IDevice::get_shader_object_layout(slang::ISession* session, slang::TypeLayoutReflection* type_layout) -> std::shared_ptr<ShaderObjectLayout>
    {
        auto it = shader_object_layout_cache.find(type_layout);
        if (it != shader_object_layout_cache.end()) {
            return it->second;
        }

        it = shader_object_layout_cache.emplace(type_layout, create_shader_object_layout(session, type_layout)).first;

        return it->second;
    }

    auto IDevice::create_root_shader_object(RHIShaderProgram const* program) -> RootShaderObjectHandle
    {
        return std::make_shared<RootShaderObject>(program);
    }

    auto IDevice::create_shader_object(slang::ISession* session, slang::TypeReflection* type, ShaderObjectType container) -> ShaderObjectHandle
    {
        if (!session) {
            session = slang_context.session;
        }
        auto shader_object_layout = get_shader_object_layout(session, type, container);

        return std::make_shared<ShaderObject>(shader_object_layout.get());
    }
}
