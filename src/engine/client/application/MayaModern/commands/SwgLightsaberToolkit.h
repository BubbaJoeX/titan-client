#ifndef SWGMAYAEDITOR_SWGLIGHTSABERTOOLKIT_H
#define SWGMAYAEDITOR_SWGLIGHTSABERTOOLKIT_H

#include <maya/MPxCommand.h>

/// Opens the SWG Lightsaber Toolkit window (MEL: blade preview, create base, import LSB).
class SwgLightsaberToolkit : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
