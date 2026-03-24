#include "ImportSat.h"
#include "ImportPathResolver.h"

#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
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

    static std::string resolveTreeFilePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty())
            return std::string();

        std::string baseDir;
        const char* envExportRoot = getenv("TITAN_EXPORT_ROOT");
        if (envExportRoot && envExportRoot[0])
        {
            baseDir = envExportRoot;
            if (!baseDir.empty() && baseDir.back() != '\\' && baseDir.back() != '/')
                baseDir += '/';
        }
        else
        {
            const char* envDataRoot = getenv("TITAN_DATA_ROOT");
            if (envDataRoot && envDataRoot[0])
            {
                baseDir = envDataRoot;
                if (!baseDir.empty() && baseDir.back() != '\\' && baseDir.back() != '/')
                    baseDir += '/';
            }
            else
            {
                std::string normalizedInput = inputFilename;
                for (auto& c : normalizedInput) if (c == '\\') c = '/';
                const auto cgPos = normalizedInput.find("compiled/game/");
                if (cgPos != std::string::npos)
                    baseDir = normalizedInput.substr(0, cgPos + 14);
                else
                {
                    const auto firstSlash = treeFilePath.find_first_of("/\\");
                    if (firstSlash != std::string::npos)
                    {
                        const std::string firstComponent = "/" + treeFilePath.substr(0, firstSlash + 1);
                        const auto pos = normalizedInput.find(firstComponent);
                        if (pos != std::string::npos)
                            baseDir = normalizedInput.substr(0, pos + 1);
                    }
                }
                if (baseDir.empty())
                {
                    const auto lastSlash = normalizedInput.find_last_of('/');
                    if (lastSlash != std::string::npos)
                        baseDir = normalizedInput.substr(0, lastSlash + 1);
                }
            }
        }

        if (baseDir.empty())
            return treeFilePath;

        std::string resolved = baseDir + treeFilePath;
        for (auto& c : resolved) if (c == '\\') c = '/';
        return resolved;
    }

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

    int32 meshGeneratorCount = 0;
    int32 skeletonTemplateCount = 0;

    iff.enterChunk(TAG_INFO);
    meshGeneratorCount = iff.read_int32();
    skeletonTemplateCount = iff.read_int32();
    if (isVersion1)
    {
        std::string legacyPath = iff.read_stdstring();
        (void)legacyPath;
    }
    else if (isVersion2)
    {
        std::string asgPath = iff.read_stdstring();
        (void)asgPath;
    }
    else if (isVersion3)
    {
        iff.read_int8();  // createAnimationController
    }
    iff.exitChunk(TAG_INFO);

    std::vector<std::string> meshGeneratorPaths;
    iff.enterChunk(TAG_MSGN);
    for (int32 i = 0; i < meshGeneratorCount; ++i)
        meshGeneratorPaths.push_back(iff.read_stdstring());
    iff.exitChunk(TAG_MSGN);

    std::vector<std::string> skeletonTemplatePaths;
    std::vector<std::string> attachmentTransformNames;
    iff.enterChunk(TAG_SKTI);
    for (int32 i = 0; i < skeletonTemplateCount; ++i)
    {
        skeletonTemplatePaths.push_back(iff.read_stdstring());
        attachmentTransformNames.push_back(iff.read_stdstring());
    }
    iff.exitChunk(TAG_SKTI);

    // v0003 SAT may contain LATX, LDTB, SFSK, APAG, etc. Skip without fully parsing;
    // use exitChunk(true)/exitForm(true) so unread chunk/form data does not trip IFF checks.
    while (iff.getNumberOfBlocksLeft() > 0)
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

    iff.exitForm(versionTag);
    iff.exitForm(TAG_SMAT);

    // MayaExporter convention: no asset group; SKTM -> master|root__skeleton, SLOD -> baseName|l0|root__baseName_l0
    std::string firstSkeletonRootName;  // e.g. "root__all_b" - root joint of first skeleton
    for (size_t i = 0; i < skeletonTemplatePaths.size(); ++i)
    {
        const std::string& relativePath = skeletonTemplatePaths[i];
        const std::string resolvedPath = resolveTreeFilePath(relativePath, filename);
        const std::string& attachmentName = (i < attachmentTransformNames.size()) ? attachmentTransformNames[i] : std::string();

        MString cmd = "importSkeleton -i \"";
        cmd += resolvedPath.c_str();
        cmd += "\"";

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
                cmd += " -parent \"";
                cmd += parentPath.c_str();
                cmd += "\"";
            }
            else
            {
                std::cerr << "ImportSat: attachment joint not found (root=" << firstSkeletonRootName << " attach=" << attachmentName << "), face skeleton will be at root" << std::endl;
            }
        }

        status = MGlobal::executeCommand(cmd, true, true);
        if (!status)
            std::cerr << "ImportSat: failed to import skeleton [" << resolvedPath << "]" << std::endl;
        else if (i == 0)
        {
            firstSkeletonRootName = expectedRootJointShortNameFromSkeletonFile(resolvedPath);
            if (firstSkeletonRootName.empty())
                firstSkeletonRootName = getSkeletonRootNameFallback(relativePath);
        }
    }

    for (size_t i = 0; i < meshGeneratorPaths.size(); ++i)
    {
        const std::string& relativePath = meshGeneratorPaths[i];
        const std::string resolvedPath = resolveTreeFilePath(relativePath, filename);

        if (hasExtension(relativePath, ".lmg"))
        {
            MString cmd = "importLodMesh -i \"";
            cmd += resolvedPath.c_str();
            cmd += "\"";
            status = MGlobal::executeCommand(cmd, true, true);
            if (!status)
                std::cerr << "ImportSat: failed to import LOD mesh [" << resolvedPath << "]" << std::endl;
        }
        else
        {
            MString cmd = "importSkeletalMesh -i \"";
            cmd += resolvedPath.c_str();
            cmd += "\"";
            status = MGlobal::executeCommand(cmd, true, true);
            if (!status)
                std::cerr << "ImportSat: failed to import skeletal mesh [" << resolvedPath << "]" << std::endl;
        }
    }

    ensureSwgLighting();
    return MS::kSuccess;
}
