#ifndef SWGMAYAEDITOR_SWGMAKESHIP_H
#define SWGMAYAEDITOR_SWGMAKESHIP_H

#include <maya/MPxCommand.h>

/// Creates `swgShip` / `swgShip_geo` / `swgShip_skeleton` / `swgShip_root` plus `swgShipBundlePaths` for MGN + bundle export.
class SwgMakeShip : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
