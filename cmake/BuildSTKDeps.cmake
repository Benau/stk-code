# zlib
include(ExternalProject)

list(APPEND ZLIB_ARGS ${CROSS_ARGS})
list(APPEND ZLIB_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND ZLIB_ARGS -DBUILD_TESTING=OFF)

# From cmake file in zlib:
# "On unix-like platforms the library is almost always called libz"
if (UNIX)
    set(ZLIB_LIBRARY_NAME "/lib/libz.a")
else()
    set(ZLIB_LIBRARY_NAME "/lib/libzlibstatic.a")
endif()
ExternalProject_Add(zlib
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/zlib"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${ZLIB_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${ZLIB_LIBRARY_NAME}"
)

ExternalProject_Get_Property(zlib INSTALL_DIR)
# All library install include headers go here
set(UNIVERSAL_HEADER_DIR "${INSTALL_DIR}/include")
set(ZLIB_INCLUDE_DIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")

add_library(zlib_library STATIC IMPORTED)
set(ZLIB_LIBRARY_PATH "${INSTALL_DIR}${ZLIB_LIBRARY_NAME}")
set_target_properties(zlib_library PROPERTIES IMPORTED_LOCATION ${ZLIB_LIBRARY_PATH})
set(ZLIB_LIBRARY zlib_library CACHE INTERNAL "")
add_dependencies(zlib_library zlib)

# MbedTLS
list(APPEND MBEDTLS_ARGS ${CROSS_ARGS})
list(APPEND MBEDTLS_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND MBEDTLS_ARGS -DENABLE_PROGRAMS=OFF)
list(APPEND MBEDTLS_ARGS -DENABLE_TESTING=OFF)
list(APPEND MBEDTLS_ARGS -DBUILD_TESTING=OFF)
list(APPEND MBEDTLS_ARGS -DUSE_SHARED_MBEDTLS_LIBRARY=OFF)
list(APPEND MBEDTLS_ARGS -DUSE_STATIC_MBEDTLS_LIBRARY=ON)
# It's safe to ignore warnings in windows for socket to int conversion because
# max size is 2^24
if(WIN32)
    list(APPEND MBEDTLS_ARGS -DMBEDTLS_FATAL_WARNINGS=OFF)
endif()

set(MBEDTLS_LIBRARY_NAME "/lib/libmbedtls.a")
set(MBEDX509_LIBRARY_NAME "/lib/libmbedx509.a")
set(MBEDCRYPTO_LIBRARY_NAME "/lib/libmbedcrypto.a")
ExternalProject_Add(mbedtls
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/mbedtls"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${MBEDTLS_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${MBEDTLS_LIBRARY_NAME}"
                     "<INSTALL_DIR>${MBEDX509_LIBRARY_NAME}"
                     "<INSTALL_DIR>${MBEDCRYPTO_LIBRARY_NAME}"
)

set(MBEDTLS_INCLUDE_DIRS ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(mbedtls_library STATIC IMPORTED)
set(MBEDTLS_LIBRARY_PATH "${INSTALL_DIR}${MBEDTLS_LIBRARY_NAME}")
set_target_properties(mbedtls_library PROPERTIES IMPORTED_LOCATION ${MBEDTLS_LIBRARY_PATH})
set(MBEDTLS_LIBRARY mbedtls_library CACHE INTERNAL "")
add_dependencies(mbedtls_library mbedtls)

add_library(mbedx509_library STATIC IMPORTED)
set(MBEDX509_LIBRARY_PATH "${INSTALL_DIR}${MBEDX509_LIBRARY_NAME}")
set_target_properties(mbedx509_library PROPERTIES IMPORTED_LOCATION ${MBEDX509_LIBRARY_PATH})
set(MBEDX509_LIBRARY mbedx509_library CACHE INTERNAL "")
add_dependencies(mbedx509_library mbedtls)

add_library(mbedcrypto_library STATIC IMPORTED)
set(MBEDCRYPTO_LIBRARY_PATH "${INSTALL_DIR}${MBEDCRYPTO_LIBRARY_NAME}")
set_target_properties(mbedcrypto_library PROPERTIES IMPORTED_LOCATION ${MBEDCRYPTO_LIBRARY_PATH})
set(MBEDCRYPTO_LIBRARY mbedcrypto_library CACHE INTERNAL "")
add_dependencies(mbedcrypto_library mbedtls)

# Curl
list(APPEND CURL_ARGS ${CROSS_ARGS})
list(APPEND CURL_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND CURL_ARGS -DBUILD_TESTING=OFF)
list(APPEND CURL_ARGS -DBUILD_CURL_EXE=OFF)
list(APPEND CURL_ARGS -DBUILD_SHARED_LIBS=OFF)
if(WIN32)
    list(APPEND CURL_ARGS -DCURL_STATIC_CRT=ON)
    # Windows XP compatibility
    list(APPEND CURL_ARGS -DENABLE_INET_PTON=OFF)
endif()
# Required in windows
add_definitions(-DCURL_STATICLIB)
list(APPEND CURL_ARGS -DCMAKE_C_FLAGS=-DCURL_STATICLIB)
list(APPEND CURL_ARGS -DCMAKE_USE_MBEDTLS=ON)
list(APPEND CURL_ARGS -DMBEDTLS_INCLUDE_DIRS=${UNIVERSAL_HEADER_DIR})
list(APPEND CURL_ARGS -DMBEDTLS_LIBRARY=${MBEDTLS_LIBRARY_PATH})
list(APPEND CURL_ARGS -DMBEDX509_LIBRARY=${MBEDX509_LIBRARY_PATH})
list(APPEND CURL_ARGS -DMBEDCRYPTO_LIBRARY=${MBEDCRYPTO_LIBRARY_PATH})
list(APPEND CURL_ARGS -DUSE_ZLIB=ON)
list(APPEND CURL_ARGS -DZLIB_LIBRARY=${ZLIB_LIBRARY_PATH})
list(APPEND CURL_ARGS -DZLIB_INCLUDE_DIR=${UNIVERSAL_HEADER_DIR})
list(APPEND CURL_ARGS -DCMAKE_USE_OPENSSL=OFF)
list(APPEND CURL_ARGS -DCMAKE_USE_LIBSSH=OFF)
list(APPEND CURL_ARGS -DCMAKE_USE_LIBSSH2=OFF)
list(APPEND CURL_ARGS -DCMAKE_USE_GSSAPI=OFF)
list(APPEND CURL_ARGS -DUSE_NGHTTP2=OFF)
list(APPEND CURL_ARGS -DUSE_QUICHE=OFF)
list(APPEND CURL_ARGS -DHTTP_ONLY=ON)
# Don't use system bundle path, stk includes its own
list(APPEND CURL_ARGS -DCURL_CA_BUNDLE=none)
list(APPEND CURL_ARGS -DCURL_CA_PATH=none)

set(CURL_LIBRARY_NAME "/lib/libcurl.a")
ExternalProject_Add(curl
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/curl"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${CURL_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${CURL_LIBRARY_NAME}"
)
add_dependencies(curl zlib mbedtls)

set(CURL_INCLUDE_DIRS ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(curl_library STATIC IMPORTED)
set(CURL_LIBRARY_PATH "${INSTALL_DIR}${CURL_LIBRARY_NAME}")
set_target_properties(curl_library PROPERTIES IMPORTED_LOCATION ${CURL_LIBRARY_PATH})
set(CURL_LIBRARIES curl_library CACHE INTERNAL "")
add_dependencies(curl_library curl)

if(NOT IOS)
    # SQlite
    set(SQLITE_LIBRARY_NAME "/lib/libsqlite3.a")
    ExternalProject_Add(sqlite
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/sqlite"
        INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
        CMAKE_ARGS ${CROSS_ARGS} -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        BUILD_BYPRODUCTS "<INSTALL_DIR>${SQLITE_LIBRARY_NAME}"
    )

    set(SQLITE3_INCLUDEDIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
    add_library(sqlite_library STATIC IMPORTED)
    set(SQLITE_LIBRARY_PATH "${INSTALL_DIR}${SQLITE_LIBRARY_NAME}")
    set_target_properties(sqlite_library PROPERTIES IMPORTED_LOCATION ${SQLITE_LIBRARY_PATH})
    set(SQLITE3_LIBRARY sqlite_library CACHE INTERNAL "")
    add_dependencies(sqlite_library sqlite)
endif()

if(SERVER_ONLY)
    return()
endif()

# libpng
list(APPEND PNG_ARGS ${CROSS_ARGS})
list(APPEND PNG_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND PNG_ARGS -DPNG_SHARED=OFF)
list(APPEND PNG_ARGS -DPNG_TESTS=OFF)
list(APPEND PNG_ARGS -DBUILD_TESTING=OFF)
list(APPEND PNG_ARGS -DZLIB_LIBRARY=${ZLIB_LIBRARY_PATH})
list(APPEND PNG_ARGS -DZLIB_INCLUDE_DIR=${UNIVERSAL_HEADER_DIR})
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR
   CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
    list(APPEND PNG_ARGS -DPNG_ARM_NEON=off)
endif()

set(PNG_LIBRARY_NAME "/lib/libpng16.a")
ExternalProject_Add(libpng
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/libpng"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${PNG_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${PNG_LIBRARY_NAME}"
)
add_dependencies(libpng zlib)

set(PNG_PNG_INCLUDE_DIR "${UNIVERSAL_HEADER_DIR}/libpng16" CACHE INTERNAL "")
add_library(png_library STATIC IMPORTED)
set(PNG_LIBRARY_PATH "${INSTALL_DIR}${PNG_LIBRARY_NAME}")
set_target_properties(png_library PROPERTIES IMPORTED_LOCATION ${PNG_LIBRARY_PATH})
set(PNG_LIBRARY png_library CACHE INTERNAL "")
add_dependencies(png_library libpng)

# FreeType bootstrap
list(APPEND FREETYPE_ARGS ${CROSS_ARGS})
list(APPEND FREETYPE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND FREETYPE_ARGS -DPNG_LIBRARY=${PNG_LIBRARY_PATH})
list(APPEND FREETYPE_ARGS -DPNG_PNG_INCLUDE_DIR=${PNG_PNG_INCLUDE_DIR})
list(APPEND FREETYPE_ARGS -DZLIB_LIBRARY=${ZLIB_LIBRARY_PATH})
list(APPEND FREETYPE_ARGS -DZLIB_INCLUDE_DIR=${UNIVERSAL_HEADER_DIR})
list(APPEND FREETYPE_ARGS -DBUILD_SHARED_LIBS=OFF)
list(APPEND FREETYPE_ARGS -DBUILD_TESTING=OFF)
list(APPEND FREETYPE_ARGS -DFT_WITH_ZLIB=ON)
list(APPEND FREETYPE_ARGS -DFT_WITH_BZIP2=OFF)
list(APPEND FREETYPE_ARGS -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE)
list(APPEND FREETYPE_ARGS -DCMAKE_DISABLE_FIND_PACKAGE_BrotliDec=TRUE)
list(APPEND FREETYPE_ARGS -DFT_WITH_PNG=ON)

set(FREETYPE_LIBRARY_NAME "/lib/libfreetype.a")
ExternalProject_Add(freetype-bootstrap
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/freetype"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${FREETYPE_ARGS}
        -DFT_WITH_HARFBUZZ=OFF -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE
)
add_dependencies(freetype-bootstrap zlib libpng)

set(FREETYPE_INCLUDE_DIRS "${UNIVERSAL_HEADER_DIR}/freetype2" CACHE INTERNAL "")
set(FREETYPE_LIBRARY_PATH "${INSTALL_DIR}${FREETYPE_LIBRARY_NAME}")

# Harfbuzz
list(APPEND HARFBHZZ_ARGS ${CROSS_ARGS})
list(APPEND HARFBHZZ_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND HARFBHZZ_ARGS -DBUILD_SHARED_LIBS=OFF)
list(APPEND HARFBHZZ_ARGS -DHB_HAVE_CORETEXT=OFF)
list(APPEND HARFBHZZ_ARGS -DHB_BUILD_SUBSET=OFF)
list(APPEND HARFBHZZ_ARGS -DHB_HAVE_FREETYPE=ON)
list(APPEND HARFBHZZ_ARGS -DFREETYPE_LIBRARY=${FREETYPE_LIBRARY_PATH})

# Find linking warning
if(IOS)
    list(APPEND HARFBHZZ_ARGS -DCMAKE_C_FLAGS=-fvisibility=hidden\ -fvisibility-inlines-hidden)
    list(APPEND HARFBHZZ_ARGS -DCMAKE_CXX_FLAGS=-fvisibility=hidden\ -fvisibility-inlines-hidden)
endif()

# From FindFreetype cmake module: it looks for FREETYPE_INCLUDE_DIR_ft2build
# and FREETYPE_INCLUDE_DIR_freetype2 to set FREETYPE_INCLUDE_DIRS
list(APPEND HARFBHZZ_ARGS -DFREETYPE_INCLUDE_DIR_ft2build=${FREETYPE_INCLUDE_DIRS})
list(APPEND HARFBHZZ_ARGS -DFREETYPE_INCLUDE_DIR_freetype2=${FREETYPE_INCLUDE_DIRS})

set(HARFBUZZ_LIBRARY_NAME "/lib/libharfbuzz.a")
ExternalProject_Add(harfbuzz
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/harfbuzz"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${HARFBHZZ_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${HARFBUZZ_LIBRARY_NAME}"
)
add_dependencies(harfbuzz freetype-bootstrap)

# Use in STK only
set(HARFBUZZ_INCLUDEDIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")

set(HARFBUZZ_INCLUDE_DIRS "${UNIVERSAL_HEADER_DIR}/harfbuzz")
add_library(harfbuzz_library STATIC IMPORTED)
set(HARFBUZZ_LIBRARY_PATH "${INSTALL_DIR}${HARFBUZZ_LIBRARY_NAME}")
set_target_properties(harfbuzz_library PROPERTIES IMPORTED_LOCATION ${HARFBUZZ_LIBRARY_PATH})
set(HARFBUZZ_LIBRARY harfbuzz_library CACHE INTERNAL "")
add_dependencies(harfbuzz_library harfbuzz)

# Freetype
list(APPEND FREETYPE_ARGS -DFT_WITH_HARFBUZZ=ON)
list(APPEND FREETYPE_ARGS -DHARFBUZZ_LIBRARIES={HARFBUZZ_LIBRARY_PATH})
list(APPEND FREETYPE_ARGS -DHARFBUZZ_INCLUDE_DIRS=${HARFBUZZ_INCLUDE_DIRS})

ExternalProject_Add(freetype
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/freetype"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${FREETYPE_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${FREETYPE_LIBRARY_NAME}"
)
add_dependencies(freetype harfbuzz freetype-bootstrap)

add_library(freetype_library STATIC IMPORTED)
set_target_properties(freetype_library PROPERTIES IMPORTED_LOCATION ${FREETYPE_LIBRARY_PATH})
set(FREETYPE_LIBRARY freetype_library CACHE INTERNAL "")
# TODO: remove it in the future
set(FREETYPE_LIBRARIES freetype_library CACHE INTERNAL "")
add_dependencies(freetype_library freetype)

# libogg
list(APPEND OGG_ARGS ${CROSS_ARGS})
list(APPEND OGG_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND OGG_ARGS -DBUILD_SHARED_LIBS=OFF)
list(APPEND OGG_ARGS -DBUILD_TESTING=OFF)
list(APPEND OGG_ARGS -DINSTALL_DOCS=OFF)
list(APPEND OGG_ARGS -DINSTALL_PKG_CONFIG_MODULE=OFF)
list(APPEND OGG_ARGS -DINSTALL_CMAKE_PACKAGE_MODULE=OFF)

set(OGG_LIBRARY_NAME "/lib/libogg.a")
ExternalProject_Add(ogg
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/libogg"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${OGG_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${OGG_LIBRARY_NAME}"
)

set(OGGVORBIS_OGG_INCLUDE_DIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(ogg_library STATIC IMPORTED)
set(OGG_LIBRARY_PATH "${INSTALL_DIR}${OGG_LIBRARY_NAME}")
set_target_properties(ogg_library PROPERTIES IMPORTED_LOCATION ${OGG_LIBRARY_PATH})
set(OGGVORBIS_OGG_LIBRARY ogg_library CACHE INTERNAL "")
add_dependencies(ogg_library ogg)

# libvorbis
list(APPEND VORBIS_ARGS ${CROSS_ARGS})
list(APPEND VORBIS_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND VORBIS_ARGS -DBUILD_SHARED_LIBS=OFF)
list(APPEND VORBIS_ARGS -DOGG_LIBRARY=${OGG_LIBRARY_PATH})
list(APPEND VORBIS_ARGS -DOGG_INCLUDE_DIR=${UNIVERSAL_HEADER_DIR})
list(APPEND VORBIS_ARGS -DINSTALL_CMAKE_PACKAGE_MODULE=OFF)

set(VORBIS_LIBRARY_NAME "/lib/libvorbis.a")
set(VORBISFILE_LIBRARY_NAME "/lib/libvorbisfile.a")
set(VORBISENC_LIBRARY_NAME "/lib/libvorbisenc.a")
ExternalProject_Add(vorbis
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/libvorbis"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${VORBIS_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${VORBIS_LIBRARY_NAME}"
                     "<INSTALL_DIR>${VORBISFILE_LIBRARY_NAME}"
                     "<INSTALL_DIR>${VORBISENC_LIBRARY_NAME}"
)
add_dependencies(vorbis ogg)

set(OGGVORBIS_VORBIS_INCLUDE_DIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(vorbis_library STATIC IMPORTED)
set(VORBIS_LIBRARY_PATH "${INSTALL_DIR}${VORBIS_LIBRARY_NAME}")
set_target_properties(vorbis_library PROPERTIES IMPORTED_LOCATION ${VORBIS_LIBRARY_PATH})
set(OGGVORBIS_VORBIS_LIBRARY vorbis_library CACHE INTERNAL "")
add_dependencies(vorbis_library vorbis)

add_library(vorbisfile_library STATIC IMPORTED)
set(VORBISFILE_LIBRARY_PATH "${INSTALL_DIR}${VORBISFILE_LIBRARY_NAME}")
set_target_properties(vorbisfile_library PROPERTIES IMPORTED_LOCATION ${VORBISFILE_LIBRARY_PATH})
set(OGGVORBIS_VORBISFILE_LIBRARY vorbisfile_library CACHE INTERNAL "")
add_dependencies(vorbisfile_library vorbis)

add_library(vorbisenc_library STATIC IMPORTED)
set(VORBISENC_LIBRARY_PATH "${INSTALL_DIR}${VORBISENC_LIBRARY_NAME}")
set_target_properties(vorbisenc_library PROPERTIES IMPORTED_LOCATION ${VORBISENC_LIBRARY_PATH})
set(OGGVORBIS_VORBISENC_LIBRARY vorbisenc_library CACHE INTERNAL "")
add_dependencies(vorbisenc_library vorbis)

# SDL2
list(APPEND SDL2_ARGS ${CROSS_ARGS})
list(APPEND SDL2_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND SDL2_ARGS -DBUILD_SHARED_LIBS=OFF)
# Respect cmake cflags then CCFLAGS environment variable (stk addition in sdl2)
list(APPEND SDL2_ARGS -DSTK_BUILD_DEPS=ON)
if(IOS)
    # STK addition
    list(APPEND SDL2_ARGS -DIOS_CROSS=ON)
    # STK doesn't use sdl main
    list(APPEND SDL2_ARGS -DCMAKE_C_FLAGS=-DIOS_DYLIB=1)
    # hidapi in iOS requires permission
    list(APPEND SDL2_ARGS -DHIDAPI=OFF)
endif()

set(SDL2_LIBRARY_NAME "/lib/libSDL2.a")
ExternalProject_Add(sdl2
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/sdl2"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${SDL2_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${SDL2_LIBRARY_NAME}"
)

set(SDL2_INCLUDEDIR "${UNIVERSAL_HEADER_DIR}/SDL2" CACHE INTERNAL "")
add_library(sdl2_library STATIC IMPORTED)
set(SDL2_LIBRARY_PATH "${INSTALL_DIR}${SDL2_LIBRARY_NAME}")
set_target_properties(sdl2_library PROPERTIES IMPORTED_LOCATION ${SDL2_LIBRARY_PATH})
set(SDL2_LIBRARY sdl2_library CACHE INTERNAL "")
add_dependencies(sdl2_library sdl2)

# libjpeg
list(APPEND JPEG_ARGS ${CROSS_ARGS})
list(APPEND JPEG_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND JPEG_ARGS -DENABLE_SHARED=OFF)
list(APPEND JPEG_ARGS -DBUILD_TESTING=OFF)
# libjpeg install library to lib64 folder, override it
list(APPEND JPEG_ARGS -DCMAKE_INSTALL_DEFAULT_LIBDIR=lib)

set(JPEG_LIBRARY_NAME "/lib/libjpeg.a")
ExternalProject_Add(libjpeg
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/libjpeg"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${JPEG_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${JPEG_LIBRARY_NAME}"
)

set(JPEG_INCLUDE_DIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(jpeg_library STATIC IMPORTED)
set(JPEG_LIBRARY_PATH "${INSTALL_DIR}${JPEG_LIBRARY_NAME}")
set_target_properties(jpeg_library PROPERTIES IMPORTED_LOCATION ${JPEG_LIBRARY_PATH})
set(JPEG_LIBRARY jpeg_library CACHE INTERNAL "")
add_dependencies(jpeg_library libjpeg)

# Use system openal in apple
if(APPLE)
    return()
endif()

# OpenAL
list(APPEND OPENAL_ARGS ${CROSS_ARGS})
list(APPEND OPENAL_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>)
list(APPEND OPENAL_ARGS -DLIBTYPE=STATIC)
list(APPEND OPENAL_ARGS -DALSOFT_UTILS=OFF)
list(APPEND OPENAL_ARGS -DALSOFT_NO_CONFIG_UTIL=ON)
list(APPEND OPENAL_ARGS -DALSOFT_EXAMPLES=OFF)
list(APPEND OPENAL_ARGS -DALSOFT_TESTS=OFF)

if(MINGW)
    set(OPENAL_LIBRARY_NAME "/lib/libOpenAL32.a")
else()
    set(OPENAL_LIBRARY_NAME "/lib/libopenal.a")
endif()
ExternalProject_Add(openal
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/dependencies-universal/openal"
    INSTALL_DIR "${PROJECT_BINARY_DIR}/deps"
    CMAKE_ARGS ${OPENAL_ARGS}
    BUILD_BYPRODUCTS "<INSTALL_DIR>${OPENAL_LIBRARY_NAME}"
)

set(OPENAL_INCLUDE_DIR ${UNIVERSAL_HEADER_DIR} CACHE INTERNAL "")
add_library(openal_library STATIC IMPORTED)
set(OPENAL_LIBRARY_PATH "${INSTALL_DIR}${OPENAL_LIBRARY_NAME}")
set_target_properties(openal_library PROPERTIES IMPORTED_LOCATION ${OPENAL_LIBRARY_PATH})
set(OPENAL_LIBRARY openal_library CACHE INTERNAL "")
add_dependencies(openal_library openal)
