#ifndef SWGMAYAEDITOR_SWGVEHICLETOOLKIT_H
#define SWGMAYAEDITOR_SWGVEHICLETOOLKIT_H

#include <maya/MPxCommand.h>

/// Sources swgVehicleToolkit.mel and opens the SWG Vehicle Toolkit window.
class SwgVehicleToolkit : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
