#include "mgn.h"
#include "SwgTranslatorNames.h"
#include "ImportLodMesh.h"
#include "ImportPathResolver.h"

#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Tag.h"
#include "Vector.h"
#include "PackedArgb.h"
#include "Quaternion.h"
#include "OcclusionZoneSet.h"
#include "MayaUtility.h"
#include "MayaSceneBuilder.h"
#include "SwgTrtsIo.h"

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MVector.h>
#include <maya/MStringArray.h>
#include <maya/MPointArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnMesh.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObjectArray.h>
#include <maya/MSelectionList.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>

#include <vector>
#include <array>
#include <algorithm>
#include <cstring>
#include <set>
#include <map>
#include <cctype>
#include <string>

namespace
{
    std::string mgnTrimBundleToken(std::string s)
    {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        return s;
    }

    /// Optional string attributes on the mesh parent transform: tree paths (semicolon/newline-separated).
    /// After a successful .mgn write, each path is resolved with resolveImportPath and copied next to the .mgn.
    /// `swgShipBundlePaths` — spacecraft / cockpit IFF bundles; `swgVehicleBundlePaths` — ground vehicle bundles (separate workflow).
    void copyMgnBundlePathsFromAttr(MFnDependencyNode& dep, const char* attrName, const char* logLabel, const std::string& outDir)
    {
        if (!dep.hasAttribute(attrName))
            return;

        MStatus st;
        MPlug plug = dep.findPlug(attrName, true, &st);
        if (!st || plug.isNull())
            return;
        MString mlist;
        if (plug.getValue(mlist) != MS::kSuccess)
            return;
        const std::string raw(mlist.asChar());
        if (raw.empty())
            return;

        std::string token;
        const auto flushToken = [&]()
        {
            const std::string t = mgnTrimBundleToken(token);
            token.clear();
            if (t.empty())
                return;
            const std::string resolved = resolveImportPath(t);
            if (!MayaUtility::fileExists(resolved))
            {
                MGlobal::displayWarning(MString("[MGN export] ") + attrName + ": file not found: " + resolved.c_str());
                return;
            }
            const size_t bn = resolved.find_last_of("/\\");
            const std::string base = (bn == std::string::npos) ? resolved : resolved.substr(bn + 1);
            const std::string dst = outDir + base;
            if (!MayaUtility::copyFile(resolved, dst))
                MGlobal::displayWarning(MString("[MGN export] failed to copy bundle file to ") + dst.c_str());
            else
            {
                MGlobal::displayInfo(MString("[MGN export] copied ") + logLabel + " bundle IFF: " + base.c_str());
                mirrorExportToDataRootExported(resolved, base);
            }
        };

        for (char c : raw)
        {
            if (c == ';' || c == '\n' || c == '\r')
                flushToken();
            else
                token += c;
        }
        flushToken();
    }

    void copyMgnShipBundleIFFs(const MDagPath& meshDag, const char* mgnOutputPath)
    {
        MDagPath xformPath = meshDag;
        if (xformPath.hasFn(MFn::kMesh))
        {
            MStatus pst = xformPath.pop();
            if (pst != MS::kSuccess)
                return;
        }

        MFnDependencyNode dep(xformPath.node());

        const std::string outFull(mgnOutputPath);
        const size_t lastSlash = outFull.find_last_of("/\\");
        if (lastSlash == std::string::npos)
            return;
        const std::string outDir = outFull.substr(0, lastSlash + 1);

        copyMgnBundlePathsFromAttr(dep, "swgVehicleBundlePaths", "vehicle", outDir);
        copyMgnBundlePathsFromAttr(dep, "swgShipBundlePaths", "ship", outDir);
    }
}

const int MgnTranslator::ms_blendTargetNameSize = 64;

const Tag TAG_SKMG = TAG(S,K,M,G);
const Tag TAG_SKTM = TAG(S,K,T,M);
const Tag TAG_XFNM = TAG(X,F,N,M);
const Tag TAG_POSN = TAG(P,O,S,N);
const Tag TAG_TWHD = TAG(T,W,H,D);
const Tag TAG_TWDT = TAG(T,W,D,T);
const Tag TAG_NORM = TAG(N,O,R,M);
const Tag TAG_BLTS = TAG(B,L,T,S);
const Tag TAG_FOZC = TAG(F,O,Z,C);
const Tag TAG_DOT3 = TAG(D,O,T,3);
const Tag TAG_HPTS = TAG(H,P,T,S);
const Tag TAG_STAT = TAG(S,T,A,T);
const Tag TAG_PSDT = TAG(P,S,D,T);
const Tag TAG_PIDX = TAG(P,I,D,X);
const Tag TAG_NIDX = TAG(N,I,D,X);
const Tag TAG_VDCL = TAG(V,C,D,L);
const Tag TAG_TXCI = TAG(T,X,C,I);
const Tag TAG_TCSF = TAG(T,C,S,F);
const Tag TAG_TCSD = TAG(T,C,S,D);
const Tag TAG_PRIM = TAG(P,R,I,M);
const Tag TAG_OZN = TAG3(O,Z,N);
const Tag TAG_OZC = TAG3(O,Z,C);
const Tag TAG_ZTO = TAG3(Z,T,O);
const Tag TAG_DYN = TAG3(D,Y,N);
const Tag TAG_BLT = TAG3(B,L,T);

struct MgnTranslator::Dot3Vector
{
public:
    Dot3Vector(float x, float y, float z, float flipState);
    Vector  m_dot3Vector;
    float   m_flipState;
};
inline MgnTranslator::Dot3Vector::Dot3Vector(float x, float y, float z, float flipState):
        m_dot3Vector(x, y, z),
        m_flipState(flipState)
{}

// ======================================================================

class MgnTranslator::Hardpoint
{
public:
    Hardpoint(const char *hardpointName, const char *parentName, const Quaternion &rotation, const Vector &position);
    [[nodiscard]] const std::string &getHardpointName() const;
    [[nodiscard]] const std::string &getParentName() const;
    [[nodiscard]] const Vector &getPosition() const;
    [[nodiscard]] const Quaternion &getRotation() const;
private:
    std::string m_hardpointName;
    std::string  m_parentName;
    Quaternion m_rotation;
    Vector m_position;
};
MgnTranslator::Hardpoint::Hardpoint(const char *hardpointName, const char *parentName, const Quaternion &rotation, const Vector &position)
        :	 m_hardpointName(hardpointName),
              m_parentName(parentName),
              m_rotation(rotation),
              m_position(position)
{
}
// ----------------------------------------------------------------------

inline const std::string &MgnTranslator::Hardpoint::getHardpointName() const
{
    return m_hardpointName;
}

// ----------------------------------------------------------------------

inline const std::string &MgnTranslator::Hardpoint::getParentName() const
{
    return m_parentName;
}

// ----------------------------------------------------------------------

inline const Vector &MgnTranslator::Hardpoint::getPosition() const
{
    return m_position;
}

// ----------------------------------------------------------------------

inline const Quaternion &MgnTranslator::Hardpoint::getRotation() const
{
    return m_rotation;
}

// ======================================================================

class DrawPrimitive
{
public:

};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class MgnTranslator::IndexedTriListPrimitive: public DrawPrimitive
{
public:
    int              m_triangleCount;
    std::vector<int> m_indices;

};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class MgnTranslator::OccludedIndexedTriListPrimitive: public DrawPrimitive
{
public:
    int              m_triangleCount;
    std::vector<int> m_indices;
};

// ======================================================================

class MgnTranslator::PerShaderData
{
public:
    std::string                m_shaderTemplateName;

    [[nodiscard]] const std::string &getShaderTemplateName() const {
        return m_shaderTemplateName;
    }

    void setShaderTemplateName(const std::string &mShaderTemplateName) {
        m_shaderTemplateName = mShaderTemplateName;
    }

    [[nodiscard]] int getVertexCount() const {
        return m_vertexCount;
    }

    void setVertexCount(int mVertexCount) {
        m_vertexCount = mVertexCount;
    }

    [[nodiscard]] std::vector<int> getPositionIndices() const {
        return m_positionIndices;
    }

    void setPositionIndices(const std::vector<int> &mPositionIndices) {
        m_positionIndices = mPositionIndices;
    }

    [[nodiscard]] std::vector<int> getNormalIndices() const {
        return m_normalIndices;
    }

    void setNormalIndices(const std::vector<int> &mNormalIndices) {
        m_normalIndices = mNormalIndices;
    }

    [[nodiscard]] std::vector<int> getDot3VectorIndices() const {
        return m_dot3VectorIndices;
    }

    void setDot3VectorIndices(const std::vector<int> &mDot3VectorIndices) {
        m_dot3VectorIndices = mDot3VectorIndices;
    }

    [[nodiscard]] std::vector<PackedArgb> getDiffuseColors() const {
        return m_diffuseColors;
    }

    void setDiffuseColors(const std::vector<PackedArgb> &mDiffuseColors) {
        m_diffuseColors = mDiffuseColors;
    }

    [[nodiscard]] std::vector<int> getTextureCoordinateSetDimensionality() const {
        return m_textureCoordinateSetDimensionality;
    }

    void setTextureCoordinateSetDimensionality(const std::vector<int> &mTextureCoordinateSetDimensionality) {
        m_textureCoordinateSetDimensionality = mTextureCoordinateSetDimensionality;
    }

    [[nodiscard]] std::vector<std::vector<float>> &getTextureCoordinateSetData() {
        return m_textureCoordinateSetData;
    }

    void setTextureCoordinateSetData(const std::vector<std::vector<float>> &mTextureCoordinateSetData) {
        m_textureCoordinateSetData = mTextureCoordinateSetData;
    }

    [[nodiscard]] std::vector<DrawPrimitive *> getDrawPrimitives() const {
        return m_drawPrimitives;
    }

    void setDrawPrimitives(const std::vector<DrawPrimitive *> &mDrawPrimitives) {
        m_drawPrimitives = mDrawPrimitives;
    }

private:
    int                        m_vertexCount{};
    std::vector<int>           m_positionIndices;
    std::vector<int>           m_normalIndices;
    std::vector<int>           m_dot3VectorIndices;
    std::vector<PackedArgb>    m_diffuseColors;

    std::vector<int>           m_textureCoordinateSetDimensionality;
    std::vector<std::vector<float>> m_textureCoordinateSetData;
    std::vector<DrawPrimitive*>  m_drawPrimitives;
};

// ======================================================================

struct MgnTranslator::BlendVector
{
public:
    BlendVector(int index, const Vector &deltaVector);

public:
    int     m_index;
    Vector  m_deltaVector;
    
};
MgnTranslator::BlendVector::BlendVector(int index, const Vector &deltaVector)
:   m_index(index),
    m_deltaVector(deltaVector)
{
}

// ======================================================================

class MgnTranslator::BlendTarget
    {
public:
    
    BlendTarget();
    ~BlendTarget();
    
    void               load_0001(Iff &iff);
    void               load_0002(Iff &iff);
    void               load_0003(Iff &iff);
    void               load_0004(Iff &iff);
    
    [[nodiscard]] const std::string &getCustomizationVariablePathName() const;

private:
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    
    class HardpointTarget
        {
    public:
        
        HardpointTarget(int dynamicHardpointIndex, const Vector &deltaPosition, const Quaternion &deltaRotation);
    
    private:
        
        int         m_dynamicHardpointIndex;
        Vector      m_deltaPosition;
        Quaternion  m_deltaRotation;
        
        };
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    
    typedef std::vector<BlendVector>     BlendVectorVector;
    typedef std::vector<HardpointTarget> HardpointTargetVector;
    
    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

public:
    
    std::string            m_customizationVariablePathName;
    BlendVectorVector      m_positions;
    BlendVectorVector      m_normals;
    BlendVectorVector      m_dot3Vectors;
    HardpointTargetVector *m_hardpointTargets;
    };

MgnTranslator::BlendTarget::BlendTarget() :
        m_customizationVariablePathName(),
        m_positions(),
        m_normals(),
        m_dot3Vectors(),
        m_hardpointTargets(nullptr)
{
}

// ----------------------------------------------------------------------

MgnTranslator::BlendTarget::~BlendTarget()
{
    delete m_hardpointTargets;
}

// ----------------------------------------------------------------------

MgnTranslator::BlendTarget::HardpointTarget::HardpointTarget(int dynamicHardpointIndex, const Vector &deltaPosition, const Quaternion &deltaRotation)
        :	m_dynamicHardpointIndex(dynamicHardpointIndex),
        m_deltaPosition(deltaPosition),
        m_deltaRotation(deltaRotation)
{
}

// ----------------------------------------------------------------------

/**
 * Retrieve the CustomizationData variable pathname for this BlendTarget
 * instance.
 *
 * @return  the CustomizationData variable pathname for this BlendTarget instance .
 */
inline const std::string &MgnTranslator::BlendTarget::getCustomizationVariablePathName() const
{
    return m_customizationVariablePathName;
}

// ----------------------------------------------------------------------

void MgnTranslator::BlendTarget::load_0001(Iff &iff)
{
    iff.enterForm(TAG_BLT);
    
    //-- load blend target general info
    iff.enterChunk(TAG_INFO);
    
    char blendTargetName[ms_blendTargetNameSize];
    
    const int positionCount = iff.read_int32();
    const int normalCount   = iff.read_int32();
    
    //-- construct customization variable pathname
    //   note: at this point all customization variables are in the shared_owner directory.
    iff.read_string(blendTargetName, sizeof(blendTargetName));
    m_customizationVariablePathName  = "/shared_owner/";
    m_customizationVariablePathName += blendTargetName;
    
    iff.exitChunk(TAG_INFO);
    
    //-- load position data
    const bool enteredPosn = iff.enterChunk(TAG_POSN, true);
    DEBUG_FATAL(!enteredPosn && positionCount, ("expecting delta position data"));
    
    if (enteredPosn)
    {
        m_positions.reserve(static_cast<size_t>(positionCount));
        
        for (int i = 0; i < positionCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_positions.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_POSN);
    }
    
    //-- load normal data
    const bool enteredNorm = iff.enterChunk(TAG_NORM, true);
    DEBUG_FATAL(!enteredNorm && normalCount, ("expecting delta position data"));
    if (enteredNorm)
    {
        m_normals.reserve(static_cast<size_t>(normalCount));
        
        for (int i = 0; i < normalCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_normals.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_NORM);
    }
    
    iff.exitForm(TAG_BLT);
}

// ----------------------------------------------------------------------

inline void MgnTranslator::BlendTarget::load_0002(Iff &iff)
{
    // same as 0001 format
    load_0001(iff);
}

// ----------------------------------------------------------------------

void MgnTranslator::BlendTarget::load_0003(Iff &iff)
{
    iff.enterForm(TAG_BLT);
    
    //-- load blend target general info
    iff.enterChunk(TAG_INFO);
    
    char blendTargetName[ms_blendTargetNameSize];
    
    const int positionCount = iff.read_int32();
    const int normalCount   = iff.read_int32();
    
    //-- construct customization variable pathname
    //   note: at this point all customization variables are in the shared_owner directory.
    iff.read_string(blendTargetName, sizeof(blendTargetName));
    m_customizationVariablePathName  = "/shared_owner/";
    m_customizationVariablePathName += blendTargetName;
    
    iff.exitChunk(TAG_INFO);
    
    //-- load position data
    const bool enteredPosn = iff.enterChunk(TAG_POSN, true);
    DEBUG_FATAL(!enteredPosn && positionCount, ("expecting delta position data"));
    
    if (enteredPosn)
    {
        m_positions.reserve(static_cast<size_t>(positionCount));
        
        for (int i = 0; i < positionCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_positions.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_POSN);
    }
    
    //-- load normal data
    const bool enteredNorm = iff.enterChunk(TAG_NORM, true);
    DEBUG_FATAL(!enteredNorm && normalCount, ("expecting delta position data"));
    if (enteredNorm)
    {
        m_normals.reserve(static_cast<size_t>(normalCount));
        
        for (int i = 0; i < normalCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_normals.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_NORM);
    }
    
    //-- load hardpoint target data (affected hardpoints)
    if (iff.enterChunk(TAG_HPTS, true))
    {
        const int hardpointCount = static_cast<int>(iff.read_int16());
        DEBUG_FATAL(hardpointCount < 1, ("bad hardpoint count %d", hardpointCount));
        
        // create hardpoint target array
        m_hardpointTargets = new HardpointTargetVector();
        m_hardpointTargets->reserve(static_cast<size_t>(hardpointCount));
        
        for (int i = 0; i < hardpointCount; ++i)
        {
            const int         dynamicHardpointIndex = static_cast<int>(iff.read_int16());
            const Vector      deltaPosition         = iff.read_floatVector();
            const Quaternion  deltaRotation         = iff.read_floatQuaternion();
            
            m_hardpointTargets->push_back(HardpointTarget(dynamicHardpointIndex, deltaPosition, deltaRotation));
        }
        
        iff.exitChunk(TAG_HPTS, true);
    }
    
    iff.exitForm(TAG_BLT);
}

// ----------------------------------------------------------------------

void MgnTranslator::BlendTarget::load_0004(Iff &iff)
{
    
    iff.enterForm(TAG_BLT);
    
    //-- load blend target general info
    iff.enterChunk(TAG_INFO);
    
    char blendTargetName[ms_blendTargetNameSize];
    
    const int positionCount = iff.read_int32();
    const int normalCount   = iff.read_int32();
    
    //-- construct customization variable pathname
    //   note: at this point all customization variables are in the shared_owner directory.
    iff.read_string(blendTargetName, sizeof(blendTargetName));
    m_customizationVariablePathName  = "/shared_owner/";
    m_customizationVariablePathName += blendTargetName;
    
    iff.exitChunk(TAG_INFO);
    
    //-- load position data
    
    const bool enteredPosn = iff.enterChunk(TAG_POSN, true);
    DEBUG_FATAL(!enteredPosn && positionCount, ("expecting delta position data"));
    
    if (enteredPosn)
    {
        
        m_positions.reserve(static_cast<size_t>(positionCount));
        
        for (int i = 0; i < positionCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_positions.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_POSN);
    }
    
    //-- load normal data
    const bool enteredNorm = iff.enterChunk(TAG_NORM, true);
    DEBUG_FATAL(!enteredNorm && normalCount, ("expecting delta position data"));
    if (enteredNorm)
    {
        
        m_normals.reserve(static_cast<size_t>(normalCount));
        
        for (int i = 0; i < normalCount; ++i)
        {
            const int     index  = iff.read_int32();
            const Vector  vector = iff.read_floatVector();
            
            m_normals.push_back(BlendVector(index, vector));
        }
        
        iff.exitChunk(TAG_NORM);
    }
    
    //-- Load DOT3 per-pixel-lighting normal map vector. 0004 only
    if (iff.enterChunk(TAG_DOT3, true))
    {
        // Strip data loading if DOT3 is shut off.
        //if (!GraphicsOptionTags::get(TAG_DOT3))
        //    iff.exitChunk(TAG_DOT3, true);
        //else
        //{
        
            const int dot3VectorCount = static_cast<int>(iff.read_int32());
            
            m_dot3Vectors.reserve(static_cast<size_t>(dot3VectorCount));
            
            for (int i = 0; i < dot3VectorCount; ++i)
            {
                const int     index  = iff.read_int32();
                const Vector  vector = iff.read_floatVector();
                
                m_dot3Vectors.push_back(BlendVector(index, vector));
            }
            
            // @todo change to non-optional end-of-chunk processing when implemented.
            
            iff.exitChunk(TAG_DOT3, true);
        //}
    }
    
    //-- load hardpoint target data (affected hardpoints), 0003 and 0004 only
    if (iff.enterChunk(TAG_HPTS, true))
    {
        const int hardpointCount = static_cast<int>(iff.read_int16());
        DEBUG_FATAL(hardpointCount < 1, ("bad hardpoint count %d", hardpointCount));
        
        // create hardpoint target array
        IS_NULL(m_hardpointTargets);
        
        m_hardpointTargets = new HardpointTargetVector();
        m_hardpointTargets->reserve(static_cast<size_t>(hardpointCount));
        
        for (int i = 0; i < hardpointCount; ++i)
        {
            const int         dynamicHardpointIndex = static_cast<int>(iff.read_int16());
            const Vector      deltaPosition         = iff.read_floatVector();
            const Quaternion  deltaRotation         = iff.read_floatQuaternion();
            
            m_hardpointTargets->push_back(HardpointTarget(dynamicHardpointIndex, deltaPosition, deltaRotation));
        }
        
        iff.exitChunk(TAG_HPTS, true);
    }
    
    iff.exitForm(TAG_BLT);
}

// ======================================================================

// creates an instance of the MgnTranslator
void* MgnTranslator::creator()
{
    return new MgnTranslator();
}

/**
 * Function that handles reading in the file (import/open operations)
 *
 * @param file the file
 * @param options options
 * @param mode file access mode
 * @return the operation success/failure
 */
MStatus MgnTranslator::reader (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    
    LOG_F(ERROR, ("Starting MgnTranslator Import"));

    std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        MGlobal::displayError("MGN import: could not resolve file path from MFileObject.");
        return MS::kFailure;
    }
    pathStd = resolveImportPath(pathStd);
    pathStd = resolveSkmgPathThroughWrappers(pathStd);
    const char* fileName = pathStd.c_str();
    if(!Iff::isValid(fileName))
    {
        //todo these need to convert to a FATAL()
        std::cerr << fileName << " could not be read as a valid IFF file!" << std::endl;
        return MS::kFailure;
    }
    Iff iff;
    if(iff.open(fileName, false))
    {
        if(iff.getRawDataSize() < 1)
        {
            std::cerr << fileName << " was read in as an IFF but its size was empty!" << std::endl;
            return MS::kFailure;
        }

        char* nameBuffer = new char[MAX_PATH];

        MFloatPointArray vertexArray;
        int totalVerticesInMesh = 0;
        MIntArray polygonConnects;

        //---- Start Form SKMG [SKMG]
        if(iff.enterForm(TAG_SKMG, false))
        {

            //------ Start Form Version XXXX [SKMG -> XXXX]
            // Valid Versions are 0002 - 0004
            switch(iff.getCurrentName())
            {
                case TAG_0004:
                    iff.enterForm(TAG_0004);
                    break;
                case TAG_0003:
                    iff.enterForm(TAG_0003);
                    break;
                case TAG_0002:
                    iff.enterForm(TAG_0002);
                    break;
                default:
                    char formName[5];
                    ConvertTagToString(iff.getCurrentName(), formName);
                    DEBUG_FATAL(true, ("Unsupported SkeletalMeshGeneratorTemplate version [%s]", formName));
            }

                //---- Start INFO Chunk [SKMG -> XXXX -> INFO]
                iff.enterChunk(TAG_INFO);
                    const int maxTransformsPerVertex = iff.read_int32();
                    const int maxTransformsPerShader = iff.read_int32();
                    const int skeletonTemplateNameCount = iff.read_int32();
                    const int transformNameCount = iff.read_int32();
                    const int positionCount = iff.read_int32();
                    const int transformWeightDataCount = iff.read_int32();
                    const int normalCount = iff.read_int32();
                    const int perShaderDataCount = iff.read_int32();
                    const int blendTargetCount = iff.read_int32();
                    const int16 occlusionZoneCount = iff.read_int16();
                    const int16 occlusionZoneCombinationCount = iff.read_int16();
                    const int16 zonesThisOccludesCount = iff.read_int16();
                    const int16 occlusionLayer = iff.read_int16();
                    iff.exitChunk(TAG_INFO);
                totalVerticesInMesh = positionCount;

                //---- Start SKTM Chunk [SKMG -> XXXX -> SKTM]
                // Contains Skeleton Template Names required on the skeleton to which this mesh is bound
                std::vector<std::string> skeletonTemplateNames;
                iff.enterChunk(TAG_SKTM);
                for(int i = 0; i < skeletonTemplateNameCount; i++)
                {
                    iff.read_string(nameBuffer, sizeof(nameBuffer) - 1);
                    skeletonTemplateNames.emplace_back(nameBuffer);
                }
                iff.exitChunk(TAG_SKTM);

                //---- Start XFMN Chunk [SKMG -> XXXX -> XFMN]
                // Transform Names
                std::vector<std::string> transformNames;
                iff.enterChunk(TAG_XFNM);
                for(int i = 0; i < transformNameCount; i++)
                {
                    iff.read_string(nameBuffer, sizeof(nameBuffer) - 1);
                    transformNames.emplace_back(nameBuffer);
                }
                iff.exitChunk(TAG_XFNM);

                //---- Start POSN Chunk [SKMG -> XXXX -> POSN]
                // Position Vectors
                // Coordinate conversion: Game X is negated for Maya
                std::vector<Vector> positions;
                iff.enterChunk(TAG_POSN);
                for(int i = 0; i < positionCount; i++)
                {
                    Vector v = iff.read_floatVector();
                    vertexArray.append(-v.x, v.y, v.z);  // Negate X for Maya coordinate system
                    positions.emplace_back(v);  // Keep original for internal use
                }
                iff.exitChunk(TAG_POSN);

                //---- Start TWHD Chunk [SKMG -> XXXX -> TWHD]
                // Transform Weighting Header Information
                std::vector<int> transformWeightCounts;
                iff.enterChunk(TAG_TWHD);
                for(int i = 0; i < positionCount; i++)
                {
                    transformWeightCounts.emplace_back(iff.read_int32());
                }
                iff.exitChunk(TAG_TWHD);

                //---- Start TWDT Chunk [SKMG -> XXXX -> TWDT]
                // Transform Weight Data
                std::vector<std::pair<int, float>> transformWeightData;
                iff.enterChunk(TAG_TWDT);
                for(int i = 0; i < transformWeightDataCount; i++)
                {
                    // int = transform index
                    // float = transform weight
                    transformWeightData.emplace_back(iff.read_int32(), iff.read_float());
                }
                iff.exitChunk(TAG_TWDT);

                //---- Start NORM Chunk [SKMG -> XXXX -> NORM]
                // Normal Vectors
                std::vector<Vector> normals;
                iff.enterChunk(TAG_NORM);
                for(int i = 0; i < normalCount; i++)
                {
                    Vector v = iff.read_floatVector();
                    normals.emplace_back(v);
                }
                iff.exitChunk(TAG_NORM);

                //---- Start DOT3 Chunk [SKMG -> XXXX -> DOT3]
                // if it exists (chunk is optional), load per-pixel lighting vectors, 0004 only
                std::vector<Dot3Vector> dot3Vectors;
                if(iff.enterChunk(TAG_DOT3, true))
                {
                    //todo from this block we would confirm DOT3 is enabled in ClientGraphics

                    const int count = iff.read_int32();
                    for(int i = 0; i < count; i++)
                    {
                        dot3Vectors.emplace_back(Dot3Vector(iff.read_float(), iff.read_float(), iff.read_float(), iff.read_float()));
                    }
                    iff.exitChunk(TAG_DOT3);
                }

                //---- Start HPTS FORM [SKMG -> XXXX -> HPTS]
                // if it exists (form is optional), load hard points, 0003/0004 only
                std::vector<Hardpoint> staticHardpoints;
                std::vector<Hardpoint> dynamicHardpoints;
                if(iff.enterForm(TAG_HPTS, true))
                {
                    //---- Start STAT Chunk [SKMG -> XXXX -> HPTS -> STAT]
                    // static (non-morphable) hard points
                    if(iff.enterChunk(TAG_STAT, true))
                    {
                        char hardpointName[256];
                        char hardpointParent[256];
                        const int hardpointCount = iff.read_int16();
                        for(int i = 0; i < hardpointCount; i++)
                        {
                            iff.read_string(hardpointName, sizeof(hardpointName)-1);
                            iff.read_string(hardpointParent, sizeof(hardpointParent)-1);
                            staticHardpoints.emplace_back(hardpointName, hardpointParent, iff.read_floatQuaternion(), iff.read_floatVector());
                        }
                        iff.exitChunk(TAG_STAT);
                    }
                    //---- Start DYN Chunk [SKMG -> XXXX -> HPTS -> DYN]
                    // dynamic (morphable) hard points
                    if(iff.enterChunk(TAG_DYN, true))
                    {
                        char hardpointName[256];
                        char hardpointParent[256];
                        const int hardpointCount = iff.read_int16();
                        for(int i = 0; i < hardpointCount; i++)
                        {
                            iff.read_string(hardpointName, sizeof(hardpointName)-1);
                            iff.read_string(hardpointParent, sizeof(hardpointParent)-1);
                            dynamicHardpoints.emplace_back(hardpointName, hardpointParent, iff.read_floatQuaternion(), iff.read_floatVector());
                        }
                        iff.exitChunk(TAG_DYN);
                    }
                    iff.exitForm(TAG_HPTS);
                }

                //---- load blend targets
                //Note: 0002 has slight code difference, but should not matter
                //todo add iff >> structure here
                std::vector<BlendTarget*> blendTargets;
                if(blendTargetCount)
                {
                    cerr << "blend target count > 0 so starting" << std::endl;
                    
                    //blendTargets.reserve(static_cast<size_t>(blendTargetCount));
                    iff.enterForm(TAG_BLTS);
                    
                    cerr << "entered form BLTS" << std::endl;
                    
                        for(int i = 0; i < blendTargetCount; i++)
                        {
                            
                            cerr << "inside bLTS FOR for count " << blendTargetCount << std::endl;
                            
                            blendTargets.push_back(new BlendTarget());
                            
                            cerr << " PUSHED BACK NOW LOADING " << blendTargetCount << std::endl;
                            
                            blendTargets.back()->load_0004(iff);
                            
                            cerr << " FINISHED LOADING " << blendTargetCount << std::endl;
                        }
                    iff.exitForm(TAG_BLTS);
                }
                
                //---- load occlusion zone names
                std::vector<int> occlusionZoneIds;
                std::vector<std::shared_ptr<CrcLowerString>> occlusionZoneNames;
                if(occlusionZoneCount)
                {
                    //occlusionZoneNames.reserve(static_cast<size_t>(occlusionZoneCount));
                    iff.enterChunk(TAG_OZN);
                    {
                        for (int i = 0; i < occlusionZoneCount; ++i)
                        {
                            char buffer[1024];
                            iff.read_string(buffer, sizeof(buffer)-1);
                            
                            occlusionZoneNames.push_back(std::make_shared<CrcLowerString>(buffer));
                        }
                    }
                    iff.exitChunk(TAG_OZN);
                    
                    OcclusionZoneSet::install();
                    OcclusionZoneSet::registerOcclusionZones(occlusionZoneNames, occlusionZoneIds);
                }
                
                cerr << "******** DONE WITH OZN CHUNK " << std::endl;

                //---- Start FOZC Chunk [SKMG -> XXXX -> FOZC]
                //Note: 0002 has slight code difference, but should not matter
                //---- (optional) load fully occluded zone combination
                std::vector<int> fullyOccludedZoneCombinations;
                if(iff.enterChunk(TAG_FOZC, true))
                {
                    const auto count = static_cast<size_t>(iff.read_uint16());
                    //fullyOccludedZoneCombinations.reserve(count);
                    for (size_t i = 0; i < count; ++i)
                    {
                        //-- get occlusion zone index relative to mesh's OZ name list
                        const int localOzIndex = static_cast<int>(iff.read_int16());
                        VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, localOzIndex, static_cast<int>(occlusionZoneIds.size()));
                        
                        //-- convert local OZ index into system-wide OZ index
                        const int systemOzIndex = occlusionZoneIds[static_cast<size_t>(localOzIndex)];
                        fullyOccludedZoneCombinations.emplace_back(systemOzIndex);
                    }
                    iff.exitChunk(TAG_FOZC);
                }
            
            cerr << "******** DONE WITH FOZC CHUNK " << std::endl;

                //---- load occlusion zone combinations
                std::vector<std::vector<int>> occlusionZoneCombinations;
                if(occlusionZoneCombinationCount)
                {
                    occlusionZoneCombinations.resize(static_cast<size_t>(occlusionZoneCombinationCount));
                    iff.enterChunk(TAG_OZC);
                    {
                        for (int i = 0; i < occlusionZoneCombinationCount; ++i)
                        {
                            //-- select the OZ combination array, size it properly
                            const int combinationZoneCount = static_cast<int>(iff.read_int16());
                            std::vector<int> &combination = occlusionZoneCombinations[static_cast<size_t>(i)];
                            combination.reserve(static_cast<size_t>(combinationZoneCount));
                            
                            for (int ozIndex = 0; ozIndex < combinationZoneCount; ++ozIndex)
                            {
                                //-- get occlusion zone index relative to mesh's OZ name list
                                const int localOzIndex  = static_cast<int>(iff.read_int16());
                                VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, localOzIndex, static_cast<int>(occlusionZoneIds.size()));
                                
                                //-- convert local OZ index into system-wide OZ index
                                const int systemOzIndex = occlusionZoneIds[static_cast<size_t>(localOzIndex)];
                                combination.emplace_back(systemOzIndex);
                            }
                        }
                    }
                    iff.exitChunk(TAG_OZC);
                }
            
            cerr << "******** DONE WITH OZC CHUNK " << std::endl;

                //---- load occlusion zones that this mesh occludes
                std::vector<int> zonesThisOccludes;
                if(zonesThisOccludesCount)
                {
                    iff.enterChunk(TAG_ZTO);
                    
                    //zonesThisOccludes.reserve(static_cast<size_t>(zonesThisOccludesCount));
                    for (int i = 0; i < zonesThisOccludesCount; ++i)
                    {
                        //-- get occlusion zone index relative to mesh's OZ name list
                        const int localOzIndex  = static_cast<int>(iff.read_int16());
                        VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, localOzIndex, static_cast<int>(occlusionZoneIds.size()));
                        
                        //-- convert local OZ index into system-wide OZ index
                        const int systemOzIndex = occlusionZoneIds[static_cast<size_t>(localOzIndex)];
                        zonesThisOccludes.push_back(systemOzIndex);
                    }
                    iff.exitChunk(TAG_ZTO);
                }
                
                //---- Before Starting per-Shader import, create a Parent Node for the Skeletal Mesh
                MFnTransform parentTransform;
                parentTransform.create();
                parentTransform.setName(file.rawName());

                //---- load per-shader data
                std::vector<PerShaderData> perShaderData;

                if(perShaderDataCount)
                {
                    // for each PSDT Form
                    for(int i = 0; i < perShaderDataCount; i++)
                    {
                        MIntArray polygonCounts;
                        auto* psd = new PerShaderData();
                        int textureCoordinateSetCount = 0;
                        int totalPolygonsInMesh = 0;

                        // tracking vectors for duplicative triangles
                        std::vector<int> positionIndexLookup;

                        //---- Start PSDT Form [SKMG -> XXXX -> PSDT]
                        iff.enterForm(TAG_PSDT);

                            //---- Start NAME Chunk [SKMG -> XXXX -> PSDT -> NAME]
                            iff.enterChunk(TAG_NAME);
                                char buffer[MAX_PATH];
                                iff.read_string(buffer, sizeof(buffer)-1);
                                psd->setShaderTemplateName(buffer);
                                cout << "shader name is " << buffer << std::endl;
                            iff.exitChunk(TAG_NAME);


                            //---- Start PIDX Chunk [SKMG -> XXXX -> PSDT -> PIDX]
                            //---- load the shader vertex position indices (indexes into mesh's positions)
                            std::vector<int> shaderPositionIndices;
                            iff.enterChunk(TAG_PIDX);
                                const int vertexCount = iff.read_int32();
                                psd->setVertexCount(vertexCount);
                                shaderPositionIndices.reserve(vertexCount);
                                for(int v = 0; v < vertexCount; v++)
                                {
                                    int x = iff.read_int32();
                                    positionIndexLookup.emplace_back(x);
                                    shaderPositionIndices.emplace_back(x);
                                }
                                psd->setPositionIndices(shaderPositionIndices);
                            iff.exitChunk(TAG_PIDX);

                            //---- Start NIDX Chunk [SKMG -> XXXX -> PSDT -> NIDX]
                            //---- shader vertex normal indices (optional)
                            std::vector<int> shaderNormalIndices;
                            if(iff.enterChunk(TAG_NIDX, true))
                            {
                                shaderNormalIndices.reserve(vertexCount);
                                for(int v = 0; v < vertexCount; v++)
                                {
                                    shaderNormalIndices.emplace_back(iff.read_int32());
                                }
                                psd->setNormalIndices(shaderNormalIndices);
                                iff.exitChunk(TAG_NIDX);
                            }

                            //---- Start DOT3 Chunk [SKMG -> XXXX -> PSDT -> NIDX]
                            //---- dot3 per-pixel-lighting vectors (optional), 0004 only
                            if(iff.enterChunk(TAG_DOT3, true))
                            {
                                //todo should be a graphics option flag check here

                                psd->getDot3VectorIndices().reserve(vertexCount);
                                for(int v = 0; v < vertexCount; v++)
                                {
                                    psd->getDot3VectorIndices().emplace_back(iff.read_int32());
                                }
                                iff.exitChunk(TAG_DOT3);
                            }

                            //---- Start VDCL Chunk [SKMG -> XXXX -> PSDT -> VDCL]
                            //---- vertex diffuse color info (optional)
                            if(iff.enterChunk(TAG_VDCL, true))
                            {
                                psd->getDiffuseColors().reserve(vertexCount);
                                for(int v = 0; v < vertexCount; v++)
                                {
                                    const uint8 a = iff.read_uint8();
                                    const uint8 r = iff.read_uint8();
                                    const uint8 g = iff.read_uint8();
                                    const uint8 b = iff.read_uint8();
                                    psd->getDiffuseColors().emplace_back(PackedArgb(a,r,g,b));
                                }
                                iff.exitChunk(TAG_VDCL);
                            }

                            //---- Start TXCI Chunk [SKMG -> XXXX -> PSDT -> TXCI]
                            //---- texture coordinate info (optional)
                            if(iff.enterChunk(TAG_TXCI, true))
                            {
                                textureCoordinateSetCount = iff.read_int32();
                                psd->getTextureCoordinateSetDimensionality().reserve(textureCoordinateSetCount);
                                for(int v = 0; v < textureCoordinateSetCount; v++)
                                {
                                    psd->getTextureCoordinateSetDimensionality().emplace_back(iff.read_int32());
                                }
                                iff.exitChunk(TAG_TXCI);
                            }

                            //---- Start TCSF Form [SKMG -> XXXX -> PSDT -> TCSF]
                            //---- texture coordinate sets (optional)
                            if(iff.enterForm(TAG_TCSF, true))
                            {
                                std::vector<std::vector<float>> textureCoordinateSetData;
                                textureCoordinateSetData.resize(static_cast<size_t>(textureCoordinateSetCount));
                                std::vector<int> texDim = psd->getTextureCoordinateSetDimensionality();
                                for(int t = 0; t < textureCoordinateSetCount; ++t)
                                {
                                    iff.enterChunk(TAG_TCSD);
                                    std::vector<float>& tcSetData = textureCoordinateSetData[static_cast<size_t>(t)];
                                    const int dimForSet = (static_cast<size_t>(t) < texDim.size())
                                        ? texDim[static_cast<size_t>(t)] : 2;
                                    const auto arraySize = static_cast<size_t>(vertexCount * dimForSet);
                                    tcSetData.resize(arraySize);
                                    for (size_t j = 0; j < arraySize; ++j)
                                        tcSetData[j] = iff.read_float();
                                    iff.exitChunk(TAG_TCSD);
                                }
                                psd->setTextureCoordinateSetData(textureCoordinateSetData);
                                iff.exitForm(TAG_TCSF);
                            }

                            //---- Start PRIM Form [SKMG -> XXXX -> PSDT -> PRIM]
                            //---- draw primitives
                            iff.enterForm(TAG_PRIM);

                                //---- Start INFO Chunk [SKMG -> XXXX -> PSDT -> PRIM -> INFO]
                                iff.enterChunk(TAG_INFO);
                                    const int primitiveCount = iff.read_int32();
                                iff.exitChunk(TAG_INFO);

                                psd->getDrawPrimitives().reserve(primitiveCount);
                                std::vector<DrawPrimitive*> drawPrimitives;
                                for(int p = 0; p < primitiveCount; p++)
                                {
                                    switch (iff.getCurrentName())
                                    {
                                        case TAG_ITL: // Indexed Triangle List Primitive
                                        {
                                            auto* thisPrimitive = new IndexedTriListPrimitive();
                                            Vector normal;
                                            int validTriangleCount = 0;

                                            //---- Start ITL Chunk [SKMG -> XXXX -> PSDT -> PRIM -> ITL]
                                            iff.enterChunk(TAG_ITL);

                                                const int triangleCount = iff.read_int32();
                                                thisPrimitive->m_triangleCount = triangleCount;
                                                thisPrimitive->m_indices.reserve(3 * triangleCount);

                                                const int pidxSize = static_cast<int>(positionIndexLookup.size());
                                                const int posSize = static_cast<int>(positions.size());

                                                for(int x = 0; x < triangleCount; x++)
                                                {
                                                    const int index0 = static_cast<int>(iff.read_int32());
                                                    const int index1 = static_cast<int>(iff.read_int32());
                                                    const int index2 = static_cast<int>(iff.read_int32());

                                                    // Bounds check: indices must be within positionIndexLookup range
                                                    if (index0 < 0 || index0 >= pidxSize ||
                                                        index1 < 0 || index1 >= pidxSize ||
                                                        index2 < 0 || index2 >= pidxSize)
                                                    {
                                                        cerr << "ITL: triangle " << x << " index out of PIDX range: "
                                                             << index0 << "/" << index1 << "/" << index2 
                                                             << " (pidxSize=" << pidxSize << ")" << std::endl;
                                                        continue;
                                                    }

                                                    const int pi0 = positionIndexLookup[index0];
                                                    const int pi1 = positionIndexLookup[index1];
                                                    const int pi2 = positionIndexLookup[index2];

                                                    // Bounds check: position indices must be within positions range
                                                    if (pi0 < 0 || pi0 >= posSize ||
                                                        pi1 < 0 || pi1 >= posSize ||
                                                        pi2 < 0 || pi2 >= posSize)
                                                    {
                                                        cerr << "ITL: triangle " << x << " position index out of range: "
                                                             << pi0 << "/" << pi1 << "/" << pi2 
                                                             << " (posSize=" << posSize << ")" << std::endl;
                                                        continue;
                                                    }

                                                    const Vector &v0 = positions[pi0];
                                                    const Vector &v1 = positions[pi1];
                                                    const Vector &v2 = positions[pi2];

                                                    // Compute normal to check for degenerate triangles
                                                    normal = (v0 - v2).cross(v1 - v0);
                                                    if (normal.magnitudeSquared() == 0.0f)
                                                    {
                                                        // Skip degenerate triangle
                                                        continue;
                                                    }

                                                    // Valid triangle - add to mesh
                                                    polygonConnects.append(pi0);
                                                    polygonConnects.append(pi1);
                                                    polygonConnects.append(pi2);
                                                    ++validTriangleCount;

                                                    thisPrimitive->m_indices.emplace_back(index0);
                                                    thisPrimitive->m_indices.emplace_back(index1);
                                                    thisPrimitive->m_indices.emplace_back(index2);
                                                }
                                            iff.exitChunk(TAG_ITL);

                                            totalPolygonsInMesh += validTriangleCount;
                                            thisPrimitive->m_triangleCount = validTriangleCount;

                                            // add this to our list
                                            drawPrimitives.emplace_back(thisPrimitive);
                                        }
                                        break;
                                        case TAG_OITL:
                                        {
                                            auto* thisPrimitive = new OccludedIndexedTriListPrimitive();
                                            Vector normal;
                                            int validTriangleCount = 0;

                                            //---- Start OITL Chunk [SKMG -> XXXX -> PSDT -> PRIM -> OITL]
                                            iff.enterChunk(TAG_OITL);

                                                const int triangleCount = iff.read_int32();
                                                thisPrimitive->m_triangleCount = triangleCount;
                                                thisPrimitive->m_indices.reserve(4 * triangleCount); // note this is 4 not 3

                                                const int pidxSize = static_cast<int>(positionIndexLookup.size());
                                                const int posSize = static_cast<int>(positions.size());

                                                for(int x = 0; x < triangleCount; x++)
                                                {
                                                    // read occlusion zone combination index.
                                                    const int occlusionZoneCombinationIndex = static_cast<int>(iff.read_int16());

                                                    const int index0 = static_cast<int>(iff.read_int32());
                                                    const int index1 = static_cast<int>(iff.read_int32());
                                                    const int index2 = static_cast<int>(iff.read_int32());

                                                    // Bounds check: indices must be within positionIndexLookup range
                                                    if (index0 < 0 || index0 >= pidxSize ||
                                                        index1 < 0 || index1 >= pidxSize ||
                                                        index2 < 0 || index2 >= pidxSize)
                                                    {
                                                        cerr << "OITL: triangle " << x << " index out of PIDX range: "
                                                             << index0 << "/" << index1 << "/" << index2 
                                                             << " (pidxSize=" << pidxSize << ")" << std::endl;
                                                        continue;
                                                    }

                                                    const int pi0 = positionIndexLookup[index0];
                                                    const int pi1 = positionIndexLookup[index1];
                                                    const int pi2 = positionIndexLookup[index2];

                                                    // Bounds check: position indices must be within positions range
                                                    if (pi0 < 0 || pi0 >= posSize ||
                                                        pi1 < 0 || pi1 >= posSize ||
                                                        pi2 < 0 || pi2 >= posSize)
                                                    {
                                                        cerr << "OITL: triangle " << x << " position index out of range: "
                                                             << pi0 << "/" << pi1 << "/" << pi2 
                                                             << " (posSize=" << posSize << ")" << std::endl;
                                                        continue;
                                                    }

                                                    const Vector &v0 = positions[pi0];
                                                    const Vector &v1 = positions[pi1];
                                                    const Vector &v2 = positions[pi2];

                                                    // Compute normal to check for degenerate triangles
                                                    normal = (v0 - v2).cross(v1 - v0);
                                                    if (normal.magnitudeSquared() == 0.0f)
                                                    {
                                                        // Skip degenerate triangle
                                                        continue;
                                                    }

                                                    // Valid triangle - add to mesh
                                                    polygonConnects.append(pi0);
                                                    polygonConnects.append(pi1);
                                                    polygonConnects.append(pi2);
                                                    ++validTriangleCount;

                                                    thisPrimitive->m_indices.emplace_back(occlusionZoneCombinationIndex);
                                                    thisPrimitive->m_indices.emplace_back(index0);
                                                    thisPrimitive->m_indices.emplace_back(index1);
                                                    thisPrimitive->m_indices.emplace_back(index2);
                                                }

                                            iff.exitChunk(TAG_OITL);

                                            totalPolygonsInMesh += validTriangleCount;
                                            thisPrimitive->m_triangleCount = validTriangleCount;

                                            // add this to our list
                                            drawPrimitives.emplace_back(thisPrimitive);
                                        }
                                        break;
                                        default:
                                        {
                                            char tagBuffer[5];
                                            ConvertTagToString(iff.getCurrentName(), tagBuffer);
                                            FATAL(true, ("unsupported draw primitive [%s]", tagBuffer));
                                        }
                                    }
                                }
                            iff.exitForm(TAG_PRIM);

                        iff.exitForm(TAG_PSDT);

                        for(int p = 0; p < (totalPolygonsInMesh); p++)
                        {
                            polygonCounts.append(3); // mesh is made of triangles, so the vertex count of each polygon is 3
                        }

                        cerr << "preparing to create mesh: " << std::endl;
                        cerr << "total vertices in mesh: " << totalVerticesInMesh << std::endl;
                        cerr << "total polygons in mesh: " << totalPolygonsInMesh << std::endl;
                        cerr << "vertex array length: " << vertexArray.length() << std::endl;
                        cerr << "polygon counts length: " << polygonCounts.length() << std::endl;
                        cerr << "polygon connects length: " << polygonCounts.length() << std::endl;


                        MStatus createStatus;
                        MFnMesh mesh;
                        
                        //---- Begin Maya Import Operations
                        mesh.create(totalVerticesInMesh, totalPolygonsInMesh, vertexArray, polygonCounts, polygonConnects, parentTransform.object(), &createStatus);
                        mesh.setName(MayaUtility::parseFileNameToNodeName(psd->getShaderTemplateName()).c_str());

                        if (createStatus == MS::kSuccess)
                        {
                            const std::vector<std::vector<float>>& tcAll = psd->getTextureCoordinateSetData();
                            std::vector<int> texDim = psd->getTextureCoordinateSetDimensionality();
                            if (textureCoordinateSetCount > 0 && !tcAll.empty() && !texDim.empty()
                                && tcAll[0].size() >= static_cast<size_t>(vertexCount * texDim[0]))
                            {
                                const int dim0 = texDim[0];
                                if (dim0 >= 1)
                                {
                                    const std::vector<float>& tc0 = tcAll[0];
                                    MFloatArray uArr, vArr;
                                    uArr.setLength(totalVerticesInMesh);
                                    vArr.setLength(totalVerticesInMesh);
                                    for (unsigned uu = 0; uu < static_cast<unsigned>(totalVerticesInMesh); ++uu)
                                    {
                                        uArr.set(0.f, uu);
                                        vArr.set(0.f, uu);
                                    }
                                    for (int j = 0; j < vertexCount; ++j)
                                    {
                                        const int pi = positionIndexLookup[static_cast<size_t>(j)];
                                        if (pi >= 0 && pi < totalVerticesInMesh)
                                        {
                                            const float uu = tc0[static_cast<size_t>(j * dim0)];
                                            const float vv = (dim0 >= 2) ? tc0[static_cast<size_t>(j * dim0 + 1)] : 0.f;
                                            uArr.set(uu, static_cast<unsigned>(pi));
                                            vArr.set(1.0f - vv, static_cast<unsigned>(pi));
                                        }
                                    }
                                    mesh.setUVs(uArr, vArr);
                                    for (int faceId = 0; faceId < totalPolygonsInMesh; ++faceId)
                                    {
                                        mesh.assignUV(faceId, 0, polygonConnects[faceId * 3 + 0]);
                                        mesh.assignUV(faceId, 1, polygonConnects[faceId * 3 + 1]);
                                        mesh.assignUV(faceId, 2, polygonConnects[faceId * 3 + 2]);
                                    }
                                }
                            }

                            MDagPath meshDag;
                            if (mesh.getPath(meshDag) == MS::kSuccess)
                            {
                                MayaSceneBuilder::ShaderGroupData sg;
                                sg.shaderTemplateName = psd->getShaderTemplateName();
                                for (int idx = 0; idx + 2 < polygonConnects.length(); idx += 3)
                                {
                                    MayaSceneBuilder::TriangleData tri;
                                    tri.indices[0] = polygonConnects[idx];
                                    tri.indices[1] = polygonConnects[idx + 1];
                                    tri.indices[2] = polygonConnects[idx + 2];
                                    sg.triangles.push_back(tri);
                                }
                                if (!sg.triangles.empty())
                                {
                                    std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
                                    MayaSceneBuilder::assignMaterials(meshDag, groups, pathStd);
                                }
                            }
                        }
                        
                        // Store ALL blend target information as attributes on the mesh for reference
                        // Note: Creating actual Maya blend shape deformers during import can cause instability
                        // The blend target data is preserved for manual setup or export
                        if (!blendTargets.empty() && createStatus == MS::kSuccess)
                        {
                            MGlobal::displayInfo(MString("  Total blend targets in MGN: ") + static_cast<int>(blendTargets.size()));
                            
                            // Store ALL blend target names - don't filter by shader
                            // The blend targets are shared across the entire mesh, not per-shader
                            std::vector<std::string> allBlendTargetNames;
                            for(const auto &blendTarget: blendTargets)
                            {
                                // Clean up the blend target name
                                std::string cleanName = blendTarget->m_customizationVariablePathName;
                                size_t prefixPos = cleanName.find("/shared_owner/");
                                if (prefixPos != std::string::npos)
                                    cleanName = cleanName.substr(prefixPos + 14);
                                allBlendTargetNames.push_back(cleanName);
                                
                                MGlobal::displayInfo(MString("    [") + static_cast<int>(allBlendTargetNames.size()-1) + 
                                    "] " + cleanName.c_str() + " (positions: " + 
                                    static_cast<int>(blendTarget->m_positions.size()) + ", normals: " +
                                    static_cast<int>(blendTarget->m_normals.size()) + ")");
                            }
                            
                            if (!allBlendTargetNames.empty())
                            {
                                // Store blend target names as attribute on the mesh's parent transform
                                MDagPath meshDagPath;
                                mesh.getPath(meshDagPath);
                                meshDagPath.pop(); // Get parent transform
                                MFnDependencyNode meshFn(meshDagPath.node());
                                
                                std::string blendNamesStr;
                                for (size_t i = 0; i < allBlendTargetNames.size(); ++i)
                                {
                                    if (i > 0) blendNamesStr += "\t";
                                    blendNamesStr += allBlendTargetNames[i];
                                }
                                
                                // Only add attribute if it doesn't exist yet (first PSDT for this mesh)
                                MPlug existingPlug = meshFn.findPlug("swgBlendTargets", true);
                                if (existingPlug.isNull())
                                {
                                    MGlobal::executeCommand(MString("addAttr -ln \"swgBlendTargets\" -dt \"string\" ") + meshFn.name());
                                    MPlug btPlug = meshFn.findPlug("swgBlendTargets", false);
                                    if (!btPlug.isNull())
                                        btPlug.setValue(MString(blendNamesStr.c_str()));
                                    
                                    MGlobal::displayInfo(MString("  Stored ") + static_cast<int>(allBlendTargetNames.size()) + 
                                        " blend targets in " + meshFn.name() + ".swgBlendTargets");
                                }
                            }
                        }

                        polygonConnects.clear();

                    } // end for each shader data (PSDT FORM)
                }

                // Texture renderer template bindings (SkeletalMeshGeneratorTemplate TRTS / TRT chunks).
                std::vector<SwgTrtsIo::Header> trtsHeaders;
                std::string trtsErr;
                if (!SwgTrtsIo::tryConsumeOptionalTrtsForm(iff, trtsHeaders, trtsErr))
                {
                    MGlobal::displayError(MString("[MgnTranslator] TRTS parse error: ") + trtsErr.c_str());
                    iff.close();
                    return MS::kFailure;
                }
                if (!trtsHeaders.empty())
                {
                    MDagPath rootPath;
                    if (MDagPath::getAPathTo(parentTransform.object(), rootPath))
                    {
                        SwgTrtsIo::applyBindingsToTransform(rootPath, trtsHeaders, file.expandedFullName().asChar());
                    }
                }
                
                // Create hardpoints from imported data
                if (!staticHardpoints.empty() || !dynamicHardpoints.empty())
                {
                    std::string meshBaseName = MayaUtility::parseFileNameToNodeName(file.rawName().asChar());
                    
                    // Convert Hardpoint to HardpointData for MayaSceneBuilder
                    std::vector<MayaSceneBuilder::HardpointData> hardpointData;
                    
                    for (const auto& hp : staticHardpoints)
                    {
                        MayaSceneBuilder::HardpointData hpd;
                        hpd.name = hp.getHardpointName();
                        hpd.parentJoint = hp.getParentName();
                        hpd.rotation[0] = hp.getRotation().x;
                        hpd.rotation[1] = hp.getRotation().y;
                        hpd.rotation[2] = hp.getRotation().z;
                        hpd.rotation[3] = hp.getRotation().w;
                        hpd.position[0] = hp.getPosition().x;
                        hpd.position[1] = hp.getPosition().y;
                        hpd.position[2] = hp.getPosition().z;
                        hardpointData.push_back(hpd);
                    }
                    
                    for (const auto& hp : dynamicHardpoints)
                    {
                        MayaSceneBuilder::HardpointData hpd;
                        hpd.name = hp.getHardpointName();
                        hpd.parentJoint = hp.getParentName();
                        hpd.rotation[0] = hp.getRotation().x;
                        hpd.rotation[1] = hp.getRotation().y;
                        hpd.rotation[2] = hp.getRotation().z;
                        hpd.rotation[3] = hp.getRotation().w;
                        hpd.position[0] = hp.getPosition().x;
                        hpd.position[1] = hp.getPosition().y;
                        hpd.position[2] = hp.getPosition().z;
                        hardpointData.push_back(hpd);
                    }
                    
                    // Build joint map from scene
                    std::map<std::string, MDagPath> jointMap;
                    MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint);
                    for (; !dagIt.isDone(); dagIt.next())
                    {
                        MDagPath jp;
                        if (dagIt.getPath(jp))
                        {
                            MFnDagNode jfn(jp);
                            std::string jname(jfn.name().asChar());
                            jointMap[jname] = jp;
                        }
                    }
                    
                    MStatus hpStatus = MayaSceneBuilder::createHardpoints(hardpointData, jointMap, meshBaseName, parentTransform.object());
                    if (hpStatus)
                    {
                        MGlobal::displayInfo(MString("Created ") + (staticHardpoints.size() + dynamicHardpoints.size()) + " hardpoints");
                    }
                }
                
                // Store occlusion zone data as attributes on the root transform for export
                if (!occlusionZoneNames.empty())
                {
                    MDagPath rootPath;
                    if (MDagPath::getAPathTo(parentTransform.object(), rootPath))
                    {
                        MFnDependencyNode rootFn(rootPath.node());
                        
                        // Store zone names as tab-separated string
                        std::string zoneNamesStr;
                        for (size_t i = 0; i < occlusionZoneNames.size(); ++i)
                        {
                            if (i > 0) zoneNamesStr += "\t";
                            zoneNamesStr += occlusionZoneNames[i]->getString();
                        }
                        MGlobal::executeCommand(MString("addAttr -ln \"swgOcclusionZones\" -dt \"string\" ") + rootFn.name());
                        MPlug ozPlug = rootFn.findPlug("swgOcclusionZones", false);
                        if (!ozPlug.isNull())
                            ozPlug.setValue(MString(zoneNamesStr.c_str()));
                        
                        // Display occlusion zones in the Script Editor for visibility
                        MGlobal::displayInfo(MString("=== Occlusion Zones (") + static_cast<int>(occlusionZoneNames.size()) + ") ===");
                        for (size_t i = 0; i < occlusionZoneNames.size(); ++i)
                        {
                            MGlobal::displayInfo(MString("  [") + static_cast<int>(i) + "] " + occlusionZoneNames[i]->getString());
                        }
                        MGlobal::displayInfo(MString("  (Stored in attribute: ") + rootFn.name() + ".swgOcclusionZones)");
                    }
                }
                
                // Store DOT3 data count as attribute for reference
                if (!dot3Vectors.empty())
                {
                    MDagPath rootPath;
                    if (MDagPath::getAPathTo(parentTransform.object(), rootPath))
                    {
                        MFnDependencyNode rootFn(rootPath.node());
                        MGlobal::executeCommand(MString("addAttr -ln \"swgDot3Count\" -at long ") + rootFn.name());
                        MPlug dot3Plug = rootFn.findPlug("swgDot3Count", false);
                        if (!dot3Plug.isNull())
                            dot3Plug.setValue(static_cast<int>(dot3Vectors.size()));
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
 * Handles writing out (exporting) the skeletal mesh generator (.mgn)
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus MgnTranslator::writer (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    MString mpath = file.expandedFullName();
    const char* fileName = mpath.asChar();
    LOG_F(ERROR, ("MGN export start: %s", fileName));

    MStatus status;

    // Find selected skinned mesh
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    if (sel.length() == 0)
    {
        MGlobal::displayError("MGN export: select a skinned mesh first");
        return MS::kFailure;
    }

    MDagPath meshPath;
    bool foundMesh = false;
    for (unsigned i = 0; i < sel.length(); ++i)
    {
        MDagPath dp;
        if (sel.getDagPath(i, dp) && (dp.hasFn(MFn::kMesh) || dp.hasFn(MFn::kTransform)))
        {
            if (dp.hasFn(MFn::kTransform))
            {
                dp.extendToShape();
                if (!dp.hasFn(MFn::kMesh))
                    continue;
            }
            meshPath = dp;
            foundMesh = true;
            break;
        }
    }

    if (!foundMesh)
    {
        MGlobal::displayError("MGN export: no mesh found in selection");
        return MS::kFailure;
    }

    MFnMesh meshFn(meshPath, &status);
    if (!status)
    {
        MGlobal::displayError("MGN export: failed to get mesh function set");
        return MS::kFailure;
    }

    // Get skin cluster
    MObject skinClusterObj;
    {
        MItDependencyGraph dgIt(meshPath.node(), MFn::kSkinClusterFilter, MItDependencyGraph::kUpstream);
        if (!dgIt.isDone())
            skinClusterObj = dgIt.currentItem();
    }

    if (skinClusterObj.isNull())
    {
        MGlobal::displayError("MGN export: mesh has no skin cluster. Bind the mesh to a skeleton first.");
        return MS::kFailure;
    }

    MFnSkinCluster skinFn(skinClusterObj, &status);
    if (!status)
    {
        MGlobal::displayError("MGN export: failed to get skin cluster function set");
        return MS::kFailure;
    }

    // Get influence objects (joints)
    MDagPathArray influences;
    unsigned numInfluences = skinFn.influenceObjects(influences, &status);
    if (!status || numInfluences == 0)
    {
        MGlobal::displayError("MGN export: no influence objects found");
        return MS::kFailure;
    }

    // Build transform name list
    std::vector<std::string> transformNames;
    transformNames.reserve(numInfluences);
    for (unsigned i = 0; i < numInfluences; ++i)
    {
        MString name = influences[i].partialPathName();
        std::string nameStr(name.asChar());
        size_t lastPipe = nameStr.rfind('|');
        if (lastPipe != std::string::npos) nameStr = nameStr.substr(lastPipe + 1);
        size_t nsColon = nameStr.rfind(':');
        if (nsColon != std::string::npos) nameStr = nameStr.substr(nsColon + 1);
        transformNames.push_back(nameStr);
    }

    // Get vertex positions
    MPointArray mayaPoints;
    meshFn.getPoints(mayaPoints, MSpace::kObject);
    const unsigned numVerts = mayaPoints.length();

    std::vector<Vector> positions;
    positions.reserve(numVerts);
    for (unsigned i = 0; i < numVerts; ++i)
    {
        // Convert to game coordinates (negate X)
        positions.emplace_back(
            static_cast<float>(-mayaPoints[i].x),
            static_cast<float>(mayaPoints[i].y),
            static_cast<float>(mayaPoints[i].z)
        );
    }

    // Get normals
    MFloatVectorArray mayaNormals;
    meshFn.getNormals(mayaNormals, MSpace::kObject);
    std::vector<Vector> normals;
    normals.reserve(mayaNormals.length());
    for (unsigned i = 0; i < mayaNormals.length(); ++i)
    {
        normals.emplace_back(
            static_cast<float>(-mayaNormals[i].x),
            static_cast<float>(mayaNormals[i].y),
            static_cast<float>(mayaNormals[i].z)
        );
    }

    // Get skin weights for each vertex
    struct TransformWeight {
        int transformIndex;
        float weight;
    };
    std::vector<std::vector<TransformWeight>> vertexWeights(numVerts);
    int maxTransformsPerVertex = 0;

    MItGeometry geomIt(meshPath);
    for (unsigned vi = 0; !geomIt.isDone(); geomIt.next(), ++vi)
    {
        MObject component = geomIt.currentItem();
        MDoubleArray weights;
        unsigned numWeights;
        skinFn.getWeights(meshPath, component, weights, numWeights);

        std::vector<TransformWeight>& vw = vertexWeights[vi];
        for (unsigned wi = 0; wi < numWeights; ++wi)
        {
            if (weights[wi] > 0.001) // Skip negligible weights
            {
                TransformWeight tw;
                tw.transformIndex = static_cast<int>(wi);
                tw.weight = static_cast<float>(weights[wi]);
                vw.push_back(tw);
            }
        }
        // Sort by weight descending and limit to 4 influences
        std::sort(vw.begin(), vw.end(), [](const TransformWeight& a, const TransformWeight& b) {
            return a.weight > b.weight;
        });
        if (vw.size() > 4) vw.resize(4);

        // Renormalize weights
        float totalWeight = 0.0f;
        for (const auto& tw : vw) totalWeight += tw.weight;
        if (totalWeight > 0.0f)
            for (auto& tw : vw) tw.weight /= totalWeight;

        if (static_cast<int>(vw.size()) > maxTransformsPerVertex)
            maxTransformsPerVertex = static_cast<int>(vw.size());
    }

    // Get UVs
    MFloatArray uArray, vArray;
    meshFn.getUVs(uArray, vArray);

    // Get triangles and per-shader data
    MObjectArray shaders;
    MIntArray shaderIndices;
    meshFn.getConnectedShaders(0, shaders, shaderIndices);

    struct PerShaderInfo {
        std::string shaderName;
        std::vector<int> positionIndices;
        std::vector<int> normalIndices;
        std::vector<std::pair<float, float>> uvs;
        std::vector<std::array<int, 3>> triangles;
    };
    std::vector<PerShaderInfo> perShaderData(shaders.length() > 0 ? shaders.length() : 1);

    // Get shader names
    for (unsigned si = 0; si < shaders.length(); ++si)
    {
        MFnDependencyNode shaderFn(shaders[si]);
        MPlug surfaceShaderPlug = shaderFn.findPlug("surfaceShader", true, &status);
        if (status)
        {
            MPlugArray connectedPlugs;
            surfaceShaderPlug.connectedTo(connectedPlugs, true, false);
            if (connectedPlugs.length() > 0)
            {
                MFnDependencyNode materialFn(connectedPlugs[0].node());
                perShaderData[si].shaderName = std::string("shader/") + materialFn.name().asChar() + ".sht";
            }
        }
        if (perShaderData[si].shaderName.empty())
            perShaderData[si].shaderName = "shader/defaultappearance.sht";
    }
    if (perShaderData.empty())
    {
        perShaderData.resize(1);
        perShaderData[0].shaderName = "shader/defaultappearance.sht";
    }

    // Iterate over polygons
    MItMeshPolygon polyIt(meshPath);
    int polyIndex = 0;
    for (; !polyIt.isDone(); polyIt.next(), ++polyIndex)
    {
        int shaderIdx = (shaderIndices.length() > 0 && polyIndex < static_cast<int>(shaderIndices.length())) 
                        ? shaderIndices[polyIndex] : 0;
        if (shaderIdx < 0 || shaderIdx >= static_cast<int>(perShaderData.size()))
            shaderIdx = 0;

        PerShaderInfo& psd = perShaderData[static_cast<size_t>(shaderIdx)];

        // Triangulate the polygon
        MIntArray triVerts;
        MPointArray triPoints;
        polyIt.getTriangles(triPoints, triVerts);

        int numTris = static_cast<int>(triVerts.length()) / 3;
        for (int ti = 0; ti < numTris; ++ti)
        {
            std::array<int, 3> tri;
            for (int tv = 0; tv < 3; ++tv)
            {
                int localIdx = triVerts[static_cast<unsigned>(ti * 3 + tv)];
                int globalIdx;
                polyIt.getVertices(triVerts); // Get polygon vertices
                
                // Find the vertex index in the polygon
                int vertIdx = -1;
                for (unsigned pvi = 0; pvi < polyIt.polygonVertexCount(); ++pvi)
                {
                    int pvGlobal = polyIt.vertexIndex(static_cast<int>(pvi));
                    if (pvGlobal == localIdx)
                    {
                        vertIdx = static_cast<int>(pvi);
                        break;
                    }
                }

                globalIdx = localIdx;
                int normalIdx = polyIt.normalIndex(vertIdx >= 0 ? vertIdx : 0);

                // Get UV
                float u = 0.0f, v = 0.0f;
                int uvIdx;
                if (polyIt.getUVIndex(vertIdx >= 0 ? vertIdx : 0, uvIdx) && uvIdx >= 0 && uvIdx < static_cast<int>(uArray.length()))
                {
                    u = uArray[static_cast<unsigned>(uvIdx)];
                    v = 1.0f - vArray[static_cast<unsigned>(uvIdx)]; // Flip V
                }

                // Add to per-shader data
                int shaderVertIdx = static_cast<int>(psd.positionIndices.size());
                psd.positionIndices.push_back(globalIdx);
                psd.normalIndices.push_back(normalIdx);
                psd.uvs.emplace_back(u, v);
                tri[static_cast<size_t>(tv)] = shaderVertIdx;
            }
            psd.triangles.push_back(tri);
        }
    }

    // Gather blend shape data from Maya
    struct BlendTargetExport {
        std::string name;
        std::vector<std::pair<int, Vector>> positionDeltas; // index, delta
        std::vector<std::pair<int, Vector>> normalDeltas;   // index, delta
    };
    std::vector<BlendTargetExport> blendTargets;
    
    // Find blend shape deformers on the mesh
    {
        MItDependencyGraph dgIt(meshPath.node(), MFn::kBlendShape, MItDependencyGraph::kUpstream);
        for (; !dgIt.isDone(); dgIt.next())
        {
            MFnBlendShapeDeformer bsFn(dgIt.currentItem(), &status);
            if (!status) continue;
            
            MObjectArray baseObjects;
            bsFn.getBaseObjects(baseObjects);
            
            // Get target shapes
            MIntArray weightIndices;
            bsFn.weightIndexList(weightIndices);
            
            for (unsigned wi = 0; wi < weightIndices.length(); ++wi)
            {
                int weightIdx = weightIndices[wi];
                MObjectArray targets;
                bsFn.getTargets(baseObjects[0], weightIdx, targets);
                
                if (targets.length() > 0)
                {
                    MFnDependencyNode targetFn(targets[0]);
                    BlendTargetExport bt;
                    bt.name = targetFn.name().asChar();
                    
                    // Get target mesh points
                    if (targets[0].hasFn(MFn::kMesh))
                    {
                        MFnMesh targetMeshFn(targets[0]);
                        MPointArray targetPoints;
                        targetMeshFn.getPoints(targetPoints, MSpace::kObject);
                        
                        // Compare with base mesh to get deltas
                        for (unsigned vi = 0; vi < targetPoints.length() && vi < mayaPoints.length(); ++vi)
                        {
                            MVector delta = targetPoints[vi] - mayaPoints[vi];
                            if (delta.length() > 0.0001)
                            {
                                Vector gameDelta(
                                    static_cast<float>(-delta.x),
                                    static_cast<float>(delta.y),
                                    static_cast<float>(delta.z)
                                );
                                bt.positionDeltas.emplace_back(static_cast<int>(vi), gameDelta);
                            }
                        }
                    }
                    
                    if (!bt.positionDeltas.empty())
                        blendTargets.push_back(bt);
                }
            }
        }
    }
    
    // Gather occlusion zone data from mesh transform attributes
    std::vector<std::string> occlusionZoneNames;
    std::vector<int> zonesThisOccludes;
    int occlusionLayer = -1;
    {
        MDagPath meshTransformPath = meshPath;
        meshTransformPath.pop(); // Get transform
        MFnDependencyNode meshTransformFn(meshTransformPath.node());
        
        MPlug ozPlug = meshTransformFn.findPlug("swgOcclusionZones", false);
        if (!ozPlug.isNull())
        {
            MString ozStr;
            ozPlug.getValue(ozStr);
            std::string zones(ozStr.asChar());
            size_t start = 0;
            while (start < zones.size())
            {
                size_t end = zones.find('\t', start);
                if (end == std::string::npos) end = zones.size();
                std::string zone = zones.substr(start, end - start);
                if (!zone.empty()) occlusionZoneNames.push_back(zone);
                start = end + 1;
            }
        }
        
        MPlug layerPlug = meshTransformFn.findPlug("swgOcclusionLayer", false);
        if (!layerPlug.isNull())
            layerPlug.getValue(occlusionLayer);
    }
    
    // Gather hardpoint data from child locators
    struct HardpointExport {
        std::string name;
        std::string parentJoint;
        Quaternion rotation;
        Vector position;
        bool isDynamic;
    };
    std::vector<HardpointExport> staticHardpoints;
    std::vector<HardpointExport> dynamicHardpoints;
    {
        MDagPath meshTransformPath = meshPath;
        meshTransformPath.pop();
        MFnDagNode meshTransformFn(meshTransformPath);
        
        for (unsigned ci = 0; ci < meshTransformFn.childCount(); ++ci)
        {
            MObject childObj = meshTransformFn.child(ci);
            if (!childObj.hasFn(MFn::kLocator)) continue;
            
            MDagPath locPath;
            MDagPath::getAPathTo(childObj, locPath);
            locPath.pop(); // Get transform
            
            MFnTransform locFn(locPath);
            std::string locName(locFn.name().asChar());
            
            // Check if it's a hardpoint (starts with hp_)
            if (locName.find("hp_") != 0) continue;
            
            HardpointExport hp;
            hp.name = locName;
            
            // Get parent joint name from attribute or default
            MPlug parentPlug = locFn.findPlug("swgHardpointParent", false);
            if (!parentPlug.isNull())
            {
                MString parentStr;
                parentPlug.getValue(parentStr);
                hp.parentJoint = parentStr.asChar();
            }
            
            // Get transform
            MVector trans = locFn.getTranslation(MSpace::kTransform);
            hp.position = Vector(static_cast<float>(-trans.x), static_cast<float>(trans.y), static_cast<float>(trans.z));
            
            MEulerRotation rot;
            locFn.getRotation(rot);
            Quaternion qx(static_cast<float>(rot.x), Vector::unitX);
            Quaternion qy(static_cast<float>(-rot.y), Vector::unitY);
            Quaternion qz(static_cast<float>(-rot.z), Vector::unitZ);
            hp.rotation = qz * (qy * qx);
            
            // Check if dynamic
            MPlug dynPlug = locFn.findPlug("swgHardpointDynamic", false);
            hp.isDynamic = false;
            if (!dynPlug.isNull())
                dynPlug.getValue(hp.isDynamic);
            
            if (hp.isDynamic)
                dynamicHardpoints.push_back(hp);
            else
                staticHardpoints.push_back(hp);
        }
    }

    // Write SKMG file
    Iff iff(4 * 1024 * 1024);
    iff.insertForm(TAG_SKMG);
    {
        iff.insertForm(TAG_0004); // Version 4
        {
            // INFO chunk
            iff.insertChunk(TAG_INFO);
            {
                iff.insertChunkData(static_cast<int32>(maxTransformsPerVertex));
                iff.insertChunkData(static_cast<int32>(numInfluences)); // max transforms per shader (use total)
                iff.insertChunkData(static_cast<int32>(0)); // skeleton template count (will use XFNM instead)
                iff.insertChunkData(static_cast<int32>(transformNames.size()));
                iff.insertChunkData(static_cast<int32>(positions.size()));
                iff.insertChunkData(static_cast<int32>(normals.size()));
                iff.insertChunkData(static_cast<int32>(perShaderData.size()));
                iff.insertChunkData(static_cast<int32>(blendTargets.size())); // blend target count
                iff.insertChunkData(static_cast<int16>(occlusionZoneNames.size())); // occlusion zone count
                iff.insertChunkData(static_cast<int16>(0)); // occlusion zone combination count
                iff.insertChunkData(static_cast<int16>(zonesThisOccludes.size())); // zones this occludes count
                iff.insertChunkData(static_cast<int16>(occlusionLayer)); // occlusion layer
            }
            iff.exitChunk(TAG_INFO);

            // XFNM chunk - transform names
            iff.insertChunk(TAG_XFNM);
            {
                for (const std::string& name : transformNames)
                    iff.insertChunkString(name.c_str());
            }
            iff.exitChunk(TAG_XFNM);

            // POSN chunk - positions
            iff.insertChunk(TAG_POSN);
            {
                for (const Vector& p : positions)
                    iff.insertChunkFloatVector(p);
            }
            iff.exitChunk(TAG_POSN);

            // TWHD chunk - transform weight headers (count per vertex)
            iff.insertChunk(TAG_TWHD);
            {
                for (const auto& vw : vertexWeights)
                    iff.insertChunkData(static_cast<uint32>(vw.size()));
            }
            iff.exitChunk(TAG_TWHD);

            // TWDT chunk - transform weight data
            iff.insertChunk(TAG_TWDT);
            {
                for (const auto& vw : vertexWeights)
                {
                    for (const auto& tw : vw)
                    {
                        iff.insertChunkData(static_cast<uint32>(tw.transformIndex));
                        iff.insertChunkData(tw.weight);
                    }
                }
            }
            iff.exitChunk(TAG_TWDT);

            // NORM chunk - normals
            iff.insertChunk(TAG_NORM);
            {
                for (const Vector& n : normals)
                    iff.insertChunkFloatVector(n);
            }
            iff.exitChunk(TAG_NORM);

            // Per-shader data forms
            for (const PerShaderInfo& psd : perShaderData)
            {
                iff.insertForm(TAG_PSDT);
                {
                    // NAME chunk - shader name
                    iff.insertChunk(TAG_NAME);
                    iff.insertChunkString(psd.shaderName.c_str());
                    iff.exitChunk(TAG_NAME);

                    // PIDX chunk - position indices
                    iff.insertChunk(TAG_PIDX);
                    {
                        iff.insertChunkData(static_cast<int32>(psd.positionIndices.size()));
                        for (int idx : psd.positionIndices)
                            iff.insertChunkData(static_cast<int32>(idx));
                    }
                    iff.exitChunk(TAG_PIDX);

                    // NIDX chunk - normal indices
                    iff.insertChunk(TAG_NIDX);
                    {
                        for (int idx : psd.normalIndices)
                            iff.insertChunkData(static_cast<int32>(idx));
                    }
                    iff.exitChunk(TAG_NIDX);

                    // TXCI chunk - texture coordinate info
                    iff.insertChunk(TAG_TXCI);
                    {
                        iff.insertChunkData(static_cast<int32>(1)); // 1 UV set
                        iff.insertChunkData(static_cast<int32>(2)); // 2D UVs
                    }
                    iff.exitChunk(TAG_TXCI);

                    // TCSF form - texture coordinate sets
                    iff.insertForm(TAG_TCSF);
                    {
                        iff.insertChunk(TAG_TCSD);
                        {
                            for (const auto& uv : psd.uvs)
                            {
                                iff.insertChunkData(uv.first);
                                iff.insertChunkData(uv.second);
                            }
                        }
                        iff.exitChunk(TAG_TCSD);
                    }
                    iff.exitForm(TAG_TCSF);

                    // PRIM form - primitives
                    iff.insertForm(TAG_PRIM);
                    {
                        iff.insertChunk(TAG_INFO);
                        iff.insertChunkData(static_cast<int32>(1)); // 1 primitive
                        iff.exitChunk(TAG_INFO);

                        // ITL chunk - indexed triangle list
                        iff.insertChunk(TAG_ITL);
                        {
                            iff.insertChunkData(static_cast<int32>(psd.triangles.size() * 3));
                            for (const auto& tri : psd.triangles)
                            {
                                iff.insertChunkData(static_cast<int32>(tri[0]));
                                iff.insertChunkData(static_cast<int32>(tri[1]));
                                iff.insertChunkData(static_cast<int32>(tri[2]));
                            }
                        }
                        iff.exitChunk(TAG_ITL);
                    }
                    iff.exitForm(TAG_PRIM);
                }
                iff.exitForm(TAG_PSDT);
            }
            
            // Write HPTS form - hardpoints (if any)
            if (!staticHardpoints.empty() || !dynamicHardpoints.empty())
            {
                iff.insertForm(TAG_HPTS);
                {
                    // STAT chunk - static hardpoints
                    if (!staticHardpoints.empty())
                    {
                        iff.insertChunk(TAG_STAT);
                        iff.insertChunkData(static_cast<int16>(staticHardpoints.size()));
                        for (const auto& hp : staticHardpoints)
                        {
                            iff.insertChunkString(hp.name.c_str());
                            iff.insertChunkString(hp.parentJoint.c_str());
                            iff.insertChunkFloatQuaternion(hp.rotation);
                            iff.insertChunkFloatVector(hp.position);
                        }
                        iff.exitChunk(TAG_STAT);
                    }
                    
                    // DYN chunk - dynamic hardpoints
                    if (!dynamicHardpoints.empty())
                    {
                        iff.insertChunk(TAG_DYN);
                        iff.insertChunkData(static_cast<int16>(dynamicHardpoints.size()));
                        for (const auto& hp : dynamicHardpoints)
                        {
                            iff.insertChunkString(hp.name.c_str());
                            iff.insertChunkString(hp.parentJoint.c_str());
                            iff.insertChunkFloatQuaternion(hp.rotation);
                            iff.insertChunkFloatVector(hp.position);
                        }
                        iff.exitChunk(TAG_DYN);
                    }
                }
                iff.exitForm(TAG_HPTS);
            }
            
            // Write BLTS form - blend targets (if any)
            if (!blendTargets.empty())
            {
                iff.insertForm(TAG_BLTS);
                {
                    for (const auto& bt : blendTargets)
                    {
                        iff.insertForm(TAG_BLT);
                        {
                            // INFO chunk
                            iff.insertChunk(TAG_INFO);
                            iff.insertChunkData(static_cast<int32>(bt.positionDeltas.size()));
                            iff.insertChunkData(static_cast<int32>(bt.normalDeltas.size()));
                            iff.insertChunkString(bt.name.c_str());
                            iff.exitChunk(TAG_INFO);
                            
                            // POSN chunk - position deltas
                            iff.insertChunk(TAG_POSN);
                            for (const auto& pd : bt.positionDeltas)
                            {
                                iff.insertChunkData(static_cast<int32>(pd.first));
                                iff.insertChunkFloatVector(pd.second);
                            }
                            iff.exitChunk(TAG_POSN);
                            
                            // NORM chunk - normal deltas (empty for now)
                            iff.insertChunk(TAG_NORM);
                            for (const auto& nd : bt.normalDeltas)
                            {
                                iff.insertChunkData(static_cast<int32>(nd.first));
                                iff.insertChunkFloatVector(nd.second);
                            }
                            iff.exitChunk(TAG_NORM);
                        }
                        iff.exitForm(TAG_BLT);
                    }
                }
                iff.exitForm(TAG_BLTS);
            }
            
            // Write OZN chunk - occlusion zone names (if any)
            if (!occlusionZoneNames.empty())
            {
                iff.insertChunk(TAG_OZN);
                for (const auto& name : occlusionZoneNames)
                    iff.insertChunkString(name.c_str());
                iff.exitChunk(TAG_OZN);
            }
            
            // Write ZTO chunk - zones this occludes (if any)
            if (!zonesThisOccludes.empty())
            {
                iff.insertChunk(TAG_ZTO);
                for (int zoneIdx : zonesThisOccludes)
                    iff.insertChunkData(static_cast<int16>(zoneIdx));
                iff.exitChunk(TAG_ZTO);
            }
        }
        iff.exitForm(TAG_0004);
    }
    iff.exitForm(TAG_SKMG);

    if (!iff.write(fileName))
    {
        MGlobal::displayError(MString("MGN export: failed to write ") + fileName);
        return MS::kFailure;
    }

    copyMgnShipBundleIFFs(meshPath, fileName);

    {
        const std::string mgnFull(fileName);
        const size_t ls = mgnFull.find_last_of("/\\");
        const std::string bn = (ls == std::string::npos) ? mgnFull : mgnFull.substr(ls + 1);
        mirrorExportToDataRootExported(mgnFull, bn);
    }

    MGlobal::displayInfo(MString("MGN exported: ") + fileName + " (" + numVerts + " verts, " + transformNames.size() + " influences)");
    return MS::kSuccess;
}

/**
 * @return the file type this translator handles
 */
MString MgnTranslator::defaultExtension () const
{
    return "mgn";
}

MString MgnTranslator::filter () const
{
    return MString(swg_translator::kFilterMgn);
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind MgnTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && !strcasecmp(pathStr.c_str() + nameLength - 4, ".mgn"))
        return kCouldBeMyFileType;
    return kNotMyFileType;
}

