#include "dds.h"
#include "SwgTranslatorNames.h"
#include "DdsToTgaConverter.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"

#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxFileTranslator.h>

#include <cstring>
#include <string>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

void* DdsTranslator::creator()
{
    return new DdsTranslator();
}

MString DdsTranslator::defaultExtension() const
{
    return "dds";
}

MString DdsTranslator::filter() const
{
    return MString(swg_translator::kFilterDds);
}

MPxFileTranslator::MFileKind DdsTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && STRICMP(pathStr.c_str() + nameLength - 4, ".dds") == 0)
        return kCouldBeMyFileType;
    return kNotMyFileType;
}

MStatus DdsTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        MGlobal::displayError("DDS import: could not resolve file path from MFileObject.");
        return MS::kFailure;
    }
    std::string baseName = MayaUtility::parseFileNameToNodeName(pathStd);
    if (baseName.empty())
        baseName = "ddsTexture";
    MString name(baseName.c_str());

    std::string pathToUse(pathStd);
    const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
    std::string outputDir;
    if (textureWriteDir && textureWriteDir[0])
    {
        outputDir = textureWriteDir;
        MayaUtility::createDirectory(outputDir.c_str());
    }
    const std::string tgaPath = DdsToTgaConverter::convertToTga(pathToUse, outputDir);
    if (!tgaPath.empty())
        pathToUse = tgaPath;
    for (size_t i = 0; i < pathToUse.size(); ++i)
        if (pathToUse[i] == '\\') pathToUse[i] = '/';

    MString fileNode = name + "_file";
    MString cmd = "shadingNode -asTexture file -n \"";
    cmd += fileNode;
    cmd += "\"";
    MStatus status = MGlobal::executeCommand(cmd);
    if (!status) return status;

    cmd = "setAttr \"";
    cmd += fileNode;
    cmd += ".fileTextureName\" -type \"string\" \"";
    cmd += pathToUse.c_str();
    cmd += "\"";
    status = MGlobal::executeCommand(cmd);
    if (!status) return status;
    cmd = "setAttr \"";
    cmd += fileNode;
    cmd += ".colorSpace\" -type \"string\" \"sRGB\"";
    MGlobal::executeCommand(cmd);
    cmd = "setAttr \"";
    cmd += fileNode;
    cmd += ".ignoreColorSpaceFileRules\" 1";
    return MGlobal::executeCommand(cmd);
}
