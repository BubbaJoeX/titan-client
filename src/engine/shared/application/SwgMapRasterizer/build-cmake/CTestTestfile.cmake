# CMake generated Testfile for 
# Source directory: D:/titan/client/src/engine/shared/application/SwgMapRasterizer
# Build directory: D:/titan/client/src/engine/shared/application/SwgMapRasterizer/build-cmake
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[SwgMapRasterizer_help]=] "D:/titan/exe/win64_rel/SwgMapRasterizer_d.exe" "-noPause" "-help")
  set_tests_properties([=[SwgMapRasterizer_help]=] PROPERTIES  LABELS "swg;tools" WORKING_DIRECTORY "D:/titan/exe/win64_rel" _BACKTRACE_TRIPLES "D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;108;add_test;D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[SwgMapRasterizer_help]=] "D:/titan/exe/win64_rel/SwgMapRasterizer_d.exe" "-noPause" "-help")
  set_tests_properties([=[SwgMapRasterizer_help]=] PROPERTIES  LABELS "swg;tools" WORKING_DIRECTORY "D:/titan/exe/win64_rel" _BACKTRACE_TRIPLES "D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;108;add_test;D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[SwgMapRasterizer_help]=] "D:/titan/exe/win64_rel/SwgMapRasterizer_d.exe" "-noPause" "-help")
  set_tests_properties([=[SwgMapRasterizer_help]=] PROPERTIES  LABELS "swg;tools" WORKING_DIRECTORY "D:/titan/exe/win64_rel" _BACKTRACE_TRIPLES "D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;108;add_test;D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[SwgMapRasterizer_help]=] "D:/titan/exe/win64_rel/SwgMapRasterizer_d.exe" "-noPause" "-help")
  set_tests_properties([=[SwgMapRasterizer_help]=] PROPERTIES  LABELS "swg;tools" WORKING_DIRECTORY "D:/titan/exe/win64_rel" _BACKTRACE_TRIPLES "D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;108;add_test;D:/titan/client/src/engine/shared/application/SwgMapRasterizer/CMakeLists.txt;0;")
else()
  add_test([=[SwgMapRasterizer_help]=] NOT_AVAILABLE)
endif()
