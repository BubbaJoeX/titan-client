#ifndef SWGMAYAEDITOR_SWGMAKELIGHTSABER_H
#define SWGMAYAEDITOR_SWGMAKELIGHTSABER_H

#include <maya/MPxCommand.h>

/// Creates a capped cylinder (0.1 m diameter, 0.6 m height) and `swgLsb*` attributes for LSB export prep.
class SwgMakeLightsaber : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
