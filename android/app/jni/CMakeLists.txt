# Krypton Egg for Android: SDL3 + libmain.so (loader) + libkegame.so (game, virtual PC, host).
#
# Built by Gradle (android/app/build.gradle) for arm64-v8a and x86_64. See
# docs/android/architecture.md and docs/android/building.md.
#
#   libSDL3.so    SDL 3.4.16 from source (-DKE_SDL3_SOURCE_DIR)
#   libmain.so    port/android/loader.c: reserves a window below 2 GB, loads libkegame.so into it
#   libkegame.so  the ILP32 game world (src/*.c, port/asm/*.c, generated _DATA, its C library)
#                 + the LP64 virtual PC (port/vhw), host (port/host), platform (posix) and the
#                 Android host (port/android)
cmake_minimum_required(VERSION 3.22)
project(kryptonegg_android C ASM)

get_filename_component(KE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(KE_SDL3_SOURCE_DIR "C:/tools/download/sdl3/SDL3-3.4.16" CACHE PATH "SDL 3.4.16 source tree")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- SDL3 -------------------------------------------------------------------------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${KE_SDL3_SOURCE_DIR}" SDL3)

# ---- the ILP32 game world ------------------------------------------------------------------
# Every object is produced by port/android/tools/ilp32_world.py (32-bit-pointer code
# generation for this CPU, reassembled for the 64-bit target, Windows-identical data layout).
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "x86_64")
else()
  message(FATAL_ERROR "Krypton Egg supports arm64-v8a and x86_64 (got ${ANDROID_ABI})")
endif()
set(KE_ILP32_TOOL "${KE_ROOT}/port/android/tools/ilp32_world.py")
set(KE_ILP32_DEPS "${KE_ILP32_TOOL}" "${KE_ROOT}/port/tools/gcc_pack_data.py"
    "${KE_ROOT}/manifest.json" "${KE_ROOT}/assets/KE.EXE")
file(GLOB KE_ILP32_HEADERS CONFIGURE_DEPENDS "${KE_ROOT}/port/android/ilp32/include/*.h"
     "${KE_ROOT}/port/include/watcom/*.h" "${KE_ROOT}/port/include/*.h" "${KE_ROOT}/port/vhw/vhw.h"
     "${KE_ROOT}/port/asm/*.h")
file(GLOB KE_GAME_SOURCES CONFIGURE_DEPENDS "${KE_ROOT}/src/*.c")
file(GLOB KE_ASM_SOURCES CONFIGURE_DEPENDS "${KE_ROOT}/port/asm/*.c")
set(KE_ILP32_OBJECTS)
set(KE_ILP32_DIR "${CMAKE_CURRENT_BINARY_DIR}/ilp32")

function(ke_ilp32_object kind src name)
  set(obj "${KE_ILP32_DIR}/${name}.o")
  add_custom_command(OUTPUT "${obj}"
    COMMAND "${Python3_EXECUTABLE}" "${KE_ILP32_TOOL}" --abi ${ANDROID_ABI}
            --clang "${CMAKE_C_COMPILER}" --api ${ANDROID_PLATFORM_LEVEL}
            --kind ${kind} --src "${src}" --out "${obj}"
    DEPENDS "${src}" ${KE_ILP32_DEPS} ${KE_ILP32_HEADERS}
    COMMENT "ILP32 world: ${name}"
    VERBATIM)
  set(KE_ILP32_OBJECTS ${KE_ILP32_OBJECTS} "${obj}" PARENT_SCOPE)
endfunction()

foreach(src ${KE_GAME_SOURCES})
  get_filename_component(name "${src}" NAME_WE)
  ke_ilp32_object(game "${src}" "${name}")
endforeach()
foreach(src ${KE_ASM_SOURCES})
  get_filename_component(name "${src}" NAME_WE)
  ke_ilp32_object(asm "${src}" "asm_${name}")
endforeach()
ke_ilp32_object(data "${KE_ROOT}/port/stubs/asm_data.S" "asm_data")
ke_ilp32_object(clib "${KE_ROOT}/port/android/ilp32/ke32_clib.c" "ke32_clib")

# The world may call the host only through 32-bit-scalar entry points (build fails otherwise).
set(KE_BOUNDARY_STAMP "${KE_ILP32_DIR}/boundary.stamp")
get_filename_component(KE_NDK_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(KE_LLVM_NM llvm-nm HINTS "${KE_NDK_BIN}" NO_DEFAULT_PATH)
add_custom_command(OUTPUT "${KE_BOUNDARY_STAMP}"
  COMMAND "${Python3_EXECUTABLE}" "${KE_ROOT}/port/android/tools/check_ilp32_boundary.py"
          --nm "${KE_LLVM_NM}" --stamp "${KE_BOUNDARY_STAMP}" ${KE_ILP32_OBJECTS}
  DEPENDS ${KE_ILP32_OBJECTS} "${KE_ROOT}/port/android/tools/check_ilp32_boundary.py"
  COMMENT "ILP32 world: boundary check"
  VERBATIM)
add_custom_target(ke_ilp32_boundary DEPENDS "${KE_BOUNDARY_STAMP}")
set_source_files_properties(${KE_ILP32_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

# ---- libkegame.so ---------------------------------------------------------------------------
set(KE_LP64_SOURCES
  ${KE_ROOT}/port/vhw/vhw.c ${KE_ROOT}/port/vhw/cpu.c ${KE_ROOT}/port/vhw/portio.c
  ${KE_ROOT}/port/vhw/pic.c ${KE_ROOT}/port/vhw/pit.c ${KE_ROOT}/port/vhw/vga.c
  ${KE_ROOT}/port/vhw/kbd.c ${KE_ROOT}/port/vhw/mouse.c ${KE_ROOT}/port/vhw/joy.c
  ${KE_ROOT}/port/vhw/dma.c ${KE_ROOT}/port/vhw/sb.c ${KE_ROOT}/port/vhw/intsvc.c
  ${KE_ROOT}/port/vhw/lowmem.c ${KE_ROOT}/port/vhw/fault.c
  ${KE_ROOT}/port/platform/platform_posix.c
  ${KE_ROOT}/port/host/gamethread.c ${KE_ROOT}/port/host/log.c ${KE_ROOT}/port/host/config.c
  ${KE_ROOT}/port/host/input.c ${KE_ROOT}/port/host/mouse_native.c
  ${KE_ROOT}/port/host/viewport.c ${KE_ROOT}/port/host/present.c ${KE_ROOT}/port/host/clib.c
  ${KE_ROOT}/port/host/files.c
  ${KE_ROOT}/port/android/main_android.c ${KE_ROOT}/port/android/touch.c
  ${KE_ROOT}/port/android/ilp32/ke32_shims.c)
add_library(kegame SHARED ${KE_LP64_SOURCES} ${KE_ILP32_OBJECTS})
add_dependencies(kegame ke_ilp32_boundary)
target_include_directories(kegame PRIVATE "${KE_ROOT}/port/include" "${KE_ROOT}/port/android")
target_compile_definitions(kegame PRIVATE KE_GAME_ILP32=1)
target_compile_options(kegame PRIVATE -std=gnu11 -O2 -g -Wall -Wno-unused-function
  -fno-strict-aliasing -fno-omit-frame-pointer -fvisibility=hidden)
target_link_libraries(kegame PRIVATE SDL3::SDL3-shared log android m dl)
target_link_options(kegame PRIVATE -Wl,--no-undefined -Wl,-z,max-page-size=16384
  -Wl,-Bsymbolic)

# ---- libmain.so (loader) --------------------------------------------------------------------
add_library(main SHARED ${KE_ROOT}/port/android/loader.c)
target_compile_definitions(main PRIVATE KE_ANDROID_LOADER=1)
target_include_directories(main PRIVATE "${KE_ROOT}/port/android")
target_compile_options(main PRIVATE -std=gnu11 -O2 -Wall -fvisibility=hidden)
target_link_libraries(main PRIVATE log dl)
target_link_options(main PRIVATE -Wl,-z,max-page-size=16384)
# libkegame.so is loaded by the loader, not by Java: build it together with libmain.so.
add_dependencies(main kegame SDL3-shared)
