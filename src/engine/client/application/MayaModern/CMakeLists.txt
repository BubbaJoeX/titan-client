# Maya 2026 Plugin - SwgMayaEditor
# Uses devkitBase from project, targets Maya 2026 (64-bit)
# For VS 2022 Insiders: cmake -B build -G "Visual Studio 17 2022" -A x64
cmake_minimum_required(VERSION 3.13)

# Use project's devkitBase if DEVKIT_LOCATION not set
# Path from MayaModern to devkitBase (client/src/external/3rd/library/devkitBase)
# MayaModern->application->client->engine->client (5 levels) = client; need client/src
get_filename_component(CLIENT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(DEVKIT_BASE_PATH "${CLIENT_ROOT}/src/external/3rd/library/devkitBase")
set(CLIENT_SRC "${CLIENT_ROOT}/src")
if(NOT DEFINED ENV{DEVKIT_LOCATION})
    set(ENV{DEVKIT_LOCATION} "${DEVKIT_BASE_PATH}")
endif()

# Include devkitBase plugin build system (Maya 2026)
include(${DEVKIT_BASE_PATH}/cmake/pluginEntry.cmake)

# Maya 2026 is 64-bit only - ensure we build for x64 on Windows
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "Maya 2026 requires 64-bit build. Configure with -A x64")
    endif()
endif()

# Include directories
include_directories(sharedFoundation)
include_directories(sharedFile)
include_directories(sharedMath)
include_directories(unicode)
include_directories(clientGraphics)
include_directories(clientSkeletalAnimation)
include_directories(sharedCollision)
include_directories(util)
include_directories(third)
include_directories(utility)
include_directories(commands)
include_directories(translators)

add_subdirectory(unicode)

# set names
project(SwgMayaEditor)
set(PROJECT_NAME SwgMayaEditor)

# cpp stuff
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-EHa -DNOMINMAX")

# set the list of source files
set(SOURCE_FILES
        main.cpp

        clientSkeletalAnimation/MeshConstructionHelper.cpp
        clientSkeletalAnimation/MeshConstructionHelper.h
        clientSkeletalAnimation/OcclusionZoneSet.cpp
        clientSkeletalAnimation/OcclusionZoneSet.h

        sharedCollision/CollisionEnums.h
        sharedCollision/FloorTri.cpp
        sharedCollision/FloorTri.h

        sharedFoundation/Tag.h
        sharedFoundation/Globals.h
        sharedFoundation/Binary.h
        sharedFoundation/Misc.h
        sharedFoundation/Crc.cpp
        sharedFoundation/Crc.h
        sharedFoundation/CrcString.cpp
        sharedFoundation/CrcString.h
        sharedFoundation/TemporaryCrcString.cpp
        sharedFoundation/TemporaryCrcString.h
        sharedFoundation/PersistentCrcString.cpp
        sharedFoundation/PersistentCrcString.h
        sharedFoundation/CrcLowerString.cpp
        sharedFoundation/CrcLowerString.h
        sharedFoundation/Os.cpp
        sharedFoundation/Os.h
        sharedFoundation/LessPointerComparator.h
        sharedFoundation/PointerDeleter.h
        sharedFoundation/StringCompare.h
        sharedFoundation/ConfigFile.cpp
        sharedFoundation/ConfigFile.h

        sharedFile/AbstractFile.cpp
        sharedFile/AbstractFile.h
        sharedFile/StdioFile.cpp
        sharedFile/StdioFile.h
        sharedFile/OsFile.cpp
        sharedFile/OsFile.h
        sharedFile/Iff.cpp
        sharedFile/Iff.h

        sharedMath/FloatMath.h
        sharedMath/PackedArgb.cpp
        sharedMath/PackedArgb.h
        sharedMath/Quaternion.cpp
        sharedMath/Quaternion.h
        sharedMath/Transform.cpp
        sharedMath/Transform.h
        sharedMath/Vector.cpp
        sharedMath/Vector.h
        sharedMath/VectorArgb.cpp
        sharedMath/VectorArgb.h
        sharedMath/CompressedQuaternion.h
        sharedMath/CompressedQuaternion.cpp

        third/backward.cpp
        third/backward.hpp
        third/loguru.cpp
        third/loguru.hpp


        translators/ans.cpp
        translators/ans.h
        translators/flr.cpp
        translators/flr.h
        translators/mgn.cpp
        translators/mgn.h
        translators/msh.cpp
        translators/msh.h
        translators/skt.cpp
        translators/skt.h
        translators/sat.cpp
        translators/sat.h
        translators/pob.cpp
        translators/pob.h
        translators/dds.cpp
        translators/dds.h
        translators/lod.cpp
        translators/lod.h
        translators/lmg.cpp
        translators/lmg.h
        translators/lsb.cpp
        translators/lsb.h
        translators/LsbAttributes.cpp
        translators/LsbAttributes.h
        translators/SwgTranslatorNames.h

        utility/MayaUtility.cpp
        utility/MayaUtility.h
        utility/MayaConversions.cpp
        utility/MayaConversions.h
        utility/MayaCompoundString.cpp
        utility/MayaCompoundString.h

        commands/SetDirectoryCommand.cpp
        commands/SetDirectoryCommand.h
        commands/SetBaseDirectory.cpp
        commands/SetBaseDirectory.h
        commands/GetDataRootDirCommand.cpp
        commands/GetDataRootDirCommand.h
        commands/ImportPathResolver.cpp
        commands/ImportPathResolver.h
        commands/SwgTrtsIo.cpp
        commands/SwgTrtsIo.h
        commands/MayaSceneBuilder.cpp
        commands/MayaSceneBuilder.h
        commands/DdsToTgaConverter.cpp
        commands/DdsToTgaConverter.h
        commands/TgaToDdsConverter.cpp
        commands/TgaToDdsConverter.h
        commands/ExportSkeleton.cpp
        commands/ImportSkeleton.cpp
        commands/ExportSkeleton.h
        commands/ImportSkeleton.h
        commands/ImportAnimation.cpp
        commands/ImportAnimation.h
        commands/RevertToBindPose.cpp
        commands/RevertToBindPose.h
        commands/ImportSat.cpp
        commands/ImportSat.h
        commands/SatRoundTrip.cpp
        commands/SatRoundTrip.h
        commands/ExportSat.cpp
        commands/ExportSat.h
        commands/ImportPob.cpp
        commands/ImportPob.h
        commands/ExportPob.cpp
        commands/ExportPob.h
        commands/CreatePobTemplate.cpp
        commands/CreatePobTemplate.h
        commands/AddPobPortal.cpp
        commands/AddPobPortal.h
        commands/ReportPobPortals.cpp
        commands/ReportPobPortals.h
        commands/PobAuthoringShared.cpp
        commands/PobAuthoringShared.h
        commands/PobExtendedCommands.cpp
        commands/PobExtendedCommands.h
        commands/ImportLodMesh.cpp
        commands/ImportLodMesh.h
        commands/ImportSkeletalMesh.cpp
        commands/ImportSkeletalMesh.h
        commands/ImportShader.cpp
        commands/ImportShader.h
        commands/ExportShader.cpp
        commands/ExportShader.h
        commands/ImportStaticMesh.cpp
        commands/ImportStaticMesh.h
        commands/ImportStructure.cpp
        commands/ImportStructure.h
        commands/SwgAssetDissector.cpp
        commands/SwgAssetDissector.h
        commands/SwgAnimationBrowser.cpp
        commands/SwgAnimationBrowser.h
        commands/SkeletonTemplateWriter.cpp
        commands/SkeletonTemplateWriter.h
        commands/StaticMeshWriter.cpp
        commands/StaticMeshWriter.h
        commands/ExportStaticMesh.cpp
        commands/ExportStaticMesh.h
        commands/PrepareStaticMeshExport.cpp
        commands/PrepareStaticMeshExport.h
        commands/SwgAddStaticMeshHardpoint.cpp
        commands/SwgAddStaticMeshHardpoint.h
        commands/SwgApplyWavefrontMtl.cpp
        commands/SwgApplyWavefrontMtl.h
        commands/SwgReformatMesh.cpp
        commands/SwgReformatMesh.h
        commands/SwgMakeLightsaber.cpp
        commands/SwgMakeLightsaber.h
        commands/SwgMakeShip.cpp
        commands/SwgMakeShip.h
        commands/SwgMakeVehicle.cpp
        commands/SwgMakeVehicle.h
        commands/SwgLightsaberToolkit.cpp
        commands/SwgLightsaberToolkit.h
        commands/SwgVehicleToolkit.cpp
        commands/SwgVehicleToolkit.h
        commands/ShaderExporter.cpp
        commands/ShaderExporter.h
        )


# the required Maya Libraries
set(LIBRARIES
        OpenMaya Foundation OpenMayaUI OpenMayaAnim OpenMayaFX OpenMayaRender
        )

# our third party libraries
find_package(Threads)

# ZLib - build from project source (zlib-1.2.3)
add_library(zlib STATIC
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/adler32.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/compress.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/crc32.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/deflate.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/gzio.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/inffast.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/inflate.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/inftrees.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/trees.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/uncompr.c
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3/zutil.c
)
target_include_directories(zlib PUBLIC
    ${CLIENT_ROOT}/src/external/3rd/library/zlib-1.2.3
    ${CLIENT_ROOT}/src/external/3rd/library/zlib/include
)

# now build the plugin
build_plugin()
target_link_libraries(${CMAKE_PROJECT_NAME} ws2_32 zlib)

# Optional: CMake target swgDeployMayaPlugins copies .mll + MEL into Maya's plug-ins (see SWG_MAYA_PLUGIN_INSTALL_DIR).
# Close Maya (or unloadPlugin SwgMayaEditor) before deploy — Windows locks the .mll while loaded, so copies can fail or show 0 updated.
set(SWG_MAYA_PLUGIN_INSTALL_DIR "" CACHE PATH
    "If non-empty, target swgDeployMayaPlugins copies SwgMayaEditor.mll and companion .mel files here.")

# Copy swgAssetDissector.mel to build output (next to .mll so plugin can find it)
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgAssetDissector.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgAssetDissector.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgAnimationBrowser.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgAnimationBrowser.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgStaticMeshExport.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgStaticMeshExport.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/pobAuthoring.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/pobAuthoring.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgLightsaberToolkit.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgLightsaberToolkit.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgVehicleToolkit.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgVehicleToolkit.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgMshExportOptions.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgMshExportOptions.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgKeepFrameZeroKeys.mel
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgKeepFrameZeroKeys.mel
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgSplitMeshFourQuadrants.py
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/swgSplitMeshFourQuadrants.py
    COMMENT "Copying MEL scripts to plugin directory"
)

# Deploy into Maya's plug-ins folder (Administrator if under Program Files; quit Maya first so the .mll is not locked).
if(SWG_MAYA_PLUGIN_INSTALL_DIR)
    add_custom_target(
        swgDeployMayaPlugins
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgAssetDissector.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgAnimationBrowser.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgStaticMeshExport.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/pobAuthoring.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgLightsaberToolkit.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgVehicleToolkit.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgMshExportOptions.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgKeepFrameZeroKeys.mel
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/swgSplitMeshFourQuadrants.py
            "${SWG_MAYA_PLUGIN_INSTALL_DIR}"
        DEPENDS ${CMAKE_PROJECT_NAME}
        COMMENT "Deploy SwgMayaEditor.mll + MEL to SWG_MAYA_PLUGIN_INSTALL_DIR (close Maya if copy fails)"
    )
endif()
