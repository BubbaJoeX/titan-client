#include "flr.h"
#include "SwgTranslatorNames.h"

#include "CollisionEnums.h"
#include "FloorTri.h"
#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Tag.h"
#include "Vector.h"
#include "MayaSceneBuilder.h"
#include "MayaUtility.h"
#include "SwgImportTrace.h"

#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MDagModifier.h>
#include <maya/MGlobal.h>
#include <maya/MPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    const Tag TAG_FLOR = TAG(F,L,O,R);
    const Tag TAG_VERT = TAG(V,E,R,T);
    const Tag TAG_TRIS = TAG(T,R,I,S);
    const Tag TAG_PNOD = TAG(P,N,O,D);
    const Tag TAG_PEDG = TAG(P,E,D,G);

    static void flrLog(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        SwgImportTrace::logV("FlrTranslator", fmt, args);
        va_end(args);
    }

    static void skipOneIffBlock(Iff& iff)
    {
        if (iff.getNumberOfBlocksLeft() <= 0)
            return;
        if (iff.isCurrentForm())
        {
            iff.enterForm();
            while (iff.getNumberOfBlocksLeft() > 0)
                skipOneIffBlock(iff);
            iff.exitForm();
        }
        else
        {
            iff.enterChunk();
            iff.exitChunk();
        }
    }

    static void appendTriCorners(const FloorTri& tri, std::vector<int>& indices)
    {
        indices.push_back(tri.getCornerIndex(0));
        indices.push_back(tri.getCornerIndex(1));
        indices.push_back(tri.getCornerIndex(2));
    }

    static bool readFlorVertices(Iff& iff, Tag versionTag, std::vector<Vector>& vertices)
    {
        iff.enterChunk(TAG_VERT);
        if (versionTag == TAG_0006)
        {
            const int vertexCount = iff.read_int32();
            vertices.resize(static_cast<size_t>(vertexCount));
            for (int i = 0; i < vertexCount; ++i)
                vertices[static_cast<size_t>(i)] = iff.read_floatVector();
        }
        else
        {
            while (iff.getChunkLengthLeft() > 0)
                vertices.push_back(iff.read_floatVector());
        }
        iff.exitChunk(TAG_VERT);
        return !vertices.empty();
    }

    static bool readFlorTriangles(Iff& iff, Tag versionTag, std::vector<int>& indices)
    {
        iff.enterChunk(TAG_TRIS);
        if (versionTag == TAG_0000)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.setIndex(iff.read_int32());
                tri.setCornerIndex(0, iff.read_int32());
                tri.setCornerIndex(1, iff.read_int32());
                tri.setCornerIndex(2, iff.read_int32());
                iff.read_int32();
                iff.read_int32();
                iff.read_int32();
                iff.read_floatVector();
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0001)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.setIndex(iff.read_int32());
                tri.setCornerIndex(0, iff.read_int32());
                tri.setCornerIndex(1, iff.read_int32());
                tri.setCornerIndex(2, iff.read_int32());
                iff.read_int32();
                iff.read_int32();
                iff.read_int32();
                iff.read_floatVector();
                iff.read_bool8();
                iff.read_bool8();
                iff.read_bool8();
                iff.read_bool8();
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0002)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.read_0000(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0003 || versionTag == TAG_0004)
        {
            while (iff.getChunkLengthLeft() > 0)
            {
                FloorTri tri;
                tri.read_0001(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0005)
        {
            const int triCount = iff.getChunkLengthTotal() / FloorTri::getOnDiskSize_0001();
            for (int i = 0; i < triCount; ++i)
            {
                FloorTri tri;
                tri.read_0001(iff);
                appendTriCorners(tri, indices);
            }
        }
        else if (versionTag == TAG_0006)
        {
            const int triCount = iff.read_int32();
            for (int i = 0; i < triCount; ++i)
            {
                FloorTri tri;
                tri.read_0002(iff);
                appendTriCorners(tri, indices);
            }
        }
        else
        {
            iff.exitChunk(TAG_TRIS);
            return false;
        }
        iff.exitChunk(TAG_TRIS);
        return indices.size() >= 3;
    }

    static void skipFlorTailBlocks(Iff& iff, Tag versionTag)
    {
        while (!iff.atEndOfForm())
        {
            if (versionTag == TAG_0004 && !iff.isCurrentForm())
            {
                const Tag chunkTag = iff.getCurrentName();
                if (chunkTag == TAG_PNOD || chunkTag == TAG_PEDG)
                {
                    iff.enterChunk(chunkTag);
                    iff.exitChunk(chunkTag);
                    continue;
                }
            }
            skipOneIffBlock(iff);
        }
    }

    /**
     * Read FLOR/0000..0006 geometry (VERT + TRIS). Skips pathfinding / box-tree tail blocks.
     * Matches sharedCollision/FloorMesh.cpp version matrix.
     */
    static bool readFlorGeometry(Iff& iff, Tag versionTag, std::vector<Vector>& vertices, std::vector<int>& indices)
    {
        iff.enterForm(versionTag);
        if (!readFlorVertices(iff, versionTag, vertices))
        {
            iff.exitForm(versionTag);
            return false;
        }
        if (!readFlorTriangles(iff, versionTag, indices))
        {
            iff.exitForm(versionTag);
            return false;
        }
        skipFlorTailBlocks(iff, versionTag);
        iff.exitForm(versionTag);
        return true;
    }

    static bool loadFlorFile(const char* flrPath, std::vector<Vector>& vertices, std::vector<int>& indices)
    {
        if (!flrPath || !flrPath[0] || !Iff::isValid(flrPath))
            return false;
        Iff iff;
        if (!iff.open(flrPath, false))
            return false;
        if (iff.getCurrentName() != TAG_FLOR)
            return false;

        iff.enterForm(TAG_FLOR);
        const Tag versionTag = iff.getCurrentName();
        const bool ok =
            (versionTag == TAG_0000 || versionTag == TAG_0001 || versionTag == TAG_0002 ||
             versionTag == TAG_0003 || versionTag == TAG_0004 || versionTag == TAG_0005 ||
             versionTag == TAG_0006)
                ? readFlorGeometry(iff, versionTag, vertices, indices)
                : false;
        iff.exitForm(TAG_FLOR);
        return ok;
    }

    static MStatus buildFlrPreviewMesh(
        const std::vector<Vector>& vertices,
        const std::vector<int>& indices,
        const char* meshName,
        MObject parentObj,
        MDagPath& outPath)
    {
        if (vertices.empty() || indices.size() < 3)
            return MS::kFailure;

        std::vector<float> positions;
        positions.reserve(vertices.size() * 3);
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            positions.push_back(vertices[i].x);
            positions.push_back(vertices[i].y);
            positions.push_back(vertices[i].z);
        }
        std::vector<float> normals(positions.size(), 0.0f);

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

        MStatus status = MayaSceneBuilder::createMesh(positions, normals, groups, meshName ? meshName : "floor", outPath);
        if (!status)
            return status;
        status = MayaSceneBuilder::assignPobCollisionPreviewMaterial(outPath);
        if (!status)
            flrLog("assignPobCollisionPreviewMaterial failed (floor preview shading)");
        if (!parentObj.isNull())
        {
            MDagPath meshTransformPath = outPath;
            if (meshTransformPath.hasFn(MFn::kMesh))
                meshTransformPath.pop(1);
            MDagModifier mod;
            status = mod.reparentNode(meshTransformPath.node(), parentObj);
            if (status)
                status = mod.doIt();
        }
        return status;
    }
}

void* FlrTranslator::creator()
{
    return new FlrTranslator();
}

MStatus FlrTranslator::createMeshFromFlr(const char* flrPath, const char* meshName, MObject parentObj, MDagPath& outPath)
{
    flrLog("createMeshFromFlr: %s", flrPath ? flrPath : "(null)");
    std::vector<Vector> vertices;
    std::vector<int> indices;
    if (!loadFlorFile(flrPath, vertices, indices))
    {
        flrLog("Failed to parse FLOR geometry");
        return MS::kFailure;
    }

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MStatus status = buildFlrPreviewMesh(vertices, indices, meshName, parentObj, outPath);
    if (!status)
        flrLog("createMesh failed");
    else
        flrLog("createMeshFromFlr OK");
    return status;
}

MStatus FlrTranslator::reader(const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    std::string fileName = MayaUtility::fileObjectPathForIdentify(file);
    if (fileName.empty())
        fileName = file.expandedFullName().asChar();
    flrLog("reader: %s", fileName.c_str());
    if (!Iff::isValid(fileName.c_str()))
    {
        std::cerr << fileName << " could not be read as a valid IFF file!" << std::endl;
        return MS::kFailure;
    }

    std::vector<Vector> vertices;
    std::vector<int> indices;
    if (!loadFlorFile(fileName.c_str(), vertices, indices))
    {
        std::cerr << "FlrTranslator: failed to parse FLOR geometry in " << fileName << std::endl;
        return MS::kFailure;
    }

    if (vertices.empty() || indices.size() < 3)
    {
        std::cerr << "FlrTranslator: no geometry in " << fileName << std::endl;
        return MS::kSuccess;
    }

    std::string meshName = fileName;
    const size_t lastSlash = meshName.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        meshName = meshName.substr(lastSlash + 1);
    const size_t dot = meshName.find_last_of('.');
    if (dot != std::string::npos)
        meshName = meshName.substr(0, dot);

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MDagPath meshPath;
    MStatus status = buildFlrPreviewMesh(vertices, indices, meshName.c_str(), MObject::kNullObj, meshPath);
    if (!status)
    {
        flrLog("createMesh failed");
        std::cerr << "FlrTranslator: failed to create mesh" << std::endl;
        return MS::kFailure;
    }
    flrLog("reader OK");
    return MS::kSuccess;
}

/**
 * Handles writing out (exporting) the floor
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus FlrTranslator::writer(const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    const char* fileName = file.expandedFullName().asChar();
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    MDagPath meshPath;
    bool found = false;
    for (unsigned i = 0; i < sel.length(); ++i)
    {
        if (sel.getDagPath(i, meshPath) && meshPath.hasFn(MFn::kMesh))
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        std::cerr << "FlrTranslator: select a mesh to export" << std::endl;
        return MS::kFailure;
    }

    MFnMesh meshFn(meshPath);
    MPointArray mayaPoints;
    MStatus meshStatus = meshFn.getPoints(mayaPoints, MSpace::kObject);
    if (!meshStatus)
    {
        std::cerr << "FlrTranslator: failed to get mesh points" << std::endl;
        return MS::kFailure;
    }
    const unsigned numVerts = mayaPoints.length();
    if (numVerts < 3)
    {
        std::cerr << "FlrTranslator: mesh has too few vertices" << std::endl;
        return MS::kFailure;
    }

    std::vector<Vector> vertices(static_cast<size_t>(numVerts));
    for (unsigned i = 0; i < numVerts; ++i)
    {
        vertices[static_cast<size_t>(i)].x = static_cast<float>(-mayaPoints[i].x);
        vertices[static_cast<size_t>(i)].y = static_cast<float>(mayaPoints[i].y);
        vertices[static_cast<size_t>(i)].z = static_cast<float>(mayaPoints[i].z);
    }

    MItMeshPolygon polyIt(meshPath);
    std::vector<int> indices;
    std::vector<Vector> triNormals;
    MIntArray polyVerts;
    for (; !polyIt.isDone(); polyIt.next())
    {
        if (polyIt.polygonVertexCount() != 3)
        {
            std::cerr << "FlrTranslator: non-triangular face found, triangulate mesh first" << std::endl;
            return MS::kFailure;
        }
        polyIt.getVertices(polyVerts);
        if (polyVerts.length() < 3) continue;
        int idx[3] = { static_cast<int>(polyVerts[0]), static_cast<int>(polyVerts[1]), static_cast<int>(polyVerts[2]) };
        indices.push_back(idx[0]);
        indices.push_back(idx[1]);
        indices.push_back(idx[2]);
        Vector v0(vertices[static_cast<size_t>(idx[0])]);
        Vector v1(vertices[static_cast<size_t>(idx[1])]);
        Vector v2(vertices[static_cast<size_t>(idx[2])]);
        Vector e1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        Vector e2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
        float nx = e1.y * e2.z - e1.z * e2.y;
        float ny = e1.z * e2.x - e1.x * e2.z;
        float nz = e1.x * e2.y - e1.y * e2.x;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-6f)
        {
            nx /= len;
            ny /= len;
            nz /= len;
        }
        triNormals.push_back(Vector(nx, ny, nz));
    }

    Iff iff(8192, true);
    iff.insertForm(TAG_FLOR);
    iff.insertForm(TAG_0006);

    iff.insertChunk(TAG_VERT);
    iff.insertChunkData(static_cast<int32>(numVerts));
    for (unsigned i = 0; i < numVerts; ++i)
        iff.insertChunkFloatVector(vertices[static_cast<size_t>(i)]);
    iff.exitChunk(TAG_VERT);

    iff.insertChunk(TAG_TRIS);
    const int triCount = static_cast<int>(indices.size() / 3);
    iff.insertChunkData(triCount);
    for (int t = 0; t < triCount; ++t)
    {
        const int c0 = indices[static_cast<size_t>(t * 3 + 0)];
        const int c1 = indices[static_cast<size_t>(t * 3 + 1)];
        const int c2 = indices[static_cast<size_t>(t * 3 + 2)];
        iff.insertChunkData(c0);
        iff.insertChunkData(c1);
        iff.insertChunkData(c2);
        iff.insertChunkData(t);
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkFloatVector(triNormals[static_cast<size_t>(t)]);
        iff.insertChunkData(static_cast<uint8>(FET_Uncrossable));
        iff.insertChunkData(static_cast<uint8>(FET_Uncrossable));
        iff.insertChunkData(static_cast<uint8>(FET_Uncrossable));
        iff.insertChunkData(static_cast<uint8>(false));
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkData(static_cast<int32>(-1));
        iff.insertChunkData(static_cast<int32>(-1));
    }
    iff.exitChunk(TAG_TRIS);

    iff.exitForm(TAG_0006);
    iff.exitForm(TAG_FLOR);

    if (!iff.write(fileName))
    {
        std::cerr << "FlrTranslator: failed to write " << fileName << std::endl;
        return MS::kFailure;
    }
    return MS::kSuccess;
}

/**
 * @return the file type this translator handles
 */
MString FlrTranslator::defaultExtension () const
{
    return "flr";
}

MString FlrTranslator::filter () const
{
    return MString(swg_translator::kFilterFlr);
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind FlrTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && !strcasecmp(pathStr.c_str() + nameLength - 4, ".flr"))
        return kCouldBeMyFileType;
    return kNotMyFileType;
}
