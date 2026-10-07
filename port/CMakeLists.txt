# Portable build of the TH16 decompilation: the game sources in ../src with
# GCC or Clang, against the stand-in Windows/DirectX headers in include/ and
# the platform layer in src/. See NOTES.md.
#
#   cmake -S port -B build-port/clang64 -DCMAKE_CXX_COMPILER=clang++
#   cmake -S port -B build-port/gcc32 -DCMAKE_CXX_COMPILER=g++ -DTH16_M32=ON
#   cmake --build build-port/clang64
#
# The matching MSVC build (scripts/build.py) never reads this file.
cmake_minimum_required(VERSION 3.16)

option(TH16_M32 "Build a 32-bit x86 binary (keeps the original's struct layout)" OFF)
if(TH16_M32)
    # In the flags CMake starts from, so that its compiler checks see a
    # 32-bit target (CMAKE_SIZEOF_VOID_P 4) and find_package picks the
    # 32-bit libraries (lib32/cmake/SDL2).
    set(CMAKE_CXX_FLAGS_INIT "-m32")
endif()

project(th16_port CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(TH16_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Every game source except the matching build's scaffolding, which only
# exists to shape MSVC's output: src/stub/ (Opaque.cpp, values kept out of
# LTCG's sight and sinks for the harness) and src/harness/ (stand-in
# callers). The non-recursive glob leaves both directories out; the one game
# global in src/stub/, g_zero_vec2, is defined in src/game_tables.cpp.
file(GLOB TH16_GAME_SOURCES CONFIGURE_DEPENDS ${TH16_ROOT}/src/*.cpp)
file(GLOB TH16_PORT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# TH16_NULL_RENDERER builds d3d9_null.cpp (a Direct3D 9 that draws nothing,
# for headless tests) in place of the renderer: every other d3d9_*.cpp and
# the D3DX texture functions (d3dx9_tex*.cpp).
option(TH16_NULL_RENDERER "Build the null Direct3D 9 instead of the renderer" OFF)
if(TH16_NULL_RENDERER)
    list(FILTER TH16_PORT_SOURCES EXCLUDE REGEX "/src/(d3d9_|d3dx9_tex)[^/]*\\.cpp$")
    list(APPEND TH16_PORT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/d3d9_null.cpp)
else()
    list(FILTER TH16_PORT_SOURCES EXCLUDE REGEX "/src/d3d9_null\\.cpp$")
endif()

add_executable(th16 ${TH16_GAME_SOURCES} ${TH16_PORT_SOURCES})

target_include_directories(th16 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${TH16_ROOT}/src)
# 64-bit file offsets on 32-bit hosts too (thbgm.dat is 388 MB; seeking
# past 2 GB needs them).
target_compile_definitions(th16 PRIVATE TH16_PORT _FILE_OFFSET_BITS=64)

target_compile_options(th16 PRIVATE
    # MSVC-isms the game source relies on without including anything.
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/port_prelude.h
    # Semantics the decompiled code assumes from MSVC: type punning through
    # casts, wrapping signed arithmetic, null checks kept as written.
    -fno-strict-aliasing
    -fwrapv
    -fno-delete-null-pointer-checks
    # char is signed in MSVC; ARM Linux defaults to unsigned.
    -fsigned-char
    # Float results must not depend on the compiler fusing operations.
    -ffp-contract=off
    -fno-fast-math
)

if(TH16_M32)
    # x86-32 with SSE arithmetic, as in the original (/arch:SSE2 is MSVC's
    # default); x87 math would round differently. MSVC aligns 8-byte
    # members to 8 where the i386 System V ABI uses 4, but every struct with
    # a double or __int64 either sits in a #pragma pack(4) region or has
    # them at 8-byte offsets already, so the layouts agree without
    # -malign-double (which breaks libstdc++'s long double tables in Clang).
    # The game's static_asserts check this in the -m32 build.
    target_compile_options(th16 PRIVATE -m32 -msse2 -mfpmath=sse)
    target_link_options(th16 PRIVATE -m32)
endif()

# Warnings: keep the useful ones, silence what the decompiled MSVC-style code
# triggers by design.
target_compile_options(th16 PRIVATE
    -Wall
    -Wno-unknown-pragmas
    -Wno-invalid-offsetof
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-unused-value
    -Wno-multichar
    -Wno-write-strings
    -Wno-parentheses
    -Wno-sign-compare
    -Wno-missing-braces
    -Wno-char-subscripts
    -Wno-comment
    -Wno-switch
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(th16 PRIVATE
        # The game indexes past the end of arrays in places (for example
        # Scorefile::unlock_all writes clears[5] of an i32[5]), which GCC
        # would otherwise use to cut loops short.
        -fno-aggressive-loop-optimizations
        -Wno-class-memaccess
        -Wno-maybe-uninitialized
        -Wno-unused-local-typedefs
        -Wno-narrowing
    )
else()
    target_compile_options(th16 PRIVATE
        -Wno-writable-strings
        -Wno-c++11-narrowing
        -Wno-dynamic-class-memaccess
        -Wno-nontrivial-memaccess
        -Wno-unused-private-field
        -Wno-tautological-constant-out-of-range-compare
    )
endif()

find_package(Threads REQUIRED)
# SDL2 (or sdl2-compat), whose CMake package defines SDL2::SDL2: the window,
# events and input (win32_user.cpp, input_sdl.cpp), audio output
# (dsound_sdl.cpp) and the OpenGL context (d3d9_gl.cpp, which loads its GL
# functions through SDL, so nothing links libGL).
find_package(SDL2 REQUIRED CONFIG)
target_link_libraries(th16 PRIVATE SDL2::SDL2 Threads::Threads m)

# FreeType and fontconfig (the GDI text renderer, gdi_text.cpp) through
# pkg-config, and iconv (Shift-JIS; part of glibc, a library on macOS). For
# -m32, pkg-config has to report the 32-bit libraries; where they have their
# own pkg-config directory (Arch's lib32-*, Debian's i386-linux-gnu) it is
# used unless PKG_CONFIG_LIBDIR is set.
if(TH16_M32 AND NOT DEFINED ENV{PKG_CONFIG_LIBDIR})
    foreach(dir /usr/lib32/pkgconfig /usr/lib/i386-linux-gnu/pkgconfig)
        if(IS_DIRECTORY ${dir})
            set(ENV{PKG_CONFIG_LIBDIR} "${dir}:/usr/share/pkgconfig")
            break()
        endif()
    endforeach()
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(TH16_TEXT REQUIRED IMPORTED_TARGET freetype2 fontconfig)
target_link_libraries(th16 PRIVATE PkgConfig::TH16_TEXT)
if(APPLE)
    find_library(ICONV_LIBRARY iconv REQUIRED)
    target_link_libraries(th16 PRIVATE ${ICONV_LIBRARY})
endif()

# Tests of the DirectSound mixer (see NOTES.md, "Audio"):
# - th16_dsound_test (tests/dsound_test.cpp) drives dsound_sdl.cpp directly;
# - th16_dsound_game_test (tests/dsound_game_test.cpp) runs the game's own
#   BGM streaming code (src/DSUtil.cpp) on it.
add_executable(th16_dsound_test tests/dsound_test.cpp src/dsound_sdl.cpp)
add_executable(th16_dsound_game_test tests/dsound_game_test.cpp src/dsound_sdl.cpp
    ${TH16_ROOT}/src/DSUtil.cpp ${TH16_ROOT}/src/CriticalSections.cpp)
foreach(test_target th16_dsound_test th16_dsound_game_test)
    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${TH16_ROOT}/src)
    target_compile_definitions(${test_target} PRIVATE TH16_PORT)
    target_compile_options(${test_target} PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/include/port_prelude.h
        -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -fsigned-char -ffp-contract=off
        -Wall -Wno-unknown-pragmas -Wno-invalid-offsetof -Wno-parentheses -Wno-sign-compare
        -Wno-mismatched-new-delete)
    if(TH16_M32)
        target_compile_options(${test_target} PRIVATE -m32 -msse2 -mfpmath=sse)
        target_link_options(${test_target} PRIVATE -m32)
    endif()
    target_link_libraries(${test_target} PRIVATE SDL2::SDL2 Threads::Threads m)
endforeach()

# Tests of the platform layer (tests/platform_test.cpp): files, kernel
# objects, input, GDI text; no game code or data.
add_executable(th16_platform_test tests/platform_test.cpp
    src/crt_compat.cpp src/win32_kernel.cpp src/win32_thread.cpp src/win32_user.cpp src/win32_misc.cpp
    src/port_vfs.cpp src/port_text.cpp src/input_sdl.cpp src/dinput_sdl.cpp src/gdi_text.cpp)
target_include_directories(th16_platform_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(th16_platform_test PRIVATE TH16_PORT _FILE_OFFSET_BITS=64)
target_compile_options(th16_platform_test PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/port_prelude.h -Wall -Wno-unknown-pragmas)
if(TH16_M32)
    target_compile_options(th16_platform_test PRIVATE -m32 -msse2 -mfpmath=sse)
    target_link_options(th16_platform_test PRIVATE -m32)
endif()
target_link_libraries(th16_platform_test PRIVATE SDL2::SDL2 PkgConfig::TH16_TEXT Threads::Threads m)
if(APPLE)
    target_link_libraries(th16_platform_test PRIVATE ${ICONV_LIBRARY})
endif()

# `ctest` runs the mixer tests that need no game data, on SDL's dummy driver
# (tests/run_dsound_tests.sh runs the rest).
enable_testing()
add_test(NAME dsound COMMAND th16_dsound_test)
set_tests_properties(dsound PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_AUDIO_DRIVER=dummy")
add_test(NAME platform COMMAND th16_platform_test)
set_tests_properties(platform PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_JOYSTICK_HIDAPI=0")
