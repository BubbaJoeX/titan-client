# ======================================================================
# CMakeLists.txt for Jawa
# Asset Customization Manager Build Tool
# ======================================================================

cmake_minimum_required(VERSION 3.10)
project(Jawa VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ======================================================================
# Configuration Options
# ======================================================================

if(WIN32)
    set(DEFAULT_MIFF_PATH "Miff.exe")
    set(DEFAULT_PERL "perl")
else()
    set(DEFAULT_MIFF_PATH "Miff")
    set(DEFAULT_PERL "perl")
endif()

set(JAWA_MIFF_PATH "${DEFAULT_MIFF_PATH}" CACHE STRING "Path to Miff compiler")
set(JAWA_COLLECT_SCRIPT "collectAssetCustomizationInfo.pl" CACHE STRING "Collect script path")
set(JAWA_BUILD_SCRIPT "buildAssetCustomizationManagerData.pl" CACHE STRING "Build script path")
set(JAWA_BASE_PATH "../../sku.0/sys.shared/compiled/game/" CACHE STRING "Default base path")
set(JAWA_CLIENT_PATH "client/" CACHE STRING "Client output path")
set(JAWA_FORCE_ADD_FILE "customization/force_add_variable_usage.dat" CACHE STRING "Force add file")
set(JAWA_PERL_INTERPRETER "${DEFAULT_PERL}" CACHE STRING "Perl interpreter")
set(JAWA_PERL_LIB_PATH "" CACHE STRING "Perl library path (PERL5LIB) for .pm modules")

message(STATUS "")
message(STATUS "=== Jawa Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Miff compiler: ${JAWA_MIFF_PATH}")
message(STATUS "Perl: ${JAWA_PERL_INTERPRETER}")
if(JAWA_PERL_LIB_PATH)
    message(STATUS "Perl Lib Path: ${JAWA_PERL_LIB_PATH}")
endif()
message(STATUS "==========================")
message(STATUS "")

# ======================================================================
# Generate Config Header
# ======================================================================

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared/JawaConfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/JawaConfig.h
    @ONLY
)

# ======================================================================
# Build Target
# ======================================================================

add_executable(Jawa 
    src/shared/Jawa.cpp 
    src/shared/main.cpp
    src/shared/Jawa.h
    src/shared/FirstJawa.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/JawaConfig.h
)

# Platform settings
if(WIN32)
    target_compile_definitions(Jawa PRIVATE 
        _WIN32 
        _CRT_SECURE_NO_WARNINGS 
        NOMINMAX 
        WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(Jawa PRIVATE /W3)
        set_target_properties(Jawa PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
else()
    target_compile_definitions(Jawa PRIVATE LINUX)
    target_link_libraries(Jawa PRIVATE pthread)
    target_compile_options(Jawa PRIVATE -Wall -Wextra)
endif()

# Include directories
target_include_directories(Jawa PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Output directory
set_target_properties(Jawa PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

# ======================================================================
# Installation
# ======================================================================

install(TARGETS Jawa RUNTIME DESTINATION bin)

message(STATUS "Jawa configured successfully!")
message(STATUS "Build with: cmake --build build --config Release")
message(STATUS "")
