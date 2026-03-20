#include "dds.h"
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
    return "DDS Texture | SWG (*.dds)";
}

MPxFileTranslator::MFileKind DdsTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const char* name = fileName.resolvedName().asChar();
    const int nameLength = static_cast<int>(strlen(name));
    if (nameLength > 4 && STRICMP(name + nameLength - 4, ".dds") == 0)
        return kIsMyFileType;
    return kNotMyFileType;
}

MStatus DdsTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const MString path = file.expandedFullName();
    MString name = file.name();
    // Strip .dds extension for node name (substring end is exclusive)
    if (name.length() > 4 && name.substring(name.length() - 4, name.length()) == ".dds")
        name = name.substring(0, name.length() - 4);
    if (name.length() == 0) name = "ddsTexture";

    std::string pathToUse(path.asChar());
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
