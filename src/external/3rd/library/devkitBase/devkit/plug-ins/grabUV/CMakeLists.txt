
# set(MAYA_CURRENT_TARGET grabUV)
# add_custom_target(${MAYA_CURRENT_TARGET} ALL)
# maya_install_files(${MAYA_CURRENT_TARGET} AREA plugin 
# 					SUBDIR ${MAYA_CURRENT_TARGET}
# 					FILES 
# 					grabUVMain.cpp
# 					grabUV.pro
# 					)
# maya_target_files(${MAYA_CURRENT_TARGET} AREA plugin
# 					SUBDIR ${MAYA_CURRENT_TARGET}
#                     PUBLIC
# 					grabUVProperties.mel
# 					grabUVValues.mel
#                     )
# maya_install_files_rename(${MAYA_CURRENT_TARGET} 
# 					AREA plugin 
# 					SUBDIR ${MAYA_CURRENT_TARGET}
# 					FILES 
# 					CMakeLists.devkit CMakeLists.txt )					
# maya_target_finalize(${MAYA_CURRENT_TARGET})



cmake_minimum_required(VERSION 3.13)

# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

# specify project name
set(PROJECT_NAME grabUV)

#set .mel file list
set(MEL_FILES 
	grabUVProperties.mel
	grabUVValues.mel)


# set SOURCE_FILES
set(SOURCE_FILES
	grabUVMain.cpp
	grabUV.pro

)

# Add Required Qt modules (requires Qt archive to be expanded)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# set linking libraries
set(LIBRARIES
	 Qt6::Core
	 Qt6::Widgets
     OpenMaya
	 OpenMayaUI
	 OpenMayaRender
     Foundation
)

# Build plugin
build_plugin()

# need to set compiler definition after the call to build_plugin since the target 
# gets defined inside that function.
if (MAYA_WANT_EMS_CURVE_WIDGET)
	target_compile_definitions(${PROJECT_NAME} PRIVATE MAYA_WANT_EMS_CURVE_WIDGET)
endif()
