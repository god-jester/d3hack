## Toolchain file for Nintendo Switch homebrew with devkitA64 & libnx.
# Adapted from https://github.com/vbe0201/switch-cmake

## Generic settings

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION "DKA-NX-14")
set(CMAKE_SYSTEM_PROCESSOR "aarch64")
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# If you're doing multiplatform builds, use this variable to check
# whether you're building for the Switch.
# A macro of the same name is defined as well, to be used within code.
set(SWITCH TRUE)

## devkitPro ecosystem settings

# Define a few important devkitPro system paths.
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" DEVKITPRO)
if(NOT IS_DIRECTORY ${DEVKITPRO})
    message(FATAL_ERROR "Please install devkitA64 or set DEVKITPRO in your environment.")
endif()

set(DEVKITA64 "${DEVKITPRO}/devkitA64")
set(LIBNX "${DEVKITPRO}/libnx")
set(PORTLIBS "${DEVKITPRO}/portlibs/switch")
set(DEVKITA64_SYSROOT "${DEVKITA64}/aarch64-none-elf")

# Add devkitA64 GCC tools to CMake.
if(WIN32)
    set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc.exe")
    set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++.exe")
    set(CMAKE_LINKER "${DEVKITA64}/bin/aarch64-none-elf-ld.exe")
    set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-gcc-ar.exe" CACHE STRING "")
    set(CMAKE_AS "${DEVKITA64}/bin/aarch64-none-elf-as.exe" CACHE STRING "")
    set(CMAKE_NM "${DEVKITA64}/bin/aarch64-none-elf-gcc-nm.exe" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-gcc-ranlib.exe" CACHE STRING "")
else()
    set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
    set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++")
    set(CMAKE_LINKER "${DEVKITA64}/bin/aarch64-none-elf-ld")
    set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-gcc-ar" CACHE STRING "")
    set(CMAKE_AS "${DEVKITA64}/bin/aarch64-none-elf-as" CACHE STRING "")
    set(CMAKE_NM "${DEVKITA64}/bin/aarch64-none-elf-gcc-nm" CACHE STRING "")
    set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-gcc-ranlib" CACHE STRING "")
endif()

# devkitPro and devkitA64 provide various tools for working with
# Switch file formats, which should be made accessible from CMake.
list(APPEND CMAKE_PROGRAM_PATH "${DEVKITPRO}/tools/bin")
list(APPEND CMAKE_PROGRAM_PATH "${DEVKITA64}/bin")

# devkitPro maintains a repository of so-called portlibs,
# which can be found at https://github.com/devkitPro/pacman-packages/tree/master/switch.
# They store PKGBUILDs and patches for various libraries
# so they can be cross-compiled and used within homebrew.
# These can be installed with (dkp-)pacman.
set(WITH_PORTLIBS ON CACHE BOOL "Use portlibs?")

## Cross-compilation settings

if(WITH_PORTLIBS)
    set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITA64} ${PORTLIBS})
else()
    set(CMAKE_FIND_ROOT_PATH ${DEVKITPRO} ${DEVKITA64})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_INSTALL_PREFIX ${PORTLIBS} CACHE PATH "Install libraries to the portlibs directory")
set(CMAKE_PREFIX_PATH ${PORTLIBS} CACHE PATH "Find libraries in the portlibs directory")

## Standard library (clangd/compile_commands support)

if (IS_DIRECTORY "${DEVKITA64_SYSROOT}")
    set(CMAKE_SYSROOT "${DEVKITA64_SYSROOT}")
endif()

set(DEVKITA64_GCC_ROOT "${DEVKITA64}/lib/gcc/aarch64-none-elf")
set(DEVKITA64_GCC_VERSION "")
if (IS_DIRECTORY "${DEVKITA64_GCC_ROOT}")
    file(GLOB _devkita64_gcc_versions RELATIVE "${DEVKITA64_GCC_ROOT}" "${DEVKITA64_GCC_ROOT}/*")
    list(SORT _devkita64_gcc_versions)
    list(LENGTH _devkita64_gcc_versions _devkita64_gcc_versions_len)
    if (_devkita64_gcc_versions_len GREATER 0)
        math(EXPR _devkita64_gcc_last_idx "${_devkita64_gcc_versions_len} - 1")
        list(GET _devkita64_gcc_versions ${_devkita64_gcc_last_idx} DEVKITA64_GCC_VERSION)
    endif()
endif()

set(DEVKITA64_STDLIB_INCLUDES "")
if (DEVKITA64_GCC_VERSION)
    list(APPEND DEVKITA64_STDLIB_INCLUDES
        "${DEVKITA64_SYSROOT}/include/c++/${DEVKITA64_GCC_VERSION}"
        "${DEVKITA64_SYSROOT}/include/c++/${DEVKITA64_GCC_VERSION}/aarch64-none-elf"
        "${DEVKITA64_SYSROOT}/include/c++/${DEVKITA64_GCC_VERSION}/backward"
        "${DEVKITA64}/lib/gcc/aarch64-none-elf/${DEVKITA64_GCC_VERSION}/include"
        "${DEVKITA64}/lib/gcc/aarch64-none-elf/${DEVKITA64_GCC_VERSION}/include-fixed"
        "${DEVKITA64_SYSROOT}/include"
    )
endif()

set(_devkita64_stdinc_flags "")
foreach (_dir IN LISTS DEVKITA64_STDLIB_INCLUDES)
    if (IS_DIRECTORY "${_dir}")
        list(APPEND _devkita64_stdinc_flags "-isystem" "${_dir}")
    endif()
endforeach()
list(JOIN _devkita64_stdinc_flags " " DEVKITA64_STDLIB_INCLUDE_FLAGS)

## Options for code generation

# Technically, the Switch does support shared libraries, but the toolchain doesn't.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

add_definitions(-DSWITCH -D__SWITCH__ -D__RTLD_6XX__)

set(ARCH "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -fvisibility=hidden")

set(_d3hack_c_flags "-g -Wall -Werror -Ofast -ffunction-sections -fdata-sections -Wno-format-zero-length ${ARCH} --sysroot=${CMAKE_SYSROOT} ${DEVKITA64_STDLIB_INCLUDE_FLAGS}")
set(_d3hack_cxx_flags "${_d3hack_c_flags} -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-threadsafe-statics")

set(CMAKE_C_FLAGS "${_d3hack_c_flags}" CACHE STRING "C flags" FORCE)
set(CMAKE_CXX_FLAGS "${_d3hack_cxx_flags}" CACHE STRING "C++ flags" FORCE)
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp -g ${ARCH}" CACHE STRING "ASM flags")
# These flags are purposefully empty to use the default flags when invoking the
# devkitA64 linker. Otherwise the linker may complain about duplicate flags.
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Executable linker flags")
set(CMAKE_STATIC_LINKER_FLAGS "" CACHE STRING "Library linker flags")
set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "Module linker flags")
