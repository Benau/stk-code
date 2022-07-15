#!/bin/bash
# Inside official glslang repo and run this script with full path

GLSLANG_DIR="$(dirname $0)"
rm -rf "$GLSLANG_DIR/glslang"
rm -rf "$GLSLANG_DIR/SPIRV"
rm -rf "$GLSLANG_DIR/OGLCompilersDLL"

rm -rf build
git clean -f .
git reset --hard
# Get build_info.h
mkdir build && cd build && cmake .. && cd ..

# Remove unneeded code
rm -rf glslang/HLSL
sed -i 's/ENABLE_HLSL/REMOVE_ENABLE_HLSL/g' glslang/CMakeLists.txt
rm -rf glslang/OSDependent/Web

# Remove unneeded C interface
for file in glslang/CInterface/*
do
    echo "" > $file
done
for file in SPIRV/CInterface/*
do
    echo "" > $file
done

# Fix Switch build
sed -i 's/(UNIX OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Fuchsia")/(UNIX OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Fuchsia" OR "${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoSwitch")/g' glslang/CMakeLists.txt

# Fix Windows XP build
sed -i 's/MINGW_HAS_SECURE_API/REMOVE_MINGW_HAS_SECURE_API/g' glslang/Include/Common.h

cp -r glslang SPIRV OGLCompilersDLL "$GLSLANG_DIR"

# Copy build_info.h
cp build/include/glslang/build_info.h "$GLSLANG_DIR/glslang"
