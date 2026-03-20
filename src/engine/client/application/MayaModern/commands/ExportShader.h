#ifndef SWGMAYAEDITOR_EXPORTSHADER_H
#define SWGMAYAEDITOR_EXPORTSHADER_H

#include <maya/MPxCommand.h>

/// MEL: exportShader -i "shader/foo/bar"  (or -path)
/// Writes .sht to shaderTemplateWriteDir with TGA→DDS texture conversion.
class ExportShader : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
