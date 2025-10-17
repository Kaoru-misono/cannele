target("volk")
    set_kind("static")
    add_files("volk/volk.c")
    add_headerfiles("volk/volk.h")
    add_includedirs("volk", {public = true})
    add_packages("vulkan-headers")


-- target("ktx")
--     set_kind("static")
--     add_files(
--         "ktx/lib/texture.c",
--         "ktx/lib/hashlist.c",
--         "ktx/lib/checkheader.c",
--         "ktx/lib/swap.c",
--         "ktx/lib/memstream.c",
--         "ktx/lib/filestream.c",
--         "ktx/lib/vkloader.c"
--     )
--     add_headerfiles("ktx/include/**.h", "ktx/lib/**.h", "ktx/other_include/KHR/**.h")
--     add_includedirs("ktx/include", "ktx/other_include", {public = true})
--     add_packages("vulkan-headers")

target("nv_cluster_lod_builder")
    set_kind("static")
    add_files(
        "nv_cluster_lod_builder/src/*.cpp",
        "nv_cluster_lod_builder/nv_cluster_builder/src/*.cpp"
    )
    add_headerfiles(
        "nv_cluster_lod_builder/include/**.h",
        "nv_cluster_lod_builder/include/**.hpp",
        "nv_cluster_lod_builder/src/**.hpp",
        "nv_cluster_lod_builder/nv_cluster_builder/include/**.h",
        "nv_cluster_lod_builder/nv_cluster_builder/include/**.hpp",
        "nv_cluster_lod_builder/nv_cluster_builder/src/*.hpp"
    )
    add_includedirs(
        "nv_cluster_lod_builder/include",
        "nv_cluster_lod_builder/src",
        "nv_cluster_lod_builder/nv_cluster_builder/include",
        "nv_cluster_lod_builder/nv_cluster_builder/src"
    )
    add_defines("NVCLUSTER_BUILDER_COMPILING", {public = false})
    add_defines("NVCLUSTERLOD_BUILDER_COMPILING", {public = false})
    add_defines("NVCLUSTERLOD_MULTITHREADED=1", {public = false})
    add_defines("NVCLUSTERLOD_HAS_MESHOPTIMIER=1", {public = false})
    if is_plat("windows") then
        add_cxxflags("/std:c++20", "/W4", "/WX")
        add_cxxflags("-Wno-missing-braces", {tools = {"clang", "clang_cl"}})
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
    else
        add_cxxflags("-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wconversion", "-Werror")
        add_links("tbb", {optional = true})
    end
    add_packages("meshoptimizer")
