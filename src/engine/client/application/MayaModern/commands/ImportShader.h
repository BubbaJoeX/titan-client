#ifndef SWGMAYAEDITOR_IMPORTSHADER_H
#define SWGMAYAEDITOR_IMPORTSHADER_H

#include <maya/MObject.h>
#include <maya/MPxCommand.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <string>

class ImportShader : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;

    /** Load a .sht and create a shading group without MEL. Safe from file translators. */
    static MStatus importFromPath(
        const std::string& filename,
        const std::string& materialBase,
        MString& outShadingGroupName,
        MObject* outShadingGroup = nullptr);
};

#endif
