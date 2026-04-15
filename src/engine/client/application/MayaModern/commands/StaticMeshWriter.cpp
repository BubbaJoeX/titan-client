#include "StaticMeshWriter.h"
#include "Iff.h"
#include "Tag.h"
#include "Quaternion.h"
#include "Transform.h"
#include "Vector.h"
#include "VertexBufferFormat.h"

#include <cctype>
#include <cstring>
#include <algorithm>

namespace
{
    /// Client: ShaderTemplateList::fetch(Iff) reads SPS NAME and calls Iff::open(filename) (ShaderTemplateList.cpp).
    /// TreeFile opens that path as-is; engine preload uses "shader/foo.sht", not "shader/foo".
    static std::string ensureShaderTemplatePathForClientFetch(std::string s)
    {
        if (s.empty()) return s;
        for (char& c : s)
            if (c == '\\') c = '/';
        if (s.size() >= 4)
        {
            std::string tail = s.substr(s.size() - 4);
            for (char& c : tail)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (tail == ".sht") return s;
        }
        return s + ".sht";
    }

    const Tag TAG_APPR = TAG(A,P,P,R);
    const Tag TAG_BOX = TAG3(B,O,X);
    const Tag TAG_EXBX = TAG(E,X,B,X);
    const Tag TAG_EXSP = TAG(E,X,S,P);
    const Tag TAG_HPTS = TAG(H,P,T,S);
    const Tag TAG_HPNT = TAG(H,P,N,T);
    const Tag TAG_FLOR = TAG(F,L,O,R);
    const Tag TAG_CNT = TAG3(C,N,T);
    const Tag TAG_INDX = TAG(I,N,D,X);
    const Tag TAG_MESH = TAG(M,E,S,H);
    const Tag TAG_NULL = TAG(N,U,L,L);
    const Tag TAG_SPHR = TAG(S,P,H,R);
    const Tag TAG_SPS = TAG3(S,P,S);
    const Tag TAG_VTXA = TAG(V,T,X,A);

    /// BoxExtent::write-compatible block (EXBX/0001: nested EXSP sphere + BOX max/min).
    /// Required so MeshAppearanceTemplate gets a real extent/sphere; NULL extents leave radius 0 and break culling/view.
    static void writeBoxExtentChunk(Iff& iff, const Vector& boxMin, const Vector& boxMax)
    {
        const Vector center = (boxMin + boxMax) * 0.5f;
        const Vector diag = boxMax - boxMin;
        float radius = diag.magnitude() * 0.5f;
        if (radius < 1e-3f)
            radius = 0.1f;

        iff.insertForm(TAG_EXBX);
        iff.insertForm(TAG_0001);

        iff.insertForm(TAG_EXSP);
        iff.insertForm(TAG_0001);
        iff.insertChunk(TAG_SPHR);
        iff.insertChunkFloatVector(center);
        iff.insertChunkData(radius);
        iff.exitChunk(TAG_SPHR);
        iff.exitForm(TAG_0001);
        iff.exitForm(TAG_EXSP);

        iff.insertChunk(TAG_BOX);
        iff.insertChunkFloatVector(boxMax);
        iff.insertChunkFloatVector(boxMin);
        iff.exitChunk(TAG_BOX);

        iff.exitForm(TAG_0001);
        iff.exitForm(TAG_EXBX);
    }

    static bool tryComputeVertexBounds(const std::vector<StaticMeshWriterShaderGroup>& groups, Vector& outMin, Vector& outMax)
    {
        bool any = false;
        float minX = 0.f, minY = 0.f, minZ = 0.f, maxX = 0.f, maxY = 0.f, maxZ = 0.f;
        for (const auto& sg : groups)
        {
            const int n = static_cast<int>(sg.positions.size() / 3);
            for (int v = 0; v < n; ++v)
            {
                const float x = sg.positions[static_cast<size_t>(v) * 3];
                const float y = sg.positions[static_cast<size_t>(v) * 3 + 1];
                const float z = sg.positions[static_cast<size_t>(v) * 3 + 2];
                if (!any)
                {
                    minX = maxX = x;
                    minY = maxY = y;
                    minZ = maxZ = z;
                    any = true;
                }
                else
                {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                    if (z < minZ) minZ = z;
                    if (z > maxZ) maxZ = z;
                }
            }
        }
        if (!any)
            return false;
        constexpr float pad = 0.02f;
        outMin = Vector(minX - pad, minY - pad, minZ - pad);
        outMax = Vector(maxX + pad, maxY + pad, maxZ + pad);
        return true;
    }

    const uint32_t VB_FLAGS = VertexBufferFormat::F_position | VertexBufferFormat::F_normal |
        VertexBufferFormat::F_textureCoordinateCount1 | VertexBufferFormat::F_textureCoordinateSet0_2d;

    // Must match ShaderPrimitiveSetPrimitiveType / s_drawFunctionLookupTable in ShaderPrimitiveSetTemplate.cpp
    // (0 = SPSPT_pointList — wrong; indexed tris are SPSPT_indexedTriangleList = 9.)
    const int32_t PRIMITIVE_TYPE_INDEXED_TRIANGLE_LIST = 9;
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

    // MeshAppearanceTemplate::load_0005 always calls AppearanceTemplate::load first, which
    // requires FORM APPR. APPR/0003 order matches AppearanceTemplate::load_0003: extents,
    // collision extents, hardpoints (optional read), floors (FLOR required — bool + optional string).
    iff.insertForm(TAG_APPR);
    iff.insertForm(TAG_0003);

    Vector boxMin, boxMax;
    if (tryComputeVertexBounds(m_shaderGroups, boxMin, boxMax))
    {
        writeBoxExtentChunk(iff, boxMin, boxMax);
        writeBoxExtentChunk(iff, boxMin, boxMax);
    }
    else
    {
        iff.insertForm(TAG_NULL);
        iff.exitForm(TAG_NULL);
        iff.insertForm(TAG_NULL);
        iff.exitForm(TAG_NULL);
    }

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

    iff.insertForm(TAG_FLOR);
    iff.insertChunk(TAG_DATA);
    const bool hasFloor = !m_floorReference.empty();
    iff.insertChunkData(static_cast<int8>(hasFloor ? 1 : 0));
    if (hasFloor)
        iff.insertChunkString(m_floorReference.c_str());
    iff.exitChunk(TAG_DATA);
    iff.exitForm(TAG_FLOR);

    iff.exitForm(TAG_0003);
    iff.exitForm(TAG_APPR);

    iff.insertForm(TAG_SPS);
    iff.insertForm(TAG_0001);

    // Must match ShaderPrimitiveSetTemplate::load_sps_0001: CNT = number of following per-shader
    // FORM blocks. Skipping a group without writing it while CNT is too high desyncs the IFF reader
    // (symptom: huge vertex count, ~2 triangles in viewer).
    int32_t shaderSlotCount = 0;
    for (const auto& sg : m_shaderGroups)
    {
        if (!sg.positions.empty() && !sg.indices.empty())
            ++shaderSlotCount;
    }

    iff.insertChunk(TAG_CNT);
    iff.insertChunkData(shaderSlotCount);
    iff.exitChunk(TAG_CNT);

    for (size_t sgIdx = 0; sgIdx < m_shaderGroups.size(); ++sgIdx)
    {
        const StaticMeshWriterShaderGroup& sg = m_shaderGroups[sgIdx];
        if (sg.positions.empty() || sg.indices.empty())
            continue;

        iff.insertForm(TAG_0001);

        iff.insertChunk(TAG_NAME);
        {
            const std::string shaderPath = ensureShaderTemplatePathForClientFetch(sg.shaderTemplateName);
            iff.insertChunkString(shaderPath.c_str());
        }
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
            // V must match client / msh import: translators/msh.cpp applies Maya V = 1 - fileV, so file V = 1 - Maya V.
            // ExportStaticMesh already stores (u, 1-v_maya) in sg.uvs; do not flip again here.
            if (sg.uvs.size() >= static_cast<size_t>(v * 2 + 1))
            {
                iff.insertChunkData(static_cast<float>(sg.uvs[v*2]));
                iff.insertChunkData(static_cast<float>(sg.uvs[v*2+1]));
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
