# Adapted from https://github.com/vbe0201/switch-cmake


set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION "DKA-NX-14")
set(CMAKE_SYSTEM_PROCESSOR "aarch64")

set(SWITCH TRUE)


if(NOT DEFINED ENV{DEVKITPRO} AND DEVKITPRO_ROOT)
    set(ENV{DEVKITPRO} "${DEVKITPRO_ROOT}")
endif()
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" DEVKITPRO)
if(NOT IS_DIRECTORY ${DEVKITPRO})
    message(FATAL_ERROR "Please install devkitA64 or set DEVKITPRO in your environment.")
endif()

set(DEVKITA64 "${DEVKITPRO}/devkitA64")
set(LIBNX "${DEVKITPRO}/libnx")
set(PORTLIBS "${DEVKITPRO}/portlibs/switch")

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

list(APPEND CMAKE_PROGRAM_PATH "${DEVKITPRO}/tools/bin")
list(APPEND CMAKE_PROGRAM_PATH "${DEVKITA64}/bin")

set(WITH_PORTLIBS ON CACHE BOOL "Use portlibs?")


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


set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

add_definitions(-DSWITCH -D__SWITCH__ -D__RTLD_6XX__)

set(ARCH "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -fvisibility=hidden")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wno-error -O3 -ffunction-sections -fdata-sections ${ARCH}" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -Wno-error=unused-variable -Wno-deprecated-declarations -Wno-format-zero-length -Wno-pointer-arith -Wno-invalid-offsetof -Wno-volatile -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-aggressive-loop-optimizations -fno-strict-aliasing" CACHE STRING "C++ flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp -g ${ARCH}" CACHE STRING "ASM flags")
set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Executable linker flags")
set(CMAKE_STATIC_LINKER_FLAGS "" CACHE STRING "Library linker flags")
set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "Module linker flags")
