# Krypton Egg SDL3 port: historical src/*.c (unchanged except marked PORT: edits) + port/.
#
#   cmake -S port -B build/port -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/port
#
# Toolchain: MSYS2 mingw32 (i686 gcc) - the game stores pointers in int, so the build is
# 32-bit only. SDL3 for i686: -DKE_SDL3_DIR=C:/tools/sdl3-3.4.16-i686 (default).
cmake_minimum_required(VERSION 3.20)
project(krypton_egg_sdl3 C ASM)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "32-bit (i686) compiler required: the historical code keeps pointers in int")
endif()

set(KE_SDL3_DIR "C:/tools/sdl3-3.4.16-i686" CACHE PATH "SDL3 i686 install prefix")
list(APPEND CMAKE_PREFIX_PATH "${KE_SDL3_DIR}")
find_package(SDL3 CONFIG REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_filename_component(KE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# ---- historical game code -------------------------------------------------------------
# All units, compiled as Watcom 10.0 -3s -d2 would see them (docs/port/architecture.md,
# "Compile issue classes"):
#   -std=gnu89         K&R-style declarations, implicit int/function declarations
#   -O0                -d2 code keeps every global in memory between statements; ISR-shared
#                      variables are not volatile, busy-waits depend on this
#   -funsigned-char    Watcom default char is unsigned
#   -fpack-struct=1    Watcom 10.0 default /zp1 (docs/types.md: packed-1 layouts, PROVEN)
#   -mms-bitfields     (mingw default) bit-field storage units = Watcom's (AnimatedSprite 32 B)
#   -fno-common        one definition per global, as WLINK
#   -fno-toplevel-reorder, -fno-zero-initialized-in-bss + tools/gcc_pack_data.py launcher:
#                      data in source order, byte packed, `= 0` data stays in .data - the
#                      Watcom _DATA layout that record overlays rely on (architecture.md)
#   -fwrapv            two's-complement overflow as the 386 code
#   -fno-strict-aliasing, -fno-delete-null-pointer-checks: int<->pointer punning everywhere
file(GLOB KE_GAME_SOURCES CONFIGURE_DEPENDS "${KE_ROOT}/src/*.c")
list(LENGTH KE_GAME_SOURCES KE_GAME_COUNT)
if(NOT KE_GAME_COUNT EQUAL 49)
  message(WARNING "expected 49 historical units in src/, found ${KE_GAME_COUNT}")
endif()
add_library(ke_game OBJECT ${KE_GAME_SOURCES})
target_include_directories(ke_game PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/watcom")
target_compile_options(ke_game PRIVATE
  -std=gnu89 -O0 -g -funsigned-char -fpack-struct=1 -mms-bitfields -fno-common -fwrapv
  -fno-toplevel-reorder -fno-zero-initialized-in-bss
  -fno-strict-aliasing -fno-delete-null-pointer-checks
  -include "${CMAKE_CURRENT_SOURCE_DIR}/include/watcom/watcom_compat.h"
  # accepted Watcom-isms, each reviewed (architecture.md); keep real errors visible
  -Wno-int-conversion -Wno-incompatible-pointer-types -Wno-builtin-declaration-mismatch
  -Wno-implicit-function-declaration -Wno-implicit-int -Wno-return-mismatch
  -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-main -Wno-multichar
  -Wno-overflow -Wno-attributes)
set_target_properties(ke_game PROPERTIES C_COMPILER_LAUNCHER
  "${Python3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/tools/gcc_pack_data.py")
# The historical main() becomes the game thread entry.
set_source_files_properties("${KE_ROOT}/src/u_13a95.c" PROPERTIES COMPILE_DEFINITIONS "main=ke_game_main")

# ---- asm stubs (generated from asm/*.asm; see port/tools/gen_asm_stubs.py) ------------
file(GLOB KE_ASM_TRANSLATIONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/asm/*.c")
set(KE_STUB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs/asm_stubs.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs/asm_data.S")

# ---- virtual PC + host ----------------------------------------------------------------
set(KE_VHW_SOURCES
  vhw/vhw.c vhw/cpu.c vhw/portio.c vhw/pic.c vhw/pit.c vhw/vga.c vhw/kbd.c vhw/mouse.c
  vhw/joy.c vhw/dma.c vhw/sb.c vhw/intsvc.c vhw/lowmem.c vhw/fault.c)
set(KE_HOST_SOURCES
  host/main_sdl.c host/gamethread.c host/log.c host/input.c host/present.c host/clib.c)

add_executable(ke_sdl3 ${KE_HOST_SOURCES} ${KE_VHW_SOURCES} ${KE_STUB_SOURCES}
  ${KE_ASM_TRANSLATIONS} $<TARGET_OBJECTS:ke_game>)
target_include_directories(ke_sdl3 PRIVATE include)
target_compile_options(ke_sdl3 PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-std=gnu11 -O1 -g -Wall -Wno-unused-function -fno-strict-aliasing -fno-omit-frame-pointer>)
target_link_libraries(ke_sdl3 PRIVATE SDL3::SDL3 winmm)
# Console subsystem: the game prints its startup report with printf.
# Low memory: the image must not be relocated into 0x10000..0x10FFFF (ASLR may place a
# 32-bit image as low as 0x90000), and the process relaunches itself with the range
# reserved (relaunch_with_low_memory_reserved in host/main_sdl.c).
target_link_options(ke_sdl3 PRIVATE -mconsole -Wl,--stack,8388608 -Wl,--disable-dynamicbase
  -Wl,--image-base,0x00400000)
add_custom_command(TARGET ke_sdl3 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KE_SDL3_DIR}/bin/SDL3.dll" "$<TARGET_FILE_DIR:ke_sdl3>")

if(Python3_FOUND)
  add_custom_target(check_asm_stubs
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_asm_stubs.py" --check
    WORKING_DIRECTORY "${KE_ROOT}")
endif()

# ---- differential oracle: original KE.EXE machine code vs translations -------------------
# ke_oracle.exe links the whole port except the SDL main; tests live in oracle/test_*.c.
# Needs build/port/oracle/ke_image.bin: python port/tools/le_export.py (from assets/KE.EXE).
file(GLOB KE_ORACLE_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/oracle/test_*.c")
set(KE_HOST_LIB_SOURCES host/gamethread.c host/log.c host/input.c host/present.c host/clib.c)
add_executable(ke_oracle oracle/oracle.c oracle/oracle_main.c ${KE_ORACLE_TESTS}
  ${KE_HOST_LIB_SOURCES} ${KE_VHW_SOURCES} ${KE_STUB_SOURCES} ${KE_ASM_TRANSLATIONS}
  $<TARGET_OBJECTS:ke_game>)
set_target_properties(ke_oracle PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/oracle")
target_include_directories(ke_oracle PRIVATE include)
target_compile_options(ke_oracle PRIVATE
  $<$<COMPILE_LANGUAGE:C>:-std=gnu11 -O1 -g -Wall -Wno-unused-function -fno-strict-aliasing -fno-omit-frame-pointer>)
target_link_libraries(ke_oracle PRIVATE SDL3::SDL3 winmm)
target_link_options(ke_oracle PRIVATE -mconsole)
add_custom_command(TARGET ke_oracle POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${KE_SDL3_DIR}/bin/SDL3.dll" "$<TARGET_FILE_DIR:ke_oracle>")
