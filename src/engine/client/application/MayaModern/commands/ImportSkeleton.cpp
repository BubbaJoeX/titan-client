#include "ImportSkeleton.h"
#include "ImportPathResolver.h"
#include "MayaSceneBuilder.h"

#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>

#include <string>
#include <vector>
#include <cstdio>

namespace
{
    static const Tag TAG_SKTM = TAG(S,K,T,M);
    static const Tag TAG_SLOD = TAG(S,L,O,D);
    static const Tag TAG_PRNT = TAG(P,R,N,T);
    static const Tag TAG_RPRE = TAG(R,P,R,E);
    static const Tag TAG_RPST = TAG(R,P,S,T);
    static const Tag TAG_BPTR = TAG(B,P,T,R);
    static const Tag TAG_BPRO = TAG(B,P,R,O);
    static const Tag TAG_JROR = TAG(J,R,O,R);
    static const Tag TAG_V000 = TAG(0,0,0,0);
    static const Tag TAG_V001 = TAG(0,0,0,1);
    static const Tag TAG_V002 = TAG(0,0,0,2);

    bool readSingleSkeleton(Iff& iff, std::vector<MayaSceneBuilder::JointData>& joints)
    {
        iff.enterForm(TAG_SKTM);

        const Tag versionTag = iff.getCurrentName();
        const bool isV1 = (versionTag == TAG_V001);
        const bool isV2 = (versionTag == TAG_V002);

        if (!isV1 && !isV2)
        {
            std::cerr << "ImportSkeleton: unsupported SKTM version" << std::endl;
            return false;
        }

        iff.enterForm(versionTag);

        iff.enterChunk(TAG_INFO);
        const int32 jointCount = iff.read_int32();
        iff.exitChunk(TAG_INFO);

        joints.resize(static_cast<size_t>(jointCount));

        iff.enterChunk(TAG_NAME);
        for (int i = 0; i < jointCount; ++i)
            joints[static_cast<size_t>(i)].name = iff.read_stdstring();
        iff.exitChunk(TAG_NAME);

        iff.enterChunk(TAG_PRNT);
        for (int i = 0; i < jointCount; ++i)
            joints[static_cast<size_t>(i)].parentIndex = iff.read_int32();
        iff.exitChunk(TAG_PRNT);

        iff.enterChunk(TAG_RPRE);
        for (int i = 0; i < jointCount; ++i)
        {
            MayaSceneBuilder::JointData& jd = joints[static_cast<size_t>(i)];
            const float w = iff.read_float();
            const float x = iff.read_float();
            const float y = iff.read_float();
            const float z = iff.read_float();
            jd.preRotation[0] = x; jd.preRotation[1] = y; jd.preRotation[2] = z; jd.preRotation[3] = w;
        }
        iff.exitChunk(TAG_RPRE);

        iff.enterChunk(TAG_RPST);
        for (int i = 0; i < jointCount; ++i)
        {
            MayaSceneBuilder::JointData& jd = joints[static_cast<size_t>(i)];
            const float w = iff.read_float();
            const float x = iff.read_float();
            const float y = iff.read_float();
            const float z = iff.read_float();
            jd.postRotation[0] = x; jd.postRotation[1] = y; jd.postRotation[2] = z; jd.postRotation[3] = w;
        }
        iff.exitChunk(TAG_RPST);

        iff.enterChunk(TAG_BPTR);
        for (int i = 0; i < jointCount; ++i)
        {
            MayaSceneBuilder::JointData& jd = joints[static_cast<size_t>(i)];
            jd.bindTranslation[0] = iff.read_float();
            jd.bindTranslation[1] = iff.read_float();
            jd.bindTranslation[2] = iff.read_float();
        }
        iff.exitChunk(TAG_BPTR);

        iff.enterChunk(TAG_BPRO);
        for (int i = 0; i < jointCount; ++i)
        {
            MayaSceneBuilder::JointData& jd = joints[static_cast<size_t>(i)];
            const float w = iff.read_float();
            const float x = iff.read_float();
            const float y = iff.read_float();
            const float z = iff.read_float();
            jd.bindRotation[0] = x; jd.bindRotation[1] = y; jd.bindRotation[2] = z; jd.bindRotation[3] = w;
        }
        iff.exitChunk(TAG_BPRO);

        if (isV1)
        {
            if (!iff.atEndOfForm())
            {
                iff.enterChunk();
                iff.exitChunk();
            }
            for (int i = 0; i < jointCount; ++i)
                joints[static_cast<size_t>(i)].rotationOrder = 0;
        }
        else
        {
            iff.enterChunk(TAG_JROR);
            for (int i = 0; i < jointCount; ++i)
                joints[static_cast<size_t>(i)].rotationOrder = static_cast<int>(iff.read_uint32());
            iff.exitChunk(TAG_JROR);
        }

        iff.exitForm(versionTag);
        iff.exitForm(TAG_SKTM);
        return true;
    }
}

void* ImportSkeleton::creator()
{
    return new ImportSkeleton();
}

MStatus ImportSkeleton::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;
    std::string parentPath;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if (argName == "-i")
        {
            MString argValue = args.asString(i + 1, &status);
            if (!status)
            {
                std::cerr << "ImportSkeleton: missing filename after -i" << std::endl;
                return MS::kFailure;
            }
            filename = argValue.asChar();
            ++i;
        }
        else if (argName == "-parent" && (i + 1) < argCount)
        {
            parentPath = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportSkeleton: missing -i <filename> argument" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);

    MObject parentObj = MObject::kNullObj;
    if (!parentPath.empty())
    {
        MSelectionList sel;
        status = MGlobal::getSelectionListByName(MString(parentPath.c_str()), sel);
        if (status && sel.length() > 0)
        {
            MDagPath dagPath;
            sel.getDagPath(0, dagPath);
            parentObj = dagPath.node();
        }
        else
        {
            std::cerr << "ImportSkeleton: parent path not found: " << parentPath << std::endl;
        }
    }

    Iff iff;
    if (!iff.open(filename.c_str(), false))
    {
        std::cerr << "ImportSkeleton: failed to open " << filename << std::endl;
        return MS::kFailure;
    }

    const Tag topTag = iff.getCurrentName();

    if (topTag == TAG_SKTM)
    {
        std::vector<MayaSceneBuilder::JointData> joints;
        if (!readSingleSkeleton(iff, joints))
            return MS::kFailure;

        std::string rootName = filename;
        const auto lastSlash = rootName.find_last_of("\\/");
        if (lastSlash != std::string::npos)
            rootName = rootName.substr(lastSlash + 1);
        const auto dot = rootName.find_last_of('.');
        if (dot != std::string::npos)
            rootName = rootName.substr(0, dot);

        std::vector<MDagPath> outJointPaths;
        status = MayaSceneBuilder::createJointHierarchy(joints, rootName, outJointPaths, parentObj);
        if (!status)
        {
            std::cerr << "ImportSkeleton: failed to create joint hierarchy" << std::endl;
            return MS::kFailure;
        }
    }
    else if (topTag == TAG_SLOD)
    {
        iff.enterForm(TAG_SLOD);
        iff.enterForm(TAG_V000);

        iff.enterChunk(TAG_INFO);
        const int16 levelCount = iff.read_int16();
        iff.exitChunk(TAG_INFO);

        std::string baseName = filename;
        const auto lastSlash = baseName.find_last_of("\\/");
        if (lastSlash != std::string::npos)
            baseName = baseName.substr(lastSlash + 1);
        const auto dot = baseName.find_last_of('.');
        if (dot != std::string::npos)
            baseName = baseName.substr(0, dot);

        MObject lodGroupParent;
        {
            MString melCmd = "group -em -n \"";
            melCmd += baseName.c_str();
            melCmd += "\"";
            MStringArray result;
            status = MGlobal::executeCommand(melCmd, result);
            if (!status)
            {
                std::cerr << "ImportSkeleton: failed to create LOD group" << std::endl;
                return MS::kFailure;
            }

            melCmd = "createNode lodGroup -p \"";
            melCmd += result[0];
            melCmd += "\"";
            MGlobal::executeCommand(melCmd);

            MSelectionList sel;
            sel.add(result[0]);
            MObject lodGroupObj;
            sel.getDependNode(0, lodGroupObj);
            lodGroupParent = lodGroupObj;
        }

        for (int16 level = 0; level < levelCount; ++level)
        {
            std::vector<MayaSceneBuilder::JointData> joints;
            if (!readSingleSkeleton(iff, joints))
                return MS::kFailure;

            if (level > 0)
                continue;

            char lodLevelName[32];
            sprintf(lodLevelName, "l%d", static_cast<int>(level));
            MString melCmd = "createNode transform -n \"";
            melCmd += lodLevelName;
            melCmd += "\" -p \"";
            melCmd += baseName.c_str();
            melCmd += "\"";
            MStringArray lodResult;
            status = MGlobal::executeCommand(melCmd, lodResult);
            if (!status)
            {
                std::cerr << "ImportSkeleton: failed to create LOD level " << level << std::endl;
                return MS::kFailure;
            }

            MSelectionList lodSel;
            lodSel.add(lodResult[0]);
            MObject lodLevelObj;
            lodSel.getDependNode(0, lodLevelObj);

            char levelSuffix[32];
            sprintf(levelSuffix, "_l%d", static_cast<int>(level));
            std::string rootName = baseName + levelSuffix;

            std::vector<MDagPath> outJointPaths;
            status = MayaSceneBuilder::createJointHierarchy(joints, rootName, outJointPaths, lodLevelObj);
            if (!status)
            {
                std::cerr << "ImportSkeleton: failed to create joint hierarchy for LOD level " << level << std::endl;
                return MS::kFailure;
            }
        }

        iff.exitForm(TAG_V000);
        iff.exitForm(TAG_SLOD);
    }
    else
    {
        std::cerr << "ImportSkeleton: unrecognized top-level form, expected SKTM or SLOD" << std::endl;
        return MS::kFailure;
    }

    return MS::kSuccess;
}
