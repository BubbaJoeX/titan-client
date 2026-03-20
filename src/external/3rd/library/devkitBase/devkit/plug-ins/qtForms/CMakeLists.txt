#-
# ==========================================================================
# Copyright (c) 2023 Autodesk, Inc.
# All rights reserved.
# 
# These coded instructions, statements, and computer programs contain
# unpublished proprietary information written by Autodesk, Inc., and are
# protected by Federal copyright law. They may not be disclosed to third
# parties or copied or duplicated in any form, in whole or in part, without
# the prior written consent of Autodesk, Inc.
# ==========================================================================
#+

cmake_minimum_required(VERSION 3.13)


# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)


# specify project name
set(PROJECT_NAME qtForm)
project( ${PROJECT_NAME} )



# set SOURCE_FILES for build_plugin()
set(SOURCE_FILES
    qtForms.cpp
    qtForms.qrc    
)

# set linking libraries for build_plugin()
set(LIBRARIES
     OpenMaya
     Foundation
)

# Add Required Qt modules (requires Qt archive to be expanded)
set( libs Widgets UiTools )
find_qt( "${libs}" )

# Build plugin
build_plugin() 



