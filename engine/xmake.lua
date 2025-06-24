add_requires("glm")
add_requires("glfw")
add_requires("vulkan-headers", "vulkan-memory-allocator")
add_requires("shaderc", "spirv-cross", "directxshadercompiler")
add_requires("assimp", "stb", "tinygltf")
add_requires("imgui", {configs = {glfw = true}})
add_requires("xxhash")
add_requires("enkits")

add_defines("VK_NO_PROTOTYPES")
if is_plat("windows") then
    add_defines("VK_USE_PLATFORM_WIN32_KHR")
    add_defines("NOMINMAX")
end

includes("library/xmake.lua")

target("engine") do
    set_kind("binary")
    add_files("source/**.cpp")
    add_headerfiles("source/**.hpp", "shader/hpp/**.hpp")
    add_includedirs("source", {public = true}, "shader/hpp")

    add_defines("CPP_SCOPE")

    add_deps("volk")
    add_packages("glm")
    add_packages("glfw")
    add_packages("vulkan-headers", "vulkan-memory-allocator")
    add_packages("shaderc", "spirv-cross", "directxshadercompiler")
    add_packages("assimp", "stb", "tinygltf")
    add_packages("imgui")
    add_packages("xxhash")
    add_packages("enkits")

    after_build_file(function (target, sourcefile, opt)
        -- import("xmake_modules.file")
        -- local generated_file = path.join(target:targetdir(), "generated", sourcefile)
        -- local file_mtime = os.mtime(sourcefile)
        -- local gen_mtime = os.mtime(generated_file)
        -- if file_mtime > gen_mtime then
        --     print(file_mtime, " ", gen_mtime)
        --     print("Regenerate file: " .. sourcefile)
        --     file.process_file(sourcefile, generated_file)
        -- end
        -- print(path.directory(sourcefile))
    end)

    after_build(function (target)
        io.writefile(target:targetdir().."/engine_path.txt", target:scriptdir())
    end)
end
