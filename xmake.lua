add_rules("mode.debug", "mode.release")

set_languages("cxx23")
set_toolchains("clang-cl")
add_cxflags("/utf-8")

if is_mode("debug") then
    add_defines("CNE_DEBUG")
end

if is_plat("windows") then
    add_cxflags("/Zc:__cplusplus", { force = true })
end

includes("engine/xmake.lua")
