#include "RevertToBindPose.h"

#include <maya/MAnimControl.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
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

    // 1. Select all joints in scene
    MSelectionList jointSelection;
    MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
    if (!status)
        return MS::kFailure;

    for (; !dagIt.isDone(); dagIt.next())
    {
        MDagPath path;
        status = dagIt.getPath(path);
        if (status)
            jointSelection.add(path);
    }

    if (jointSelection.length() == 0)
    {
        MGlobal::displayWarning("swgRevertToBindPose: No joints found in scene.");
        return MS::kSuccess;
    }

    // 2. Select all joints and delete their animation curves
    MGlobal::setActiveSelectionList(jointSelection);
    
    // Delete all animation on selected joints (rotation and translation)
    status = MGlobal::executeCommand("cutKey -clear -at translateX -at translateY -at translateZ "
                                     "-at rotateX -at rotateY -at rotateZ");
    if (!status)
        MGlobal::displayWarning("swgRevertToBindPose: cutKey failed (may have no keys).");

    // 3. Go to bind pose using dagPose command
    // First try the dagPose approach (works if bind pose was stored)
    status = MGlobal::executeCommand("dagPose -restore -global -bindPose");
    if (!status)
    {
        // Fallback: go to frame 0 (some rigs don't have dagPose)
        MGlobal::displayWarning("swgRevertToBindPose: dagPose failed, going to frame 0.");
        MAnimControl::setCurrentTime(MTime(0.0, MTime::uiUnit()));
    }

    // 4. Set timeline to frame 0
    MAnimControl::setCurrentTime(MTime(0.0, MTime::uiUnit()));

    MGlobal::clearSelectionList();
    MGlobal::displayInfo("swgRevertToBindPose: Cleared animation and restored bind pose.");
    return MS::kSuccess;
}
