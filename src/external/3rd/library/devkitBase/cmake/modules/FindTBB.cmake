# The MIT License (MIT)
#
# Copyright (c) 2015 Justus Calvin
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#
# FindTBB
# -------
#
# Find TBB include directories and libraries.
#
# Usage:
#
#  find_package(TBB [major[.minor]] [EXACT]
#               [QUIET] [REQUIRED]
#               [[COMPONENTS] [components...]]
#               [OPTIONAL_COMPONENTS components...]) 
#
# where the allowed components are tbbmalloc, tbbmalloc_proxy or tbb_preview (when using TBB2020). 
# Users may modify the behavior of this module with the following variables:
#
# * TBB_ROOT_DIR          - The base directory the of TBB installation.
# * TBB_INCLUDE_DIR       - The directory that contains the TBB headers files.
# * TBB_LIBRARY           - The directory that contains the TBB library files.
# * TBB_<library>_LIBRARY - The path of the TBB the corresponding TBB library. 
#                           These libraries, if specified, override the 
#                           corresponding library search results, where <library>
#                           may be tbb, tbb_debug, tbbmalloc, tbbmalloc_debug,
#                           tbb_preview, or tbb_preview_debug.
# * TBB_USE_DEBUG_BUILD   - The debug version of tbb libraries, if present, will
#                           be used instead of the release version.
#
# Users may modify the behavior of this module with the following environment variable:
#
# * TBBROOT
#
# This module will set the following variables:
#
# * TBB_FOUND             - Set to false, or undefined, if we haven’t found, or
#                           don’t want to use TBB.
# * TBB_<component>_FOUND - If False, optional <component> part of TBB sytem is
#                           not available.
# * TBB_VERSION           - The full version string
# * TBB_VERSION_MAJOR     - The major version
# * TBB_VERSION_MINOR     - The minor version
# * TBB_INTERFACE_VERSION - The interface version number defined in 
#                           tbb/tbb_stddef.h.
# * TBB_<library>_LIBRARY_RELEASE - The path of the TBB release version of 
#                           <library>, where <library> may be tbb, tbb_debug,
#                           tbbmalloc, tbbmalloc_debug, tbb_preview, or 
#                           tbb_preview_debug.
# * TBB_<library>_LIBRARY_DEGUG - The path of the TBB release version of 
#                           <library>, where <library> may be tbb, tbb_debug,
#                           tbbmalloc, tbbmalloc_debug, tbb_preview, or 
#                           tbb_preview_debug.
#
# The following varibles should be used to build and link with TBB:
#
# * TBB_INCLUDE_DIRS        - The include directory for TBB.
# * TBB_LIBRARIES           - The libraries to link against to use TBB.
# * TBB_LIBRARIES_RELEASE   - The release libraries to link against to use TBB.
# * TBB_LIBRARIES_DEBUG     - The debug libraries to link against to use TBB.
# * TBB_DEFINITIONS         - Definitions to use when compiling code that uses
#                             TBB.
# * TBB_DEFINITIONS_RELEASE - Definitions to use when compiling release code that
#                             uses TBB.
# * TBB_DEFINITIONS_DEBUG   - Definitions to use when compiling debug code that
#                             uses TBB.
#
# This module will also create the "tbb" target that may be used when building
# executables and libraries.

include(FindPackageHandleStandardArgs)

if(NOT TBB_FOUND)

  ##################################
  # Check the build type
  ##################################
  
  if(NOT DEFINED TBB_USE_DEBUG_BUILD)
    if(CMAKE_BUILD_TYPE MATCHES "(Debug|DEBUG|debug|RelWithDebInfo|RELWITHDEBINFO|relwithdebinfo)")
      set(TBB_BUILD_TYPE DEBUG)
    else()
      set(TBB_BUILD_TYPE RELEASE)
    endif()
  elseif(TBB_USE_DEBUG_BUILD)
    set(TBB_BUILD_TYPE DEBUG)
  else()
    set(TBB_BUILD_TYPE RELEASE)
  endif()
  
  ##################################
  # Set the TBB search directories
  ##################################
  
  # Define search paths based on user input and environment variables
  set(TBB_SEARCH_DIR ${TBB_ROOT_DIR} $ENV{TBBROOT})
  
  # Define the search directories based on the current platform
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TBB_DEFAULT_SEARCH_DIR "C:/Program Files/Intel/TBB"
                               "C:/Program Files (x86)/Intel/TBB")

    # Set the target architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(TBB_ARCHITECTURE "intel64")
    else()
      set(TBB_ARCHITECTURE "ia32")
    endif()

    # Set the TBB search library path search suffix based on the version of VC
    if(WINDOWS_STORE)
      set(TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc11_ui")
    elseif(MSVC14)
      set(TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc14")
    elseif(MSVC12)
      set(TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc12")
    elseif(MSVC11)
      set(TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc11")
    elseif(MSVC10)
      set(TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc10")
    endif()

    # Add the library path search suffix for the VC independent version of TBB
    list(APPEND TBB_LIB_PATH_SUFFIX "lib/${TBB_ARCHITECTURE}/vc_mt")

  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # OS X
    set(TBB_DEFAULT_SEARCH_DIR "/opt/intel/tbb")
    
    # TODO: Check to see which C++ library is being used by the compiler.
    if(NOT ${CMAKE_SYSTEM_VERSION} VERSION_LESS 13.0)
      # The default C++ library on OS X 10.9 and later is libc++
      set(TBB_LIB_PATH_SUFFIX "lib/libc++")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux
    set(TBB_DEFAULT_SEARCH_DIR "/opt/intel/tbb")
    
    # TODO: Check compiler version to see the suffix should be <arch>/gcc4.1 or
    #       <arch>/gcc4.1. For now, assume that the compiler is more recent than
    #       gcc 4.4.x or later.
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
      set(TBB_LIB_PATH_SUFFIX "lib/intel64/gcc4.4")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
      set(TBB_LIB_PATH_SUFFIX "lib/ia32/gcc4.4")
    endif()
  endif()
  
  # by default, search in the root lib directory if exists.
  list(PREPEND TBB_LIB_PATH_SUFFIX "lib")
  
  ##################################
  # Find the TBB include dir
  ##################################
  
  find_path(TBB_INCLUDE_DIRS tbb/tbb.h
      HINTS ${TBB_INCLUDE_DIR} ${TBB_SEARCH_DIR}
      PATHS ${TBB_DEFAULT_SEARCH_DIR}
      PATH_SUFFIXES include)

  ##################################
  # Set version strings
  ##################################

  if(TBB_INCLUDE_DIRS)
    # Try looking for version string using header file name from both onetbb as well as tbb 2020. 
    set(TBB_VERSION_HEADER_LIST "oneapi/tbb/version.h" "tbb/tbb_stddef.h")
    foreach(HEADER ${TBB_VERSION_HEADER_LIST})
      set(HEADER_FILE "${TBB_INCLUDE_DIRS}/${HEADER}")
      if(EXISTS ${HEADER_FILE})
        file(READ ${HEADER_FILE} _tbb_version_file)
        break()
      endif()
    endforeach()

    if(NOT _tbb_version_file)
      message(FATAL_ERROR "TBB headers not found!")
    endif()

    string(REGEX REPLACE ".*#define TBB_VERSION_MAJOR ([0-9]+).*" "\\1"
        TBB_VERSION_MAJOR "${_tbb_version_file}")
    string(REGEX REPLACE ".*#define TBB_VERSION_MINOR ([0-9]+).*" "\\1"
        TBB_VERSION_MINOR "${_tbb_version_file}")
    string(REGEX REPLACE ".*#define TBB_INTERFACE_VERSION ([0-9]+).*" "\\1"
        TBB_INTERFACE_VERSION "${_tbb_version_file}")
    string(REGEX REPLACE ".*#define __TBB_BINARY_VERSION ([0-9]+).*" "\\1"
        TBB_BINARY_VERSION "${_tbb_version_file}")
    set(TBB_VERSION "${TBB_VERSION_MAJOR}.${TBB_VERSION_MINOR}")
  endif()

  ##################################
  # Find TBB components
  ##################################

  if(TBB_VERSION_MAJOR GREATER_EQUAL 2021)
    set(TBB_SEARCH_COMPONENTS tbbmalloc_proxy tbbmalloc tbb)
  elseif(TBB_VERSION_MAJOR EQUAL 2020)
    set(TBB_SEARCH_COMPONENTS tbb_preview tbbmalloc_proxy tbbmalloc tbb)
  else()
    message(FATAL_ERROR "Unsupported TBB version (${TBB_VERSION}) found!")
  endif()

  # Find each component
  foreach(_comp ${TBB_SEARCH_COMPONENTS})
    if(";${TBB_FIND_COMPONENTS};tbb;" MATCHES ";${_comp};")
      set(lib_names_to_find ${_comp})
      set(debug_lib_names_to_find "${_comp}_debug")
      if ("${_comp}" STREQUAL "tbb" AND TBB_VERSION_MAJOR GREATER_EQUAL 2021)
        # if using TBB 2021 and above, try to find tbb filenames by platform
        # specific naming convention that utilizes TBB_BINARY_VERSION.
        set(lib_names_to_find 
          "tbb${TBB_BINARY_VERSION}.lib" 
          "libtbb.${TBB_BINARY_VERSION}.dylib" 
          "libtbb.so.${TBB_BINARY_VERSION}")
        set(debug_lib_names_to_find 
          "tbb${TBB_BINARY_VERSION}_debug.lib" 
          "libtbb_debug.${TBB_BINARY_VERSION}.dylib" 
          "libtbb_debug.so.${TBB_BINARY_VERSION}")
      endif()


      # Search for the libraries
      find_library(TBB_${_comp}_LIBRARY_RELEASE
          NAMES ${lib_names_to_find}
          HINTS ${TBB_LIBRARY} ${TBB_SEARCH_DIR}
          PATHS ${TBB_DEFAULT_SEARCH_DIR}
          PATH_SUFFIXES ${TBB_LIB_PATH_SUFFIX})

      find_library(TBB_${_comp}_LIBRARY_DEBUG
          NAMES ${debug_lib_names_to_find}
          HINTS ${TBB_LIBRARY} ${TBB_SEARCH_DIR}
          PATHS ${TBB_DEFAULT_SEARCH_DIR}
          PATH_SUFFIXES ${TBB_LIB_PATH_SUFFIX})

      if(TBB_${_comp}_LIBRARY_DEBUG)
        list(APPEND TBB_LIBRARIES_DEBUG "${TBB_${_comp}_LIBRARY_DEBUG}")
      endif()
      if(TBB_${_comp}_LIBRARY_RELEASE)
        list(APPEND TBB_LIBRARIES_RELEASE "${TBB_${_comp}_LIBRARY_RELEASE}")
      endif()
      if(TBB_${_comp}_LIBRARY_${TBB_BUILD_TYPE} AND NOT TBB_${_comp}_LIBRARY)
        set(TBB_${_comp}_LIBRARY "${TBB_${_comp}_LIBRARY_${TBB_BUILD_TYPE}}")
      endif()

      if(TBB_${_comp}_LIBRARY AND EXISTS "${TBB_${_comp}_LIBRARY}")
        set(TBB_${_comp}_FOUND TRUE)
      else()
        set(TBB_${_comp}_FOUND FALSE)
      endif()

      # Mark internal variables as advanced
      mark_as_advanced(TBB_${_comp}_LIBRARY_RELEASE)
      mark_as_advanced(TBB_${_comp}_LIBRARY_DEBUG)
      mark_as_advanced(TBB_${_comp}_LIBRARY)

    endif()
  endforeach()

  ##################################
  # Set compile flags and libraries
  ##################################

  set(TBB_DEFINITIONS_RELEASE "")
  set(TBB_DEFINITIONS_DEBUG "-DTBB_USE_DEBUG=1")
    
  if(TBB_LIBRARIES_${TBB_BUILD_TYPE})
    set(TBB_DEFINITIONS "${TBB_DEFINITIONS_${TBB_BUILD_TYPE}}")
    set(TBB_LIBRARIES "${TBB_LIBRARIES_${TBB_BUILD_TYPE}}")
  elseif(TBB_LIBRARIES_RELEASE)
    set(TBB_DEFINITIONS "${TBB_DEFINITIONS_RELEASE}")
    set(TBB_LIBRARIES "${TBB_LIBRARIES_RELEASE}")
  elseif(TBB_LIBRARIES_DEBUG)
    set(TBB_DEFINITIONS "${TBB_DEFINITIONS_DEBUG}")
    set(TBB_LIBRARIES "${TBB_LIBRARIES_DEBUG}")
  endif()

  find_package_handle_standard_args(TBB 
      REQUIRED_VARS TBB_INCLUDE_DIRS TBB_LIBRARIES
      HANDLE_COMPONENTS
      VERSION_VAR TBB_VERSION)

  ##################################
  # Create targets
  ##################################

  if(NOT CMAKE_VERSION VERSION_LESS 3.0 AND TBB_FOUND)
    add_library(tbb SHARED IMPORTED)
    set_target_properties(tbb PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${TBB_INCLUDE_DIRS}"
      INTERFACE_COMPILE_DEFINITIONS "${TBB_DEFINITIONS}")
    # For Windows, Add IMPORTED_IMPLIB property but not IMPORTED_LOCATION,
    # Since the dll is placed in the application directory.
    # For other OS, IMPORTED_IMPLIB is not applicable.
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set_target_properties(tbb PROPERTIES
        IMPORTED_IMPLIB "${TBB_LIBRARIES}")
      if(DEFINED ENV{TBBROOT})
        # transform folder and extension from lib, .lib to bin, .dll.
        # eg. c:\tbbroot\lib\tbb12.lib -> c:\tbbroot\bin\tbb12.dll.
        foreach(LIB_PATH ${TBB_LIBRARIES})
          string(REPLACE ".lib" ".dll" LIB_PATH "${LIB_PATH}")
          string(REPLACE "lib" "bin" LIB_PATH "${LIB_PATH}")
          list(APPEND TBB_BINARIES "${LIB_PATH}")
        endforeach()
        set_target_properties(tbb PROPERTIES
          IMPORTED_LOCATION "${TBB_BINARIES}")
      endif()
    else()
      set_target_properties(tbb PROPERTIES
        IMPORTED_LOCATION "${TBB_LIBRARIES}")
    endif()
  endif()

  mark_as_advanced(TBB_INCLUDE_DIRS TBB_LIBRARIES)

  unset(TBB_ARCHITECTURE)
  unset(TBB_BUILD_TYPE)
  unset(TBB_LIB_PATH_SUFFIX)
  unset(TBB_DEFAULT_SEARCH_DIR)

endif()
