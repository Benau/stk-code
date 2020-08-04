# Usage:
# cmake .. -DCCTOOLS_PREFIX=/path/to/cctools -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-ios.cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DRT=/path/to/libclang_rt.ios.a

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Darwin)
SET(CMAKE_SYSTEM_PROCESSOR arm64)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER ${CCTOOLS_PREFIX}/bin/arm-apple-darwin11-clang)
SET(CMAKE_CXX_COMPILER ${CCTOOLS_PREFIX}/bin/arm-apple-darwin11-clang++)

set(IOS_CPP_FLAGS "-I${CCTOOLS_PREFIX}/SDK/iPhoneOS13.5.sdk/usr/include/c++")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mios-version-min=9.0.0 ${IOS_CPP_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mios-version-min=9.0.0 ${IOS_CPP_FLAGS}")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mios-version-min=9.0.0 ${RT}")
SET(CMAKE_OSX_SYSROOT ${CCTOOLS_PREFIX}/SDK/iPhoneOS13.5.sdk)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH ${CCTOOLS_PREFIX}/SDK/iPhoneOS13.5.sdk)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ALWAYS)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(USE_WIIUSE FALSE CACHE BOOL "")
set(USE_SQLITE3 FALSE CACHE BOOL "")
set(IOS TRUE CACHE BOOL "")
