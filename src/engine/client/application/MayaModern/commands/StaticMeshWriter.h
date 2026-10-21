#ifndef SWGMAYAEDITOR_STATICMESHWRITER_H
#define SWGMAYAEDITOR_STATICMESHWRITER_H

#include <string>
#include <vector>
#include <cstdint>

struct StaticMeshWriterHardpoint
{
    std::string name;
    float position[3];
    float rotation[4];  // quaternion x,y,z,w
};

struct StaticMeshWriterShaderGroup
{
    std::string shaderTemplateName;
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<uint16_t> indices;
};

class StaticMeshWriter
{
public:
    void setFloorReference(const std::string& path);
    /// APPR collision extent: the bounding box (props block like a crate) or NULL. Buildings need NULL: the
    /// engine takes an appearance's collision extent before the POB cell's CMSH, so a box there makes the shell
    /// (or a room) one solid block.
    void setCollisionBox(bool box) { m_collisionBox = box; }
    void addHardpoint(const StaticMeshWriterHardpoint& hp);
    void addShaderGroup(const StaticMeshWriterShaderGroup& sg);

    bool write(class Iff& iff) const;

private:
    std::string m_floorReference;
    bool m_collisionBox = true;
    std::vector<StaticMeshWriterHardpoint> m_hardpoints;
    std::vector<StaticMeshWriterShaderGroup> m_shaderGroups;
};

#endif
