// ==================================================================
//
// MeshConstructionHelper.h
// copyright 2001 Sony Online Entertainment
//
// ==================================================================

#ifndef MESH_CONSTRUCTION_HELPER_H
#define MESH_CONSTRUCTION_HELPER_H

// ==================================================================
// includes

#include "Tag.h"
#include "Binary.h"
#include "CrcLowerString.h"
#include "LessPointerComparator.h"

#include <map>

class CrcString;
class CrcLowerString;
class LessPointerComparator;
class PackedArgb;
class ShaderTemplate;
class Vector;

// ==================================================================

class MeshConstructionHelper
{

public:

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // shader vertex format flags
    enum
    {
        SVF_xyz    = BINARY2(0000, 0001),
        SVF_normal = BINARY2(0000, 0010),
        SVF_argb   = BINARY2(0000, 0100)
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    typedef int IndexedTriangle[3];

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct TransformData
    {
    public:

        TransformData(void);
        TransformData(int transformIndex, float transformWeight);

    public:

        int  m_transformIndex;
        float m_transformWeight;

    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct PerShaderData;
    struct PerTextureRendererData;
    struct TriListHeader;
    struct TriStripHeader;
    struct VertexData;

public:

    MeshConstructionHelper(void);
    ~MeshConstructionHelper(void);

    //-- construction interface
    void                          clearAllData(void);
    void                          prepareForReading(void);

    void                          allocatePositionBuffer(int positionCount, int *firstIndexToUse, Vector **positionBuffer);
    void                          addPositionWeight(int positionIndex, int transformIndex, float weight);

    void                          allocateNormalBuffer(int normalCount, int *firstIndexToUse, Vector **normalBuffer);
    // -TRF- clients might want to modify and share color data.  wait until this is needed.
    // for now adorn per shader vertex.

    std::shared_ptr<PerShaderData> addShaderTemplate(const char *shaderTemplateName);
    void                          setVertexFormat(std::shared_ptr<PerShaderData> perShaderData, uint shaderVertexFormat);
    void                          setTextureCoordinateSetCount(std::shared_ptr<PerShaderData> perShaderData, int setCount);
    void                          setTextureCoordinateDimensionality(std::shared_ptr<PerShaderData> perShaderData, int setIndex, int dimensionality);

    int                           getNextNewVertexIndex(const std::shared_ptr<PerShaderData> perShaderData) const;
    void                          addVertexPosition(std::shared_ptr<PerShaderData> perShaderData, int positionIndex);
    void                          setVertexNormal(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, int normalIndex);
    void                          setVertexColor(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, const PackedArgb &color);
    void                          setTextureCoordinates(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, int tcSetIndex, float tc0);
    void                          setTextureCoordinates(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, int tcSetIndex, float tc0, float tc1);
    void                          setTextureCoordinates(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, int tcSetIndex, float tc0, float tc1, float tc2);
    void                          setTextureCoordinates(std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex, int tcSetIndex, float tc0, float tc1, float tc2, float tc3);

    // primitive support
    int                           addTriStrip(std::shared_ptr<PerShaderData> perShaderData, bool flipCullMode);
    void                          addTriStripVertex(std::shared_ptr<PerShaderData> perShaderData, int triStripIndex, int shaderVertexIndex);

    TriListHeader                &addTriList(std::shared_ptr<PerShaderData> perShaderData);
    void                          addTriListTri(std::shared_ptr<PerShaderData> perShaderData, TriListHeader &triList, int indexV0, int indexV1, int indexV2);

    PerTextureRendererData       *addTextureRendererTemplate(const CrcLowerString &textureRendererTemplateName);
    void                          addAffectedShaderTemplate(PerTextureRendererData *perTextureRendererData, Tag shaderTextureTag, std::shared_ptr<PerShaderData> perShaderData);

    //-- querying interface
    int                           getShaderCount(void) const;
    std::shared_ptr<PerShaderData> getPerShaderData(int shaderIndex) const;

    const CrcString              &getShaderTemplateName(const std::shared_ptr<PerShaderData> perShaderData) const;
    uint                          getVertexFormat(const std::shared_ptr<PerShaderData> perShaderData) const;
    int                           getTextureCoordinateSets(const std::shared_ptr<PerShaderData> perShaderData) const;
    int                           getTextureCoordinateDimensionality(const std::shared_ptr<PerShaderData> perShaderData, int setIndex) const;
    int                           getNumberOfTransformWeightPairs(const std::shared_ptr<PerShaderData> perShaderData) const;
    int                           getNumberOfWeightedPrimitiveIndices(const std::shared_ptr<PerShaderData> perShaderData) const;

    int                           getVertexCount(const std::shared_ptr<PerShaderData> perShaderData) const;
    const VertexData             &getVertexData(const std::shared_ptr<PerShaderData> perShaderData, int shaderVertexIndex) const;
    const Vector                 &getPosition(const VertexData &vertexData) const;
    const Vector                 &getNormal(const VertexData &vertexData) const;
    const PackedArgb             &getDiffuseColor(const VertexData &vertexData) const;
    void                          getTextureCoordinates(const std::shared_ptr<PerShaderData> perShaderData, const VertexData &vertexData, int textureCoordinateSet, float &tc0) const;
    void                          getTextureCoordinates(const std::shared_ptr<PerShaderData> perShaderData, const VertexData &vertexData, int textureCoordinateSet, float &tc0, float &tc1) const;
    void                          getTextureCoordinates(const std::shared_ptr<PerShaderData> perShaderData, const VertexData &vertexData, int textureCoordinateSet, float &tc0, float &tc1, float &tc2) const;
    void                          getTextureCoordinates(const std::shared_ptr<PerShaderData> perShaderData, const VertexData &vertexData, int textureCoordinateSet, float &tc0, float &tc1, float &tc2, float &tc3) const;

    int                           getVertexTransformCount(const VertexData &vertexData) const;
    const TransformData          &getVertexTransformData(const VertexData &vertexData, int whichTransform) const;

    int                           getTriStripCount(const std::shared_ptr<PerShaderData> perShaderData) const;
    const TriStripHeader         &getTriStrip(const std::shared_ptr<PerShaderData> perShaderData, int triStripIndex) const;
    bool                          getTriStripCullModeFlipped(const TriStripHeader &triStripHeader) const;
    int                           getTriStripVertexCount(const TriStripHeader &triStripHeader) const;
    int                           getTriStripVertexIndex(const TriStripHeader &triStripHeader, int vertexIndex) const;

    int                           getTriListCount(const std::shared_ptr<PerShaderData> perShaderData) const;
    const TriListHeader          &getTriList(const std::shared_ptr<PerShaderData> perShaderData, int triListIndex) const;
    int                           getTriListTriangleCount(const TriListHeader &triListHeader) const;
    void                          getTriListTriangle(const std::shared_ptr<PerShaderData> perShaderData, const TriListHeader &triListHeader, int triIndex, int &index0, int &index1, int &index2) const;

    int                           getTextureRendererCount() const;
    const PerTextureRendererData *getPerTextureRendererData(int index) const;
    const CrcLowerString         &getTextureRendererTemplateName(const PerTextureRendererData *perTextureRendererData) const;
    int                           getAffectedShaderCount(const PerTextureRendererData *perTextureRendererData) const;
    void                          getAffectedShaderData(const PerTextureRendererData *perTextureRendererData, int index, int *shaderIndex, Tag *shaderTextureTag) const;


private:

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    struct TransformHeader;

    struct VectorContainer;
    struct TransformHeaderContainer;
    struct TransformDataContainer;
    struct VertexDataContainer;
    struct TriStripHeaderContainer;
    struct TriStripIndexContainer;
    struct TriListHeaderContainer;
    typedef std::vector<int> IndexedTriangleContainer;
    struct PerShaderDataContainer;

    typedef std::vector<float>                                                               FloatVector;
    typedef std::map<const CrcLowerString*, PerTextureRendererData*, LessPointerComparator>  PTRDContainer;

private:

    // global (potentially reused) data
    VectorContainer                 *m_positions;
    TransformHeaderContainer        *m_transformHeaders;
    TransformDataContainer          *m_transformData;
    VectorContainer                 *m_normals;

    // shader vertex data
    VertexDataContainer             *m_vertexData;
    FloatVector                     *m_textureCoordinates;
    TriStripHeaderContainer         *m_triStripHeaders;
    TriStripIndexContainer          *m_triStripIndices;
    TriListHeaderContainer          *m_triListHeaders;
    IndexedTriangleContainer        *m_indexedTriangles;

    int                              m_usedPerShaderDataCount;
    PerShaderDataContainer          *m_perShaderData;

    int                              m_usedPerTextureRendererDataCount;
    PTRDContainer                   *m_perTextureRendererData;


private:

    // disable these
    MeshConstructionHelper(const MeshConstructionHelper&);
    MeshConstructionHelper &operator =(const MeshConstructionHelper&);

};

// ==================================================================

inline MeshConstructionHelper::TransformData::TransformData(void)
        :
        m_transformIndex(-1),
        m_transformWeight(CONST_REAL(0))
{
}

// ------------------------------------------------------------------

inline MeshConstructionHelper::TransformData::TransformData(int transformIndex, float transformWeight)
        :
        m_transformIndex(transformIndex),
        m_transformWeight(transformWeight)
{
}

// ======================================================================

inline int MeshConstructionHelper::getShaderCount(void) const
{
    return m_usedPerShaderDataCount;
}

// ======================================================================

inline int MeshConstructionHelper::getTextureRendererCount() const
{
    return m_usedPerTextureRendererDataCount;
}

// ==================================================================

#endif
