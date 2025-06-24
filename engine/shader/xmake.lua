rule("generate_shader_structs") do
    on_build_file(function (target, file, opt)
        print(path.directory(file))
    end)
end

target("shader")
    set_kind("static")
    add_rules("generate_shader_structs")
    add_files("hpp/**.hpp")
    add_headerfiles("hpp/**.hpp")
    add_includedirs("hpp")
    add_defines("SHADER_HPP")

