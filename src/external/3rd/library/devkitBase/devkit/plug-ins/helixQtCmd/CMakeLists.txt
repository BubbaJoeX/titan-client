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
set(PROJECT_NAME helixQtCmd)
project( ${PROJECT_NAME} )

# set SOURCE_FILES
set(SOURCE_FILES
   helixQtCmd.cpp
)

# set linking libraries
set(LIBRARIES
     OpenMaya
     Foundation
)
# Add Required Qt modules (requires Qt archive to be expanded)
set( libs Core Widgets )
find_qt( "${libs}" )

# Build plugin
build_plugin()
