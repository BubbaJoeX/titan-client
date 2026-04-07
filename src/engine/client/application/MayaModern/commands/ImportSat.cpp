#include "ImportSat.h"
#include "ImportPathResolver.h"
#include "ImportLodMesh.h"
#include "ImportSkeleton.h"
#include "ImportSkeletalMesh.h"
#include "SatRoundTrip.h"

#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MSelectionList.h>

#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace
{
    static const Tag TAG_SMAT = TAG(S,M,A,T);
    static const Tag TAG_MSGN = TAG(M,S,G,N);
    static const Tag TAG_SKTI = TAG(S,K,T,I);
    static const Tag TAG_SKTM = TAG(S,K,T,M);
    static const Tag TAG_SLOD = TAG(S,L,O,D);
    static const Tag TAG_V000 = TAG(0,0,0,0);
    static const Tag TAG_V001 = TAG(0,0,0,1);
    static const Tag TAG_V002 = TAG(0,0,0,2);
    static const Tag TAG_V003 = TAG(0,0,0,3);

    static bool hasExtension(const std::string& path, const char* ext)
    {
        const auto dotPos = path.find_last_of('.');
        if (dotPos == std::string::npos)
            return false;
        std::string pathExt = path.substr(dotPos);
        for (auto& c : pathExt)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + ('a' - 'A'));
        return pathExt == ext;
    }

    static void ensureSwgLighting()
    {
        MStringArray result;
        if (MGlobal::executeCommand("objExists \"swgLighting\"", result, false, false) && result.length() > 0 && result[0] == "1")
            return;

        MGlobal::executeCommand("group -em -n swgLighting");
        MGlobal::executeCommand("directionalLight -intensity 1.2 -rgb 0.95 0.92 0.85 -n swgSun");
        MGlobal::executeCommand("ambientLight -intensity 0.4 -rgb 0.52 0.52 0.58 -n swgAmbient");
        MGlobal::executeCommand("rotate -r -45 35 0 swgSun");
        MGlobal::executeCommand("parent swgSun swgAmbient swgLighting");
    }

    static std::string stripNamespace(const std::string& s)
    {
        size_t c = s.rfind(':');
        if (c != std::string::npos && c + 1 < s.size())
            return s.substr(c + 1);
        return s;
    }

    static bool strEqualIgnoreCase(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }

    // Match joint short name to attachment (e.g. "head" matches "head__all_b_l0", "Head")
    static bool jointMatchesAttachment(const std::string& jointShortName, const std::string& attachmentName)
    {
        std::string s = stripNamespace(jointShortName);
        if (strEqualIgnoreCase(s, attachmentName))
            return true;
        if (s.size() > attachmentName.size() + 2)
        {
            size_t pos = s.size() - attachmentName.size() - 2;
            if (s.substr(pos, 2) == "__" && strEqualIgnoreCase(s.substr(pos + 2), attachmentName))
                return true;
        }
        size_t u = s.find("__");
        if (u != std::string::npos && u > 0 && strEqualIgnoreCase(s.substr(0, u), attachmentName))
            return true;
        return false;
    }

    // Find attachment joint path using Maya API. Face skeleton attaches to body at Head (etc).
    static std::string findAttachmentJointPath(const std::string& rootJointName, const std::string& attachmentName)
    {
        if (attachmentName.empty())
            return std::string();

        MStatus status;
        MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
        if (!status) return std::string();

        MDagPath rootPath;
        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath path;
            dagIt.getPath(path);
            MFnDagNode fn(path);
            std::string name = stripNamespace(fn.name().asChar());
            if (name == rootJointName)
            {
                rootPath = path;
                break;
            }
            if (rootJointName.size() + 4 <= name.size() &&
                name.compare(0, rootJointName.size(), rootJointName) == 0 &&
                name.compare(rootJointName.size(), 4, "_l0") == 0)
            {
                rootPath = path;
                break;
            }
        }
        if (!rootPath.isValid())
            return std::string();

        std::string rootFullPath(rootPath.fullPathName().asChar());
        dagIt.reset();
        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath path;
            dagIt.getPath(path);
            std::string fullPath(path.fullPathName().asChar());
            if (fullPath != rootFullPath && fullPath.find(rootFullPath + "|") != 0)
                continue;
            MFnDagNode fn(path);
            std::string name(fn.name().asChar());
            if (jointMatchesAttachment(name, attachmentName))
                return path.fullPathName().asChar();
        }
        return std::string();
    }

    static std::string fileBasenameNoExtension(std::string path)
    {
        for (auto& c : path)
            if (c == '\\')
                c = '/';
        const size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos)
            path = path.substr(lastSlash + 1);
        const size_t dot = path.find_last_of('.');
        if (dot != std::string::npos)
            path = path.substr(0, dot);
        return path;
    }

    /** Parent transform of the root joint (master, or SLOD l0), for parenting mesh next to the rig. */
    static std::string parentTransformOfPrimarySkeletonJoint(const std::string& skelBasenameNoExt)
    {
        if (skelBasenameNoExt.empty())
            return std::string();

        MStatus st;
        MItDag it(MItDag::kDepthFirst, MFn::kJoint, &st);
        if (!st)
            return std::string();

        const std::string sufLod = std::string("__") + skelBasenameNoExt + "_l0";
        const std::string sufFlat = std::string("__") + skelBasenameNoExt;

        MDagPath bestJoint;
        int bestRank = 0;

        for (; !it.isDone(); it.next())
        {
            MDagPath jp;
            it.getPath(jp);
            MFnDagNode jfn(jp);
            const std::string jn = stripNamespace(std::string(jfn.name().asChar()));

            int rank = 0;
            if (jn.size() >= sufLod.size() && jn.compare(jn.size() - sufLod.size(), sufLod.size(), sufLod) == 0)
                rank = 2;
            else if (jn.size() >= sufFlat.size() && jn.compare(jn.size() - sufFlat.size(), sufFlat.size(), sufFlat) == 0)
                rank = 1;
            else
                continue;

            if (rank > bestRank)
            {
                bestRank = rank;
                bestJoint = jp;
            }
        }

        if (!bestJoint.isValid())
            return std::string();

        MDagPath parentPath = bestJoint;
        st = parentPath.pop(1);
        if (!st)
            return std::string();

        return std::string(parentPath.fullPathName().asChar());
    }

    // MayaSceneBuilder::createJointHierarchy names the root joint
    // "<rootJointFromSkt>__<basename>" or, for SLOD, "<rootJointFromSkt>__<basename>_l0".
    // ImportSat used to assume "root__<basename>", which breaks attachment parenting when
    // the first joint in the .skt is not literally "root" (e.g. hips, bn_root).
    static std::string expectedRootJointShortNameFromSkeletonFile(const std::string& resolvedPath)
    {
        Iff skelIff;
        if (!skelIff.open(resolvedPath.c_str(), false))
            return std::string();

        const Tag top = skelIff.getCurrentName();
        const std::string base = fileBasenameNoExtension(resolvedPath);

        auto skipRestOfForm = [](Iff& iff)
        {
            while (!iff.atEndOfForm())
            {
                if (iff.isCurrentForm())
                {
                    iff.enterForm();
                    iff.exitForm(true);
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk(true);
                }
            }
        };

        if (top == TAG_SKTM)
        {
            skelIff.enterForm(TAG_SKTM);
            const Tag ver = skelIff.getCurrentName();
            skelIff.enterForm(ver);

            skelIff.enterChunk(TAG_INFO);
            const int32 jointCount = skelIff.read_int32();
            skelIff.exitChunk(TAG_INFO);

            std::string firstJoint;
            if (jointCount > 0)
            {
                skelIff.enterChunk(TAG_NAME);
                firstJoint = skelIff.read_stdstring();
                skelIff.exitChunk(true);
            }

            skipRestOfForm(skelIff);
            skelIff.exitForm(ver);
            skelIff.exitForm(TAG_SKTM);

            if (!firstJoint.empty())
                return firstJoint + "__" + base;
        }
        else if (top == TAG_SLOD)
        {
            skelIff.enterForm(TAG_SLOD);
            skelIff.enterForm(TAG_V000);

            skelIff.enterChunk(TAG_INFO);
            const int16 levelCount = skelIff.read_int16();
            skelIff.exitChunk(TAG_INFO);

            std::string firstJoint;
            if (levelCount > 0)
            {
                skelIff.enterForm(TAG_SKTM);
                const Tag ver = skelIff.getCurrentName();
                skelIff.enterForm(ver);

                skelIff.enterChunk(TAG_INFO);
                const int32 jointCount = skelIff.read_int32();
                skelIff.exitChunk(TAG_INFO);

                if (jointCount > 0)
                {
                    skelIff.enterChunk(TAG_NAME);
                    firstJoint = skelIff.read_stdstring();
                    skelIff.exitChunk(true);
                }

                skipRestOfForm(skelIff);
                skelIff.exitForm(ver);
                skelIff.exitForm(TAG_SKTM);
            }

            // Remaining LOD levels (additional SKTM forms) and any other data in V000
            skipRestOfForm(skelIff);
            skelIff.exitForm(TAG_V000);
            skelIff.exitForm(TAG_SLOD);

            if (!firstJoint.empty())
                return firstJoint + "__" + base + "_l0";
        }

        return std::string();
    }

    // Legacy fallback when skeleton file cannot be peeked (matches old incorrect assumption).
    static std::string getSkeletonRootNameFallback(const std::string& skeletonPath)
    {
        return "root__" + fileBasenameNoExtension(skeletonPath);
    }
}

void* ImportSat::creator()
{
    return new ImportSat();
}

MStatus ImportSat::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

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
                std::cerr << "ImportSat: missing filename after -i" << std::endl;
                return MS::kFailure;
            }
            filename = argValue.asChar();
            ++i;
            break;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportSat: missing -i <filename> argument" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);

    Iff iff;
    if (!iff.open(filename.c_str(), false))
    {
        std::cerr << "ImportSat: failed to open " << filename << std::endl;
        return MS::kFailure;
    }

    iff.enterForm(TAG_SMAT);

    const Tag versionTag = iff.getCurrentName();
    const bool isVersion1 = (versionTag == TAG_V001);
    const bool isVersion2 = (versionTag == TAG_V002);
    const bool isVersion3 = (versionTag == TAG_V003);

    if (!isVersion1 && !isVersion2 && !isVersion3)
    {
        std::cerr << "ImportSat: unsupported SAT version (expected 0001, 0002 or 0003)" << std::endl;
        return MS::kFailure;
    }

    iff.enterForm(versionTag);

    SatRoundTripData satRt;
    {
        char verChars[8];
        ConvertTagToString(versionTag, verChars);
        satRt.versionTag.assign(verChars, 4);
    }

    int32 meshGeneratorCount = 0;
    int32 skeletonTemplateCount = 0;

    iff.enterChunk(TAG_INFO);
    meshGeneratorCount = iff.read_int32();
    skeletonTemplateCount = iff.read_int32();
    if (isVersion1)
        satRt.infoExtraString = iff.read_stdstring();
    else if (isVersion2)
        satRt.infoExtraString = iff.read_stdstring();
    else if (isVersion3)
        satRt.createAnimationController = (iff.read_int8() != 0);
    iff.exitChunk(TAG_INFO);

    iff.enterChunk(TAG_MSGN);
    for (int32 i = 0; i < meshGeneratorCount; ++i)
        satRt.meshGenerators.push_back(iff.read_stdstring());
    iff.exitChunk(TAG_MSGN);

    iff.enterChunk(TAG_SKTI);
    for (int32 i = 0; i < skeletonTemplateCount; ++i)
    {
        const std::string sk = iff.read_stdstring();
        const std::string att = iff.read_stdstring();
        satRt.skeletonTemplates.emplace_back(sk, att);
    }
    iff.exitChunk(TAG_SKTI);

    sat_round_trip::parseTailAfterSkti(iff, satRt);

    iff.exitForm(versionTag);
    iff.exitForm(TAG_SMAT);

    const std::vector<std::string>& meshGeneratorPaths = satRt.meshGenerators;
    std::vector<std::string> skeletonTemplatePaths;
    std::vector<std::string> attachmentTransformNames;
    skeletonTemplatePaths.reserve(satRt.skeletonTemplates.size());
    attachmentTransformNames.reserve(satRt.skeletonTemplates.size());
    for (const auto& pr : satRt.skeletonTemplates)
    {
        skeletonTemplatePaths.push_back(pr.first);
        attachmentTransformNames.push_back(pr.second);
    }

    // MayaExporter convention: no asset group; SKTM -> master|root__skeleton, SLOD -> baseName|l0|root__baseName_l0
    std::string firstSkeletonRootName;  // e.g. "root__all_b" - root joint of first skeleton
    for (size_t i = 0; i < skeletonTemplatePaths.size(); ++i)
    {
        const std::string& relativePath = skeletonTemplatePaths[i];
        const std::string resolvedPath = lod_path_helpers::resolveTreeFilePath(relativePath, filename);
        const std::string& attachmentName = (i < attachmentTransformNames.size()) ? attachmentTransformNames[i] : std::string();

        MArgList skelArgs;
        skelArgs.addArg(MString("-i"));
        skelArgs.addArg(MString(resolvedPath.c_str()));

        if (i > 0 && !firstSkeletonRootName.empty() && !attachmentName.empty())
        {
            // SLOD skeletons use root__name_l0; try that first, then root__name
            std::string parentPath;
            if (firstSkeletonRootName.find("_l0") == std::string::npos)
                parentPath = findAttachmentJointPath(firstSkeletonRootName + "_l0", attachmentName);
            if (parentPath.empty())
                parentPath = findAttachmentJointPath(firstSkeletonRootName, attachmentName);
            if (!parentPath.empty())
            {
                skelArgs.addArg(MString("-parent"));
                skelArgs.addArg(MString(parentPath.c_str()));
            }
            else
            {
                std::cerr << "ImportSat: attachment joint not found (root=" << firstSkeletonRootName << " attach=" << attachmentName << "), face skeleton will be at root" << std::endl;
            }
        }

        ImportSkeleton skelImporter;
        status = skelImporter.doIt(skelArgs);
        if (!status)
            std::cerr << "ImportSat: failed to import skeleton [" << resolvedPath << "]" << std::endl;
        else if (i == 0)
        {
            firstSkeletonRootName = expectedRootJointShortNameFromSkeletonFile(resolvedPath);
            if (firstSkeletonRootName.empty())
                firstSkeletonRootName = getSkeletonRootNameFallback(relativePath);
        }
    }

    std::string firstSkeletonResolved;
    if (!skeletonTemplatePaths.empty())
        firstSkeletonResolved = lod_path_helpers::resolveTreeFilePath(skeletonTemplatePaths[0], filename);

    const std::string meshParentDag = (!firstSkeletonResolved.empty())
        ? parentTransformOfPrimarySkeletonJoint(fileBasenameNoExtension(firstSkeletonResolved))
        : std::string();

    MGlobal::displayInfo(MString("ImportSat: meshParentDag=\"") + meshParentDag.c_str()
        + "\" firstSkelResolved=\"" + firstSkeletonResolved.c_str() + "\"");

    if (meshGeneratorPaths.empty())
        MGlobal::displayWarning(
            MString("ImportSat: SMAT has zero mesh generator paths (MSGN). Skeleton will import; add meshes manually or fix the SAT if this appearance should be skinned."));

    bool meshImportFailed = false;
    for (size_t i = 0; i < meshGeneratorPaths.size(); ++i)
    {
        const std::string& relativePath = meshGeneratorPaths[i];
        const std::string resolvedPath = lod_path_helpers::resolveTreeFilePath(relativePath, filename);

        MGlobal::displayInfo(MString("ImportSat: MSGN[") + static_cast<int>(i) + "] rel=\""
            + relativePath.c_str() + "\" -> \"" + resolvedPath.c_str() + "\"");

        MArgList skmArgs;
        skmArgs.addArg(MString("-i"));
        skmArgs.addArg(MString(resolvedPath.c_str()));
        if (!firstSkeletonResolved.empty())
        {
            skmArgs.addArg(MString("-skeleton"));
            skmArgs.addArg(MString(firstSkeletonResolved.c_str()));
        }
        if (!meshParentDag.empty())
        {
            skmArgs.addArg(MString("-parent"));
            skmArgs.addArg(MString(meshParentDag.c_str()));
        }

        ImportSkeletalMesh skmImporter;
        const MStatus skmStatus = skmImporter.doIt(skmArgs);
        if (!skmStatus)
        {
            meshImportFailed = true;
            MString err = "ImportSat: skeletal mesh import failed for ";
            err += resolvedPath.c_str();
            err += " (see Script Editor / stderr for ImportSkeletalMesh details).";
            MGlobal::displayError(err);
            if (hasExtension(relativePath, ".lmg"))
                std::cerr << "ImportSat: failed to import mesh generator [" << resolvedPath << "]" << std::endl;
            else
                std::cerr << "ImportSat: failed to import skeletal mesh [" << resolvedPath << "]" << std::endl;
        }
    }

    if (!meshGeneratorPaths.empty() && meshImportFailed)
    {
        MGlobal::displayError(
            "ImportSat: one or more mesh generators failed to import. Fix errors above; skeleton-only scene is not sufficient for SAT round-trip.");
        return MS::kFailure;
    }

    ensureSwgLighting();

    const std::string satBaseName = fileBasenameNoExtension(filename);
    const std::string payload = sat_round_trip::serializePayload(satRt);
    if (!sat_round_trip::createMetadataNode(satBaseName, payload))
        std::cerr << "ImportSat: warning: failed to create SAT metadata transform (swgSmatRoundTrip) for round-trip export." << std::endl;

    return MS::kSuccess;
}
