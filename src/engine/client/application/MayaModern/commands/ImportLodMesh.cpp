#include "ImportLodMesh.h"
#include "ImportPathResolver.h"
#include "Iff.h"
#include "MayaSceneBuilder.h"
#include "Tag.h"
#include "Vector.h"
#include "msh.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lod_path_helpers
{
    static const Tag kTagMlod = TAG(M, L, O, D);
    static const Tag kTagApt = TAG3(A, P, T);
    static const Tag kTagSkmg = TAG(S, K, M, G);

    std::string resolveTreeFilePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty())
            return std::string();

        std::string absProbe = treeFilePath;
        for (auto& c : absProbe)
            if (c == '\\')
                c = '/';
        if (absProbe.size() >= 2 && std::isalpha(static_cast<unsigned char>(absProbe[0])) && absProbe[1] == ':')
            return absProbe;
        if (!absProbe.empty() && absProbe[0] == '/')
            return absProbe;

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
                for (size_t i = 0; i < normalizedInput.size(); ++i)
                    if (normalizedInput[i] == '\\') normalizedInput[i] = '/';

                size_t cgPos = normalizedInput.find("compiled/game/");
                if (cgPos != std::string::npos)
                    baseDir = normalizedInput.substr(0, cgPos + 14);
                else
                {
                    size_t firstSlash = treeFilePath.find_first_of("/\\");
                    if (firstSlash != std::string::npos)
                    {
                        std::string firstComponent = "/" + treeFilePath.substr(0, firstSlash + 1);
                        size_t pos = normalizedInput.find(firstComponent);
                        if (pos != std::string::npos)
                            baseDir = normalizedInput.substr(0, pos + 1);
                    }
                }

                if (baseDir.empty())
                {
                    size_t lastSlash = normalizedInput.find_last_of('/');
                    if (lastSlash != std::string::npos)
                        baseDir = normalizedInput.substr(0, lastSlash + 1);
                }
            }
        }

        std::string pathToResolve = treeFilePath;
        {
            std::string normalizedInput = inputFilename;
            for (size_t i = 0; i < normalizedInput.size(); ++i)
                if (normalizedInput[i] == '\\') normalizedInput[i] = '/';
            size_t cgPos = normalizedInput.find("compiled/game/");
            if (cgPos != std::string::npos)
            {
                std::string afterCg = normalizedInput.substr(cgPos + 14);
                if (afterCg.find("appearance/") == 0)
                {
                    if (pathToResolve.find("appearance/") != 0 && pathToResolve.find("appearance\\") != 0)
                        pathToResolve = std::string("appearance/") + pathToResolve;
                }
            }
        }
        std::string resolved = baseDir + pathToResolve;
        for (size_t i = 0; i < resolved.size(); ++i)
            if (resolved[i] == '\\') resolved[i] = '/';
        return resolved;
    }

    std::string resolveMeshPath(const std::string& basePath)
    {
        std::string path = basePath;
        for (auto& c : path) if (c == '\\') c = '/';
        if (path.size() >= 4)
        {
            const std::string ext = path.substr(path.size() - 4);
            if (ext == ".msh" || ext == ".shp" || ext == ".mgn") return path;
        }
        Iff iff;
        if (iff.open((path + ".shp").c_str(), true))
            return path + ".shp";
        if (iff.open((path + ".msh").c_str(), true))
            return path + ".msh";
        return path + ".msh";
    }

    std::string resolveSkmgPathThroughWrappers(std::string path)
    {
        constexpr int kMaxHops = 16;
        for (int hop = 0; hop < kMaxHops; ++hop)
        {
            Iff iff;
            if (!iff.open(path.c_str(), false))
                return path;

            const Tag root = iff.getCurrentName();
            if (root == kTagSkmg)
            {
                iff.close();
                return path;
            }
            if (root == kTagApt)
            {
                iff.enterForm(kTagApt);
                iff.enterForm(TAG_0000);
                iff.enterChunk(TAG_NAME);
                std::string redirectPath = iff.read_stdstring();
                iff.exitChunk(TAG_NAME);
                iff.exitForm(TAG_0000);
                iff.exitForm(kTagApt);
                iff.close();
                if (redirectPath.empty())
                    return path;
                path = resolveMeshPath(resolveTreeFilePath(redirectPath, path));
                continue;
            }
            if (root == kTagMlod)
            {
                iff.enterForm(kTagMlod);
                iff.enterForm(TAG_0000);
                int16 detailLevelCount = 0;
                iff.enterChunk(TAG_INFO);
                detailLevelCount = iff.read_int16();
                iff.exitChunk(TAG_INFO);
                std::string firstRel;
                for (int16 i = 0; i < detailLevelCount; ++i)
                {
                    iff.enterChunk(TAG_NAME);
                    std::string s = iff.read_stdstring();
                    iff.exitChunk(TAG_NAME);
                    if (i == 0)
                        firstRel = std::move(s);
                }
                iff.exitForm(TAG_0000);
                iff.exitForm(kTagMlod);
                iff.close();
                if (firstRel.empty())
                    return path;
                path = resolveMeshPath(resolveTreeFilePath(firstRel, path));
                continue;
            }
            iff.close();
            return path;
        }
        return path;
    }
}

std::string resolveSkmgPathThroughWrappers(const std::string& resolvedImportPath)
{
    return lod_path_helpers::resolveSkmgPathThroughWrappers(resolvedImportPath);
}

namespace
{
    static void lodLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[ImportLodMesh] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }

    static const Tag TAG_MLOD = TAG(M,L,O,D);
    const Tag TAG_DTLA = TAG(D,T,L,A);
    const Tag TAG_MESH = TAG(M,E,S,H);
    const Tag TAG_APT  = TAG3(A,P,T);
    const Tag TAG_APPR = TAG(A,P,P,R);
    const Tag TAG_CHLD = TAG(C,H,L,D);
    const Tag TAG_RADR = TAG(R,A,D,R);
    const Tag TAG_TEST = TAG(T,E,S,T);
    const Tag TAG_IDTL = TAG(I,D,T,L);
    const Tag TAG_VERT = TAG(V,E,R,T);
    const Tag TAG_INDX = TAG(I,N,D,X);

    static void skipForm(Iff& iff)
    {
        iff.enterForm();
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
                skipForm(iff);
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
        iff.exitForm();
    }

    static bool readIdtl(Iff& iff, std::vector<Vector>& vertices, std::vector<int>& indices)
    {
        iff.enterForm(TAG_IDTL);
        iff.enterForm(TAG_0000);
        iff.enterChunk(TAG_VERT);
        const int numVerts = iff.getChunkLengthLeft(12) / 12;
        vertices.resize(static_cast<size_t>(numVerts));
        iff.read_floatVector(numVerts, vertices.data());
        iff.exitChunk(TAG_VERT);
        iff.enterChunk(TAG_INDX);
        const int numIndices = iff.getChunkLengthLeft(4) / 4;
        indices.resize(static_cast<size_t>(numIndices));
        for (int j = 0; j < numIndices; ++j)
            indices[static_cast<size_t>(j)] = iff.read_int32();
        iff.exitChunk(TAG_INDX);
        iff.exitForm(TAG_0000);
        iff.exitForm(TAG_IDTL);
        return !vertices.empty() && indices.size() >= 3;
    }

    static bool createShapeMesh(const std::vector<Vector>& verts, const std::vector<int>& indices,
        const char* name, const std::string& parentPath)
    {
        if (verts.empty() || indices.size() < 3) return false;
        std::vector<float> positions;
        positions.reserve(verts.size() * 3);
        for (size_t i = 0; i < verts.size(); ++i)
        {
            positions.push_back(verts[i].x);
            positions.push_back(verts[i].y);
            positions.push_back(verts[i].z);
        }
        std::vector<float> normals(positions.size(), 0.0f);
        for (size_t i = 0; i < normals.size(); i += 3)
            normals[static_cast<size_t>(i) + 1] = 1.0f;
        MayaSceneBuilder::ShaderGroupData sg;
        sg.shaderTemplateName = "shader/placeholder";
        for (size_t t = 0; t + 2 < indices.size(); t += 3)
        {
            MayaSceneBuilder::TriangleData tri;
            tri.indices[0] = indices[t];
            tri.indices[1] = indices[t + 1];
            tri.indices[2] = indices[t + 2];
            sg.triangles.push_back(tri);
        }
        std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
        MDagPath meshPath;
        if (!MayaSceneBuilder::createMesh(positions, normals, groups, name, meshPath))
            return false;
        MDagPath transformPath = meshPath;
        if (transformPath.hasFn(MFn::kMesh))
            transformPath.pop(1);
        MString parentCmd = "parent \"";
        parentCmd += transformPath.fullPathName();
        parentCmd += "\" \"";
        parentCmd += parentPath.c_str();
        parentCmd += "\"";
        MGlobal::executeCommand(parentCmd);
        return true;
    }
}

std::string resolveLodOrAptPath(const std::string& baseResolvedPath)
{
    std::string path = baseResolvedPath;
    for (auto& c : path) if (c == '\\') c = '/';
    if (path.size() >= 4)
    {
        const std::string ext = path.substr(path.size() - 4);
        if (ext == ".lod" || ext == ".apt" || ext == ".msh" || ext == ".lmg") return path;
    }
    Iff iff;
    if (iff.open((path + ".lod").c_str(), true))
        return path + ".lod";
    if (iff.open((path + ".apt").c_str(), true))
        return path + ".apt";
    if (iff.open((path + ".msh").c_str(), true))
        return path + ".msh";
    return path + ".lod";
}

std::string resolveStaticMeshPath(const std::string& basePath)
{
    std::string path = basePath;
    for (auto& c : path) if (c == '\\') c = '/';
    if (path.size() >= 4)
    {
        const std::string ext = path.substr(path.size() - 4);
        if (ext == ".apt" || ext == ".msh" || ext == ".lod") return path;
    }
    Iff iff;
    if (iff.open((path + ".apt").c_str(), true))
        return path + ".apt";
    if (iff.open((path + ".msh").c_str(), true))
        return path + ".msh";
    if (iff.open((path + ".lod").c_str(), true))
        return path + ".lod";
    return path + ".msh";
}

std::string resolvePathViaApt(const std::string& filePath)
{
    std::string path = filePath;
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '\\') path[i] = '/';

    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path;
    std::string basePath = path.substr(0, dot);
    std::string aptPath = basePath + ".apt";

    Iff iff;
    if (!iff.open(aptPath.c_str(), true))
        return path;

    if (iff.getCurrentName() != TAG_APT)
    {
        iff.close();
        return path;
    }
    iff.enterForm(TAG_APT);
    iff.enterForm(::TAG_0000);
    iff.enterChunk(::TAG_NAME);
    std::string redirectPath = iff.read_stdstring();
    iff.exitChunk(::TAG_NAME);
    iff.exitForm(::TAG_0000);
    iff.exitForm(TAG_APT);
    iff.close();

    if (redirectPath.empty())
        return path;

    std::string resolvedRedirect = lod_path_helpers::resolveTreeFilePath(redirectPath, path);
    std::string finalPath = resolveStaticMeshPath(resolvedRedirect);

    if (finalPath.size() >= 4 && finalPath.substr(finalPath.size() - 4) == ".apt")
        return resolvePathViaApt(finalPath);
    return finalPath;
}

namespace
{
    static void normalizeHighestLodSuffixInPath(std::string& meshFilename)
    {
        const std::string::size_type lp = meshFilename.rfind("_l");
        if (lp == std::string::npos || lp + 2 >= meshFilename.size())
            return;
        std::string::size_type end = lp + 2;
        while (end < meshFilename.size() && meshFilename[end] >= '0' && meshFilename[end] <= '9')
            ++end;
        const char next = (end < meshFilename.size()) ? meshFilename[end] : '\0';
        if (next != '_' && next != '.' && next != '\0')
            return;
        const std::string lodSuffix = meshFilename.substr(lp + 2, end - (lp + 2));
        if (lodSuffix != "0")
            meshFilename = meshFilename.substr(0, lp + 2) + "0" + meshFilename.substr(end);
    }

    static void consumeOneBlock(Iff& iff)
    {
        if (iff.isCurrentForm())
        {
            iff.enterForm();
            while (iff.getNumberOfBlocksLeft() > 0)
                consumeOneBlock(iff);
            iff.exitForm();
        }
        else
        {
            iff.enterChunk();
            iff.exitChunk();
        }
    }

    static bool extractDtlaL0Path(Iff& iff, std::string& outPath)
    {
        static const Tag TAG_DTLA_LOCAL = TAG(D, T, L, A);
        static const Tag TAG_DATA_LOCAL = TAG(D, A, T, A);
        static const Tag TAG_CHLD_LOCAL = TAG(C, H, L, D);

        iff.enterForm(TAG_DTLA_LOCAL);
        const Tag versionTag = iff.getCurrentName();
        iff.enterForm(versionTag);
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm() && iff.getCurrentName() == TAG_DATA_LOCAL)
                break;
            consumeOneBlock(iff);
        }
        if (iff.getNumberOfBlocksLeft() == 0 || !iff.isCurrentForm() || iff.getCurrentName() != TAG_DATA_LOCAL)
        {
            iff.exitForm(versionTag);
            iff.exitForm(TAG_DTLA_LOCAL);
            return false;
        }
        std::string l0Path;
        std::string firstPath;
        iff.enterForm(TAG_DATA_LOCAL);
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_CHLD_LOCAL)
            {
                iff.enterChunk(TAG_CHLD_LOCAL);
                const int32 childId = iff.read_int32();
                std::string childName = iff.read_stdstring();
                iff.exitChunk(TAG_CHLD_LOCAL);
                std::string childPath = childName;
                if (childPath.find("appearance/") != 0 && childPath.find("appearance\\") != 0)
                    childPath = std::string("appearance/") + childPath;
                if (firstPath.empty())
                    firstPath = childPath;
                if (childId == 0)
                    l0Path = std::move(childPath);
            }
            else
                consumeOneBlock(iff);
        }
        iff.exitForm(TAG_DATA_LOCAL);
        iff.exitForm(versionTag);
        iff.exitForm(TAG_DTLA_LOCAL);
        outPath = !l0Path.empty() ? l0Path : firstPath;
        return !outPath.empty();
    }
}

std::string resolveStaticMeshFilePathForImport(const std::string& userPath, const std::string& contextAnchor)
{
    static const Tag kMesh = TAG(M, E, S, H);
    static const Tag kApt = TAG3(A, P, T);
    static const Tag kMlod = TAG(M, L, O, D);
    static const Tag kDtla = TAG(D, T, L, A);

    std::string path = userPath;
    for (auto& c : path)
        if (c == '\\')
            c = '/';

    std::string ctx = contextAnchor.empty() ? path : contextAnchor;
    for (auto& c : ctx)
        if (c == '\\')
            c = '/';

    path = resolvePathViaApt(path);
    normalizeHighestLodSuffixInPath(path);

    constexpr int kMaxHops = 24;
    for (int hop = 0; hop < kMaxHops; ++hop)
    {
        Iff iff;
        if (!iff.open(path.c_str(), true))
            return path;

        const Tag root = iff.getCurrentName();
        if (root == kMesh)
        {
            iff.close();
            return path;
        }

        if (root == kApt)
        {
            iff.enterForm(kApt);
            iff.enterForm(TAG_0000);
            iff.enterChunk(TAG_NAME);
            std::string redirect = iff.read_stdstring();
            iff.exitChunk(TAG_NAME);
            iff.exitForm(TAG_0000);
            iff.exitForm(kApt);
            iff.close();
            if (redirect.empty())
                return path;
            const std::string prev = path;
            path = resolveStaticMeshPath(lod_path_helpers::resolveTreeFilePath(redirect, ctx));
            ctx = prev;
            path = resolvePathViaApt(path);
            normalizeHighestLodSuffixInPath(path);
            continue;
        }

        if (root == kMlod)
        {
            iff.enterForm(kMlod);
            iff.enterForm(TAG_0000);
            iff.enterChunk(TAG_INFO);
            const int16 detailLevelCount = static_cast<int16>(iff.read_int16());
            iff.exitChunk(TAG_INFO);
            std::string firstRel;
            for (int16 i = 0; i < detailLevelCount; ++i)
            {
                iff.enterChunk(TAG_NAME);
                std::string s = iff.read_stdstring();
                iff.exitChunk(TAG_NAME);
                if (i == 0)
                    firstRel = std::move(s);
            }
            iff.exitForm(TAG_0000);
            iff.exitForm(kMlod);
            iff.close();
            if (firstRel.empty())
                return path;
            std::string rel = firstRel;
            if (rel.find("appearance/") != 0 && rel.find("appearance\\") != 0)
                rel = std::string("appearance/") + rel;
            const std::string prev = path;
            path = resolveStaticMeshPath(lod_path_helpers::resolveTreeFilePath(rel, ctx));
            ctx = prev;
            path = resolvePathViaApt(path);
            normalizeHighestLodSuffixInPath(path);
            continue;
        }

        if (root == kDtla)
        {
            std::string rel;
            if (!extractDtlaL0Path(iff, rel))
            {
                iff.close();
                return path;
            }
            iff.close();
            const std::string prev = path;
            path = resolveStaticMeshPath(lod_path_helpers::resolveTreeFilePath(rel, ctx));
            ctx = prev;
            path = resolvePathViaApt(path);
            normalizeHighestLodSuffixInPath(path);
            continue;
        }

        iff.close();
        return path;
    }
    return path;
}

void* ImportLodMesh::creator()
{
    return new ImportLodMesh();
}

MStatus ImportLodMesh::doIt(const MArgList& args)
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
        if (argName == "-i" && (i + 1) < argCount)
        {
            filename = args.asString(i + 1, &status).asChar();
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
        std::cerr << "ImportLodMesh: missing -i <filename>" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);
    filename = resolveLodOrAptPath(filename);

    lodLog("Opening: %s", filename.c_str());
    Iff iff;
    if (!iff.open(filename.c_str(), false))
    {
        lodLog("FAILED to open file: %s", filename.c_str());
        std::cerr << "ImportLodMesh: failed to open " << filename << std::endl;
        return MS::kFailure;
    }

    Tag topForm = iff.getCurrentName();

    if (topForm == TAG_APT)
    {
        iff.enterForm(TAG_APT);
        iff.enterForm(TAG_0000);
        iff.enterChunk(TAG_NAME);
        std::string redirectPath = iff.read_stdstring();
        iff.exitChunk(TAG_NAME);
        iff.exitForm(TAG_0000);
        iff.exitForm(TAG_APT);
        iff.close();

        std::string resolvedRedirect = lod_path_helpers::resolveTreeFilePath(redirectPath, filename);
        filename = resolveLodOrAptPath(resolvedRedirect);
        lodLog("APT redirect -> %s", filename.c_str());
        if (!iff.open(filename.c_str(), false))
        {
            lodLog("FAILED to open redirected file: %s", filename.c_str());
            std::cerr << "ImportLodMesh: failed to open " << filename << std::endl;
            return MS::kFailure;
        }
        topForm = iff.getCurrentName();
    }

    lodLog("Top form: %s", (topForm == TAG_MLOD) ? "MLOD" : (topForm == TAG_DTLA) ? "DTLA" : (topForm == TAG_MESH) ? "MESH" : (topForm == TAG_APT) ? "APT" : "UNKNOWN");
    std::vector<MayaSceneBuilder::HardpointData> dtlaAppearanceHardpoints;
    std::vector<std::string> detailLevelPaths;
    std::vector<Vector> radarVerts, testVerts;
    std::vector<int> radarIndices, testIndices;

    if (topForm == TAG_DTLA)
    {
        lodLog("Parsing DTLA format");
        iff.enterForm(TAG_DTLA);
        Tag versionTag = iff.getCurrentName();
        iff.enterForm(versionTag);

        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
            {
                Tag formTag = iff.getCurrentName();
                if (formTag == TAG_APPR)
                {
                    std::string apprFloorScratch;
                    const size_t hpBefore = dtlaAppearanceHardpoints.size();
                    if (SwgMshImport::parseFullApprFormForHardpoints(iff, dtlaAppearanceHardpoints, apprFloorScratch))
                        lodLog("  APPR: +%zu hardpoint(s)", dtlaAppearanceHardpoints.size() - hpBefore);
                }
                else if (formTag == TAG_RADR)
                {
                    iff.enterForm(TAG_RADR);
                    if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_INFO)
                    {
                        iff.enterChunk(TAG_INFO);
                        int hasRadar = iff.read_int32();
                        iff.exitChunk(TAG_INFO);
                        if (hasRadar && iff.getNumberOfBlocksLeft() > 0)
                            readIdtl(iff, radarVerts, radarIndices);
                    }
                    iff.exitForm(TAG_RADR);
                }
                else if (formTag == TAG_TEST)
                {
                    iff.enterForm(TAG_TEST);
                    if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_INFO)
                    {
                        iff.enterChunk(TAG_INFO);
                        int hasTest = iff.read_int32();
                        iff.exitChunk(TAG_INFO);
                        if (hasTest && iff.getNumberOfBlocksLeft() > 0)
                            readIdtl(iff, testVerts, testIndices);
                    }
                    iff.exitForm(TAG_TEST);
                }
                else if (formTag == TAG_DATA)
                {
                    iff.enterForm(TAG_DATA);
                    while (iff.getNumberOfBlocksLeft() > 0)
                    {
                        if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_CHLD)
                        {
                            iff.enterChunk(TAG_CHLD);
                            iff.read_int32();
                            detailLevelPaths.push_back(iff.read_stdstring());
                            iff.exitChunk(TAG_CHLD);
                        }
                        else if (iff.isCurrentForm())
                            skipForm(iff);
                        else
                        {
                            iff.enterChunk();
                            iff.exitChunk();
                        }
                    }
                    iff.exitForm(TAG_DATA);
                }
                else
                    skipForm(iff);
            }
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
        iff.exitForm(versionTag);
        iff.exitForm(TAG_DTLA);
    }
    else if (topForm == TAG_MLOD)
    {
        lodLog("Parsing MLOD format");
        iff.enterForm(TAG_MLOD);
        iff.enterForm(TAG_0000);

        int16 detailLevelCount = 0;
        iff.enterChunk(TAG_INFO);
        detailLevelCount = iff.read_int16();
        iff.exitChunk(TAG_INFO);

        for (int16 i = 0; i < detailLevelCount; ++i)
        {
            iff.enterChunk(TAG_NAME);
            detailLevelPaths.push_back(iff.read_stdstring());
            iff.exitChunk(TAG_NAME);
        }

        iff.exitForm(TAG_0000);
        iff.exitForm(TAG_MLOD);
    }
    else if (topForm == TAG_MESH)
    {
        lodLog("Direct MESH format, using createMeshFromMsh");
        iff.close();
        MString rootPath;
        MString parentPathStr(parentPath.c_str());
        status = MshTranslator::createMeshFromMsh(filename.c_str(), rootPath, parentPathStr);
        if (!status)
        {
            std::cerr << "ImportLodMesh: failed to import .msh " << filename << std::endl;
            return status;
        }
        return MS::kSuccess;
    }
    else
    {
        lodLog("FAILED: unknown format (expected MLOD, DTLA, or MESH)");
        std::cerr << "ImportLodMesh: unknown format (expected MLOD, DTLA, or MESH)" << std::endl;
        return MS::kFailure;
    }

    std::string baseName = filename;
    {
        const size_t lastSlash = baseName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            baseName = baseName.substr(lastSlash + 1);
        const size_t dot = baseName.find_last_of('.');
        if (dot != std::string::npos)
            baseName = baseName.substr(0, dot);
    }

    if (detailLevelPaths.empty())
    {
        lodLog("No detail level paths, returning");
        return MS::kSuccess;
    }

    lodLog("LOD paths: %zu levels", detailLevelPaths.size());
    for (size_t p = 0; p < detailLevelPaths.size(); ++p)
        lodLog("  [%zu] %s", p, detailLevelPaths[p].c_str());

    MStringArray lodGroupResult;
    MStringArray existsResult;
    MGlobal::executeCommand(MString("objExists \"") + baseName.c_str() + "\"", existsResult, false, false);
    const bool groupExists = (existsResult.length() > 0 && existsResult[0].asInt() != 0);

    if (groupExists)
    {
        lodGroupResult.append(MString(baseName.c_str()));
    }
    else
    {
        MString melCmd = "group -em -n \"";
        melCmd += baseName.c_str();
        melCmd += "\"";
        status = MGlobal::executeCommand(melCmd, lodGroupResult);
        if (!status)
        {
            std::cerr << "ImportLodMesh: failed to create LOD group" << std::endl;
            return status;
        }

        MString createLodCmd = "createNode lodGroup -p \"";
        createLodCmd += lodGroupResult[0];
        createLodCmd += "\"";
        MGlobal::executeCommand(createLodCmd);
    }

    // Load primary LOD only (highest detail) - l0 = highest, l3 = lowest; loading all LODs causes freeze
    const size_t lodIndex = 0;
    for (size_t i = lodIndex; i <= lodIndex && i < detailLevelPaths.size(); ++i)
    {
        const std::string& relativePath = detailLevelPaths[i];
        std::string resolvedPath = lod_path_helpers::resolveTreeFilePath(relativePath, filename);
        resolvedPath = lod_path_helpers::resolveMeshPath(resolvedPath);

        lodLog("Loading LOD[%zu]: %s -> %s", i, relativePath.c_str(), resolvedPath.c_str());

        char lodLevelName[32];
        sprintf(lodLevelName, "l0");
        std::string lodPath = std::string(lodGroupResult[0].asChar()) + "|" + lodLevelName;

        MStringArray lodExistsResult;
        MGlobal::executeCommand(MString("objExists \"") + lodPath.c_str() + "\"", lodExistsResult, false, false);
        const bool lodExists = (lodExistsResult.length() > 0 && lodExistsResult[0].asInt() != 0);
        if (!lodExists)
        {
            MString melCmd = "createNode transform -n \"";
            melCmd += lodLevelName;
            melCmd += "\" -p \"";
            melCmd += lodGroupResult[0];
            melCmd += "\"";
            MGlobal::executeCommand(melCmd);
        }

        bool importOk = false;
        bool usedMFileImport = false;
        if (resolvedPath.size() >= 4 && resolvedPath.compare(resolvedPath.size() - 4, 4, ".msh") == 0)
        {
            lodLog("  createMeshFromMsh (static mesh only, no fallback)");
            MString rootPath;
            status = MshTranslator::createMeshFromMsh(resolvedPath.c_str(), rootPath, MString(lodPath.c_str()));
            lodLog("  createMeshFromMsh: %s", (status && rootPath.length() > 0) ? "OK" : "FAILED");
            if (status && rootPath.length() > 0)
            {
                importOk = true;
                usedMFileImport = true;
            }
        }
        else
        {
            lodLog("  importSkeletalMesh (for .mgn/.lmg/skeletal)");
            MString importCmd = "importSkeletalMesh -i \"";
            importCmd += resolvedPath.c_str();
            importCmd += "\" -parent \"";
            importCmd += lodPath.c_str();
            importCmd += "\"";
            status = MGlobal::executeCommand(importCmd, true, true);
            lodLog("  importSkeletalMesh: %s", status ? "OK" : "FAILED");
            if (status)
                importOk = true;
        }
        if (!importOk)
        {
            std::cerr << "ImportLodMesh: failed to import LOD " << resolvedPath << std::endl;
            continue;
        }

        if (!usedMFileImport)
        {
            std::string meshName = relativePath;
            {
                const size_t lastSlash = meshName.find_last_of("/\\");
                if (lastSlash != std::string::npos)
                    meshName = meshName.substr(lastSlash + 1);
                const size_t dotPos = meshName.find_last_of('.');
                if (dotPos != std::string::npos)
                    meshName = meshName.substr(0, dotPos);
            }
            MString parentCmd = "parent \"";
            parentCmd += meshName.c_str();
            parentCmd += "\" \"";
            parentCmd += lodPath.c_str();
            parentCmd += "\"";
            MGlobal::executeCommand(parentCmd);
        }

        if ((!radarVerts.empty() || !testVerts.empty()) && status)
        {
            if (!radarVerts.empty())
                createShapeMesh(radarVerts, radarIndices, "radar", lodPath);
            if (!testVerts.empty())
                createShapeMesh(testVerts, testIndices, "test", lodPath);
        }
    }

    if (!dtlaAppearanceHardpoints.empty() && lodGroupResult.length() > 0)
    {
        MString l0Path = lodGroupResult[0] + "|l0";
        MSelectionList hpSel;
        if (hpSel.add(l0Path) == MS::kSuccess)
        {
            MDagPath l0Dag;
            if (hpSel.getDagPath(0, l0Dag) == MS::kSuccess)
            {
                std::map<std::string, MDagPath> emptyJointMap;
                const std::string hpMeshName = baseName + std::string("_appearance");
                MStatus hpSt = MayaSceneBuilder::createHardpoints(dtlaAppearanceHardpoints, emptyJointMap, hpMeshName, l0Dag.node());
                lodLog("DTLA appearance hardpoints on l0: count=%zu %s", dtlaAppearanceHardpoints.size(), hpSt ? "OK" : "FAILED");
            }
        }
    }

    if (!parentPath.empty() && lodGroupResult.length() > 0)
    {
        MString parentCmd = "parent \"";
        parentCmd += lodGroupResult[0];
        parentCmd += "\" \"";
        parentCmd += parentPath.c_str();
        parentCmd += "\"";
        MGlobal::executeCommand(parentCmd);
    }

    lodLog("Import complete");
    return MS::kSuccess;
}
