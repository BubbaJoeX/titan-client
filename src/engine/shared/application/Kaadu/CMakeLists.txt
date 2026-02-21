# ======================================================================
#
# CMakeLists.txt for Kaadu
#
# Cross-platform build configuration for Asset Customization Manager Build Tool
#
# ======================================================================

cmake_minimum_required(VERSION 3.10)

project(Kaadu VERSION 1.0.0 LANGUAGES CXX)

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
set(ACM_BASE_PATH "../../sku.0/sys.client/compiled/game/" CACHE STRING "Default base path to sys.client/compiled/game/")
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
    src/shared/Kaadu.cpp
    src/shared/main.cpp
)

set(HEADERS
    src/shared/Kaadu.h
    src/shared/FirstKaadu.h
)

# Generate configuration header
configure_file(
    src/shared/AcmBuildConfig.h.in
    generated/AcmBuildConfig.h
    @ONLY
)

list(APPEND HEADERS generated/Kaadu.h)

# ======================================================================
# Build Target
# ======================================================================

# Create executable
add_executable(Kaadu ${SOURCES} ${HEADERS})

# Compiler warnings
if(MSVC)
    target_compile_options(Kaadu PRIVATE /W4)
else()
    target_compile_options(Kaadu PRIVATE -Wall -Wextra -pedantic)
endif()

# Platform-specific settings
if(WIN32)
    # Windows specific settings
    target_compile_definitions(Kaadu PRIVATE 
        _WIN32
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    
    # Set subsystem for console application
    if(MSVC)
        set_target_properties(Kaadu PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
    
elseif(UNIX)
    # Linux specific settings
    target_compile_definitions(Kaadu PRIVATE LINUX)
    target_link_libraries(Kaadu PRIVATE pthread)
    
    # Enable additional warnings on GCC/Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(Kaadu PRIVATE -Wno-unused-parameter)
    endif()
endif()

# Include directories
target_include_directories(Kaadu PRIVATE 
    src/shared
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Output directory - use multi-config generator aware paths
set_target_properties(Kaadu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# ======================================================================
# Installation
# ======================================================================

install(TARGETS Kaadu 
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# Install documentation
install(FILES 
    README.md 
    QUICKSTART.md 
    IMPLEMENTATION.md
    DESTINATION doc/Kaadu
    COMPONENT documentation
    OPTIONAL
)

# ======================================================================
# Configuration Summary
# ======================================================================

message(STATUS "")
message(STATUS "=== Kaadu Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "==================================")
message(STATUS "")

# ======================================================================
