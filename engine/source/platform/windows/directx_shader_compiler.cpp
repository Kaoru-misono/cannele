#include "../shader_compile.hpp"

#include <core/assert.hpp>

#ifdef _WIN32

#include <wrl.h>
#include <dxcapi.h>
#include <d3d12shader.h>

namespace cannele::inline platform
{
    struct DirectXShaderCompiler final: IShaderCompiler
    {
        Microsoft::WRL::ComPtr<IDxcCompiler3> compiler{};
        Microsoft::WRL::ComPtr<IDxcUtils> utils{};
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler{};

        DirectXShaderCompiler()
        {
            CNE_ASSERT(!FAILED(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))));
            CNE_ASSERT(!FAILED(utils->CreateDefaultIncludeHandler(&include_handler)));
            CNE_ASSERT(!FAILED(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))));
        }
        ~DirectXShaderCompiler() override = default;

        auto compile(std::span<std::byte> data, std::span<std::string> args) -> CompileResult override
        {
            CompileResult result{};

            auto source_buffer = DxcBuffer{
                .Ptr = data.data(),
                .Size = data.size(),
                .Encoding = DXC_CP_UTF8,
            };

            auto compiled_shader_buffer = Microsoft::WRL::ComPtr<IDxcResult>{};

            auto strip_debug = false;
            auto strip_reflection = false;

            auto compilation_args = std::vector<LPCWSTR>{};
            auto wargs_storage = std::vector<std::wstring>{};
            compilation_args.reserve(args.size());
            wargs_storage.reserve(args.size());
            for (auto& arg: args)
            {
                if (arg.empty()) continue;

                auto wchar_count = MultiByteToWideChar(
                    CP_UTF8, 0,
                    arg.data(), static_cast<int>(arg.size()),
                    nullptr, 0
                );

                auto warg = std::wstring(wchar_count, L'\0');
                MultiByteToWideChar(
                    CP_UTF8, 0,
                    arg.data(), static_cast<int>(arg.size()),
                    warg.data(), wchar_count
                );

                wargs_storage.push_back(std::move(warg));
                compilation_args.push_back(wargs_storage.back().c_str());

                if (arg == "-Qstrip_debug") {
                    strip_debug = true;
                }
                else if (arg == "-Qstrip_reflect") {
                    strip_reflection = true;
                }
            }

            auto compile_result = compiler->Compile(
                &source_buffer,
                compilation_args.data(),
                (uint32_t) compilation_args.size(),
                include_handler.Get(),
                IID_PPV_ARGS(&compiled_shader_buffer)
            );
            result.success = !FAILED(compile_result);

            auto errors = Microsoft::WRL::ComPtr<IDxcBlobUtf8>{};
            CNE_ASSERT(!FAILED(compiled_shader_buffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr)));

            if (errors && errors->GetStringLength() > 0) {
                auto error_message = errors->GetStringPointer();
                result.message = error_message;

                // FIXME:
                // warning: Member functions will not be linked to their class in the debug information.
                // See https://github.com/KhronosGroup/SPIRV-Registry/issues/203
            }

            if (result.success) {
                auto shader_object = Microsoft::WRL::ComPtr<IDxcBlob>{};
                auto success = !FAILED(compiled_shader_buffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader_object.GetAddressOf()), nullptr));

                if (success) {
                    result.shader.resize(shader_object->GetBufferSize());
                    CNE_ASSERT_WITH(!result.shader.empty(), result.message);
                    CNE_TRACE("Shader size: {}", shader_object->GetBufferSize());
                    std::memcpy(result.shader.data(), shader_object->GetBufferPointer(), shader_object->GetBufferSize());
                } else {
                    CNE_ASSERT_WITH(false, result.message);
                }
            }

            if (result.success && strip_debug) {
                auto debug_data = Microsoft::WRL::ComPtr<IDxcBlob>{};
                auto debug_data_path = Microsoft::WRL::ComPtr<IDxcBlobUtf16>{};
                auto success = !FAILED(compiled_shader_buffer->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(debug_data.GetAddressOf()), debug_data_path.GetAddressOf()));

                if (success) {
                    result.pdb.resize(debug_data->GetBufferSize());
                    std::memcpy(result.pdb.data(), debug_data->GetBufferPointer(), debug_data->GetBufferSize());
                }
            }

            if (result.success && strip_reflection) {
                auto reflection_data = Microsoft::WRL::ComPtr<IDxcBlob>{};
                auto success = !FAILED(compiled_shader_buffer->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(reflection_data.GetAddressOf()), nullptr));

                if (success) {
                    result.reflection.resize(reflection_data->GetBufferSize());
                    std::memcpy(result.reflection.data(), reflection_data->GetBufferPointer(), reflection_data->GetBufferSize());
                }
            }
            return result;
        }
    };

    auto shader_compiler() -> IShaderCompiler*
    {
        static auto compiler = std::make_unique<DirectXShaderCompiler>();
        return compiler.get();
    }

    auto ShaderCompileEnvironment::build_args() -> ShaderCompilePlatformArguments
    {
        auto result = ShaderCompilePlatformArguments{};

        result.emplace_back("-spirv");
        result.emplace_back("-enable-16bit-types");
        result.emplace_back("-fspv-extension=SPV_KHR_ray_query");

        auto dir_name = file::FileSystem::try_current()->get_directory("shader")->directory_name();
        result.emplace_back("-I");
        result.emplace_back(dir_name);

        result.emplace_back("-I");
        result.emplace_back(compile_info->file.directory());

        result.emplace_back("-E");
        result.emplace_back(compile_info->entry_point);

        result.emplace_back("-T");
        switch (compile_info->stage) {
            case EShaderStage::vertex: {
                result.emplace_back("vs_6_8");
                break;
            }
            case EShaderStage::geometry: {
                result.emplace_back("gs_6_8");
                break;
            }
            case EShaderStage::fragment: {
                result.emplace_back("ps_6_8");
                break;
            }
            case EShaderStage::compute: {
                result.emplace_back("cs_6_8");
                break;
            }
            case EShaderStage::tessellation_control: {
                result.emplace_back("hs_6_8");
                break;
            }
            case EShaderStage::tessellation_evaluation: {
                result.emplace_back("ds_6_8");
                break;
            }
            case EShaderStage::task: {
                result.emplace_back("as_6_8");
                break;
            }
            case EShaderStage::mesh: {
                result.emplace_back("ms_6_8");
                break;
            }
            default: CNE_UNREACHABLE();
        }

        for (auto& [name, value] : definitions) {
            result.emplace_back("-D");
            if (value.empty()) {
                result.emplace_back(name);
            }
            else {
                result.emplace_back(std::format("{}={}", name, value));
            }
        }

        result.emplace_back("-D");
        switch (compile_info->stage) {
            case EShaderStage::vertex: {
                result.emplace_back(std::format("{}=1", "VERTEX_SHADER"));
                break;
            }
            case EShaderStage::geometry: {
                result.emplace_back(std::format("{}=1", "GEOMETRY_SHADER"));
                break;
            }
            case EShaderStage::fragment: {
                result.emplace_back(std::format("{}=1", "FRAGMENT_SHADER"));
                break;
            }
            case EShaderStage::compute: {
                result.emplace_back(std::format("{}=1", "COMPUTE_SHADER"));
                break;
            }
            case EShaderStage::tessellation_control: {
                result.emplace_back(std::format("{}=1", "TESSELLATION_CONTROL_SHADER"));
                break;
            }
            case EShaderStage::tessellation_evaluation: {
                result.emplace_back(std::format("{}=1", "TESSELLATION_EVALUATION_SHADER"));
                break;
            }
            case EShaderStage::task: {
                result.emplace_back(std::format("{}=1", "TASK_SHADER"));
                break;
            }
            case EShaderStage::mesh: {
                result.emplace_back(std::format("{}=1", "MESH_SHADER"));
                break;
            }
            default: {
                CNE_UNREACHABLE();
            }
        }

        // To split hlsl code.
        result.emplace_back("-D");
        result.emplace_back("HLSL_SCOPE=1");

        #ifdef CNE_DEBUG
        {
            result.emplace_back("-D");
            result.emplace_back("CNE_DEBUG");
            result.emplace_back("-fspv-debug=vulkan-with-source");
            result.emplace_back("-fspv-debug=line");
        }
        #endif

        for (auto& instruction: instructions) {
            result.emplace_back(instruction);
        }

        // TODO: move this outside platform code.
        result.emplace_back(std::format("{0}={1}", "-fspv-extension", "SPV_EXT_descriptor_indexing"));
        result.emplace_back(std::format("{0}={1}", "-fspv-extension", "SPV_EXT_mesh_shader"));

        return result;
    }
}

#endif
