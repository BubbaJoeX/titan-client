#include "ExportShader.h"
#include "ShaderExporter.h"
#include "SetDirectoryCommand.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <iostream>
#include <string>

void* ExportShader::creator()
{
    return new ExportShader();
}

MStatus ExportShader::doIt(const MArgList& args)
{
    MStatus status;
    std::string shaderPath;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        if (!status) return MS::kFailure;
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if ((argName == "-path" || argName == "-i") && i + 1 < args.length(&status))
        {
            shaderPath = args.asString(i + 1, &status).asChar();
            if (status) ++i;
        }
    }

    if (shaderPath.empty())
    {
        std::cerr << "exportShader: specify shader tree path, e.g. exportShader -i \"shader/foo/bar\"\n";
        MGlobal::displayError("exportShader: use -i or -path with a shader path (e.g. shader/foo/bar)");
        return MS::kFailure;
    }

    const char* shaderWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
    if (!shaderWriteDir || !shaderWriteDir[0])
    {
        std::cerr << "exportShader: shaderTemplateWriteDir not set; run setBaseDir first.\n";
        MGlobal::displayError("exportShader: configure shaderTemplateWriteDir (setBaseDir)");
        return MS::kFailure;
    }

    std::string out = ShaderExporter::exportShader(shaderPath);
    if (out.empty())
    {
        MGlobal::displayError("exportShader: failed (see Script Editor / stderr)");
        return MS::kFailure;
    }

    MString msg;
    msg += "exportShader: wrote ";
    msg += out.c_str();
    MGlobal::displayInfo(msg);
    return MS::kSuccess;
}
