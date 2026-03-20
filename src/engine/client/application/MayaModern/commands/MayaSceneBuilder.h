#ifndef SWGMAYAEDITOR_MAYASCENEBUILDER_H
#define SWGMAYAEDITOR_MAYASCENEBUILDER_H

#include <maya/MObject.h>
#include <maya/MStatus.h>
#include <maya/MDagPath.h>

#include <map>
#include <string>
#include <vector>

class MayaSceneBuilder
{
public:
    struct JointData
    {
        std::string name;
        int parentIndex;
        float preRotation[4];
        float postRotation[4];
        float bindTranslation[3];
        float bindRotation[4];
        int rotationOrder;
    };

    struct TriangleData
    {
        int indices[3];
    };

    struct UVData
    {
        float u;
        float v;
    };

    struct SkinWeight
    {
        int transformIndex;
        float weight;
    };

    struct ShaderGroupData
    {
        std::string shaderTemplateName;
        std::vector<int> positionIndices;
        std::vector<int> normalIndices;
        std::vector<TriangleData> triangles;
        std::vector<UVData> uvs;
    };

    struct HardpointData
    {
        std::string name;
        std::string parentJoint;
        float rotation[4];  // engine quaternion (x,y,z,w)
        float position[3];  // engine vector (x,y,z)
    };

    struct BlendTargetData
    {
        std::string name;
        std::vector<int> positionIndices;
        std::vector<float> positionDeltas;
        std::vector<int> normalIndices;
        std::vector<float> normalDeltas;
    };

    static MStatus createSkinCluster(
        const MDagPath& meshPath,
        const std::vector<MDagPath>& jointPaths,
        const std::vector<std::string>& transformNames,
        const std::vector<int>& weightHeaders,
        const std::vector<SkinWeight>& weightData,
        int maxInfluencesPerVertex = 8);

    static MStatus createMaterial(
        const std::string& shaderName,
        const std::string& texturePath,
        MObject& outShadingGroup,
        const std::string& swgShaderPath = std::string(),
        const std::string& swgTexturePath = std::string());

    static MStatus assignMaterials(
        const MDagPath& meshPath,
        const std::vector<ShaderGroupData>& shaderGroups,
        const std::string& inputFilePath);

    /** Strip trailing "SG" from shader name - MayaExporter uses shader name only for paths */
    static std::string stripSGSuffixFromShaderName(const std::string& name);

    static MStatus createMesh(
        const std::vector<float>& positions,
        const std::vector<float>& normals,
        const std::vector<ShaderGroupData>& shaderGroups,
        const std::string& meshName,
        MDagPath& outMeshPath);

    static MStatus createJointHierarchy(
        const std::vector<JointData>& joints,
        const std::string& rootName,
        std::vector<MDagPath>& outJointPaths,
        MObject parentForRoot = MObject::kNullObj);

    static void engineVectorToMaya(const float engineVec[3], double& outX, double& outY, double& outZ);
    static void engineQuatToMayaEuler(const float eq[4], int rotationOrder, double& outRx, double& outRy, double& outRz);

    /// Create hardpoints as child transforms. parentJointToPathMap: joint name -> path (empty for static mesh).
    static MStatus createHardpoints(
        const std::vector<HardpointData>& hardpoints,
        const std::map<std::string, MDagPath>& parentJointToPathMap,
        const std::string& meshName,
        MObject defaultParentForEmpty = MObject::kNullObj);

    /// Create blend shape deformer from delta-based blend targets. Base mesh must exist.
    static MStatus createBlendShapes(
        const MDagPath& meshPath,
        const std::vector<BlendTargetData>& blendTargets);

    // Animation keyframe data (ANS stores deltas from bind pose)
    struct AnimKeyframe
    {
        float frame;
        float value;
    };
    struct QuatKeyframe
    {
        float frame;
        float rotation[4];  // x, y, z, w (engine format)
    };

    static MStatus setKeyframesFromDeltas(
        const MDagPath& jointPath,
        const std::string& attribute,
        const std::vector<AnimKeyframe>& deltaKeyframes,
        float fps,
        double bindPoseValue,
        bool negateDelta);

    static MStatus setRotationKeyframesFromDeltas(
        const MDagPath& jointPath,
        const std::vector<QuatKeyframe>& deltaKeyframes,
        float fps);

private:
    MayaSceneBuilder() = delete;
};

#endif
