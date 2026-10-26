#ifndef SWGMAYAEDITOR_IMPORTSAT_H
#define SWGMAYAEDITOR_IMPORTSAT_H

#include <maya/MPxCommand.h>

/** True while MPxFileTranslator::reader is on the stack. Do not MEL or displayError. */
class SwgFileImportScope
{
public:
    SwgFileImportScope();
    ~SwgFileImportScope();
    static bool active();
};

class ImportSat : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

/** Force VP2 + opaque MAIN PNG rebind. Works after File>Import even if Arnold is active. */
class SwgFixSatViewport : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
