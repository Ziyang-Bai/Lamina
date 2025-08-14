add_rules("mode.debug", "mode.release")

add_requires("libuv 056e180e11c3a2ff7120f484da9d0f15a5776fd4")
add_requires("exprtk cc1b800c2bd1ac3ac260478c915d2aec6f4eb41c")

target("lamina_core")
    set_kind("shared")
    set_languages("c++20")
    add_files(
        "interpreter/**.cpp|main.cpp|repl_input.cpp|module.cpp",
        "extensions/standard/**.cpp"
    )
    add_includedirs("interpreter")
    add_headerfiles("interpreter/*.hpp")
    add_packages("libuv")
    add_packages("exprtk")
    add_defines(
        "LAMINA_CORE_EXPORTS",
        "USE_LIBUV"
    )
    if is_plat("windows") then
        add_links("imagehlp")
    end
    add_rules("utils.symbols.export_all")

target("lamina")
    set_kind("binary")
    set_languages("c++20")
    add_deps("lamina_core")
    add_files(
        "interpreter/main.cpp",
        "interpreter/repl_input.cpp"
    )
    add_includedirs("interpreter")