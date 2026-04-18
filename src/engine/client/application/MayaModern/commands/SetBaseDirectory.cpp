#include "SetBaseDirectory.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"

#include <maya/MArgList.h>
#include <maya/MStatus.h>

#include <string>

void* SetBaseDirectory::creator()
{
    return new SetBaseDirectory();
}

MStatus SetBaseDirectory::doIt(const MArgList& args)
{
    MStatus status;
    const unsigned argCount = args.length(&status);
    if (!status || argCount != 1)
    {
        std::cerr << "setBaseDir: requires one string argument (base path)" << std::endl;
        return MS::kFailure;
    }

    MString mayaArg = args.asString(0, &status);
    if (!status)
    {
        std::cerr << "setBaseDir: failed to get argument" << std::endl;
        return MS::kFailure;
    }

    std::string baseDirectory = mayaArg.asChar();
    if (!baseDirectory.empty() && baseDirectory.back() != '\\')
        baseDirectory += '\\';

    const std::string appearanceWriteDir = baseDirectory + "appearance\\";
    const std::string shaderTemplateWriteDir = baseDirectory + "shader\\";
    const std::string textureWriteDir = baseDirectory + "texture\\";
    const std::string animationWriteDir = appearanceWriteDir + "animation\\";
    const std::string skeletonTemplateWriteDir = appearanceWriteDir + "skeleton\\";
    const std::string meshWriteDir = appearanceWriteDir + "mesh\\";
    const std::string logWriteDir = baseDirectory + "log\\";
    const std::string satWriteDir = appearanceWriteDir;

    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX, appearanceWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX, shaderTemplateWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX, textureWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::ANIMATION_WRITE_DIR_INDEX, animationWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SKELETON_TEMPLATE_WRITE_DIR_INDEX, skeletonTemplateWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::LOG_DIR_INDEX, logWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SAT_WRITE_DIR_INDEX, satWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX, baseDirectory.c_str());

    MayaUtility::createDirectory(appearanceWriteDir.c_str());
    MayaUtility::createDirectory(shaderTemplateWriteDir.c_str());
    MayaUtility::createDirectory(textureWriteDir.c_str());
    MayaUtility::createDirectory(animationWriteDir.c_str());
    MayaUtility::createDirectory(skeletonTemplateWriteDir.c_str());
    MayaUtility::createDirectory(meshWriteDir.c_str());
    MayaUtility::createDirectory(logWriteDir.c_str());
    MayaUtility::createDirectory(satWriteDir.c_str());

    const std::string exportedStagingWriteDir = baseDirectory + "exported\\";
    MayaUtility::createDirectory(exportedStagingWriteDir.c_str());

    const std::string shaderRef = "shader/";
    const std::string effectRef = "effect/";
    const std::string textureRef = "texture/";
    const std::string skeletonRef = "appearance/skeleton/";
    const std::string appearanceRef = "appearance/";

    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::APPEARANCE_REFERENCE_DIR_INDEX, appearanceRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_REFERENCE_DIR_INDEX, shaderRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::EFFECT_REFERENCE_DIR_INDEX, effectRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::TEXTURE_REFERENCE_DIR_INDEX, textureRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SKELETON_TEMPLATE_REFERENCE_DIR_INDEX, skeletonRef.c_str());

    std::cerr << "setBaseDir: data root [" << baseDirectory << "]" << std::endl;
    return MS::kSuccess;
}
