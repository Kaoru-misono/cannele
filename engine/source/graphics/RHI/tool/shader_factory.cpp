#include "shader_factory.hpp"
#include "../RHI.hpp"

#include <core/string_tool.hpp>
#include <graphics/RHI/RHI.hpp>

namespace cannele::inline graphics::rhi
{
    inline namespace
    {
        static constexpr auto hlsl_shader_path = "shader/hlsl";
    }

    ShaderFactory::ShaderFactory(IDevice* device)
        : device(device)
    {
        auto register_table = GlobalShaderRegisterTable::instance();
        for (auto& [key, shader_info]: register_table->shader_infos) {
            auto compile_batch = &register_table->compile_batches[shader_info->base_hash];
            add_shader_compile_task(shader_info.get(), compile_batch);
        }

        launch_and_wait(tasks);
        tasks.clear();
    }
    ShaderFactory::~ShaderFactory()
    {}

    auto ShaderFactory::add_shader_compile_task(ShaderCompileInfo* shader_info, ShaderPermutationCompileBatched* compile_batch) -> void
    {
        auto shader_key = shader_info->key;
        auto pending_compile_modules = &shader_modules[shader_key];
        for (auto& compile_info: compile_batch->batched_compile_info) {
            auto env = &compile_info.environment;
            auto hash = compile_info.hash;
            auto file_system = file::FileSystem::try_current();

            auto name = shader_info->file.stem();
            auto cache_path = std::format("{}/{}/{}", "runtime/shader/save", name, hash);
            if (device->backend() == EBackend::vulkan) {
                env->add_instruction(std::format("{0}={1}", "-fspv-target-env", "vulkan1.3"));
            }
            auto arguments = env->build_args();
            auto device = this->device;

            auto task = &tasks.emplace_back(new TaskSet{
                [=](this auto&& self, TaskSetPartition range, uint32_t threadnum) {
                    auto compiler = shader_compiler();

                    auto cache_file = file_system->get_file(cache_path);
                    if (!cache_file) {
                        // The cache does not exist, create it.
                        cache_file = file_system->create_file(cache_path);
                    }
                    else if (cache_file.last_mutated_time() >= shader_info->file.last_mutated_time()) {
                        // The cache is newer than the shader file, no need to re-compile.
                        auto data = cache_file.slurp_as_binary();
                        if (!data.empty()) {
                            auto create_info = ShaderModuleCreateInfo{
                                .name = name,
                                .file_name = shader_info->file.name(),
                                .entry = shader_info->entry_point,
                                .stage = shader_info->stage,
                                .code = data
                            };
                            pending_compile_modules->emplace(hash, device->create_shader_module(name, &create_info));
                            return;
                        }
                    }

                    // FIXME: Recompile when shader file included files are changed.

                    auto shader_data = shader_info->file.slurp_as_binary();

                    auto compile_result = CompileResult{};
                    compile_result = compiler->compile(shader_data, arguments);

                    if (compile_result.success) {
                        cache_file.spurt_as_binary(compile_result.shader);
                        auto create_info = ShaderModuleCreateInfo{
                            .name = name,
                            .file_name = shader_info->file.name(),
                            .entry = shader_info->entry_point,
                            .stage = shader_info->stage,
                            .code = compile_result.shader
                        };
                        pending_compile_modules->emplace(hash, device->create_shader_module(name, &create_info));
                        CNE_INFO("Shader {} compiled successfully", name);
                    } else {
                        CNE_ERROR("Failed to compile shader: {}", name);
                        if (auto message = shrink_to_discard_prefix(compile_result.message, "error: "); message != compile_result.message) {
                            CNE_ERROR("{}", message);
                        }
                    }
                    if (auto message = shrink_to_discard_prefix(compile_result.message, "warning: "); message != compile_result.message) {
                        CNE_WARN("{}", message);
                    } else {
                        CNE_ERROR("{}", compile_result.message);
                    }
                }
            });
        }
    }
}
