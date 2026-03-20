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
set(PROJECT_NAME qtAdskStyleProxy)
project( ${PROJECT_NAME} )




# set SOURCE_FILES for build_plugin()
set(SOURCE_FILES
    qtAdskStyleProxy.cpp
)

# set linking libraries for build_plugin()
set(LIBRARIES
     OpenMaya
     OpenMayaUI
)
# Add Required Qt modules (requires Qt archive to be expanded)
set( libs Core Widgets )
find_qt( "${libs}" )

# Build plugin
build_plugin()

# but use default libraries extensions instead of Maya plugins extension
set_property( TARGET ${PROJECT_NAME} PROPERTY SUFFIX )



# To use this plugin:
#
# - After it is compiled, copy the target file (qtAdskStyleProxy.dll/.so/.bundle)
#   to the '.../plugins/styles/' folder, where there should already be a
#   plugin 'AdskStyle'.
#
# - Start Maya with the style flag, like so:
#     maya -style qtadskstyleproxy
#
# - Or you can start Maya normally and use PySide to set the style:
#     from PySide2 import QtWidgets, QtGui, QtCore
#
#     app = QtWidgets.QApplication
#     st = app.setStyle('qtadskstyleproxy')





