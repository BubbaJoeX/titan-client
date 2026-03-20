#ifndef SWGMAYAEDITOR_IMPORTSHADER_H
#define SWGMAYAEDITOR_IMPORTSHADER_H

#include <maya/MPxCommand.h>

class ImportShader : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
