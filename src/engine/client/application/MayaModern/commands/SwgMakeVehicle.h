#ifndef SWGMAYAEDITOR_SWGMAKEVEHICLE_H
#define SWGMAYAEDITOR_SWGMAKEVEHICLE_H

#include <maya/MPxCommand.h>

/// Ground vehicle rig (not spacecraft): swgVehicle_geo + swgVehicleBundlePaths, swgVehicle_skeleton,
/// swgVehicle_root, seat_0..seat_{n-1}, hardpoints, swgVehicleSeatCount on root. Ships use swgMakeShip.
class SwgMakeVehicle : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
