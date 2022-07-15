project(glslang_mini)
cmake_minimum_required(VERSION 2.8.12)

# Dummy unused functions
function(glslang_add_build_info_dependency target)
endfunction()
function(glslang_only_export_explicit_symbols target)
endfunction()
function(glslang_pch target pch)
endfunction()

option(GLSLANG_MINI_SO "Build glslang_mini.so (for android)" OFF)

if(UNIX OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
endif()

# For build_info.h used in android
include_directories("./")

add_subdirectory(OGLCompilersDLL)
add_subdirectory(glslang)
add_subdirectory(SPIRV)

if(GLSLANG_MINI_SO)
    add_library(glslang_mini SHARED glslmini.cpp)
    target_link_libraries(glslang_mini
    "-Wl,--whole-archive"
    glslang SPIRV OGLCompiler
    "-Wl,--no-whole-archive")
endif()
