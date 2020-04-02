cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 OLD)

project(vcrtl CXX ASM_MASM)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/build)
find_package(WDK REQUIRED)

list(REMOVE_ITEM WDK_COMPILE_FLAGS /kernel)

wdk_add_library(vcrtl_driver STATIC
    src/algorithm.h
    src/assert.cpp
    src/assert.h
    src/bugcheck.h
    src/flags.h
    src/intrin.h
    src/limits.h
    src/memcpy.h
    src/ptr_to_member.h
    src/runtime.cpp
    src/rva.h
    src/stddef.h
    src/stdint.h
    src/type_info.cpp
    src/type_info.h
    src/type_traits.h
    src/utils.h
    src/win32_seh.h
    )

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
target_sources(vcrtl_driver PUBLIC
    src/x64/capture.asm
    src/x64/cpu_context.h
    src/x64/eh_structs_x64.h
    src/x64/fh3.cpp
    src/x64/fh4.cpp
    src/x64/throw.cpp
    src/x64/throw.h
    src/x64/unwind_handler.cpp
    )
else()
target_sources(vcrtl_driver PUBLIC
    src/x86/eh_helpers.asm
    src/x86/eh_structs_x86.h
    src/x86/fh3_x86.asm
    src/x86/memcpy.asm
    src/x86/nlg.asm
    src/x86/throw_x86.cpp
    )
list(APPEND CMAKE_ASM_MASM_FLAGS "/safeseh")
endif()

target_compile_features(vcrtl_driver PUBLIC cxx_std_17)

wdk_add_driver(vcrtl_driver_test test/test.cpp)
target_link_libraries(vcrtl_driver_test vcrtl_driver)
