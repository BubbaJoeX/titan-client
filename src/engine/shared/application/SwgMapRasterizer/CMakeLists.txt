# ======================================================================
# SwgMapRasterizer — CMake entry point
#
# On Windows, the vcxproj carries the real dependency closure (incl. dpvs,
# sharedCollision, clientGame, Direct3D, etc.). A pure CMake link of dozens
# of prebuilt .lib files drifts from that and breaks when a library is
# missing (e.g. clientGame on some trees).
#
# Default: MSBuild the official vcxproj (Debug|x64 — full link; see SWG_MSBUILD_CONFIG)
#   → client/src/compile/x64/SwgMapRasterizer/Debug/SwgMapRasterizer_d.exe
#   → copy to titan/exe/win64_rel
#
# Tests: ctest  (e.g. -help; use cmake --build --config <same as SWG_MSBUILD_CONFIG>)
# ======================================================================

cmake_minimum_required(VERSION 3.20)
project(SwgMapRasterizerDriver NONE)

# Paths: .../client/src/.../SwgMapRasterizer  →  client/src, titan root
get_filename_component(SWG_THIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SWG_CLIENT_SRC "${SWG_THIS_DIR}/../../../.." ABSOLUTE)
# swg.sln and x64.props live here; required when MSBuild has no .sln (deps use $(SolutionDir)x64.props)
get_filename_component(SWG_MSBUILD_SOLUTION_DIR "${SWG_CLIENT_SRC}/build/win32" ABSOLUTE)
# Debug|x64 has the same link closure as SwgTitan; Release|x64 in vcxproj is a minimal
# set and does not link dpvs, sharedCollision, clientGame, etc. On many trees
# clientGame is only built for Debug|x64 — use Debug for a working map tool.
set(SWG_MSBUILD_CONFIG "Debug" CACHE STRING "MSBuild configuration (x64: use Debug for full link)")
get_filename_component(SWG_X64_OUTDIR "${SWG_CLIENT_SRC}/compile/x64/SwgMapRasterizer/${SWG_MSBUILD_CONFIG}" ABSOLUTE)
if(SWG_MSBUILD_CONFIG STREQUAL "Debug")
    set(SWG_BUILT_EXENAME "SwgMapRasterizer_d.exe")
else()
    set(SWG_BUILT_EXENAME "SwgMapRasterizer_r.exe")
endif()
get_filename_component(SWG_BUILT_EXE "${SWG_X64_OUTDIR}/${SWG_BUILT_EXENAME}" ABSOLUTE)
get_filename_component(SWG_TITAN_ROOT "${SWG_THIS_DIR}/../../../../../../" ABSOLUTE)
get_filename_component(SWG_SHIP_DIR "${SWG_TITAN_ROOT}/exe/win64_rel" ABSOLUTE)

set(SWG_VCXPROJ "${SWG_THIS_DIR}/build/win32/SwgMapRasterizer.vcxproj" CACHE FILEPATH "SwgMapRasterizer Visual Studio project")

option(SWG_USE_VCXPROJ_MSBUILD
    "On Windows, build via the official vcxproj (Recommended)"
    ON
)

if(SWG_USE_VCXPROJ_MSBUILD AND WIN32)
    if(NOT SWG_TITAN_ROOT)
        message(FATAL_ERROR "Internal: SWG_TITAN_ROOT empty")
    endif()
    if(NOT EXISTS "${SWG_VCXPROJ}")
        message(FATAL_ERROR "vcxproj not found: ${SWG_VCXPROJ}")
    endif()
    # Visual Studio: MSBuild. Prefer PATH / vswhere, then BuildTools/Community/Insiders.
    set(_msb "$ENV{SWG_MSBUILD_PATH}")
    if(EXISTS "${_msb}/MSBuild.exe")
        set(SWG_MSBUILD "${_msb}/MSBuild.exe")
    elseif(DEFINED ENV{VSINSTALLDIR} AND EXISTS "$ENV{VSINSTALLDIR}MSBuild/Current/Bin/MSBuild.exe")
        set(SWG_MSBUILD "$ENV{VSINSTALLDIR}MSBuild/Current/Bin/MSBuild.exe")
    else()
        # Avoid $ENV{ProgramFiles(x86)} in find_program HINTS (parentheses)
        if(DEFINED ENV{ProgramW6432})
            set(_vs_base "$ENV{ProgramW6432}/Microsoft Visual Studio/2022")
        else()
            set(_vs_base "C:/Program Files/Microsoft Visual Studio/2022")
        endif()
        find_program(SWG_MSBUILD MSBuild
            HINTS
            "${_vs_base}/BuildTools/MSBuild/Current/Bin"
            "${_vs_base}/Community/MSBuild/Current/Bin"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin"
        )
    endif()
    if(NOT SWG_MSBUILD)
        message(FATAL_ERROR
            "MSBuild not found. Install VS/Build Tools, or set SWG_MSBUILD_PATH to the folder containing MSBuild.exe\n"
            "  Example:  cmake -B build -DSWG_MSBUILD_PATH=\"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin\"")
    endif()
    add_custom_target(SwgMapRasterizer ALL
        COMMAND "${SWG_MSBUILD}" "${SWG_VCXPROJ}" "/p:Configuration=${SWG_MSBUILD_CONFIG}" /p:Platform=x64
            "/p:SolutionDir=${SWG_MSBUILD_SOLUTION_DIR}/" /m /nologo
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SWG_SHIP_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SWG_BUILT_EXE}"
            "${SWG_SHIP_DIR}/${SWG_BUILT_EXENAME}"
        BYPRODUCTS "${SWG_BUILT_EXE}"
        WORKING_DIRECTORY "${SWG_THIS_DIR}"
        COMMENT "MSBuild + ship ${SWG_BUILT_EXENAME} to ${SWG_SHIP_DIR}"
        VERBATIM
    )
    install(FILES "${SWG_BUILT_EXE}" DESTINATION . RENAME "${SWG_BUILT_EXENAME}")
else()
    message(STATUS "SwgMapRasterizer: SWG_USE_VCXPROJ_MSBUILD=OFF — no default build; use Visual Studio to open build/win32/SwgMapRasterizer.vcxproj")
endif()

# ----------------------------------------------------------------------
# CTest: -help (process exits with code 0 from printUsage; no getchar with -noPause in WinMain)
# ----------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    if(EXISTS "${SWG_SHIP_DIR}/${SWG_BUILT_EXENAME}")
        set(_texe "${SWG_SHIP_DIR}/${SWG_BUILT_EXENAME}")
    elseif(EXISTS "${SWG_BUILT_EXE}")
        set(_texe "${SWG_BUILT_EXE}")
    else()
        set(_texe "")
    endif()
    if(_texe)
        add_test(
            NAME SwgMapRasterizer_help
            COMMAND "${_texe}" -noPause -help
            WORKING_DIRECTORY "${SWG_SHIP_DIR}"
        )
        set_property(TEST SwgMapRasterizer_help PROPERTY LABELS "swg;tools")
    else()
        message(STATUS "CTest: Skipping SwgMapRasterizer tests (build ${SWG_BUILT_EXE} first)")
    endif()
endif()

if(SWG_USE_VCXPROJ_MSBUILD AND WIN32)
    set(_m "${SWG_MSBUILD}")
else()
    set(_m "(n/a)")
endif()
message(STATUS "")
message(STATUS "=== SwgMapRasterizer (CMake → MSBuild) ===")
message(STATUS "  MSBuild:         ${_m}")
message(STATUS "  SolutionDir:     ${SWG_MSBUILD_SOLUTION_DIR}/  (x64.props + toolchain)")
message(STATUS "  vcxproj:         ${SWG_VCXPROJ}")
message(STATUS "  Config:          ${SWG_MSBUILD_CONFIG}")
message(STATUS "  Output:         ${SWG_BUILT_EXE}")
message(STATUS "  Ship:            ${SWG_SHIP_DIR}/${SWG_BUILT_EXENAME}")
message(STATUS "  Build+ship:     cmake --build <builddir> --config ${SWG_MSBUILD_CONFIG}")
message(STATUS "  Test:            ctest --test-dir <builddir> -C ${SWG_MSBUILD_CONFIG}")
message(STATUS "==========================================")
message(STATUS "")
