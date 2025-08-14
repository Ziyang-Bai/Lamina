add_rules("mode.debug", "mode.release")

package("lamina_core")
    set_urls("https://github.com/Lamina-dev/Lamina.git")
    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)
package_end()

add_requires("lamina_core main")

target("minimal")
    set_kind("shared")
    set_languages("c++20")
    add_files("ultra_minimal.cpp")
    add_packages("lamina_core")