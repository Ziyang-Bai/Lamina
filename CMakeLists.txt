cmake_minimum_required(VERSION 3.16.3)
project(Lamina)

set(CMAKE_CXX_STANDARD 20)
set(LIBUV_LIBRARY uv_a)

# 设置可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置libuv子模块路径
set(LIBUV_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/external/libuv)

# 检查libuv子模块是否存在
if(NOT EXISTS ${LIBUV_ROOT}/CMakeLists.txt)
    message(FATAL_ERROR "LibUV submodule not found. Please run 'git submodule update --init --recursive'")
endif()

# 添加libuv子目录
add_subdirectory(${LIBUV_ROOT})
# 只针对 libuv 启用 PIC
set_target_properties(uv uv_a PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 设置libuv库和包含目录
set(LIBUV_LIBRARIES uv_a)
set(LIBUV_INCLUDE_DIRS ${LIBUV_ROOT}/include)
include_directories(${LIBUV_INCLUDE_DIRS})

include_directories(interpreter)
add_library(lamina_core SHARED
        interpreter/ast.hpp
        interpreter/bigint.hpp
        interpreter/interpreter.cpp
        interpreter/interpreter.hpp
        interpreter/irrational.hpp
        interpreter/lexer.cpp
        interpreter/lexer.hpp
        interpreter/main.cpp
        interpreter/parser.cpp
        interpreter/parser.hpp
        interpreter/rational.hpp
        interpreter/trackback.hpp
        interpreter/value.hpp
        interpreter/lamina.hpp
        extensions/standard/math.cpp
        extensions/standard/stdio.cpp
        extensions/standard/random.cpp
        extensions/standard/random.hpp
        extensions/standard/times.cpp
        extensions/standard/times.hpp
        extensions/standard/array.cpp
        extensions/standard/sockets.hpp
        extensions/standard/sockets.cpp
)

target_link_libraries(lamina_core PRIVATE ${LIBUV_LIBRARY})

if(MSVC)
    # MSVC specific options
    target_compile_options(lamina_core PRIVATE /W4)
else()
    # GCC/Clang options
    target_compile_options(lamina_core PRIVATE -fvisibility=hidden)
endif()

add_executable(Lamina
        interpreter/ast.hpp
        interpreter/bigint.hpp
        interpreter/interpreter.cpp
        interpreter/interpreter.hpp
        interpreter/irrational.hpp
        interpreter/lexer.cpp
        interpreter/lexer.hpp
        interpreter/main.cpp
        interpreter/parser.cpp
        interpreter/parser.hpp
        interpreter/rational.hpp
        interpreter/trackback.hpp
        interpreter/value.hpp
        interpreter/lamina.hpp
        interpreter/module.cpp
        interpreter/module.hpp
        extensions/standard/math.cpp
        extensions/standard/array.cpp
        extensions/standard/stdio.cpp
        extensions/standard/random.cpp
        extensions/standard/random.hpp
        extensions/standard/times.cpp
        extensions/standard/times.hpp
        extensions/standard/sockets.hpp
        extensions/standard/sockets.cpp
)

# 链接LIBUV到可执行文件
target_link_libraries(Lamina PRIVATE ${LIBUV_LIBRARY})

target_include_directories(lamina_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/interpreter
)

if(UNIX)
    target_link_libraries(Lamina PRIVATE
            lamina_core
            dl
    )
else()
    target_link_libraries(Lamina PRIVATE
        lamina_core
        uv
    )
endif()

# 链接libuv库到lamina_core
target_link_libraries(lamina_core PRIVATE uv)

if(UNIX AND NOT APPLE)
    message(STATUS "Building on Linux system.")

    file(GLOB_RECURSE EXTENSION_SOURCE_FILES
            "extensions/*.cpp"
            "extensions/*/*.cpp"
            "extensions/*/*/*.cpp"
    )

    foreach(SOURCE_FILE ${EXTENSION_SOURCE_FILES})
        if(NOT SOURCE_FILE MATCHES "extensions/standard")
            get_filename_component(SOURCE_NAME ${SOURCE_FILE} NAME_WE)
            add_library(${SOURCE_NAME} SHARED ${SOURCE_FILE})
            # 为扩展也链接LIBUV
            target_link_libraries(${SOURCE_NAME} lamina_core ${LIBUV_LIBRARY})
            set_target_properties(${SOURCE_NAME} PROPERTIES
                    LIBRARY_OUTPUT_DIRECTory ${CMAKE_SOURCE_DIR}/extensions
                    POSITION_INDEPENDENT_CODE ON
            )
            message(STATUS "Building extension: ${SOURCE_NAME} with core library and LIBUV")
        endif()
    endforeach()
endif()
