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
    void addHardpoint(const StaticMeshWriterHardpoint& hp);
    void addShaderGroup(const StaticMeshWriterShaderGroup& sg);

    bool write(class Iff& iff) const;

private:
    std::string m_floorReference;
    std::vector<StaticMeshWriterHardpoint> m_hardpoints;
    std::vector<StaticMeshWriterShaderGroup> m_shaderGroups;
};

#endif
