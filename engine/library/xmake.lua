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
