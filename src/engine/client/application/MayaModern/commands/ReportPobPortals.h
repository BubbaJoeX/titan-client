#ifndef SWGMAYAEDITOR_REPORTPOBPORTALS_H
#define SWGMAYAEDITOR_REPORTPOBPORTALS_H

#include <maya/MPxCommand.h>

class ReportPobPortals : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
