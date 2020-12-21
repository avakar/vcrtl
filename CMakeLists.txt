# This project implements various aspects of the VS runtime library
# and splits them amongst multiple targets. You can pick only those parts
# of the run-time you actually need.
#
# The public targets you can link to are as follows.
#
# * `vcrtl::bugcheck` -- contains the header `<vcrtl/bugcheck.h>` that you
#   need to include to implement `vcrtl::on_bug_check()`.
# * `vcrtl::rtc` -- support for "Basic Runtime Checks" and "Smaller Type Check".
# * `vcrtl::gs` -- support for "Security Check", aka `/GS`
# * `vcrtl::ehs` -- exception handling support, needed for `/EHs`
# * `vcrtl::rtti` -- support for `typeid`; needed when `/GR` is in effect.
#   Note that the idea of RTTI being a pre-requisite for exception handling
#   is a common, and very much incorrect, myth.
#
# As a user of the library, you must implement the `vcrtl::on_bug_check()`
# function; we provide example implementation for WDK projects in
#
# * `vcrtl::wdk_bugcheck`.
#
# You can also link against
#
# * `vcrtl::wdk`,
#
# which collects all the above libraries into a single target to link against
# from a wdk project.

cmake_minimum_required(VERSION 3.19)
project(vcrtl CXX ASM_MASM)



add_library(vcrtl_bugcheck INTERFACE
    include/vcrtl/bugcheck.h
    )
target_compile_features(vcrtl_bugcheck INTERFACE cxx_std_17)
target_include_directories(vcrtl_bugcheck INTERFACE include)
add_library(vcrtl::bugcheck ALIAS vcrtl_bugcheck)



add_library(vcrtl_rtc STATIC
    src/rtc/rtc.cpp
    )
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL Win32)
    target_sources(vcrtl_rtc PRIVATE
        src/rtc/check_esp.x86.asm
        )
endif()
target_compile_features(vcrtl_rtc PUBLIC cxx_std_17)
target_compile_options(vcrtl_rtc PRIVATE "/GS-")
add_library(vcrtl::rtc ALIAS vcrtl_rtc)



add_library(vcrtl_ehs STATIC
    src/algorithm.h
    src/assert.cpp
    src/assert.h
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
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL x64)
    target_sources(vcrtl_ehs PRIVATE
        src/x64/capture.asm
        src/x64/cpu_context.h
        src/x64/eh_structs_x64.h
        src/x64/fh3.cpp
        src/x64/fh4.cpp
        src/x64/throw.cpp
        src/x64/throw.h
        src/x64/unwind_handler.cpp
        )
elseif ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL Win32)
    target_sources(vcrtl_ehs PRIVATE
        src/x86/eh_helpers.asm
        src/x86/eh_structs_x86.h
        src/x86/fh3_x86.asm
        src/x86/memcpy.asm
        src/x86/nlg.asm
        src/x86/throw_x86.cpp
        )
    list(APPEND CMAKE_ASM_MASM_FLAGS "/safeseh")
endif()
target_compile_features(vcrtl_ehs PUBLIC cxx_std_17)
target_compile_options(vcrtl_ehs PRIVATE "/GS-")
target_link_libraries(vcrtl_ehs INTERFACE vcrtl::rtc)
add_library(vcrtl::ehs ALIAS vcrtl_ehs)



add_library(vcrtl_wdk_bugcheck STATIC
    src/ntos_bugcheck.cpp
    )
add_library(vcrtl::wdk_bugcheck ALIAS vcrtl_wdk_bugcheck)


add_library(vcrtl_wdk INTERFACE)
target_link_libraries(vcrtl_wdk INTERFACE vcrtl::ehs vcrtl::wdk_bugcheck)
add_library(vcrtl::wdk ALIAS vcrtl_wdk)



if(BUILD_TESTING)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build)
    find_package(WDK)
    if (WDK_FOUND)
        list(REMOVE_ITEM WDK_COMPILE_FLAGS /kernel)
        wdk_add_driver(vcrtl_driver_test test/test.cpp)
        target_link_libraries(vcrtl_driver_test vcrtl::wdk)
    endif()
endif()
