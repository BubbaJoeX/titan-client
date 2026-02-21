# ======================================================================
#
# CMakeLists.txt for AcmBuildTool
#
# Cross-platform build configuration for Asset Customization Manager Build Tool
#
# ======================================================================

cmake_minimum_required(VERSION 3.10)

project(AcmBuildTool VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# ======================================================================
# Configuration Options
# ======================================================================

# Miff compiler path
if(WIN32)
    set(DEFAULT_MIFF_PATH "Miff.exe")
else()
    set(DEFAULT_MIFF_PATH "Miff")
endif()

set(ACM_MIFF_PATH "${DEFAULT_MIFF_PATH}" CACHE STRING "Path to Miff compiler executable")
set(ACM_COLLECT_SCRIPT "collectAssetCustomizationInfo.pl" CACHE STRING "Path to collectAssetCustomizationInfo.pl script")
set(ACM_BUILD_SCRIPT "buildAssetCustomizationManagerData.pl" CACHE STRING "Path to buildAssetCustomizationManagerData.pl script")
set(ACM_BASE_PATH "../../sku.0/sys.shared/compiled/game/" CACHE STRING "Default base path to sys.shared/compiled/game/")
set(ACM_CLIENT_PATH "client/" CACHE STRING "Output path for client files")
set(ACM_FORCE_ADD_FILE "customization/force_add_variable_usage.dat" CACHE STRING "Relative path to force_add_variable_usage.dat from base path")
set(ACM_PERL_INTERPRETER "perl" CACHE STRING "Perl interpreter command")

message(STATUS "")
message(STATUS "=== ACM Configuration Paths ===")
message(STATUS "Miff compiler: ${ACM_MIFF_PATH}")
message(STATUS "Collect script: ${ACM_COLLECT_SCRIPT}")
message(STATUS "Build script: ${ACM_BUILD_SCRIPT}")
message(STATUS "Base path: ${ACM_BASE_PATH}")
message(STATUS "Client output: ${ACM_CLIENT_PATH}")
message(STATUS "Perl interpreter: ${ACM_PERL_INTERPRETER}")
message(STATUS "===============================")
message(STATUS "")

# ======================================================================
# Source files
# ======================================================================

set(SOURCES
    src/shared/AcmBuildTool.cpp
    src/shared/main.cpp
)

set(HEADERS
    src/shared/AcmBuildTool.h
    src/shared/FirstAcmBuildTool.h
)

# Generate configuration header
configure_file(
    src/shared/AcmBuildConfig.h.in
    generated/AcmBuildConfig.h
    @ONLY
)

list(APPEND HEADERS generated/AcmBuildConfig.h)

# ======================================================================
# Build Target
# ======================================================================

# Create executable
add_executable(AcmBuildTool ${SOURCES} ${HEADERS})

# Compiler warnings
if(MSVC)
    target_compile_options(AcmBuildTool PRIVATE /W4)
else()
    target_compile_options(AcmBuildTool PRIVATE -Wall -Wextra -pedantic)
endif()

# Platform-specific settings
if(WIN32)
    # Windows specific settings
    target_compile_definitions(AcmBuildTool PRIVATE 
        _WIN32
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    
    # Set subsystem for console application
    if(MSVC)
        set_target_properties(AcmBuildTool PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
    
elseif(UNIX)
    # Linux specific settings
    target_compile_definitions(AcmBuildTool PRIVATE LINUX)
    target_link_libraries(AcmBuildTool PRIVATE pthread)
    
    # Enable additional warnings on GCC/Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(AcmBuildTool PRIVATE -Wno-unused-parameter)
    endif()
endif()

# Include directories
target_include_directories(AcmBuildTool PRIVATE 
    src/shared
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Output directory - use multi-config generator aware paths
set_target_properties(AcmBuildTool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# ======================================================================
# Installation
# ======================================================================

install(TARGETS AcmBuildTool 
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# Install documentation
install(FILES 
    README.md 
    QUICKSTART.md 
    IMPLEMENTATION.md
    DESTINATION doc/AcmBuildTool
    COMPONENT documentation
    OPTIONAL
)

# ======================================================================
# Configuration Summary
# ======================================================================

message(STATUS "")
message(STATUS "=== AcmBuildTool Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "==================================")
message(STATUS "")

# ======================================================================
