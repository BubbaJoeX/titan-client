#include "ExportSkeleton.h"
#include "SkeletonTemplateWriter.h"
#include "SetDirectoryCommand.h"
#include "MayaConversions.h"
#include "MayaCompoundString.h"
#include "MayaUtility.h"

#include "Iff.h"
#include "Quaternion.h"
#include "Vector.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>

#include <cstring>
#include <string>

namespace
{
    constexpr int SKELETON_TEMPLATE_WRITE_DIR_INDEX = 4;
    constexpr int DEFAULT_IFF_SIZE = 65536;
}

void* ExportSkeleton::creator()
{
    return new ExportSkeleton();
}

MStatus ExportSkeleton::doIt(const MArgList& args)
{
    MStatus status;
    int bindPoseFrame = -10;
    std::string targetPath;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        if (!status)
            return MS::kFailure;
        MString argName = args.asString(i, &status);
        if (!status)
            return MS::kFailure;

        if (argName == "-bp" && i + 1 < args.length(&status))
        {
            bindPoseFrame = args.asInt(i + 1, &status);
            if (status)
                ++i;
        }
        else if (argName == "-path" && i + 1 < args.length(&status))
        {
            targetPath = args.asString(i + 1, &status).asChar();
            if (status)
                ++i;
        }
    }

    MSelectionList sel;
    status = MGlobal::getActiveSelectionList(sel);
    if (!status || sel.length() == 0)
    {
        std::cerr << "ExportSkeleton: select a joint or skeleton root" << std::endl;
        return MS::kFailure;
    }

    MDagPath dagPath;
    status = sel.getDagPath(0, dagPath);
    if (!status)
    {
        std::cerr << "ExportSkeleton: failed to get DAG path for selection" << std::endl;
        return MS::kFailure;
    }

    std::string skeletonName;
    if (!performSingleSkeletonExport(bindPoseFrame, dagPath, skeletonName, targetPath.empty() ? std::string() : targetPath))
        return MS::kFailure;

    MGlobal::displayInfo(MString("Exported skeleton: ") + skeletonName.c_str());
    return MS::kSuccess;
}

bool ExportSkeleton::performSingleSkeletonExport(int bindPoseFrameNumber, const MDagPath& targetDagPath, std::string& skeletonName, const std::string& outputPathOverride)
{
    MStatus status;

    MFnDagNode fnDag(targetDagPath, &status);
    if (!status)
    {
        std::cerr << "ExportSkeleton: failed to get DAG node" << std::endl;
        return false;
    }
    MString rootName = fnDag.name(&status);
    if (!status)
        return false;

    MayaCompoundString compoundName(rootName);
    if (compoundName.getComponentCount() > 1)
        skeletonName = compoundName.getComponentStdString(1);
    else
        skeletonName = rootName.asChar();

    const auto dot = skeletonName.find_last_of('.');
    if (dot != std::string::npos)
        skeletonName = skeletonName.substr(0, dot);

    if (!MayaUtility::goToBindPose(bindPoseFrameNumber))
    {
        std::cerr << "ExportSkeleton: failed to go to bind pose" << std::endl;
        return false;
    }

    std::string outputPath;
    if (!outputPathOverride.empty())
    {
        outputPath = outputPathOverride;
        if (outputPath.find('.') == std::string::npos)
            outputPath += ".skt";
    }
    else
    {
        outputPath = SetDirectoryCommand::getDirectoryString(SKELETON_TEMPLATE_WRITE_DIR_INDEX);
        if (outputPath.empty())
        {
            std::cerr << "ExportSkeleton: skeleton output directory not configured" << std::endl;
            return false;
        }
        if (outputPath.back() != '\\' && outputPath.back() != '/')
            outputPath += '\\';
        outputPath += skeletonName + ".skt";
    }

    SkeletonTemplateWriter writer;
    const int firstParentIndex = -1;
    if (!addMayaJoint(writer, targetDagPath, firstParentIndex))
    {
        std::cerr << "ExportSkeleton: failed to add joints" << std::endl;
        MayaUtility::enableDeformers();
        return false;
    }

    Iff iff(DEFAULT_IFF_SIZE, true);
    writer.write(iff);

    if (!iff.write(outputPath.c_str(), true))
    {
        std::cerr << "ExportSkeleton: failed to write " << outputPath << std::endl;
        MayaUtility::enableDeformers();
        return false;
    }

    MayaUtility::enableDeformers();
    return true;
}

bool ExportSkeleton::addMayaJoint(SkeletonTemplateWriter& writer, const MDagPath& targetDagPath, int parentIndex)
{
    MStatus status;
    const int nodeType = targetDagPath.apiType(&status);
    if (!status)
        return false;

    int jointIndex = parentIndex;
    MString jointName;

    if (MayaUtility::ignoreNode(targetDagPath))
    {
        jointIndex = parentIndex;
        jointName = "<ignored joint>";
    }
    else
    {
        if (nodeType != MFn::kJoint && nodeType != MFn::kTransform)
            return true;

        if (nodeType == MFn::kTransform)
        {
            if (!MayaUtility::hasNodeTypeInHierarchy(targetDagPath, MFn::kJoint) &&
                !MayaUtility::hasNodeTypeInHierarchy(targetDagPath, MFn::kMesh))
                return true;
        }

        MFnTransform fnTransform(targetDagPath, &status);
        if (!status)
        {
            std::cerr << "ExportSkeleton: failed MFnTransform for " << targetDagPath.partialPathName().asChar() << std::endl;
            return false;
        }

        jointName = fnTransform.name(&status);
        if (!status)
            return false;

        MayaCompoundString compoundName(jointName);
        const bool hasFilename = compoundName.getComponentCount() > 1;
        const bool isNewSkeletonTemplate = hasFilename && writer.getJointCount() != 0;

        if (isNewSkeletonTemplate)
            return true;

        if (hasFilename)
            jointName = compoundName.getComponentString(0);

        MQuaternion mayaRotateAxisQuat = fnTransform.rotateOrientation(MSpace::kTransform, &status);
        if (!status)
            return false;
        MEulerRotation mayaRotateAxisEuler = mayaRotateAxisQuat.asEulerRotation();
        Quaternion preMultiplyRotation = MayaConversions::convertRotation(mayaRotateAxisEuler);

        Quaternion postMultiplyRotation;
        if (nodeType == MFn::kJoint)
        {
            MFnIkJoint fnJoint(targetDagPath, &status);
            if (!status)
                return false;
            MEulerRotation mayaJointOrientation;
            status = fnJoint.getOrientation(mayaJointOrientation);
            if (!status)
                return false;
            postMultiplyRotation = MayaConversions::convertRotation(mayaJointOrientation);
        }
        else
        {
            postMultiplyRotation = Quaternion::identity;
        }

        MVector mayaJointTranslation = fnTransform.translation(MSpace::kTransform, &status);
        if (!status)
            return false;
        Vector bindPoseTranslation = MayaConversions::convertVector(mayaJointTranslation);

        MEulerRotation mayaJointRotation;
        status = fnTransform.getRotation(mayaJointRotation);
        if (!status)
            return false;
        Quaternion bindPoseRotation = MayaConversions::convertRotation(mayaJointRotation);

        SkeletonTemplateWriter::JointRotationOrder jointRotationOrder = SkeletonTemplateWriter::JRO_COUNT;
        switch (mayaJointRotation.order)
        {
        case MEulerRotation::kXYZ: jointRotationOrder = SkeletonTemplateWriter::JRO_xyz; break;
        case MEulerRotation::kYZX: jointRotationOrder = SkeletonTemplateWriter::JRO_yzx; break;
        case MEulerRotation::kZXY: jointRotationOrder = SkeletonTemplateWriter::JRO_zxy; break;
        case MEulerRotation::kXZY: jointRotationOrder = SkeletonTemplateWriter::JRO_xzy; break;
        case MEulerRotation::kYXZ: jointRotationOrder = SkeletonTemplateWriter::JRO_yxz; break;
        case MEulerRotation::kZYX: jointRotationOrder = SkeletonTemplateWriter::JRO_zyx; break;
        default:
            jointRotationOrder = SkeletonTemplateWriter::JRO_xyz;
        }

        jointIndex = -1;
        if (!writer.addJoint(parentIndex, jointName.asChar(), preMultiplyRotation, postMultiplyRotation,
                            bindPoseTranslation, bindPoseRotation, jointRotationOrder, &jointIndex))
        {
            std::cerr << "ExportSkeleton: failed to add joint " << jointName.asChar() << std::endl;
            return false;
        }
    }

    const unsigned childCount = targetDagPath.childCount(&status);
    if (!status)
        return false;

    for (unsigned i = 0; i < childCount; ++i)
    {
        MObject childObject = targetDagPath.child(i, &status);
        if (!status)
            continue;

        const int apiType = childObject.apiType();
        if (apiType != MFn::kJoint && apiType != MFn::kTransform)
            continue;

        MFnDagNode fnChild(childObject, &status);
        if (!status)
            continue;

        MDagPath childDagPath;
        if (!fnChild.getPath(childDagPath))
            continue;

        if (!addMayaJoint(writer, childDagPath, jointIndex))
            return false;
    }

    return true;
}
