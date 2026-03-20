#include "mgn.h"

#include "Iff.h"
#include "Globals.h"
#include "Misc.h"
#include "Tag.h"
#include "Vector.h"
#include "PackedArgb.h"
#include "Quaternion.h"
#include "OcclusionZoneSet.h"
#include "MayaUtility.h"

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

#include <vector>

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
const Tag TAG_TRTS = TAG(T,R,T,S);
const Tag TAG_OZN = TAG3(O,Z,N);
const Tag TAG_OZC = TAG3(O,Z,C);
const Tag TAG_ZTO = TAG3(Z,T,O);
const Tag TAG_DYN = TAG3(D,Y,N);
const Tag TAG_TRT = TAG3(T,R,T);
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
    
    const char* fileName = file.expandedFullName().asChar();
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
                std::vector<Vector> positions;
                iff.enterChunk(TAG_POSN);
                for(int i = 0; i < positionCount; i++)
                {
                    Vector v = iff.read_floatVector();
                    vertexArray.append(v.x, v.y, v.z);
                    positions.emplace_back(v);
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
                                std::vector<std::vector<float>> textureCoordinateSetData = psd->getTextureCoordinateSetData();
                                std::vector<int> textureCoordinateSetDimensionality = psd->getTextureCoordinateSetDimensionality();
                                textureCoordinateSetData.resize(static_cast<size_t>(textureCoordinateSetCount));
                                // load up each texture coordinate set's data
                                for(int t = 0; t < textureCoordinateSetCount; ++t)
                                {
                                    //---- Start TCSD Chunk [SKMG -> XXXX -> PSDT -> TCSF -> TCSD]
                                    iff.enterChunk(TAG_TCSD);
                                    /*
                                        std::vector<float> &tcSetData = textureCoordinateSetData[static_cast<size_t>(i)];
                                    
                                        const auto arraySize = static_cast<size_t>(vertexCount * textureCoordinateSetDimensionality[static_cast<size_t>(i)]);
                                        tcSetData.reserve(arraySize);
                                    
                                        for (size_t j = 0; j < arraySize; ++j)
                                        {
                                            tcSetData.push_back(iff.read_float());
                                        }
                                        psd->setTextureCoordinateSetData(textureCoordinateSetData);
                                     */
                                    iff.exitChunk(TAG_TCSD);
                                }
                                iff.exitForm(TAG_TCSF);
                            }

                            //---- Start PRIM Form [SKMG -> XXXX -> PSDT -> PRIM]
                            //---- draw primitives
                            iff.enterForm(TAG_PRIM);

                                //---- Start INFO Chunk [SKMG -> XXXX -> PSDT -> PRIM -> INFO]
                                iff.enterChunk(TAG_INFO);
                                    const int primitiveCount = iff.read_int32();
                                iff.exitChunk(TAG_INFO);

                                int zeroAreaTriCount = 0;
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
                                            int localZeroAreaTriCount = 0;

                                            //---- Start ITL Chunk [SKMG -> XXXX -> PSDT -> PRIM -> ITL]
                                            iff.enterChunk(TAG_ITL);

                                                const int triangleCount = iff.read_int32();
                                                thisPrimitive->m_triangleCount = triangleCount;
                                                thisPrimitive->m_indices.reserve(3 * triangleCount);
                                                totalPolygonsInMesh = triangleCount;

                                                for(int x = 0; x < triangleCount; x++)
                                                {

                                                    const int index0 = static_cast<int>(iff.read_int32());
                                                    const int index1 = static_cast<int>(iff.read_int32());
                                                    const int index2 = static_cast<int>(iff.read_int32());

                                                    polygonConnects.append(positionIndexLookup[index0]);
                                                    polygonConnects.append(positionIndexLookup[index1]);
                                                    polygonConnects.append(positionIndexLookup[index2]);

                                                    const Vector &v0 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index0)])];
                                                    const Vector &v1 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index1)])];
                                                    const Vector &v2 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index2)])];

                                                    // Compute normal.
                                                    normal = (v0 - v2).cross(v1 - v0);
                                                    if (normal.magnitudeSquared () == 0.0f)
                                                    {
                                                        //-- Do not keep this triangle.
                                                        ++localZeroAreaTriCount;
                                                    }
                                                    else
                                                    {
                                                        //-- Keep this triangle.
                                                        thisPrimitive->m_indices.emplace_back(index0);
                                                        thisPrimitive->m_indices.emplace_back(index1);
                                                        thisPrimitive->m_indices.emplace_back(index2);
                                                    }

                                                }
                                            iff.exitChunk(TAG_ITL);
                                            //-- Accumulate total zero are tri counts.
                                            zeroAreaTriCount += localZeroAreaTriCount;

                                            //-- Adjust  triangle count for primitive.
                                            thisPrimitive->m_triangleCount -= localZeroAreaTriCount;

                                            // add this to our list
                                            drawPrimitives.emplace_back(thisPrimitive);
                                        }
                                        break;
                                        case TAG_OITL:
                                        {
                                            auto* thisPrimitive = new OccludedIndexedTriListPrimitive();
                                            Vector normal;
                                            int localZeroAreaTriCount = 0;

                                            //---- Start OITL Chunk [SKMG -> XXXX -> PSDT -> PRIM -> OITL]
                                            iff.enterChunk(TAG_OITL);

                                                const int triangleCount = iff.read_int32();
                                                thisPrimitive->m_triangleCount = triangleCount;
                                                thisPrimitive->m_indices.reserve(4 * triangleCount); // note this is 4 not 3
                                                totalPolygonsInMesh = triangleCount;

                                                for(int x = 0; x < triangleCount; x++)
                                                {
                                                    // read occlusion zone combination index.
                                                    const int occlusionZoneCombinationIndex = static_cast<int>(iff.read_int16());

                                                    const int index0 = static_cast<int>(iff.read_int32());
                                                    const int index1 = static_cast<int>(iff.read_int32());
                                                    const int index2 = static_cast<int>(iff.read_int32());

                                                    polygonConnects.append(positionIndexLookup[index0]);
                                                    polygonConnects.append(positionIndexLookup[index1]);
                                                    polygonConnects.append(positionIndexLookup[index2]);

                                                    const Vector &v0 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index0)])];
                                                    const Vector &v1 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index1)])];
                                                    const Vector &v2 = positions[static_cast<std::vector<Vector>::size_type>(positionIndexLookup[static_cast<std::vector<int>::size_type>(index2)])];

                                                    // Compute normal.
                                                    normal = (v0 - v2).cross(v1 - v0);
                                                    if (normal.magnitudeSquared() == 0.0f)
                                                    {
                                                        //-- Do not keep this triangle.
                                                        ++localZeroAreaTriCount;
                                                    } else
                                                    {
                                                        //-- Keep this triangle.
                                                        thisPrimitive->m_indices.emplace_back(occlusionZoneCombinationIndex);
                                                        thisPrimitive->m_indices.emplace_back(index0);
                                                        thisPrimitive->m_indices.emplace_back(index1);
                                                        thisPrimitive->m_indices.emplace_back(index2);
                                                    }
                                                }

                                            iff.exitChunk(TAG_OITL);

                                            //-- Accumulate total zero are tri counts.
                                            zeroAreaTriCount += localZeroAreaTriCount;

                                            //-- Adjust  triangle count for primitive.
                                            thisPrimitive->m_triangleCount -= localZeroAreaTriCount;

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
                        
                        // when we create this mesh, we need to iterate through its position and normal indices
                        // to see if we have any blend targets that cross through, because if so, that blend
                        // target belongs to this mesh
                        for(const auto &blendTarget: blendTargets)
                        {
                            bool blendBelongsToThisMesh = false;
                            std::vector<int> shaderNormals = psd->getNormalIndices();
                            std::vector<int> shaderPositions = psd->getPositionIndices();
                            
                            for(const auto &normal: blendTarget->m_normals)
                            {
                                if(std::find(shaderNormals.begin(), shaderNormals.end(), normal.m_index) != shaderNormals.end())
                                {
                                    blendBelongsToThisMesh = true;
                                }
                            }
                            if(!blendBelongsToThisMesh)
                            {
                                for(const auto &position: blendTarget->m_positions)
                                {
                                    if(std::find(shaderPositions.begin(), shaderPositions.end(), position.m_index) != shaderPositions.end())
                                    {
                                        blendBelongsToThisMesh = true;
                                    }
                                }
                            }
                            if(blendBelongsToThisMesh)
                            {
                                // create a new mesh with vertices modified by the positions of the blend target
                                MFnMesh targetMesh;
                                MFloatPointArray vertexArrayForBlend;
                                vertexArray.copy(vertexArrayForBlend);
                                
                                // for each vector in the mesh
                                for(int x = 0; x < vertexArrayForBlend.length(); x++)
                                {
                                    // for each index identified in the blend target
                                    for(auto & m_position : blendTarget->m_positions)
                                    {
                                        // if the index in the blend target matches the index of the vertex array
                                        if(x == m_position.m_index)
                                        {
                                            // override the position of the vertex array based on the position of the blend target
                                            Vector v = m_position.m_deltaVector;
                                            vertexArrayForBlend[x] = MFloatPoint(v.x, v.y, v.x);
                                        }
                                    }
                                }
                                targetMesh.create(totalVerticesInMesh, totalPolygonsInMesh, vertexArrayForBlend, polygonCounts, polygonConnects, parentTransform.object(), &createStatus);
                                targetMesh.setName(blendTarget->m_customizationVariablePathName.c_str());
                            }
                        }

                        polygonConnects.clear();

                    } // end for each shader data (PSDT FORM)
                }

                //---- Start TRTS Form [SKMG -> XXXX -> TRTS]
                //Note: 0002 has slight code difference, but should not matter
                //---- load texture renderer templates (optional)
                if(iff.enterForm(TAG_TRTS, true))
                {
                    iff.enterChunk(TAG_INFO);
                    const int headerCount = iff.read_int32();
                    const int entryCount = iff.read_int32();
                    iff.exitChunk(TAG_INFO);

                    //todo need to wait to see an example of this to implement it properly
                }
                
                // join all children of the mesh
                
                
                MPointArray unblendedPositions;
                
                
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
 * Handles writing out (exporting) the skeleton
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus MgnTranslator::writer (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
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
    return "Skeletal Mesh Generator | SWG (*.mgn)";
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
    const char *name = fileName.resolvedName().asChar();
    int nameLength = (int)strlen(name);
    if((nameLength>4) && !strcasecmp(name+nameLength-4, ".mgn"))
    {
        return (kIsMyFileType);
    }
    return (kNotMyFileType);
}

