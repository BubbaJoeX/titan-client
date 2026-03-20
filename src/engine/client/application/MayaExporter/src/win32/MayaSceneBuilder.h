// ======================================================================
//
// MayaSceneBuilder.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// Utility class for creating Maya scene elements from intermediate data.
//
// ======================================================================

#ifndef INCLUDED_MayaSceneBuilder_H
#define INCLUDED_MayaSceneBuilder_H

// ======================================================================

#include "maya/MObject.h"
#include "maya/MStatus.h"
#include "maya/MDagPath.h"

#include <map>
#include <string>
#include <vector>

// ======================================================================

class Messenger;

// ======================================================================

class MayaSceneBuilder
{
public:

	static void install(Messenger *newMessenger);
	static void remove();

	// -----------------------------------------------------------------
	// Skeleton data
	// -----------------------------------------------------------------

	struct JointData
	{
		std::string  name;
		int          parentIndex;
		float        preRotation[4];   // engine quaternion (x,y,z,w) from RPRE
		float        postRotation[4];  // engine quaternion (x,y,z,w) from RPST
		float        bindTranslation[3]; // engine vector (x,y,z) from BPTR
		float        bindRotation[4];  // engine quaternion (x,y,z,w) from BPRO
		int          rotationOrder;    // JointRotationOrder enum from JROR
	};

	// -----------------------------------------------------------------
	// Skinning data
	// -----------------------------------------------------------------

	struct SkinWeight
	{
		int    transformIndex;
		float  weight;
	};

	// -----------------------------------------------------------------
	// Mesh data
	// -----------------------------------------------------------------

	struct VertexData
	{
		float  position[3];
		float  normal[3];
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

	struct ShaderGroupData
	{
		std::string                shaderTemplateName;
		std::vector<int>           positionIndices;
		std::vector<int>           normalIndices;
		std::vector<TriangleData>  triangles;
		std::vector<UVData>        uvs;
		std::vector<uint32>        vertexColors;
		std::vector< std::vector<float> > extraUVSets;
	};

	// -----------------------------------------------------------------
	// Blend target data
	// -----------------------------------------------------------------

	struct BlendTargetData
	{
		std::string            name;
		std::vector<int>       positionIndices;
		std::vector<float>     positionDeltas;
		std::vector<int>       normalIndices;
		std::vector<float>     normalDeltas;
	};

	// -----------------------------------------------------------------
	// Hardpoint data
	// -----------------------------------------------------------------

	struct HardpointData
	{
		std::string  name;
		std::string  parentJoint;
		float        rotation[4]; // engine quaternion (x,y,z,w)
		float        position[3]; // engine vector (x,y,z)
	};

	// -----------------------------------------------------------------
	// Animation data
	// -----------------------------------------------------------------

	struct AnimKeyframe
	{
		int    frame;
		float  value;
	};

	struct QuatKeyframe
	{
		int    frame;
		float  rotation[4]; // engine quaternion (x,y,z,w)
	};

	// -----------------------------------------------------------------
	// Scene creation methods
	// -----------------------------------------------------------------

	/// Create joint hierarchy. If parentForRoot is valid, roots under it (for SLOD l0/l1); otherwise creates "master" group.
	static MStatus createJointHierarchy(
		const std::vector<JointData> & joints,
		const std::string & rootName,
		std::vector<MDagPath> & outJointPaths,
		MObject parentForRoot = MObject::kNullObj);

	static MStatus createMesh(
		const std::vector<float> & positions,
		const std::vector<float> & normals,
		const std::vector<ShaderGroupData> & shaderGroups,
		const std::string & meshName,
		MDagPath & outMeshPath);

	static MStatus createSkinCluster(
		const MDagPath & meshPath,
		const std::vector<MDagPath> & jointPaths,
		const std::vector<std::string> & transformNames,
		const std::vector<int> & weightHeaders,
		const std::vector<SkinWeight> & weightData);

	static MStatus createBlendShapes(
		const MDagPath & meshPath,
		const std::vector<BlendTargetData> & blendTargets);

	/// Create hardpoints as child transforms under skeleton joints (or defaultParent when parentJoint empty).
	/// parentJointToPathMap: joint short name -> DAG path (use jointMap from ImportSkeletalMesh).
	/// meshName: used for exporter-compatible naming "hp_meshname_hardpointname" for round-trip.
	/// defaultParentForEmpty: when parentJoint is empty (e.g. static mesh), parent to this; otherwise world.
	static MStatus createHardpoints(
		const std::vector<HardpointData> & hardpoints,
		const std::map<std::string, MDagPath> & parentJointToPathMap,
		const std::string & meshName,
		MObject defaultParentForEmpty = MObject::kNullObj);

	static MStatus createMaterial(
		const std::string & shaderName,
		const std::string & texturePath,
		MObject & outShadingGroup);

	static MStatus assignMaterials(
		const MDagPath & meshPath,
		const std::vector<ShaderGroupData> & shaderGroups,
		const std::string & inputFilePath);

	static MStatus setKeyframes(
		const MDagPath & jointPath,
		const std::string & attribute,
		const std::vector<AnimKeyframe> & keyframes,
		float fps);

	static MStatus setRotationKeyframes(
		const MDagPath & jointPath,
		const std::vector<QuatKeyframe> & keyframes,
		float fps);

	/// Apply animation deltas on top of bind pose (ANS stores deltas from bind pose).
	/// Rotation: final = delta * bindPose. Translation: final = bindPose + delta.
	static MStatus setRotationKeyframesFromDeltas(
		const MDagPath & jointPath,
		const std::vector<QuatKeyframe> & deltaKeyframes,
		float fps);

	static MStatus setKeyframesFromDeltas(
		const MDagPath & jointPath,
		const std::string & attribute,
		const std::vector<AnimKeyframe> & deltaKeyframes,
		float fps,
		double bindPoseValue,
		bool negateDelta = false);

	// -----------------------------------------------------------------
	// Coordinate conversion helpers (engine <-> Maya)
	//
	// The engine uses a left-handed coordinate system where:
	//   engineVector = (-mayaX, mayaY, mayaZ)
	//   engineQuat   = convertRotation(mayaEuler) with Y,Z negated
	//
	// These helpers perform the inverse conversion for import.
	// -----------------------------------------------------------------

	static void engineVectorToMaya(const float engineVec[3], double &outX, double &outY, double &outZ);
	static void engineQuatToMayaEuler(const float engineQuat[4], int rotationOrder, double &outRx, double &outRy, double &outRz);

private:

	MayaSceneBuilder();
	MayaSceneBuilder(const MayaSceneBuilder &);
	MayaSceneBuilder & operator=(const MayaSceneBuilder &);
};

// ======================================================================

#endif
