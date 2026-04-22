# Install script for directory: D:/titan/client/src/external/3rd/library/pcre/pcre-8.45

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/PCRE")
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

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/Debug/pcred.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/Release/pcre.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/MinSizeRel/pcre.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/RelWithDebInfo/pcre.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/Debug/pcreposixd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/Release/pcreposix.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/MinSizeRel/pcreposix.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/RelWithDebInfo/pcreposix.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/pcre.h"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/pcreposix.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man1" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre-config.1"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcregrep.1"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcretest.1"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/man/man3" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre16.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre32.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_assign_jit_stack.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_compile.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_compile2.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_config.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_copy_named_substring.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_copy_substring.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_dfa_exec.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_exec.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_free_study.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_free_substring.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_free_substring_list.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_fullinfo.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_get_named_substring.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_get_stringnumber.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_get_stringtable_entries.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_get_substring.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_get_substring_list.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_jit_exec.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_jit_stack_alloc.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_jit_stack_free.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_maketables.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_pattern_to_host_byte_order.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_refcount.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_study.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_utf16_to_host_byte_order.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_utf32_to_host_byte_order.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcre_version.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreapi.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrebuild.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrecallout.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrecompat.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrecpp.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcredemo.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrejit.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrelimits.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrematching.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrepartial.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrepattern.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreperform.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreposix.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreprecompile.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcresample.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcrestack.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcresyntax.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreunicode.3"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/pcreunicode.3"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/pcre/html" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/index.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre-config.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre16.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre32.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_assign_jit_stack.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_compile.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_compile2.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_config.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_copy_named_substring.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_copy_substring.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_dfa_exec.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_exec.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_free_study.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_free_substring.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_free_substring_list.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_fullinfo.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_get_named_substring.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_get_stringnumber.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_get_stringtable_entries.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_get_substring.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_get_substring_list.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_jit_exec.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_jit_stack_alloc.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_jit_stack_free.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_maketables.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_pattern_to_host_byte_order.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_refcount.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_study.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_utf16_to_host_byte_order.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_utf32_to_host_byte_order.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcre_version.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcreapi.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrebuild.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrecallout.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrecompat.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrecpp.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcredemo.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcregrep.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrejit.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrelimits.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrematching.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrepartial.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrepattern.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcreperform.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcreposix.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcreprecompile.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcresample.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcrestack.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcresyntax.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcretest.html"
    "D:/titan/client/src/external/3rd/library/pcre/pcre-8.45/doc/html/pcreunicode.html"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES
    "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/libpcre.pc"
    "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/libpcreposix.pc"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/pcre-config")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/install_local_manifest.txt"
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
  file(WRITE "D:/titan/client/src/external/3rd/library/pcre/build_x64_Release/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
