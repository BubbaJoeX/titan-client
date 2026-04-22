# Install script for directory: D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/libxml2")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libxml2/libxml" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/c14n.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/catalog.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/chvalid.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/debugXML.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/dict.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/encoding.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/entities.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/globals.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/hash.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/HTMLparser.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/HTMLtree.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/list.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/nanoftp.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/nanohttp.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/parser.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/parserInternals.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/pattern.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/relaxng.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/SAX.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/SAX2.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/schemasInternals.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/schematron.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/threads.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/tree.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/uri.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/valid.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xinclude.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xlink.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlIO.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlautomata.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlerror.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlexports.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlmemory.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlmodule.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlreader.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlregexp.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlsave.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlschemas.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlschemastypes.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlstring.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlunicode.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xmlwriter.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xpath.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xpathInternals.h"
    "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/include/libxml/xpointer.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/Debug/libxml2sd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/Release/libxml2s.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/MinSizeRel/libxml2s.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/RelWithDebInfo/libxml2s.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "documentation" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/doc/xml2-config.1")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "documentation" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/doc/xmlcatalog.1")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "documentation" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/doc/xmllint.1")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "documentation" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/libxml2" TYPE DIRECTORY FILES "D:/titan/client/src/external/3rd/library/libxml2/libxml2-2.12.7/doc/" REGEX "/makefile\\.[^/]*$" EXCLUDE REGEX "/[^/]*\\.1$" EXCLUDE REGEX "/[^/]*\\.py$" EXCLUDE REGEX "/[^/]*\\.res$" EXCLUDE REGEX "/[^/]*\\.xml$" EXCLUDE REGEX "/[^/]*\\.xsl$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/libxml2-config.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/libxml2-config-version.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7/libxml2-export.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7/libxml2-export.cmake"
         "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7/libxml2-export-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7/libxml2-export.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/libxml2-2.12.7" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/CMakeFiles/Export/ca6a5331874af549446af425660e8246/libxml2-export-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libxml2/libxml" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/libxml/xmlversion.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/libxml-2.0.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/xml2-config")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/titan/client/src/external/3rd/library/libxml2/build_x64_Debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
