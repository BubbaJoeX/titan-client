#include "ImportSkeletalMesh.h"
#include "ImportPathResolver.h"
#include "ImportLodMesh.h"
#include "MayaSceneBuilder.h"
#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>

#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <iostream>

namespace
{
    const Tag TAG_BLT  = TAG3(B,L,T);
    const Tag TAG_BLTS = TAG(B,L,T,S);
    const Tag TAG_DOT3 = TAG(D,O,T,3);
    const Tag TAG_DYN  = TAG3(D,Y,N);
    const Tag TAG_FOZC = TAG(F,O,Z,C);
    const Tag TAG_HPTS = TAG(H,P,T,S);
    const Tag TAG_ITL  = TAG3(I,T,L);
    const Tag TAG_NIDX = TAG(N,I,D,X);
    const Tag TAG_NORM = TAG(N,O,R,M);
    const Tag TAG_OITL = TAG(O,I,T,L);
    const Tag TAG_OZC  = TAG3(O,Z,C);
    const Tag TAG_OZN  = TAG3(O,Z,N);
    const Tag TAG_PIDX = TAG(P,I,D,X);
    const Tag TAG_POSN = TAG(P,O,S,N);
    const Tag TAG_PRIM = TAG(P,R,I,M);
    const Tag TAG_PSDT = TAG(P,S,D,T);
    const Tag TAG_SKMG = TAG(S,K,M,G);
    const Tag TAG_SKTM = TAG(S,K,T,M);
    const Tag TAG_STAT = TAG(S,T,A,T);
    const Tag TAG_TCSD = TAG(T,C,S,D);
    const Tag TAG_TCSF = TAG(T,C,S,F);
    const Tag TAG_TRTS = TAG(T,R,T,S);
    const Tag TAG_TXCI = TAG(T,X,C,I);
    const Tag TAG_TWDT = TAG(T,W,D,T);
    const Tag TAG_TWHD = TAG(T,W,H,D);
    const Tag TAG_VDCL = TAG(V,D,C,L);
    const Tag TAG_XFNM = TAG(X,F,N,M);
    const Tag TAG_ZTO  = TAG3(Z,T,O);

    static std::string getJointShortName(const std::string& mayaJointName)
    {
        // SWG joints use "name__skeleton" format; short name is before "__"
        size_t dbl = mayaJointName.find("__");
        if (dbl != std::string::npos && dbl > 0)
            return mayaJointName.substr(0, dbl);
        return mayaJointName;
    }

    // MayaExporter MayaCompoundString::getComponentString(0) - first segment before '_'
    // Matches "lThigh_all_b_l0" -> "lThigh", "root__all_b" -> "root"
    static std::string getComponentString0(const std::string& mayaJointName)
    {
        size_t u = mayaJointName.find('_');
        if (u != std::string::npos && u > 0)
            return mayaJointName.substr(0, u);
        return mayaJointName;
    }

    static std::string stripUnderscores(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
            if (c != '_')
                out += c;
        return out;
    }

    // Maya namespaced joints (e.g. "lom:lThigh") must match MGN transform names ("lThigh")
    static std::string stripNamespace(const std::string& mayaName)
    {
        const size_t colon = mayaName.rfind(':');
        if (colon != std::string::npos && colon + 1 < mayaName.size())
            return mayaName.substr(colon + 1);
        return mayaName;
    }

    static void addJointToMap(std::map<std::string, MDagPath>& jointMap,
        const std::string& key, const MDagPath& dagPath)
    {
        if (!key.empty() && jointMap.find(key) == jointMap.end())
            jointMap[key] = dagPath;
    }

    static std::string toLower(const std::string& s)
    {
        std::string out = s;
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
        return out;
    }

    static bool findJoint(const std::map<std::string, MDagPath>& jointMap,
        const std::string& key, std::map<std::string, MDagPath>::const_iterator& out)
    {
        out = jointMap.find(key);
        if (out != jointMap.end()) return true;
        out = jointMap.find(toLower(key));
        return out != jointMap.end();
    }

    static std::string pathBasenameNoExt(const std::string& p)
    {
        std::string s = p;
        const size_t ls = s.find_last_of("/\\");
        if (ls != std::string::npos)
            s = s.substr(ls + 1);
        const size_t dot = s.find_last_of('.');
        if (dot != std::string::npos)
            s = s.substr(0, dot);
        return s;
    }

    /** SWG LOD joint suffix on imported bindings, e.g. root_jointname_l0 */
    static std::string stripTrailingLodSuffix(const std::string& s)
    {
        size_t i = s.size();
        while (i > 0 && std::isdigit(static_cast<unsigned char>(s[i - 1])))
            --i;
        if (i >= 2 && s[i - 1] == 'l' && s[i - 2] == '_')
            return s.substr(0, i - 2);
        return s;
    }

    /**
     * XFNM uses engine skeleton names (often short, e.g. "root"); Maya root joints are name__filebase_l0 after SLOD import.
     * Try several aliases so SAT→skeletal mesh picks up the rig that was imported just before.
     */
    static bool resolveTransformToJoint(
        const std::string& tn,
        const std::map<std::string, MDagPath>& jointMap,
        const std::vector<std::string>& skelBases,
        std::map<std::string, MDagPath>::const_iterator& out)
    {
        if (tn.empty())
            return false;
        if (findJoint(jointMap, tn, out))
            return true;
        if (findJoint(jointMap, stripUnderscores(tn), out))
            return true;
        if (findJoint(jointMap, getJointShortName(tn), out))
            return true;
        if (findJoint(jointMap, getComponentString0(tn), out))
            return true;
        if (findJoint(jointMap, getComponentString0(stripNamespace(tn)), out))
            return true;

        std::string lod1 = stripTrailingLodSuffix(tn);
        if (lod1 != tn)
        {
            if (findJoint(jointMap, lod1, out) ||
                findJoint(jointMap, getJointShortName(lod1), out) ||
                findJoint(jointMap, getComponentString0(lod1), out))
                return true;
            std::string lod2 = stripTrailingLodSuffix(lod1);
            if (lod2 != lod1 &&
                (findJoint(jointMap, lod2, out) ||
                    findJoint(jointMap, getJointShortName(lod2), out) ||
                    findJoint(jointMap, getComponentString0(lod2), out)))
                return true;
        }

        for (const std::string& base : skelBases)
        {
            if (base.empty())
                continue;
            const std::string c0 = tn + std::string("__") + base;
            if (findJoint(jointMap, c0, out))
                return true;
            const std::string c1 = tn + std::string("__") + base + "_l0";
            if (findJoint(jointMap, c1, out))
                return true;
            const std::string c2 = tn + std::string("__") + base + "_l1";
            if (findJoint(jointMap, c2, out))
                return true;
        }
        return false;
    }
}

void* ImportSkeletalMesh::creator()
{
    return new ImportSkeletalMesh();
}

MStatus ImportSkeletalMesh::doIt(const MArgList& args)
{
    MStatus status;
    std::string inputFilename;
    std::string skeletonFilename;
    std::string parentPath;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if ((argName == "-i" || argName == "-input") && (i + 1) < argCount)
        {
            inputFilename = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if (argName == "-skeleton" && (i + 1) < argCount)
        {
            skeletonFilename = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if ((argName == "-parent" || argName == "-p") && (i + 1) < argCount)
        {
            parentPath = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (inputFilename.empty())
    {
        std::cerr << "ImportSkeletalMesh: missing -i <filename>" << std::endl;
        return MS::kFailure;
    }

    {
        const std::string rawInput = inputFilename;
        inputFilename = resolveImportPath(inputFilename);
        inputFilename = resolveSkmgPathThroughWrappers(inputFilename);
        MGlobal::displayInfo(MString("ImportSkeletalMesh: input=") + rawInput.c_str()
            + " -> resolved=" + inputFilename.c_str());
    }
    if (!skeletonFilename.empty())
        skeletonFilename = resolveImportPath(skeletonFilename);

    Iff iff;
    if (!iff.open(inputFilename.c_str(), false))
    {
        MGlobal::displayError(MString("ImportSkeletalMesh: failed to open: ") + inputFilename.c_str());
        std::cerr << "ImportSkeletalMesh: failed to open " << inputFilename << std::endl;
        return MS::kFailure;
    }

    const Tag topFormTag = iff.getCurrentName();
    if (topFormTag != TAG_SKMG)
    {
        char topStr[5]; ConvertTagToString(topFormTag, topStr);
        MGlobal::displayError(MString("ImportSkeletalMesh: expected FORM SKMG but found FORM ") + topStr
            + " in " + inputFilename.c_str());
        std::cerr << "ImportSkeletalMesh: expected FORM SKMG but found FORM " << topStr << std::endl;
        return MS::kFailure;
    }

    iff.enterForm(TAG_SKMG);
    const Tag versionTag = iff.getCurrentName();
    if (versionTag != TAG_0002 && versionTag != TAG_0003 && versionTag != TAG_0004)
    {
        char formName[5];
        ConvertTagToString(versionTag, formName);
        MGlobal::displayError(
            MString("ImportSkeletalMesh: unsupported SKMG version ") + formName
            + " (expected 0002, 0003, or 0004). File may be corrupt or a newer format.");
        std::cerr << "ImportSkeletalMesh: unsupported SKMG version " << formName << std::endl;
        iff.exitForm(TAG_SKMG);
        return MS::kFailure;
    }
    iff.enterForm(versionTag);

    int32 maxTransformsPerVertex   = 0;
    int32 maxTransformsPerShader   = 0;
    int32 skeletonTemplateCount    = 0;
    int32 transformNameCount      = 0;
    int32 positionCount           = 0;
    int32 transformWeightDataCount = 0;
    int32 normalCount             = 0;
    int32 perShaderDataCount      = 0;
    int32 blendTargetCount        = 0;
    int16 occlusionZoneNameCount  = 0;
    int16 occlusionZoneCombCount  = 0;
    int16 zonesThisOccludesCount   = 0;
    int16 occlusionLayer          = 0;

    iff.enterChunk(TAG_INFO);
    maxTransformsPerVertex   = iff.read_int32();
    maxTransformsPerShader   = iff.read_int32();
    skeletonTemplateCount    = iff.read_int32();
    transformNameCount      = iff.read_int32();
    positionCount           = iff.read_int32();
    transformWeightDataCount = iff.read_int32();
    normalCount             = iff.read_int32();
    perShaderDataCount      = iff.read_int32();
    blendTargetCount        = iff.read_int32();
    occlusionZoneNameCount  = iff.read_int16();
    occlusionZoneCombCount  = iff.read_int16();
    zonesThisOccludesCount  = iff.read_int16();
    occlusionLayer          = iff.read_int16();
    iff.exitChunk(TAG_INFO);

    {
        MString msg = "ImportSkeletalMesh: INFO pos=";
        msg += static_cast<int>(positionCount);
        msg += " norm="; msg += static_cast<int>(normalCount);
        msg += " xfnm="; msg += static_cast<int>(transformNameCount);
        msg += " shaders="; msg += static_cast<int>(perShaderDataCount);
        msg += " blends="; msg += static_cast<int>(blendTargetCount);
        msg += " ozn="; msg += static_cast<int>(occlusionZoneNameCount);
        msg += " ozc="; msg += static_cast<int>(occlusionZoneCombCount);
        msg += " zto="; msg += static_cast<int>(zonesThisOccludesCount);
        MGlobal::displayInfo(msg);
    }

    std::vector<std::string> skeletonTemplateNames;
    iff.enterChunk(TAG_SKTM);
    for (int32 i = 0; i < skeletonTemplateCount; ++i)
        skeletonTemplateNames.push_back(iff.read_stdstring());
    iff.exitChunk(TAG_SKTM);

    std::vector<std::string> transformNames;
    iff.enterChunk(TAG_XFNM);
    for (int32 i = 0; i < transformNameCount; ++i)
        transformNames.push_back(iff.read_stdstring());
    iff.exitChunk(TAG_XFNM);

    std::vector<float> positions;
    positions.reserve(static_cast<size_t>(positionCount) * 3);
    iff.enterChunk(TAG_POSN);
    for (int32 i = 0; i < positionCount; ++i)
    {
        positions.push_back(iff.read_float());
        positions.push_back(iff.read_float());
        positions.push_back(iff.read_float());
    }
    iff.exitChunk(TAG_POSN);

    std::vector<int> weightHeaders;
    weightHeaders.reserve(static_cast<size_t>(positionCount));
    iff.enterChunk(TAG_TWHD);
    for (int32 i = 0; i < positionCount; ++i)
        weightHeaders.push_back(static_cast<int>(iff.read_int32()));
    iff.exitChunk(TAG_TWHD);

    std::vector<MayaSceneBuilder::SkinWeight> weightData;
    weightData.reserve(static_cast<size_t>(transformWeightDataCount));
    iff.enterChunk(TAG_TWDT);
    for (int32 i = 0; i < transformWeightDataCount; ++i)
    {
        MayaSceneBuilder::SkinWeight sw;
        sw.transformIndex = static_cast<int>(iff.read_int32());
        sw.weight        = iff.read_float();
        weightData.push_back(sw);
    }
    iff.exitChunk(TAG_TWDT);

    std::vector<float> normals;
    normals.reserve(static_cast<size_t>(normalCount) * 3);
    iff.enterChunk(TAG_NORM);
    for (int32 i = 0; i < normalCount; ++i)
    {
        normals.push_back(iff.read_float());
        normals.push_back(iff.read_float());
        normals.push_back(iff.read_float());
    }
    iff.exitChunk(TAG_NORM);

    if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DOT3)
    {
        iff.enterChunk(TAG_DOT3);
        iff.exitChunk(TAG_DOT3);
    }

    std::vector<MayaSceneBuilder::HardpointData> staticHardpoints;
    std::vector<MayaSceneBuilder::HardpointData> dynamicHardpoints;
    std::vector<MayaSceneBuilder::BlendTargetData> blendTargets;

    if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_HPTS)
    {
        iff.enterForm(TAG_HPTS);
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            const Tag chunkTag = iff.getCurrentName();
            if (chunkTag == TAG_STAT)
            {
                iff.enterChunk(TAG_STAT);
                const int16 count = iff.read_int16();
                for (int i = 0; i < static_cast<int>(count); ++i)
                {
                    MayaSceneBuilder::HardpointData hp;
                    iff.read_string(hp.name);
                    iff.read_string(hp.parentJoint);
                    const float qw = iff.read_float();
                    const float qx = iff.read_float();
                    const float qy = iff.read_float();
                    const float qz = iff.read_float();
                    hp.rotation[0] = qx;
                    hp.rotation[1] = qy;
                    hp.rotation[2] = qz;
                    hp.rotation[3] = qw;
                    hp.position[0] = iff.read_float();
                    hp.position[1] = iff.read_float();
                    hp.position[2] = iff.read_float();
                    staticHardpoints.push_back(hp);
                }
                iff.exitChunk(TAG_STAT);
            }
            else if (chunkTag == TAG_DYN)
            {
                iff.enterChunk(TAG_DYN);
                const int16 count = iff.read_int16();
                for (int i = 0; i < static_cast<int>(count); ++i)
                {
                    MayaSceneBuilder::HardpointData hp;
                    iff.read_string(hp.name);
                    iff.read_string(hp.parentJoint);
                    const float qw = iff.read_float();
                    const float qx = iff.read_float();
                    const float qy = iff.read_float();
                    const float qz = iff.read_float();
                    hp.rotation[0] = qx;
                    hp.rotation[1] = qy;
                    hp.rotation[2] = qz;
                    hp.rotation[3] = qw;
                    hp.position[0] = iff.read_float();
                    hp.position[1] = iff.read_float();
                    hp.position[2] = iff.read_float();
                    dynamicHardpoints.push_back(hp);
                }
                iff.exitChunk(TAG_DYN);
            }
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
        iff.exitForm(TAG_HPTS);
    }

    if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_BLTS)
    {
        iff.enterForm(TAG_BLTS);
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            iff.enterForm(TAG_BLT);
            MayaSceneBuilder::BlendTargetData bt;
            iff.enterChunk(TAG_INFO);
            const int32 btPosCount = iff.read_int32();
            const int32 btNormCount = iff.read_int32();
            bt.name = iff.read_stdstring();
            bt.positionIndices.reserve(static_cast<size_t>(btPosCount));
            bt.positionDeltas.reserve(static_cast<size_t>(btPosCount) * 3);
            bt.normalIndices.reserve(static_cast<size_t>(btNormCount));
            bt.normalDeltas.reserve(static_cast<size_t>(btNormCount) * 3);
            iff.exitChunk(TAG_INFO);

            if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_POSN)
            {
                iff.enterChunk(TAG_POSN);
                while (iff.getChunkLengthLeft(4) > 0)
                {
                    const int32 idx = iff.read_int32();
                    const float dx = iff.read_float();
                    const float dy = iff.read_float();
                    const float dz = iff.read_float();
                    bt.positionIndices.push_back(static_cast<int>(idx));
                    bt.positionDeltas.push_back(dx);
                    bt.positionDeltas.push_back(dy);
                    bt.positionDeltas.push_back(dz);
                }
                iff.exitChunk(TAG_POSN);
            }

            if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_NORM)
            {
                iff.enterChunk(TAG_NORM);
                while (iff.getChunkLengthLeft(4) > 0)
                {
                    const int32 idx = iff.read_int32();
                    const float dx = iff.read_float();
                    const float dy = iff.read_float();
                    const float dz = iff.read_float();
                    bt.normalIndices.push_back(static_cast<int>(idx));
                    bt.normalDeltas.push_back(dx);
                    bt.normalDeltas.push_back(dy);
                    bt.normalDeltas.push_back(dz);
                }
                iff.exitChunk(TAG_NORM);
            }

            while (iff.getNumberOfBlocksLeft() > 0)
            {
                if (iff.isCurrentForm())
                {
                    iff.enterForm();
                    iff.exitForm();
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            blendTargets.push_back(bt);
            iff.exitForm(TAG_BLT);
        }
        iff.exitForm(TAG_BLTS);
    }

    // Occlusion data — match SkeletalMeshGeneratorTemplate::load_0002 / load_0004 (between BLTS and PSDT).
    if (occlusionZoneNameCount > 0)
    {
        iff.enterChunk(TAG_OZN);
        for (int16 i = 0; i < occlusionZoneNameCount; ++i)
            (void)iff.read_stdstring();
        iff.exitChunk(TAG_OZN);
    }
    if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_FOZC)
    {
        iff.enterChunk(TAG_FOZC);
        const int foCount = static_cast<int>(iff.read_uint16());
        for (int i = 0; i < foCount; ++i)
            (void)iff.read_int16();
        iff.exitChunk(TAG_FOZC);
    }
    if (occlusionZoneCombCount > 0)
    {
        iff.enterChunk(TAG_OZC);
        for (int16 i = 0; i < occlusionZoneCombCount; ++i)
        {
            const int16 combN = iff.read_int16();
            for (int16 j = 0; j < combN; ++j)
                (void)iff.read_int16();
        }
        iff.exitChunk(TAG_OZC);
    }
    if (zonesThisOccludesCount > 0)
    {
        iff.enterChunk(TAG_ZTO);
        for (int16 i = 0; i < zonesThisOccludesCount; ++i)
            (void)iff.read_int16();
        iff.exitChunk(TAG_ZTO);
    }
    while (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm())
    {
        iff.enterChunk();
        iff.exitChunk();
    }
    while (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() != TAG_PSDT)
    {
        if (iff.getCurrentName() == TAG_TRTS)
        {
            iff.enterForm(TAG_TRTS);
            iff.exitForm(TAG_TRTS);
            continue;
        }
        iff.enterForm();
        while (!iff.atEndOfForm())
        {
            if (iff.isCurrentForm())
            {
                iff.enterForm();
                iff.exitForm();
            }
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
        iff.exitForm();
    }

    std::vector<MayaSceneBuilder::ShaderGroupData> shaderGroups;

    for (int32 shaderIndex = 0; shaderIndex < perShaderDataCount; ++shaderIndex)
    {
        iff.enterForm(TAG_PSDT);

        MayaSceneBuilder::ShaderGroupData sg;

        iff.enterChunk(TAG_NAME);
        sg.shaderTemplateName = iff.read_stdstring();
        iff.exitChunk(TAG_NAME);

        int32 shaderVertexCount = 0;
        iff.enterChunk(TAG_PIDX);
        shaderVertexCount = iff.read_int32();
        sg.positionIndices.reserve(static_cast<size_t>(shaderVertexCount));
        for (int32 i = 0; i < shaderVertexCount; ++i)
            sg.positionIndices.push_back(static_cast<int>(iff.read_int32()));
        iff.exitChunk(TAG_PIDX);

        if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_NIDX)
        {
            iff.enterChunk(TAG_NIDX);
            sg.normalIndices.reserve(static_cast<size_t>(shaderVertexCount));
            for (int32 i = 0; i < shaderVertexCount; ++i)
                sg.normalIndices.push_back(static_cast<int>(iff.read_int32()));
            iff.exitChunk(TAG_NIDX);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DOT3)
        {
            iff.enterChunk(TAG_DOT3);
            iff.exitChunk(TAG_DOT3);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_VDCL)
        {
            iff.enterChunk(TAG_VDCL);
            iff.exitChunk(TAG_VDCL);
        }

        int32 texCoordSetCount = 0;
        std::vector<int32> texCoordDimensionality;

        if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_TXCI)
        {
            iff.enterChunk(TAG_TXCI);
            texCoordSetCount = iff.read_int32();
            texCoordDimensionality.reserve(static_cast<size_t>(texCoordSetCount));
            for (int32 i = 0; i < texCoordSetCount; ++i)
                texCoordDimensionality.push_back(iff.read_int32());
            iff.exitChunk(TAG_TXCI);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TCSF)
        {
            iff.enterForm(TAG_TCSF);
            bool firstSetRead = false;

            for (int32 setIndex = 0; setIndex < texCoordSetCount; ++setIndex)
            {
                iff.enterChunk(TAG_TCSD);
                const int32 dim = (setIndex < static_cast<int32>(texCoordDimensionality.size())) ? texCoordDimensionality[static_cast<size_t>(setIndex)] : 2;

                if (!firstSetRead && dim >= 2)
                {
                    sg.uvs.reserve(static_cast<size_t>(shaderVertexCount));
                    for (int32 vi = 0; vi < shaderVertexCount; ++vi)
                    {
                        MayaSceneBuilder::UVData uv;
                        uv.u = iff.read_float();
                        uv.v = iff.read_float();
                        for (int32 d = 2; d < dim; ++d)
                            iff.read_float();
                        sg.uvs.push_back(uv);
                    }
                    firstSetRead = true;
                }
                else
                {
                    for (int32 vi = 0; vi < shaderVertexCount; ++vi)
                    {
                        for (int32 d = 0; d < dim; ++d)
                            iff.read_float();
                    }
                }
                iff.exitChunk(TAG_TCSD);
            }
            iff.exitForm(TAG_TCSF);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_PRIM)
        {
            iff.enterForm(TAG_PRIM);
            iff.enterChunk(TAG_INFO);
            iff.read_int32();
            iff.exitChunk(TAG_INFO);

            while (iff.getNumberOfBlocksLeft() > 0)
            {
                const Tag primTag = iff.getCurrentName();

                if (primTag == TAG_ITL)
                {
                    iff.enterChunk(TAG_ITL);
                    const int32 triCount = iff.read_int32();
                    for (int32 t = 0; t < triCount; ++t)
                    {
                        MayaSceneBuilder::TriangleData tri;
                        tri.indices[0] = static_cast<int>(iff.read_int32());
                        tri.indices[1] = static_cast<int>(iff.read_int32());
                        tri.indices[2] = static_cast<int>(iff.read_int32());
                        sg.triangles.push_back(tri);
                    }
                    iff.exitChunk(TAG_ITL);
                }
                else if (primTag == TAG_OITL)
                {
                    iff.enterChunk(TAG_OITL);
                    const int32 triCount = iff.read_int32();
                    for (int32 t = 0; t < triCount; ++t)
                    {
                        iff.read_int16();
                        MayaSceneBuilder::TriangleData tri;
                        tri.indices[0] = static_cast<int>(iff.read_int32());
                        tri.indices[1] = static_cast<int>(iff.read_int32());
                        tri.indices[2] = static_cast<int>(iff.read_int32());
                        sg.triangles.push_back(tri);
                    }
                    iff.exitChunk(TAG_OITL);
                }
                else
                {
                    if (iff.isCurrentForm())
                    {
                        iff.enterForm();
                        iff.exitForm();
                    }
                    else
                    {
                        iff.enterChunk();
                        iff.exitChunk();
                    }
                }
            }
            iff.exitForm(TAG_PRIM);
        }

        shaderGroups.push_back(sg);
        iff.exitForm(TAG_PSDT);
    }

    {
        int totalTriangles = 0;
        for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
            totalTriangles += static_cast<int>(shaderGroups[sg].triangles.size());
        if (positionCount <= 0 || totalTriangles <= 0)
        {
            if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TRTS)
            {
                iff.enterForm(TAG_TRTS);
                iff.exitForm(TAG_TRTS);
            }
            iff.exitForm(versionTag);
            iff.exitForm(TAG_SKMG);
            MString err = "ImportSkeletalMesh: no mesh geometry (positions=";
            err += static_cast<int>(positionCount);
            err += ", shaderGroups=";
            err += static_cast<int>(shaderGroups.size());
            err += ", triangles=";
            err += totalTriangles;
            err += "). Parser may be out of sync with this .lmg/.mgn (check Script Editor / stderr).";
            MGlobal::displayError(err);
            std::cerr << "ImportSkeletalMesh: no geometry positions=" << positionCount << " groups=" << shaderGroups.size()
                << " triangles=" << totalTriangles << " perShaderDataCount=" << perShaderDataCount << std::endl;
            return MS::kFailure;
        }
    }

    if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TRTS)
    {
        iff.enterForm(TAG_TRTS);
        iff.exitForm(TAG_TRTS);
    }

    iff.exitForm(versionTag);
    iff.exitForm(TAG_SKMG);

    std::string meshName = inputFilename;
    {
        const size_t lastSlash = meshName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            meshName = meshName.substr(lastSlash + 1);
        const size_t dotPos = meshName.find_last_of('.');
        if (dotPos != std::string::npos)
            meshName = meshName.substr(0, dotPos);
    }

    MDagPath meshPath;
    status = MayaSceneBuilder::createMesh(positions, normals, shaderGroups, meshName, meshPath);
    if (!status)
    {
        std::cerr << "ImportSkeletalMesh: failed to create mesh" << std::endl;
        return status;
    }
    {
        MFnDagNode meshDg(meshPath);
        MGlobal::displayInfo(MString("ImportSkeletalMesh: created ") + meshDg.fullPathName());
    }

    if (!parentPath.empty())
    {
        MSelectionList sel;
        status = MGlobal::getSelectionListByName(MString(parentPath.c_str()), sel);
        if (status && sel.length() > 0)
        {
            MDagPath parentDag;
            sel.getDagPath(0, parentDag);

            MDagPath transformPath = meshPath;
            if (transformPath.hasFn(MFn::kMesh))
                transformPath.pop(1);

            MFnDagNode transformFn(transformPath);
            MString parentCmd = "parent \"";
            parentCmd += transformFn.fullPathName();
            parentCmd += "\" \"";
            parentCmd += parentDag.fullPathName();
            parentCmd += "\"";
            status = MGlobal::executeCommand(parentCmd);
            if (status)
            {
                std::string newPath = parentDag.fullPathName().asChar();
                newPath += "|";
                newPath += transformFn.name().asChar();
                sel.clear();
                sel.add(MString(newPath.c_str()));
                if (sel.length() > 0)
                    sel.getDagPath(0, meshPath);
            }
        }
    }

    status = MayaSceneBuilder::assignMaterials(meshPath, shaderGroups, inputFilename);
    if (!status)
        std::cerr << "ImportSkeletalMesh: failed to assign materials" << std::endl;

    if (!blendTargets.empty())
    {
        status = MayaSceneBuilder::createBlendShapes(meshPath, blendTargets);
        if (!status)
            std::cerr << "ImportSkeletalMesh: failed to create blend shapes" << std::endl;
    }

    const bool needJoints = !transformNames.empty();
    if (needJoints)
    {
        std::vector<MDagPath> jointPaths;
        std::map<std::string, MDagPath> jointMap;

        std::set<std::string> skeletonFilters;
        for (size_t i = 0; i < skeletonTemplateNames.size(); ++i)
        {
            std::string filter = skeletonTemplateNames[i];
            const size_t lastSlash = filter.find_last_of("/\\");
            if (lastSlash != std::string::npos)
                filter = filter.substr(lastSlash + 1);
            const size_t dot = filter.find_last_of('.');
            if (dot != std::string::npos)
                filter = filter.substr(0, dot);
            if (!filter.empty())
                skeletonFilters.insert(filter);
        }

        auto buildJointMap = [&](bool useSkeletonFilter) {
            jointMap.clear();
            MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
            for (; !dagIt.isDone(); dagIt.next())
            {
                MDagPath dagPath;
                dagIt.getPath(dagPath);
                MFnDagNode dagFn(dagPath);
                const std::string mayaName(dagFn.name().asChar());

                if (useSkeletonFilter && !skeletonFilters.empty())
                {
                    MString fullPath = dagPath.fullPathName();
                    std::string pathStr(fullPath.asChar());
                    bool inFilter = false;
                    for (const auto& f : skeletonFilters)
                    {
                        if (pathStr.find(f) != std::string::npos)
                        {
                            inFilter = true;
                            break;
                        }
                    }
                    if (!inFilter)
                        continue;
                }

                addJointToMap(jointMap, mayaName, dagPath);
                const std::string shortName = getJointShortName(mayaName);
                addJointToMap(jointMap, shortName, dagPath);
                addJointToMap(jointMap, toLower(shortName), dagPath);
                addJointToMap(jointMap, stripUnderscores(shortName), dagPath);
                addJointToMap(jointMap, stripNamespace(mayaName), dagPath);
                std::string comp0 = getComponentString0(stripNamespace(mayaName));
                addJointToMap(jointMap, comp0, dagPath);
                addJointToMap(jointMap, toLower(comp0), dagPath);
            }
        };

        // Use all scene joints - skeleton filter excludes joints when mesh references different skeleton path
        buildJointMap(false);

        std::vector<std::string> skelBases;
        {
            std::set<std::string> seenBase;
            if (!skeletonFilename.empty())
            {
                const std::string b = pathBasenameNoExt(skeletonFilename);
                if (!b.empty() && seenBase.insert(toLower(b)).second)
                    skelBases.push_back(b);
            }
            for (size_t si = 0; si < skeletonTemplateNames.size(); ++si)
            {
                const std::string b = pathBasenameNoExt(skeletonTemplateNames[si]);
                if (!b.empty() && seenBase.insert(toLower(b)).second)
                    skelBases.push_back(b);
            }
        }

        jointPaths.clear();
        std::set<std::string> seenJointPath;
        size_t resolvedInfluences = 0;
        std::map<std::string, MDagPath>::const_iterator jit;
        for (size_t i = 0; i < transformNames.size(); ++i)
        {
            const std::string& tn = transformNames[i];
            if (!resolveTransformToJoint(tn, jointMap, skelBases, jit))
                continue;
            ++resolvedInfluences;
            MString fp = jit->second.fullPathName();
            const std::string fps(fp.asChar());
            if (seenJointPath.insert(fps).second)
                jointPaths.push_back(jit->second);
        }

        if (resolvedInfluences < transformNames.size())
        {
            std::cerr << "ImportSkeletalMesh: resolved " << resolvedInfluences << " of " << transformNames.size()
                << " influence transform names to scene joints (skin weights need a match per XFNM entry)" << std::endl;
        }

        if (jointPaths.empty() && transformWeightDataCount > 0)
        {
            MGlobal::displayWarning(MString(
                "ImportSkeletalMesh: no joints matched mesh transform names; mesh imported without skinCluster. "
                "Check skeleton import (LOD l0 uses root names like joint__basename_l0) vs SKMG XFNM."));
        }

        if (!jointPaths.empty())
        {
            int maxInf = (maxTransformsPerVertex > 0) ? maxTransformsPerVertex : 8;
            status = MayaSceneBuilder::createSkinCluster(meshPath, jointPaths, transformNames, weightHeaders, weightData, maxInf);
            if (!status)
                std::cerr << "ImportSkeletalMesh: failed to create skin cluster" << std::endl;
        }

        std::vector<MayaSceneBuilder::HardpointData> allHardpoints;
        allHardpoints.insert(allHardpoints.end(), staticHardpoints.begin(), staticHardpoints.end());
        allHardpoints.insert(allHardpoints.end(), dynamicHardpoints.begin(), dynamicHardpoints.end());

        if (!allHardpoints.empty() && !jointMap.empty())
        {
            status = MayaSceneBuilder::createHardpoints(allHardpoints, jointMap, meshName);
            if (!status)
                std::cerr << "ImportSkeletalMesh: failed to create hardpoints" << std::endl;
        }
    }

    {
        int totalTris = 0;
        for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
            totalTris += static_cast<int>(shaderGroups[sg].triangles.size());
        MString msg = "ImportSkeletalMesh: SUCCESS mesh=\"";
        msg += meshName.c_str();
        msg += "\" verts="; msg += static_cast<int>(positionCount);
        msg += " tris="; msg += totalTris;
        msg += " xfnm="; msg += static_cast<int>(transformNames.size());
        MGlobal::displayInfo(msg);
    }
    return MS::kSuccess;
}
