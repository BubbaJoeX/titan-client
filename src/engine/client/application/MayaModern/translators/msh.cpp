#include "msh.h"
#include "SwgTranslatorNames.h"
#include "ExportStaticMesh.h"
#include "ImportLodMesh.h"
#include "SwgIffFormatVersions.h"

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
#include <iostream>
#include <exception>
#include <cstring>
#include <cctype>
#include <set>
#include <string>
#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    /// Parse semicolon-separated SwgMsh export options (see scripts/swgMshExportOptions.mel).
    bool parseMshExportOptionBool(const MString& options, const char* keyEq)
    {
        const char* cs = options.asChar();
        if (!cs || !cs[0]) return false;
        std::string str(cs);
        const std::string key(keyEq);
        size_t scan = 0;
        bool found = false;
        bool value = false;
        size_t pos = 0;
        while ((pos = str.find(key, scan)) != std::string::npos)
        {
            size_t vpos = pos + key.size();
            while (vpos < str.size() && (str[vpos] == ' ' || str[vpos] == '\t'))
                ++vpos;
            if (vpos < str.size())
            {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(str[vpos])));
                value = (c == '1' || c == 't' || c == 'y');
                found = true;
            }
            scan = pos + key.size();
        }
        return found ? value : false;
    }

    /// Import-only: must not match `visualHardpoints=` inside unrelated option values (substring false positives).
    /// Semicolon-separated tokens; `visualHardpoints` must be the key at the start of a token. Last token wins.
    /// Missing key => false (attribute-only hardpoints). Explicit `visualHardpoints=0` => false.
    bool mshImportVisualHardpointsOptInFromOptions(const MString& options)
    {
        const char* cs = options.asChar();
        if (!cs || !cs[0])
            return false;

        const std::string keyEq = "visualHardpoints=";
        std::string str(cs);
        for (char& ch : str)
        {
            if (ch == '\n' || ch == '\r')
                ch = ';';
        }
        bool foundKey = false;
        bool enabled = false;

        size_t start = 0;
        for (;;)
        {
            const size_t semi = str.find(';', start);
            std::string token = (semi == std::string::npos) ? str.substr(start) : str.substr(start, semi - start);

            size_t a = 0;
            size_t b = token.size();
            while (a < b && (token[a] == ' ' || token[a] == '\t'))
                ++a;
            while (b > a && (token[b - 1] == ' ' || token[b - 1] == '\t'))
                --b;
            if (a < b)
            {
                token = token.substr(a, b - a);
                if (token.size() >= keyEq.size() && token.compare(0, keyEq.size(), keyEq) == 0)
                {
                    size_t v = keyEq.size();
                    while (v < token.size() && (token[v] == ' ' || token[v] == '\t'))
                        ++v;
                    if (v < token.size())
                    {
                        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(token[v])));
                        enabled = (c == '1' || c == 't' || c == 'y');
                        foundKey = true;
                    }
                }
            }
            if (semi == std::string::npos)
                break;
            start = semi + 1;
        }
        return foundKey ? enabled : false;
    }

    bool legacyTriangleFlipFromMshExportOptions(const MString& options)
    {
        return parseMshExportOptionBool(options, "legacyTriangleFlip=");
    }

    bool objExportDirectUvFromMshExportOptions(const MString& options)
    {
        return parseMshExportOptionBool(options, "objExportDirectUv=");
    }

    bool mshOptionKeySpecified(const MString& opt, const char* keyEq)
    {
        const char* cs = opt.asChar();
        if (!cs || !cs[0])
            return false;
        return std::string(cs).find(keyEq) != std::string::npos;
    }

    /// Matches SwgMsh defaultOptionsString (main.cpp): objExportDirectUv=1 when key omitted — SWG→SWG round-trip with default export.
    bool mshImportUsesDirectUvFromOptions(const MString& options)
    {
        if (!mshOptionKeySpecified(options, "objExportDirectUv="))
            return true;
        return parseMshExportOptionBool(options, "objExportDirectUv=");
    }
}

namespace SwgMshImport
{
    namespace
    {
        const Tag TAG_HPTS = TAG(H, P, T, S);
        const Tag TAG_HPNT = TAG(H, P, N, T);
        const Tag TAG_FLOR = TAG(F, L, O, R);
    }

    static void skipArbitraryFormContents(Iff& iff)
    {
        iff.enterForm();
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
                skipArbitraryFormContents(iff);
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
        iff.exitForm();
    }

    void consumeApprInnerForHardpointsAndFloor(
        Iff& iff,
        std::vector<MayaSceneBuilder::HardpointData>& hardpoints,
        std::string& floorReferencePath,
        bool parseFlorForms)
    {
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
                    if (parseFlorForms)
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
                        iff.enterForm(TAG_FLOR);
                        iff.exitForm(TAG_FLOR);
                    }
                }
                else
                {
                    iff.enterForm(formTag);
                    iff.exitForm(formTag);
                }
            }
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }
    }

    bool parseFullApprFormForHardpoints(
        Iff& iff,
        std::vector<MayaSceneBuilder::HardpointData>& hardpoints,
        std::string& floorReferencePath)
    {
        static const Tag kAppr = TAG(A, P, P, R);
        if (iff.getCurrentName() != kAppr)
            return false;
        iff.enterForm(kAppr);
        const Tag inner = iff.getCurrentName();
        if (inner != ::TAG_0001 && inner != ::TAG_0002 && inner != ::TAG_0003)
        {
            iff.enterForm(inner);
            while (iff.getNumberOfBlocksLeft() > 0)
            {
                if (iff.isCurrentForm())
                    skipArbitraryFormContents(iff);
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(inner);
            iff.exitForm(kAppr);
            return false;
        }
        iff.enterForm(inner);
        const bool parseFlor = (inner != ::TAG_0001);
        consumeApprInnerForHardpointsAndFloor(iff, hardpoints, floorReferencePath, parseFlor);
        iff.exitForm(inner);
        iff.exitForm(kAppr);
        return true;
    }
}

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
    bool s_mshImportVisualHardpoints = false;
    /// Set at reader() entry from objExportDirectUv option; used when building per-vertex UV arrays.
    bool s_mshImportUvDirectEngineToMaya = true;

    static const Tag TAG_APPR = TAG(A,P,P,R);
    static const Tag TAG_CNTR = TAG(C,N,T,R);
    static const Tag TAG_RADI = TAG(R,A,D,I);
    static const Tag TAG_SPS_NS = TAG3(S,P,S);
}

MStatus MshTranslator::createMeshFromMsh(const char* mshPath, MString& outRootPath, const MString& parentPath, bool visualHardpoints)
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
    const MString readerOpts = visualHardpoints ? MString("visualHardpoints=1") : MString();
    MStatus status = t->reader(fileObj, readerOpts, MPxFileTranslator::kImportAccessMode);
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
    mshLog("reader options string: [%s]", options.asChar() ? options.asChar() : "");

    s_mshImportVisualHardpoints = mshImportVisualHardpointsOptInFromOptions(options);
    s_mshImportUvDirectEngineToMaya = mshImportUsesDirectUvFromOptions(options);
    mshLog("  UV import mode: %s", s_mshImportUvDirectEngineToMaya ? "direct (matches default SwgMsh export / OBJ path)" : "1-V flip (legacy; pair with export objExportDirectUv OFF for round-trip)");

    // APT redirect + DTLA/MLOD unwrapping (parity with legacy ImportStaticMesh / importLodMesh).
    const std::string pathRaw(fileName);
    std::string pathToLoad = resolveStaticMeshFilePathForImport(pathRaw, pathRaw);
    if (pathToLoad != pathRaw)
        mshLog("  Resolved mesh path -> %s", pathToLoad.c_str());

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

        MObject meshImportRootObj = MObject::kNullObj;

        const Tag meshTopTag = iff.getCurrentName();
        if (meshTopTag != TAG_MESH)
        {
            char topTn[5];
            ConvertTagToString(meshTopTag, topTn);
            mshLog("Top form is [%s], not MESH — wrong file type or incomplete DTLA/MLOD resolve. Path: %s", topTn, pathToLoad.c_str());
            MGlobal::displayError(MString("[MshTranslator] Expected FORM MESH at file root, found ") + topTn + ". Path: " + pathToLoad.c_str());
            iff.close();
            return MS::kFailure;
        }

        iff.enterForm(TAG_MESH);
        const Tag meshInnerTag = iff.getCurrentName();
        if (meshInnerTag != ::TAG_0002 && meshInnerTag != ::TAG_0003 && meshInnerTag != ::TAG_0004 && meshInnerTag != ::TAG_0005)
        {
            char tn[5];
            ConvertTagToString(meshInnerTag, tn);
            mshLog("Unsupported MESH inner [%s] — client MeshAppearanceTemplate supports 0002..0005 (see SwgIffFormatVersions.h)", tn);
            MGlobal::displayError(MString("[MshTranslator] Unsupported MESH inner FORM ") + tn
                + " (expected 0002..0005). File: " + pathToLoad.c_str()
                + " — try importLodMesh if this is a .lod/DTLA appearance chain.");
            iff.close();
            return MS::kFailure;
        }
        iff.enterForm(meshInnerTag);
        {
            char tn[5];
            ConvertTagToString(meshInnerTag, tn);
            mshLog("  Entered MESH/%s", tn);
        }

        std::vector<MayaSceneBuilder::HardpointData> hardpoints;
        std::string floorReferencePath;

        if ((meshInnerTag == ::TAG_0004 || meshInnerTag == ::TAG_0005)
            && iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_APPR)
        {
            iff.enterForm(TAG_APPR);
            const Tag apprInnerTag = iff.getCurrentName();
            if (apprInnerTag != ::TAG_0001 && apprInnerTag != ::TAG_0002 && apprInnerTag != ::TAG_0003)
            {
                char tn[5];
                ConvertTagToString(apprInnerTag, tn);
                mshLog("Unsupported APPR inner [%s] — client AppearanceTemplate supports 0001..0003", tn);
                MGlobal::displayError(MString("[MshTranslator] Unsupported APPR block in .msh (expected 0001..0003). "));
                iff.close();
                return MS::kFailure;
            }
            iff.enterForm(apprInnerTag);
            const bool parseFlor = (apprInnerTag != ::TAG_0001);
            SwgMshImport::consumeApprInnerForHardpointsAndFloor(iff, hardpoints, floorReferencePath, parseFlor);
            iff.exitForm(apprInnerTag);
            iff.exitForm(TAG_APPR);
            mshLog("  APPR: %zu hardpoints, floor=%s", hardpoints.size(), floorReferencePath.c_str());
        }

        Tag spsInnerTag = ::TAG_0001;

        if (iff.seekForm(TAG_SPS_NS))
        {
            mshLog("  Found SPS form");
            iff.enterForm(TAG_SPS_NS);
            spsInnerTag = iff.getCurrentName();
            if (spsInnerTag != ::TAG_0000 && spsInnerTag != ::TAG_0001)
            {
                char tn[5];
                ConvertTagToString(spsInnerTag, tn);
                mshLog("Unsupported SPS inner [%s] — client ShaderPrimitiveSetTemplate supports 0000 (u32 INDX) and 0001 (u16)", tn);
                MGlobal::displayError(MString("[MshTranslator] Unsupported SPS version in .msh (expected 0000 or 0001). "));
                iff.close();
                return MS::kFailure;
            }
            iff.enterForm(spsInnerTag);
            const bool useIndicesU32 = (spsInnerTag == ::TAG_0000);
            mshLog("  SPS inner: %s indices", useIndicesU32 ? "uint32" : "uint16");
            iff.enterChunk(TAG_CNT);
            const int32 numberOfShadersRaw = iff.read_int32();
            iff.exitChunk(TAG_CNT);
            if (numberOfShadersRaw < 1 || numberOfShadersRaw > 2048)
            {
                mshLog("  Invalid SPS shader count %d (must be 1..2048)", static_cast<int>(numberOfShadersRaw));
                MGlobal::displayError(MString("[MshTranslator] Invalid MESH/SPS shader count in .msh. File: ") + pathToLoad.c_str());
                iff.close();
                return MS::kFailure;
            }
            const int32 numberOfShaders = numberOfShadersRaw;
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
            meshImportRootObj = parentTransform.object();
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
                        (void)numberOfPrimitives;
                    iff.exitChunk(TAG_INFO);


                    iff.enterForm(); // This is the version # of the LocalShaderPrimitiveTemplate
                        iff.enterChunk(TAG_INFO);
                            const int primitiveTypeInt = iff.read_int32();
                            (void)primitiveTypeInt;
                            // todo VALIDATE_RANGE_INCLUSIVE_INCLUSIVE here against SPSPT:: list
                            const bool hasIndices = iff.read_bool8();
                            const bool hasSortedIndices = iff.read_bool8();
                        iff.exitChunk(TAG_INFO);


                        // Load Vertex Buffer (VertexBuffer::load_0003)
                        iff.enterForm(TAG_VTXA);
                            iff.enterForm(::TAG_0003); //todo validate version number
                                iff.enterChunk(TAG_INFO);

                                    // Read Vertex Buffer Bit Mask
                                    VertexBufferFormat vbf;
                                    vbf.setFlags(static_cast<VertexBufferFormat::Flags>(iff.read_uint32()));
                                    int numberOfTextureCoordinateSets = vbf.getNumberOfTextureCoordinateSets();

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
                                    // Guard absurd counts (corrupt IFF / format mismatch) — avoids hour-long "freezes".
                                    constexpr int kMaxReasonableVertices = 32 * 1024 * 1024;
                                    if (numberOfVertices < 0 || numberOfVertices > kMaxReasonableVertices)
                                    {
                                        mshLog("    Invalid vertex count %d (shader %d)", numberOfVertices, i + 1);
                                        MGlobal::displayError(MString("[MshTranslator] Invalid vertex count in .msh DATA (corrupt file or unsupported layout). Path: ")
                                            + pathToLoad.c_str());
                                        iff.close();
                                        return MS::kFailure;
                                    }
                                    mshLog("    Vertices: %d (shader %d)", numberOfVertices, i + 1);

                                    totalVerticesInMesh += numberOfVertices;
                                iff.exitChunk(TAG_INFO);

                                iff.enterChunk(::TAG_DATA);
                                    // IMPORTANT: Do not use `while (!iff.atEndOfForm())` here. If the vertex layout implied by
                                    // VertexBufferFormat does not match the bytes on disk (re-exported / mixed client assets),
                                    // the reader under-fills each vertex and never reaches the end of the DATA chunk — Maya
                                    // appears frozen. Iterate exactly `numberOfVertices` times (same contract as
                                    // VertexBuffer::load_0003), then drain any trailing padding bytes.
                                    for (int vertIndex = 0; vertIndex < numberOfVertices; ++vertIndex)
                                    {
                                        if (vertIndex == 0 || vertIndex + 1 == numberOfVertices
                                            || (numberOfVertices > 1000 && (vertIndex + 1) % 10000 == 0))
                                            mshLog("    Vertex %d/%d", vertIndex + 1, numberOfVertices);
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
                                                vArray.append(s_mshImportUvDirectEngineToMaya ? v : (1.0f - v));
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
                                    while (!iff.atEndOfForm())
                                        (void)iff.read_uint8();

                                iff.exitChunk(::TAG_DATA);
                            iff.exitForm(::TAG_0003);
                        iff.exitForm(TAG_VTXA);

                        // load normal index buffer
                        if(hasIndices)
                        {
                            iff.enterChunk(TAG_INDX);
                                const int numberOfIndices = iff.read_int32();
                                constexpr int kMaxReasonableIndices = 256 * 1024 * 1024;
                                if (numberOfIndices < 3 || numberOfIndices > kMaxReasonableIndices
                                    || (numberOfIndices % 3) != 0)
                                {
                                    mshLog("    Invalid index count %d (shader %d)", numberOfIndices, i + 1);
                                    MGlobal::displayError(MString("[MshTranslator] Invalid triangle index count in .msh INDX. Path: ")
                                        + pathToLoad.c_str());
                                    iff.close();
                                    return MS::kFailure;
                                }
                                totalPolygonsInMesh += numberOfIndices;
                                for(int m = 0; m < numberOfIndices; m++)
                                {
                                    if (useIndicesU32)
                                        polygonConnects.append(static_cast<int>(iff.read_int32()));
                                    else
                                        polygonConnects.append(static_cast<int>(iff.read_uint16()));
                                }
                            iff.exitChunk(TAG_INDX);
                        }
                        // Directional sorted index arrays (ShaderPrimitiveSetTemplate::LocalShaderPrimitiveTemplate SIDX).
                        // Client may skip loading when RAM-limited but still skips the chunk; we always consume bytes.
                        if (hasSortedIndices)
                        {
                            iff.enterChunk(TAG_SIDX);
                            const int nArrays = iff.read_int32();
                            for (int si = 0; si < nArrays; ++si)
                            {
                                IGNORE_RETURN(iff.read_floatVector());
                                const int nSortedIdx = iff.read_int32();
                                for (int k = 0; k < nSortedIdx; ++k)
                                {
                                    if (useIndicesU32)
                                        IGNORE_RETURN(iff.read_int32());
                                    else
                                        IGNORE_RETURN(iff.read_uint16());
                                }
                            }
                            iff.exitChunk(TAG_SIDX);
                            if (nArrays > 0)
                                mshLog("    SIDX: %d directional index array(s) consumed", nArrays);
                        }
                    iff.exitForm(); // this exits the LocalShaderPrimitiveTemplate form
                iff.exitForm(); // this should exit the # of the shader form (end for loop)

                for(int p = 0; p < (totalPolygonsInMesh / 3); p++)
                {
                    polygonCounts.append(3); // mesh is made of triangles, so the vertex count of each polygon is 3
                }

                if (totalVerticesInMesh < 1 || polygonConnects.length() < 3 || (totalPolygonsInMesh % 3) != 0)
                {
                    mshLog("  Shader %d: no valid indexed geometry (hasIndices=%s verts=%d indices=%d) — check MESH/SPS version",
                            i + 1, hasIndices ? "true" : "false", totalVerticesInMesh, polygonConnects.length());
                    MGlobal::displayError(MString("[MshTranslator] Cannot build mesh: missing vertex/index data for shader primitive. ")
                        + "Try importLodMesh with the .apt path, setBaseDir to your tree root, or verify the .msh MESH/0005 SPS matches this plug-in.");
                    iff.close();
                    return MS::kFailure;
                }

                MStatus createStatus(MS::kFailure);
                try
                {
                    mshLog("    createMesh: verts=%d tris=%d uvCount=%d idx=%d polyCounts=%d",
                        totalVerticesInMesh,
                        (totalPolygonsInMesh / 3),
                        uArray.length(),
                        polygonConnects.length(),
                        polygonCounts.length());

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
                catch (const std::exception& e)
                {
                    mshLog("    mesh.create exception: %s", e.what());
                }
                if(createStatus.statusCode() != MS::kSuccess)
                    mshLog("    mesh.create failed: %s", createStatus.errorString().asChar());
                else
                    mshLog("    mesh.create OK");

                // reset everything for end of for loop
                totalVerticesInMesh = 0;
                totalPolygonsInMesh = 0;
                vertexArray.clear();
                polygonCounts.clear();
                polygonConnects.clear();
                uArray.clear();
                vArray.clear();
            }

            iff.exitForm(spsInnerTag);
            iff.exitForm(TAG_SPS_NS);

            if (meshInnerTag == ::TAG_0002 || meshInnerTag == ::TAG_0003)
            {
                if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_CNTR)
                {
                    iff.enterChunk(TAG_CNTR);
                    IGNORE_RETURN(iff.read_floatVector());
                    iff.exitChunk(TAG_CNTR);
                }
                if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_RADI)
                {
                    iff.enterChunk(TAG_RADI);
                    IGNORE_RETURN(iff.read_float());
                    iff.exitChunk(TAG_RADI);
                }
            }

            iff.exitForm(meshInnerTag);

            if (meshInnerTag == ::TAG_0002 || meshInnerTag == ::TAG_0003)
            {
                mshLog("  MESH 0002/0003: post-inner tail (extents / HPTS / FLOR per client)");
                SwgMshImport::consumeApprInnerForHardpointsAndFloor(iff, hardpoints, floorReferencePath, true);
            }

            iff.exitForm(TAG_MESH);
        }
        else
        {
            mshLog("  No SPS form - importing APPR only (hardpoints, floor)");
            iff.exitForm(meshInnerTag);
            iff.exitForm(TAG_MESH);

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
            meshImportRootObj = parentTransform.object();
            if (!parentObj.isNull())
            {
                MDagPath createdPath;
                if (MDagPath::getAPathTo(parentTransform.object(), createdPath))
                    s_createdRootPathForMshImport = createdPath.fullPathName();
            }
        }

        if (!meshImportRootObj.isNull())
        {
            if (!hardpoints.empty())
            {
                std::string meshBaseName = MayaUtility::parseFileNameToNodeName(file.rawName().asChar());
                std::map<std::string, MDagPath> emptyJointMap;
                MStatus hpStatus;
                if (s_mshImportVisualHardpoints)
                {
                    hpStatus = MayaSceneBuilder::createHardpoints(hardpoints, emptyJointMap, meshBaseName, meshImportRootObj);
                    if (hpStatus)
                        mshLog("  Created %zu visual hardpoints (hp_*)", hardpoints.size());
                }
                else
                {
                    std::vector<MayaSceneBuilder::HardpointData> dynEmpty;
                    hpStatus = MayaSceneBuilder::storeSkmgHardpointsOnMeshTransform(meshImportRootObj, hardpoints, dynEmpty);
                    if (hpStatus)
                        mshLog("  Stored %zu hardpoints on swgSkmgHardpoints (no viewport nodes)", hardpoints.size());
                }
            }
            if (!floorReferencePath.empty())
            {
                MStatus floorStatus;
                MFnTransform floorFn;
                MObject floorObj = floorFn.create(meshImportRootObj, &floorStatus);
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
            
            // Create collision group for user to add collision geometry
            // User can middle-mouse drag geometry into this group for export
            {
                MStatus collStatus;
                MFnTransform collFn;
                MObject collObj = collFn.create(meshImportRootObj, &collStatus);
                if (collStatus)
                {
                    collFn.setName("collision");
                    // Add attribute to mark this as a collision container
                    MGlobal::executeCommand(MString("addAttr -ln \"swgCollisionGroup\" -at bool -dv 1 ") + collFn.name());
                    // Add attribute for collision type (box, sphere, mesh, composite)
                    MGlobal::executeCommand(MString("addAttr -ln \"swgCollisionType\" -dt \"string\" ") + collFn.name());
                    MPlug typePlug = collFn.findPlug("swgCollisionType", false);
                    if (!typePlug.isNull())
                        typePlug.setValue(MString("box")); // Default to box collision
                    mshLog("  Created collision group (drag collision geometry here for export)");
                }
            }
        }

        iff.close();

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
        MGlobal::displayError("SwgMsh export: nothing selected. Select a mesh, then export.");
        return MS::kFailure;
    }

    MDagPath dagPath;
    bool foundMesh = false;
    bool meshWithoutShader = false;
    for (unsigned si = 0; si < sel.length(); ++si)
    {
        status = sel.getDagPath(si, dagPath);
        if (!status) continue;
        MDagPath meshPath;
        if (MayaUtility::findFirstMeshShapeWithShadersInHierarchy(dagPath, meshPath))
        {
            dagPath = meshPath;
            foundMesh = true;
            break;
        }
        if (MayaUtility::findFirstMeshShapeInHierarchy(dagPath, meshPath))
            meshWithoutShader = true;
    }
    if (!foundMesh)
    {
        if (meshWithoutShader)
        {
            std::cerr << "[MshTranslator] Export: mesh has no shading group" << std::endl;
            MGlobal::displayError(
                "SwgMsh export: mesh has no shader assignment. Found geometry but no material on the mesh "
                "that would be exported. After combining, assign a material to the combined mesh, delete "
                "hidden leftover shapes, or select the combined mesh shape directly.");
            return MS::kFailure;
        }
        std::cerr << "[MshTranslator] Export: no mesh under selection (need a mesh anywhere under selected transforms)"
                  << std::endl;
        MGlobal::displayError(
            "SwgMsh export: no mesh found under the selection. Select a group or transform that contains a mesh "
            "(nested geo is OK), or the mesh shape.");
        return MS::kFailure;
    }

    const char* filePath = file.expandedFullName().asChar();
    const bool legacyTriFromExportDialog = legacyTriangleFlipFromMshExportOptions(options);
    const bool objUvFromExportDialog = objExportDirectUvFromMshExportOptions(options);
    std::string outMeshPath, outAptPath;
    ExportStaticMesh cmd;
    if (!cmd.performExport(dagPath, filePath, outMeshPath, outAptPath, false, legacyTriFromExportDialog, false,
            objUvFromExportDialog, options))
    {
        std::cerr << "[MshTranslator] Export: performExport failed" << std::endl;
        MGlobal::displayError(
            "SwgMsh export failed. Open Script Editor for lines starting with [ExportStaticMesh] or [ShaderExporter]. "
            "Often: setBaseDir <export root>, assign a material, or fix write permission on the export folder.");
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
    return MString(swg_translator::kFilterMsh);
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

