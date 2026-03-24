#include "msh.h"
#include "ExportStaticMesh.h"
#include "ImportLodMesh.h"

#include "Iff.h"
#include "Globals.h"
#include "MayaSceneBuilder.h"
#include "Misc.h"
#include "Quaternion.h"
#include "Transform.h"
#include "VertexBufferFormat.h"
#include "VertexBufferIterator.h"
#include "MayaUtility.h"

#include <maya/MDagPath.h>
#include <maya/MStatus.h>
#include <maya/MObject.h>
#include <maya/MString.h>
#include <maya/MVector.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MFileIO.h>
#include <maya/MFileObject.h>
#include <maya/MFnTransform.h>
#include <maya/MItDag.h>
#include <maya/MNamespace.h>
#include <maya/MSelectionList.h>

#include <maya/MIntArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>

#include <ios>
#include <cstring>
#include <set>
#include <string>
#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    static void mshLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[MshTranslator] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }

    MString s_parentPathForMshImport;
    MString s_createdRootPathForMshImport;

    static const Tag TAG_APPR = TAG(A,P,P,R);
    static const Tag TAG_HPTS = TAG(H,P,T,S);
    static const Tag TAG_HPNT = TAG(H,P,N,T);
    static const Tag TAG_FLOR = TAG(F,L,O,R);
}

MStatus MshTranslator::createMeshFromMsh(const char* mshPath, MString& outRootPath, const MString& parentPath)
{
    mshLog("createMeshFromMsh: %s", mshPath);
    std::set<std::string> before;
    for (MItDag it(MItDag::kDepthFirst, MFn::kTransform); !it.isDone(); it.next())
    {
        MDagPath path;
        it.getPath(path);
        if (path.length() == 1)
            before.insert(std::string(path.fullPathName().asChar()));
    }

    mshLog("  Scene has %zu root transforms before import", before.size());
    s_parentPathForMshImport = parentPath;
    s_createdRootPathForMshImport = MString();
    MFileObject fileObj;
    fileObj.setRawFullName(mshPath);
    MshTranslator* t = static_cast<MshTranslator*>(creator());
    mshLog("  Calling reader...");
    MStatus status = t->reader(fileObj, MString(), MPxFileTranslator::kImportAccessMode);
    mshLog("  Reader returned: %s", status ? "OK" : "FAILED");
    s_parentPathForMshImport = MString();
    delete t;
    if (!status) return status;

    if (s_createdRootPathForMshImport.length() > 0)
    {
        outRootPath = s_createdRootPathForMshImport;
        s_createdRootPathForMshImport = MString();
        return MS::kSuccess;
    }

    for (MItDag it(MItDag::kDepthFirst, MFn::kTransform); !it.isDone(); it.next())
    {
        MDagPath path;
        it.getPath(path);
        if (path.length() == 1 && before.find(std::string(path.fullPathName().asChar())) == before.end())
        {
            outRootPath = path.fullPathName();
            return MS::kSuccess;
        }
    }
    return MS::kFailure;
}

//Creates one instance of the MshTranslator
void* MshTranslator::creator()
{
    return new MshTranslator();
}

/**
 * Function that handles reading in the file (import/open operations)
 *
 * @param file the file
 * @param options options
 * @param mode file access mode
 * @return the operation success/failure
 */
MStatus MshTranslator::reader (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    const char* fileName = file.expandedFullName().asChar();
    mshLog("reader: %s", fileName);

    // APT is the entry point for static meshes. Open APT first, get redirect, load that.
    // Never load .msh when .apt exists.
    std::string pathToLoad = resolvePathViaApt(fileName);
    if (pathToLoad != fileName)
        mshLog("  APT redirect -> %s", pathToLoad.c_str());

    if(!Iff::isValid(pathToLoad.c_str()))
    {
        std::cerr << pathToLoad << " could not be read as a valid IFF file!" << std::endl;
        return MS::kFailure;
    }
    Iff iff;
    if(iff.open(pathToLoad.c_str(), false))
    {
        if(iff.getRawDataSize() < 1)
        {
            std::cerr << pathToLoad << " was read in as an IFF but its size was empty!" << std::endl;
            return MS::kFailure;
        }


        MFloatPointArray vertexArray;
        MIntArray polygonCounts;
        MIntArray polygonConnects;
        MFloatArray uArray;
        MFloatArray vArray;
        int totalVerticesInMesh = 0;
        int totalPolygonsInMesh = 0;

        iff.enterForm(TAG_MESH);
        iff.enterForm(TAG_0005); //todo handle version variation here
        mshLog("  Entered MESH/0005");

        std::vector<MayaSceneBuilder::HardpointData> hardpoints;
        std::string floorReferencePath;

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_APPR)
        {
            iff.enterForm(TAG_APPR);
            iff.enterForm(::TAG_0003);

            while (iff.getNumberOfBlocksLeft() > 0)
            {
                if (iff.isCurrentForm())
                {
                    const Tag formTag = iff.getCurrentName();
                    if (formTag == TAG_HPTS)
                    {
                        iff.enterForm(TAG_HPTS);
                        while (iff.getNumberOfBlocksLeft() > 0)
                        {
                            if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_HPNT)
                            {
                                iff.enterChunk(TAG_HPNT);
                                MayaSceneBuilder::HardpointData hp;
                                const Transform hpTransform = iff.read_floatTransform();
                                std::string hpName;
                                iff.read_string(hpName);
                                hp.name = hpName;
                                hp.parentJoint = "";
                                const Vector& pos = hpTransform.getPosition_p();
                                hp.position[0] = pos.x;
                                hp.position[1] = pos.y;
                                hp.position[2] = pos.z;
                                const Quaternion q(hpTransform);
                                hp.rotation[0] = static_cast<float>(q.x);
                                hp.rotation[1] = static_cast<float>(q.y);
                                hp.rotation[2] = static_cast<float>(q.z);
                                hp.rotation[3] = static_cast<float>(q.w);
                                hardpoints.push_back(hp);
                                iff.exitChunk(TAG_HPNT);
                            }
                            else if (iff.isCurrentForm())
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
                        iff.exitForm(TAG_HPTS);
                    }
                    else if (formTag == TAG_FLOR)
                    {
                        iff.enterForm(TAG_FLOR);
                        if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == ::TAG_DATA)
                        {
                            iff.enterChunk(::TAG_DATA);
                            if (iff.read_bool8())
                                iff.read_string(floorReferencePath);
                            iff.exitChunk(::TAG_DATA);
                        }
                        iff.exitForm(TAG_FLOR);
                    }
                    else
                    {
                        iff.enterForm();
                        iff.exitForm();
                    }
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(::TAG_0003);
            iff.exitForm(TAG_APPR);
            mshLog("  APPR: %zu hardpoints, floor=%s", hardpoints.size(), floorReferencePath.c_str());
        }

        if(iff.seekForm(TAG_SPS))
        {
            mshLog("  Found SPS form");
            iff.enterForm(TAG_SPS);
            iff.enterForm(TAG_0001); //todo validate version compatibility
            iff.enterChunk(TAG_CNT);
            const int32 numberOfShaders = iff.read_int32();
            iff.exitChunk(TAG_CNT);
            mshLog("  Shaders: %d", numberOfShaders);
            
            MObject parentObj = MObject::kNullObj;
            if (s_parentPathForMshImport.length() > 0)
            {
                MSelectionList sel;
                if (sel.add(s_parentPathForMshImport) == MS::kSuccess)
                {
                    MDagPath parentDag;
                    if (sel.getDagPath(0, parentDag))
                        parentObj = parentDag.node();
                }
            }
            
            MFnTransform parentTransform;
            if (!parentObj.isNull())
                parentTransform.create(parentObj);
            else
                parentTransform.create();
            parentTransform.setName(file.rawName());
            if (!parentObj.isNull())
            {
                MDagPath createdPath;
                if (MDagPath::getAPathTo(parentTransform.object(), createdPath))
                    s_createdRootPathForMshImport = createdPath.fullPathName();
            }
            
            for(int i = 0; i < numberOfShaders; i++)
            {
                mshLog("  Shader %d/%d", i + 1, numberOfShaders);
                iff.enterForm(); // e.g., 0001 for number of shader

                    iff.enterChunk(TAG_NAME);
                        char shaderName[1024];
                        iff.read_string(shaderName, sizeof(shaderName) - 1); //todo we're just doing nothing with this rn ?
                    iff.exitChunk(TAG_NAME);

                    iff.enterChunk(TAG_INFO);
                        const int numberOfPrimitives = iff.read_int32();
                        cout << "number of primitives for first shader is " << numberOfPrimitives << std::endl;
                    iff.exitChunk(TAG_INFO);


                    iff.enterForm(); // This is the version # of the LocalShaderPrimitiveTemplate
                        iff.enterChunk(TAG_INFO);
                            const int primitiveTypeInt = iff.read_int32();
                            cout << "primitive int type is: " << primitiveTypeInt << std::endl;
                            // todo VALIDATE_RANGE_INCLUSIVE_INCLUSIVE here against SPSPT:: list
                            const bool hasIndices = iff.read_bool8();
                            const bool hasSortedIndices = iff.read_bool8();
                            cout << "has sorted indices: " << hasSortedIndices << std::endl;
                        iff.exitChunk(TAG_INFO);


                        // Load Vertex Buffer (VertexBuffer::load_0003)
                        iff.enterForm(TAG_VTXA);
                            iff.enterForm(::TAG_0003); //todo validate version number
                                iff.enterChunk(TAG_INFO);

                                    // Read Vertex Buffer Bit Mask
                                    VertexBufferFormat vbf;
                                    vbf.setFlags(static_cast<VertexBufferFormat::Flags>(iff.read_uint32()));
                                    int numberOfTextureCoordinateSets = vbf.getNumberOfTextureCoordinateSets();

                                    cout << "number of Texture Coordinate Sets is: " << numberOfTextureCoordinateSets << std::endl;

                                    bool skipDot3 = false;
                                    //todo this is also supposed to check if [ClientGraphics] DOT3 is enabled but we don't have a good way to do that
                                    // and I'm not sure if there is a point based on assets we've tried reading in for editing
                                    if(numberOfTextureCoordinateSets > 0 && vbf.getTextureCoordinateSetDimension(numberOfTextureCoordinateSets-1) == 4)
                                    {
                                        --numberOfTextureCoordinateSets;
                                        vbf.setTextureCoordinateSetDimension(numberOfTextureCoordinateSets, 1);
                                        vbf.setNumberOfTextureCoordinateSets(numberOfTextureCoordinateSets);
                                        skipDot3 = true;
                                    }
                                    const int numberOfVertices = iff.read_int32();
                                    mshLog("    Vertices: %d (shader %d)", numberOfVertices, i + 1);

                                    totalVerticesInMesh += numberOfVertices;
                                iff.exitChunk(TAG_INFO);

                                iff.enterChunk(::TAG_DATA);
                                    int vertCount = 0;
                                    do
                                    {
                                        ++vertCount;
                                        if (vertCount == 1 || vertCount == numberOfVertices || (numberOfVertices > 1000 && vertCount % 10000 == 0))
                                            mshLog("    Vertex %d/%d", vertCount, numberOfVertices);
                                        if(vbf.hasPosition())
                                        {
                                            Vector pos = iff.read_floatVector();
                                            vertexArray.append(-pos.x, pos.y, pos.z);
                                        }
                                        if(vbf.isTransformed())
                                        {
                                            IGNORE_RETURN(iff.read_float());
                                        }
                                        if(vbf.hasNormal())
                                        {
                                            IGNORE_RETURN(iff.read_floatVector());
                                        }
                                        if(vbf.hasPointSize())
                                        {
                                            IGNORE_RETURN(iff.read_float());
                                        }
                                        if(vbf.hasColor0())
                                        {
                                            IGNORE_RETURN(iff.read_uint32());
                                        }
                                        if(vbf.hasColor1())
                                        {
                                            IGNORE_RETURN(iff.read_uint32());
                                        }

                                        for(int j = 0; j < numberOfTextureCoordinateSets; ++j)
                                        {
                                            const int dimension = vbf.getTextureCoordinateSetDimension(j);
                                            float u = 0.0f, v = 0.0f;
                                            if (dimension >= 1) u = iff.read_float();
                                            if (dimension >= 2) v = iff.read_float();
                                            for (int k = 2; k < dimension; ++k)
                                                IGNORE_RETURN(iff.read_float());
                                            if (j == 0)
                                            {
                                                uArray.append(u);
                                                vArray.append(1.0f - v);
                                            }
                                        }
                                        if(skipDot3)
                                        {
                                            IGNORE_RETURN(iff.read_float());
                                            IGNORE_RETURN(iff.read_float());
                                            IGNORE_RETURN(iff.read_float());
                                            IGNORE_RETURN(iff.read_float());
                                        }
                                    }
                                    while (!iff.atEndOfForm());


                                    cout << "at end of this block and now about to exit chunk DATA 0003" << std::endl;

                                iff.exitChunk(::TAG_DATA);
                            iff.exitForm(::TAG_0003);
                        iff.exitForm(TAG_VTXA);

                        // load normal index buffer
                        if(hasIndices)
                        {
                            cout << "starting load normal index buffer inside hasIndices" << std::endl;
                            iff.enterChunk(TAG_INDX);
                                const int numberOfIndices = iff.read_int32();
                                cout << "number of indices is " << numberOfIndices << std::endl;
                                totalPolygonsInMesh += numberOfIndices;
                                for(int m = 0; m < numberOfIndices; m++)
                                {
                                    polygonConnects.append(iff.read_uint16());
                                }
                            iff.exitChunk(TAG_INDX);
                        }
                        // load directional index buffers
                        if(hasSortedIndices) //todo do we need to do this ?
                        {
                            cout << "has sorted indices was true " << std::endl;
                        }
                    iff.exitForm(); // this exits the LocalShaderPrimitiveTemplate form
                iff.exitForm(); // this should exit the # of the shader form (end for loop)

                for(int p = 0; p < (totalPolygonsInMesh / 3); p++)
                {
                    polygonCounts.append(3); // mesh is made of triangles, so the vertex count of each polygon is 3
                }

                MStatus createStatus;
                try
                {
                    cerr << "preparing to create mesh: " << std::endl;
                    cerr << "total vertices in mesh: " << totalVerticesInMesh << std::endl;
                    cerr << "total polygons in mesh: " << (totalPolygonsInMesh / 3) << std::endl;
                    cerr << "vertex array length: " << vertexArray.length() << std::endl;
                    cerr << "polygon counts length: " << polygonCounts.length() << std::endl;
                    cerr << "polygon connects length: " << polygonCounts.length() << std::endl;

                    MFnMesh mesh;
                    mesh.create(totalVerticesInMesh, (totalPolygonsInMesh / 3), vertexArray, polygonCounts, polygonConnects, parentTransform.object(), &createStatus);
                    std::string shaderBaseName = MayaSceneBuilder::stripSGSuffixFromShaderName(shaderName);
                    mesh.setName(MayaUtility::parseFileNameToNodeName(shaderBaseName).c_str());

                    if (createStatus == MS::kSuccess)
                    {
                        if (uArray.length() == static_cast<unsigned>(totalVerticesInMesh) && uArray.length() > 0)
                        {
                            mesh.setUVs(uArray, vArray);
                            const int numFaces = totalPolygonsInMesh / 3;
                            for (int faceId = 0; faceId < numFaces; ++faceId)
                            {
                                mesh.assignUV(faceId, 0, polygonConnects[faceId * 3 + 0]);
                                mesh.assignUV(faceId, 1, polygonConnects[faceId * 3 + 1]);
                                mesh.assignUV(faceId, 2, polygonConnects[faceId * 3 + 2]);
                            }
                            mshLog("    UVs applied: %d vertices", totalVerticesInMesh);
                        }
                        MDagPath meshPath;
                        mesh.getPath(meshPath);
                        MayaSceneBuilder::ShaderGroupData sg;
                        sg.shaderTemplateName = shaderName;
                        for (int idx = 0; idx + 2 < totalPolygonsInMesh; idx += 3)
                        {
                            MayaSceneBuilder::TriangleData tri;
                            tri.indices[0] = polygonConnects[idx];
                            tri.indices[1] = polygonConnects[idx + 1];
                            tri.indices[2] = polygonConnects[idx + 2];
                            sg.triangles.push_back(tri);
                        }
                        if (!sg.triangles.empty())
                        {
                            mshLog("    assignMaterials: %s", shaderName);
                            std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
                            MayaSceneBuilder::assignMaterials(meshPath, groups, pathToLoad.c_str());
                            mshLog("    assignMaterials done");
                        }
                    }
                }
                catch (std::exception e)
                {
                    cerr << "EXCEPTION RAISED: " << e.what() << std::endl;
                }
                if(createStatus.statusCode() != MS::kSuccess)
                {
                    cerr << "MESH CREATE STATUS WAS NOT SUCCESS" << std::endl;
                    cerr << "MESH STATUS CODE: " << createStatus.errorString();
                }
                else
                {
                    cerr << "MESH CREATE STATUS WAS SUCCESS";
                }

                // reset everything for end of for loop
                totalVerticesInMesh = 0;
                totalPolygonsInMesh = 0;
                vertexArray.clear();
                polygonCounts.clear();
                polygonConnects.clear();
                uArray.clear();
                vArray.clear();
            }

            if (!hardpoints.empty())
            {
                std::string meshBaseName = MayaUtility::parseFileNameToNodeName(file.rawName().asChar());
                std::map<std::string, MDagPath> emptyJointMap;
                MStatus hpStatus = MayaSceneBuilder::createHardpoints(hardpoints, emptyJointMap, meshBaseName, parentTransform.object());
                if (hpStatus)
                    mshLog("  Created %zu hardpoints", hardpoints.size());
            }

            if (!floorReferencePath.empty())
            {
                MStatus floorStatus;
                MFnTransform floorFn;
                MObject floorObj = floorFn.create(parentTransform.object(), &floorStatus);
                if (floorStatus)
                {
                    floorFn.setName("floor_component");
                    MGlobal::executeCommand(MString("addAttr -ln \"floorPath\" -dt \"string\" ") + floorFn.name());
                    MPlug pathPlug = floorFn.findPlug("floorPath", false);
                    if (!pathPlug.isNull())
                        pathPlug.setValue(MString(floorReferencePath.c_str()));
                    mshLog("  Created floor component placeholder: %s", floorReferencePath.c_str());
                }
            }

            iff.close();
        }
        else
        {
            mshLog("  No SPS form - importing APPR only (hardpoints, floor)");
            MObject parentObj = MObject::kNullObj;
            if (s_parentPathForMshImport.length() > 0)
            {
                MSelectionList sel;
                if (sel.add(s_parentPathForMshImport) == MS::kSuccess)
                {
                    MDagPath parentDag;
                    if (sel.getDagPath(0, parentDag))
                        parentObj = parentDag.node();
                }
            }
            MFnTransform parentTransform;
            if (!parentObj.isNull())
                parentTransform.create(parentObj);
            else
                parentTransform.create();
            parentTransform.setName(file.rawName());
            if (!parentObj.isNull())
            {
                MDagPath createdPath;
                if (MDagPath::getAPathTo(parentTransform.object(), createdPath))
                    s_createdRootPathForMshImport = createdPath.fullPathName();
            }
            if (!hardpoints.empty())
            {
                std::string meshBaseName = MayaUtility::parseFileNameToNodeName(file.rawName().asChar());
                std::map<std::string, MDagPath> emptyJointMap;
                MStatus hpStatus = MayaSceneBuilder::createHardpoints(hardpoints, emptyJointMap, meshBaseName, parentTransform.object());
                if (hpStatus)
                    mshLog("  Created %zu hardpoints (no geometry)", hardpoints.size());
            }
            if (!floorReferencePath.empty())
            {
                MStatus floorStatus;
                MFnTransform floorFn;
                MObject floorObj = floorFn.create(parentTransform.object(), &floorStatus);
                if (floorStatus)
                {
                    floorFn.setName("floor_component");
                    MGlobal::executeCommand(MString("addAttr -ln \"floorPath\" -dt \"string\" ") + floorFn.name());
                    MPlug pathPlug = floorFn.findPlug("floorPath", false);
                    if (!pathPlug.isNull())
                        pathPlug.setValue(MString(floorReferencePath.c_str()));
                    mshLog("  Created floor component placeholder: %s", floorReferencePath.c_str());
                }
            }
            iff.close();
        }




        return MS::kSuccess;

    }
    else
    {
        std::cerr << "fell out of IFF options, bad error! :(" << std::endl;
        return MS::kFailure;
    }

}

/**
 * Handles writing out (exporting) the mesh
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus MshTranslator::writer (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    MSelectionList sel;
    MStatus status = MGlobal::getActiveSelectionList(sel);
    if (!status || sel.length() == 0)
    {
        std::cerr << "[MshTranslator] Export: select a mesh first" << std::endl;
        return MS::kFailure;
    }

    MDagPath dagPath;
    status = sel.getDagPath(0, dagPath);
    if (!status)
    {
        std::cerr << "[MshTranslator] Export: failed to get DAG path" << std::endl;
        return MS::kFailure;
    }

    if (!dagPath.hasFn(MFn::kMesh))
        dagPath.extendToShape();
    if (!dagPath.hasFn(MFn::kMesh))
    {
        std::cerr << "[MshTranslator] Export: selection is not a mesh" << std::endl;
        return MS::kFailure;
    }

    const char* filePath = file.expandedFullName().asChar();
    std::string outMeshPath, outAptPath;
    ExportStaticMesh cmd;
    if (!cmd.performExport(dagPath, filePath, outMeshPath, outAptPath))
    {
        std::cerr << "[MshTranslator] Export: performExport failed" << std::endl;
        return MS::kFailure;
    }

    return MS::kSuccess;
}

/**
 * @return the file type this translator handles
 */
MString MshTranslator::defaultExtension () const
{
    return "msh";
}

/**
 * @return file filter for dialog - .apt and .msh grouped (APT is entry point for static mesh)
 */
MString MshTranslator::filter () const
{
    return "Static Mesh Redirector - SWG (*.apt *.msh)";
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind MshTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4)
    {
        const char* ext = pathStr.c_str() + nameLength - 4;
        if (!strcasecmp(ext, ".msh") || !strcasecmp(ext, ".apt"))
            return kCouldBeMyFileType;
    }
    return kNotMyFileType;
}

