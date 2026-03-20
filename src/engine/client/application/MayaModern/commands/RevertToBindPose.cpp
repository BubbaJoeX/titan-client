#include "RevertToBindPose.h"

#include <maya/MAnimControl.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

void* RevertToBindPose::creator()
{
    return new RevertToBindPose();
}

MStatus RevertToBindPose::doIt(const MArgList& args)
{
    MStatus status;

    // Go to frame 0 so cutKey preserves bind pose (frame 0) values
    MAnimControl::setCurrentTime(MTime(0.0, MTime::kFilm));

    MSelectionList selection;
    status = MGlobal::getActiveSelectionList(selection);

    bool useSelection = (selection.length() > 0);

    if (!useSelection)
    {
        // No selection: find all joints in the scene
        MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
        if (!status)
            return MS::kFailure;

        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath path;
            status = dagIt.getPath(path);
            if (status)
                selection.add(path);
        }

        if (selection.length() == 0)
        {
            MGlobal::displayWarning("swgRevertToBindPose: No joints selected and no joints found in scene.");
            return MS::kSuccess;
        }

        MGlobal::setActiveSelectionList(selection);
    }

    // cutKey -clear: remove all keys, leave attributes at current (frame 0) values
    status = MGlobal::executeCommand("cutKey -clear -hierarchy above -controlPoints 0 -shape 0");
    if (!status)
    {
        MGlobal::displayError("swgRevertToBindPose: cutKey failed.");
        return status;
    }

    if (!useSelection)
        MGlobal::clearSelectionList();

    MGlobal::displayInfo("swgRevertToBindPose: Reverted to bind pose (cleared animation).");
    return MS::kSuccess;
}
