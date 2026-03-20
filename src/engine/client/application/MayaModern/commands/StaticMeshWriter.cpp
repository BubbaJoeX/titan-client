#include "StaticMeshWriter.h"
#include "Iff.h"
#include "Tag.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Vector.h"
#include "VertexBufferFormat.h"

#include <cstring>
#include <algorithm>

namespace
{
    const Tag TAG_APPR = TAG(A,P,P,R);
    const Tag TAG_HPTS = TAG(H,P,T,S);
    const Tag TAG_HPNT = TAG(H,P,N,T);
    const Tag TAG_FLOR = TAG(F,L,O,R);
    const Tag TAG_CNT = TAG3(C,N,T);
    const Tag TAG_INDX = TAG(I,N,D,X);
    const Tag TAG_MESH = TAG(M,E,S,H);
    const Tag TAG_SPS = TAG3(S,P,S);
    const Tag TAG_VTXA = TAG(V,T,X,A);

    const uint32_t VB_FLAGS = VertexBufferFormat::F_position | VertexBufferFormat::F_normal |
        VertexBufferFormat::F_textureCoordinateCount1 | VertexBufferFormat::F_textureCoordinateSet0_2d;

    const int32_t PRIMITIVE_TYPE_INDEXED_TRIANGLE_LIST = 0;
}

void StaticMeshWriter::setFloorReference(const std::string& path)
{
    m_floorReference = path;
}

void StaticMeshWriter::addHardpoint(const StaticMeshWriterHardpoint& hp)
{
    m_hardpoints.push_back(hp);
}

void StaticMeshWriter::addShaderGroup(const StaticMeshWriterShaderGroup& sg)
{
    m_shaderGroups.push_back(sg);
}

bool StaticMeshWriter::write(Iff& iff) const
{
    iff.insertForm(TAG_MESH);
    iff.insertForm(TAG_0005);

    if (!m_hardpoints.empty() || !m_floorReference.empty())
    {
        iff.insertForm(TAG_APPR);
        iff.insertForm(TAG_0003);

        if (!m_hardpoints.empty())
        {
            iff.insertForm(TAG_HPTS);
            for (const auto& hp : m_hardpoints)
            {
                iff.insertChunk(TAG_HPNT);
                Transform t;
                t.setPosition_p(Vector(hp.position[0], hp.position[1], hp.position[2]));
                Quaternion q(hp.rotation[3], hp.rotation[0], hp.rotation[1], hp.rotation[2]);
                q.getTransformPreserveTranslation(&t);
                iff.insertChunkFloatTransform(t);
                iff.insertChunkString(hp.name.c_str());
                iff.exitChunk(TAG_HPNT);
            }
            iff.exitForm(TAG_HPTS);
        }

        if (!m_floorReference.empty())
        {
            iff.insertForm(TAG_FLOR);
            iff.insertChunk(TAG_DATA);
            iff.insertChunkData(static_cast<int8>(1));
            iff.insertChunkString(m_floorReference.c_str());
            iff.exitChunk(TAG_DATA);
            iff.exitForm(TAG_FLOR);
        }

        iff.exitForm(TAG_0003);
        iff.exitForm(TAG_APPR);
    }

    iff.insertForm(TAG_SPS);
    iff.insertForm(TAG_0001);

    iff.insertChunk(TAG_CNT);
    iff.insertChunkData(static_cast<int32>(m_shaderGroups.size()));
    iff.exitChunk(TAG_CNT);

    for (size_t sgIdx = 0; sgIdx < m_shaderGroups.size(); ++sgIdx)
    {
        const StaticMeshWriterShaderGroup& sg = m_shaderGroups[sgIdx];
        if (sg.positions.empty() || sg.indices.empty())
            continue;

        iff.insertForm(TAG_0001);

        iff.insertChunk(TAG_NAME);
        iff.insertChunkString(sg.shaderTemplateName.c_str());
        iff.exitChunk(TAG_NAME);

        iff.insertChunk(TAG_INFO);
        iff.insertChunkData(static_cast<int32>(1));
        iff.exitChunk(TAG_INFO);

        iff.insertForm(TAG_0001);

        iff.insertChunk(TAG_INFO);
        iff.insertChunkData(static_cast<int32>(PRIMITIVE_TYPE_INDEXED_TRIANGLE_LIST));
        iff.insertChunkData(static_cast<int8>(1));
        iff.insertChunkData(static_cast<int8>(0));
        iff.exitChunk(TAG_INFO);

        iff.insertForm(TAG_VTXA);
        iff.insertForm(TAG_0003);

        iff.insertChunk(TAG_INFO);
        iff.insertChunkData(static_cast<uint32>(VB_FLAGS));
        const int32_t vertCount = static_cast<int32>(sg.positions.size() / 3);
        iff.insertChunkData(vertCount);
        iff.exitChunk(TAG_INFO);

        iff.insertChunk(TAG_DATA);
        for (int v = 0; v < vertCount; ++v)
        {
            iff.insertChunkFloatVector(Vector(sg.positions[v*3], sg.positions[v*3+1], sg.positions[v*3+2]));
            iff.insertChunkFloatVector(Vector(sg.normals[v*3], sg.normals[v*3+1], sg.normals[v*3+2]));
            if (sg.uvs.size() >= static_cast<size_t>(v * 2 + 1))
            {
                iff.insertChunkData(static_cast<float>(sg.uvs[v*2]));
                iff.insertChunkData(static_cast<float>(1.0f - sg.uvs[v*2+1]));
            }
            else
            {
                iff.insertChunkData(0.0f);
                iff.insertChunkData(0.0f);
            }
        }
        iff.exitChunk(TAG_DATA);

        iff.exitForm(TAG_0003);
        iff.exitForm(TAG_VTXA);

        iff.insertChunk(TAG_INDX);
        iff.insertChunkData(static_cast<int32>(sg.indices.size()));
        for (uint16_t idx : sg.indices)
            iff.insertChunkData(idx);
        iff.exitChunk(TAG_INDX);

        iff.exitForm(TAG_0001);
        iff.exitForm(TAG_0001);
    }

    iff.exitForm(TAG_0001);
    iff.exitForm(TAG_SPS);
    iff.exitForm(TAG_0005);
    iff.exitForm(TAG_MESH);

    return true;
}
