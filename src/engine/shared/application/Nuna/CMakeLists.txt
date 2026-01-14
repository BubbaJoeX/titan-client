# ======================================================================
# Nuna - TRE Archive Packer/Unpacker
# CMake Build Configuration
# Copyright (c) Titan Project
# ======================================================================

cmake_minimum_required(VERSION 3.16)
project(Nuna VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ======================================================================
# Bundled zlib (from project's external libraries)
# ======================================================================

set(ZLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/3rd/library/zlib-1.2.3")

set(ZLIB_SOURCES
    ${ZLIB_DIR}/adler32.c
    ${ZLIB_DIR}/compress.c
    ${ZLIB_DIR}/crc32.c
    ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/gzio.c
    ${ZLIB_DIR}/infback.c
    ${ZLIB_DIR}/inffast.c
    ${ZLIB_DIR}/inflate.c
    ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/trees.c
    ${ZLIB_DIR}/uncompr.c
    ${ZLIB_DIR}/zutil.c
)

add_library(zlib_bundled STATIC ${ZLIB_SOURCES})
target_include_directories(zlib_bundled PUBLIC ${ZLIB_DIR})

if(MSVC)
    target_compile_definitions(zlib_bundled PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(zlib_bundled PRIVATE /W0)
endif()

# ======================================================================
# Nuna executable
# ======================================================================

set(NUNA_SOURCES
    src/Nuna.cpp
    src/main.cpp
)

set(NUNA_HEADERS
    src/Nuna.h
    src/NunaCrc.h
    src/NunaCompression.h
    src/NunaCrypto.h
)

# Create executable
add_executable(nuna ${NUNA_SOURCES} ${NUNA_HEADERS})

# Include directories
target_include_directories(nuna PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ZLIB_DIR}
)

# Link libraries
target_link_libraries(nuna PRIVATE zlib_bundled)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(nuna PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(nuna PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(nuna PRIVATE NOMINMAX)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(nuna PRIVATE /W4)
else()
    target_compile_options(nuna PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
install(TARGETS nuna DESTINATION bin)

# ======================================================================
# Build as library (optional)
# ======================================================================

add_library(nuna_lib STATIC
    src/Nuna.cpp
    ${NUNA_HEADERS}
)

target_include_directories(nuna_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ZLIB_DIR}
)
target_link_libraries(nuna_lib PUBLIC zlib_bundled)

if(WIN32)
    target_compile_definitions(nuna_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
