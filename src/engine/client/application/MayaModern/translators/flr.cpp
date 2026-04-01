#include "flr.h"
#include "SwgTranslatorNames.h"

#include "CollisionEnums.h"
#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Tag.h"
#include "Vector.h"
#include "MayaSceneBuilder.h"
#include "MayaUtility.h"

#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
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

    static void flrLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[FlrTranslator] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }
}

// creates an instance of the FlrTranslator
void* FlrTranslator::creator()
{
    return new FlrTranslator();
}

/**
 * Read FLOR version 0006: VERT (int32 count + vectors), TRIS (int32 count + FloorTri_0002 each).
 * FloorTri_0002: corners(3*int32), index(int32), neighbors(3*int32), normal(Vector),
 *   edgeTypes(3*uint8), fallthrough(bool8), partTag(int32), portalIds(3*int32).
 */
static bool readFlor0006(Iff& iff, std::vector<Vector>& vertices, std::vector<int>& indices)
{
    iff.enterForm(TAG_0006);

    iff.enterChunk(TAG_VERT);
    const int vertexCount = iff.read_int32();
    vertices.resize(static_cast<size_t>(vertexCount));
    for (int i = 0; i < vertexCount; ++i)
        vertices[static_cast<size_t>(i)] = iff.read_floatVector();
    iff.exitChunk(TAG_VERT);

    iff.enterChunk(TAG_TRIS);
    const int triCount = iff.read_int32();
    indices.reserve(static_cast<size_t>(triCount) * 3);
    for (int i = 0; i < triCount; ++i)
    {
        const int c0 = iff.read_int32();
        const int c1 = iff.read_int32();
        const int c2 = iff.read_int32();
        iff.read_int32();
        iff.read_int32();
        iff.read_int32();
        iff.read_int32();
        iff.read_floatVector();
        iff.read_uint8();
        iff.read_uint8();
        iff.read_uint8();
        iff.read_bool8();
        iff.read_int32();
        iff.read_int32();
        iff.read_int32();
        iff.read_int32();
        indices.push_back(c0);
        indices.push_back(c1);
        indices.push_back(c2);
    }
    iff.exitChunk(TAG_TRIS);

    iff.exitForm(TAG_0006);
    return true;
}

MStatus FlrTranslator::createMeshFromFlr(const char* flrPath, const char* meshName, MObject parentObj, MDagPath& outPath)
{
    flrLog("createMeshFromFlr: %s", flrPath ? flrPath : "(null)");
    if (!flrPath || !flrPath[0] || !Iff::isValid(flrPath))
        return MS::kFailure;
    Iff iff;
    if (!iff.open(flrPath, false))
    {
        flrLog("Failed to open file");
        return MS::kFailure;
    }
    const Tag tagFlor = TAG(F,L,O,R);
    if (iff.getCurrentName() != tagFlor)
        return MS::kFailure;

    iff.enterForm(tagFlor);
    std::vector<Vector> vertices;
    std::vector<int> indices;

    const Tag versionTag = iff.getCurrentName();
    if (versionTag == TAG_0006)
    {
        if (!readFlor0006(iff, vertices, indices))
        {
            iff.exitForm(tagFlor);
            return MS::kFailure;
        }
    }
    else
    {
        iff.exitForm(tagFlor);
        return MS::kFailure;
    }
    iff.exitForm(tagFlor);

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

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MStatus status = MayaSceneBuilder::createMesh(positions, normals, groups, meshName ? meshName : "floor", outPath);
    if (!status)
    {
        flrLog("createMesh failed");
        return status;
    }
    flrLog("createMeshFromFlr OK");

    if (!parentObj.isNull())
    {
        MDagPath meshTransformPath = outPath;
        if (meshTransformPath.hasFn(MFn::kMesh))
            meshTransformPath.pop(1);
        MFnDagNode parentFn(parentObj);
        MString parentCmd = "parent \"";
        parentCmd += meshTransformPath.fullPathName();
        parentCmd += "\" \"";
        parentCmd += parentFn.fullPathName();
        parentCmd += "\"";
        status = MGlobal::executeCommand(parentCmd);
    }
    return status;
}

MStatus FlrTranslator::reader(const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    const char* fileName = file.expandedFullName().asChar();
    flrLog("reader: %s", fileName);
    if (!Iff::isValid(fileName))
    {
        std::cerr << fileName << " could not be read as a valid IFF file!" << std::endl;
        return MS::kFailure;
    }
    Iff iff;
    if (!iff.open(fileName, false))
    {
        std::cerr << fileName << " could not be opened" << std::endl;
        return MS::kFailure;
    }
    if (iff.getRawDataSize() < 1)
    {
        std::cerr << fileName << " was read in as an IFF but its size was empty!" << std::endl;
        return MS::kFailure;
    }

    if (iff.getCurrentName() != TAG_FLOR)
    {
        std::cerr << fileName << " is not a FLOR file" << std::endl;
        return MS::kFailure;
    }

    iff.enterForm(TAG_FLOR);
    std::vector<Vector> vertices;
    std::vector<int> indices;

    const Tag versionTag = iff.getCurrentName();
    if (versionTag == TAG_0006)
    {
        if (!readFlor0006(iff, vertices, indices))
        {
            iff.exitForm(TAG_FLOR);
            return MS::kFailure;
        }
    }
    else
    {
        std::cerr << "FlrTranslator: unsupported FLOR version" << std::endl;
        iff.exitForm(TAG_FLOR);
        return MS::kFailure;
    }

    iff.exitForm(TAG_FLOR);

    if (vertices.empty() || indices.size() < 3)
    {
        std::cerr << "FlrTranslator: no geometry in " << fileName << std::endl;
        return MS::kSuccess;
    }

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

    std::string meshName = fileName;
    const size_t lastSlash = meshName.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        meshName = meshName.substr(lastSlash + 1);
    const size_t dot = meshName.find_last_of('.');
    if (dot != std::string::npos)
        meshName = meshName.substr(0, dot);

    flrLog("Creating mesh: %zu vertices, %zu triangles", vertices.size(), indices.size() / 3);
    MDagPath meshPath;
    MStatus status = MayaSceneBuilder::createMesh(positions, normals, groups, meshName, meshPath);
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
