# ======================================================================
# CMakeLists.txt for SwgMapRasterizer
# Planet Map Image Generator Tool
#
# Generates top-down terrain map images from .trn files for use as
# planet map textures in the game UI (ui_planetmap.inc).
# ======================================================================

cmake_minimum_required(VERSION 3.10)
project(SwgMapRasterizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ======================================================================
# Engine Library Root Paths
# ======================================================================

# Paths relative to this CMakeLists.txt location:
# SwgMapRasterizer is at: client/src/engine/shared/application/SwgMapRasterizer/

set(ENGINE_SHARED_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../library")
set(ENGINE_CLIENT_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../client/library")
set(EXTERNAL_OURS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/ours/library")
set(EXTERNAL_3RD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external/3rd/library")

# ======================================================================
# Source Files
# ======================================================================

set(SOURCES
    src/shared/SwgMapRasterizer.cpp
    src/shared/SwgMapRasterizer.h
    src/shared/FirstSwgMapRasterizer.h
    src/win32/WinMain.cpp
    src/win32/FirstSwgMapRasterizer.cpp
)

# ======================================================================
# Build Target
# ======================================================================

add_executable(SwgMapRasterizer WIN32 ${SOURCES})

# ======================================================================
# Compiler Settings
# ======================================================================

if(WIN32)
    target_compile_definitions(SwgMapRasterizer PRIVATE
        _WIN32
        WIN32
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(SwgMapRasterizer PRIVATE _WIN64)
    else()
        target_compile_definitions(SwgMapRasterizer PRIVATE _USE_32BIT_TIME_T=1)
    endif()
    if(MSVC)
        target_compile_options(SwgMapRasterizer PRIVATE /W3 /MP)
        set_source_files_properties(src/win32/FirstSwgMapRasterizer.cpp PROPERTIES
            COMPILE_FLAGS "/YcFirstSwgMapRasterizer.h"
        )
        set_source_files_properties(src/shared/SwgMapRasterizer.cpp src/win32/WinMain.cpp PROPERTIES
            COMPILE_FLAGS "/YuFirstSwgMapRasterizer.h"
        )
    endif()
endif()

# ======================================================================
# Include Directories
#
# Each engine library exposes public headers via include/public/<libname>/
# Some also need src/shared/ or src/win32/ for internal headers.
# ======================================================================

# -- Shared Engine Libraries --
set(SHARED_LIBS
    sharedCompression
    sharedDebug
    sharedFile
    sharedFoundation
    sharedFoundationTypes
    sharedImage
    sharedIoWin
    sharedLog
    sharedMath
    sharedMathArchive
    sharedMemoryBlockManager
    sharedMemoryManager
    sharedMessageDispatch
    sharedObject
    sharedRandom
    sharedRegex
    sharedSynchronization
    sharedTerrain
    sharedThread
    sharedUtility
    sharedXml
)

foreach(LIB ${SHARED_LIBS})
    if(EXISTS "${ENGINE_SHARED_LIB_ROOT}/${LIB}/include/public")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_SHARED_LIB_ROOT}/${LIB}/include/public"
        )
    endif()
    # Some libraries also need their src directories for internal includes
    if(EXISTS "${ENGINE_SHARED_LIB_ROOT}/${LIB}/src/shared")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_SHARED_LIB_ROOT}/${LIB}/src/shared"
        )
    endif()
    if(WIN32 AND EXISTS "${ENGINE_SHARED_LIB_ROOT}/${LIB}/src/win32")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_SHARED_LIB_ROOT}/${LIB}/src/win32"
        )
    endif()
endforeach()

# -- Client Engine Libraries (for GPU rendering mode) --
set(CLIENT_LIBS
    clientGraphics
    clientObject
    clientTerrain
)

foreach(LIB ${CLIENT_LIBS})
    if(EXISTS "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/include/public")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/include/public"
        )
    endif()
    if(EXISTS "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/src/shared")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/src/shared"
        )
    endif()
    if(WIN32 AND EXISTS "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/src/win32")
        target_include_directories(SwgMapRasterizer PRIVATE
            "${ENGINE_CLIENT_LIB_ROOT}/${LIB}/src/win32"
        )
    endif()
endforeach()

# External libraries (our own)
target_include_directories(SwgMapRasterizer PRIVATE
    "${EXTERNAL_OURS_ROOT}/fileInterface/include/public"
    "${EXTERNAL_OURS_ROOT}/archive/include/public"
    "${EXTERNAL_OURS_ROOT}/unicodeArchive/include/public"
    "${EXTERNAL_OURS_ROOT}/unicode/include/public"
    "${EXTERNAL_OURS_ROOT}/localization/include/public"
)

# Third-party libraries
target_include_directories(SwgMapRasterizer PRIVATE
    "${EXTERNAL_3RD_ROOT}/ati_compress"
)

# Local source directories
target_include_directories(SwgMapRasterizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/src/win32
)

# ======================================================================
# Library Dependencies
#
# These are the engine libraries that SwgMapRasterizer links against.
# The actual library targets should be defined by the parent build
# system. If building standalone, these need to be built separately.
# ======================================================================

# Core shared libraries (always needed)
set(REQUIRED_SHARED_LIBRARIES
    sharedFoundation
    sharedThread
    sharedDebug
    sharedCompression
    sharedFile
    sharedMath
    sharedImage
    sharedObject
    sharedRandom
    sharedRegex
    sharedTerrain
    sharedUtility
    sharedSynchronization
    sharedMemoryManager
)

# Client libraries (needed for GPU rendering mode)
set(REQUIRED_CLIENT_LIBRARIES
    clientGraphics
    clientObject
    clientTerrain
)

# Link libraries if targets exist (when part of a larger CMake build)
foreach(LIB ${REQUIRED_SHARED_LIBRARIES})
    if(TARGET ${LIB})
        target_link_libraries(SwgMapRasterizer PRIVATE ${LIB})
    endif()
endforeach()

foreach(LIB ${REQUIRED_CLIENT_LIBRARIES})
    if(TARGET ${LIB})
        target_link_libraries(SwgMapRasterizer PRIVATE ${LIB})
    endif()
endforeach()

# Windows system libraries
if(WIN32)
    target_link_libraries(SwgMapRasterizer PRIVATE
        kernel32
        user32
        gdi32
        advapi32
    )
endif()

# ======================================================================
# Output Configuration
# ======================================================================

set_target_properties(SwgMapRasterizer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "SwgMapRasterizer"
)

# ======================================================================
# Installation
# ======================================================================

install(TARGETS SwgMapRasterizer RUNTIME DESTINATION bin)

# ======================================================================

message(STATUS "")
message(STATUS "=== SwgMapRasterizer Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared lib root: ${ENGINE_SHARED_LIB_ROOT}")
message(STATUS "Client lib root: ${ENGINE_CLIENT_LIB_ROOT}")
message(STATUS "=======================================")
message(STATUS "")
