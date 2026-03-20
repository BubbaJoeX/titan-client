// ======================================================================
//
// MayaSceneBuilder.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "MayaSceneBuilder.h"
#include "DdsToTgaConverter.h"

#include "Messenger.h"

#include "maya/MFnIkJoint.h"
#include "maya/MFnMesh.h"
#include "maya/MFnSkinCluster.h"
#include "maya/MFnBlendShapeDeformer.h"
#include "maya/MFnLambertShader.h"
#include "maya/MFnSet.h"
#include "maya/MFnDagNode.h"
#include "maya/MFnTransform.h"
#include "maya/MFnAnimCurve.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MDGModifier.h"
#include "maya/MSelectionList.h"
#include "maya/MGlobal.h"
#include "maya/MQuaternion.h"
#include "maya/MEulerRotation.h"
#include "maya/MFloatPointArray.h"
#include "maya/MFloatArray.h"
#include "maya/MIntArray.h"
#include "maya/MDoubleArray.h"
#include "maya/MPointArray.h"
#include "maya/MFnSingleIndexedComponent.h"
#include "maya/MItDependencyGraph.h"
#include "maya/MDagPathArray.h"
#include "maya/MPlug.h"
#include "maya/MPlugArray.h"
#include "maya/MTime.h"
#include "maya/MVector.h"

#include <map>
#include <cmath>
#include <cstdlib>
#include <cstdio>

// ======================================================================

namespace
{
	Messenger * messenger;

	// The exporter converts Maya euler rotations to engine quaternions via:
	//   qx = Quaternion(euler.x, unitX)
	//   qy = Quaternion(-euler.y, unitY)
	//   qz = Quaternion(-euler.z, unitZ)
	//   result = compose in reversed order based on euler.order
	//
	// The engine Quaternion(angle, axis) constructor produces:
	//   w = cos(angle/2), xyz = axis * sin(angle/2)
	//
	// The IFF stores quaternions as (w, x, y, z).
	// Our JointData stores them as (x, y, z, w) after reading from IFF.
	//
	// To invert the conversion, we decompose the engine quaternion back to
	// a Maya euler rotation. The simplest correct approach is to use Maya's
	// own MQuaternion -> MEulerRotation conversion, but we must first undo
	// the coordinate system transform (negate Y and Z of the euler angles,
	// which is equivalent to conjugating the Y and Z components of the quat).
	//
	// Engine coordinate system: X-negated left-handed
	//   enginePos = (-mayaX, mayaY, mayaZ)
	//   For rotations: the Y and Z euler angles are negated during export.
	//   This is equivalent to: engineQuat = conjugateYZ(mayaQuat) in certain
	//   formulations, but the exact inverse depends on the euler order.
	//
	// The safest approach for round-trip fidelity: use the same conversion
	// functions the exporter uses, but in reverse. Since MayaConversions is
	// available at link time, we can use it directly for the forward path
	// and invert numerically.

	MEulerRotation::RotationOrder jroToMayaOrder(int jro)
	{
		switch (jro)
		{
			case 0: return MEulerRotation::kXYZ;
			case 1: return MEulerRotation::kXZY;
			case 2: return MEulerRotation::kYXZ;
			case 3: return MEulerRotation::kYZX;
			case 4: return MEulerRotation::kZXY;
			case 5: return MEulerRotation::kZYX;
			default: return MEulerRotation::kXYZ;
		}
	}

	MTransformationMatrix::RotationOrder jroToTransformOrder(int jro)
	{
		switch (jro)
		{
			case 0: return MTransformationMatrix::kXYZ;
			case 1: return MTransformationMatrix::kXZY;
			case 2: return MTransformationMatrix::kYXZ;
			case 3: return MTransformationMatrix::kYZX;
			case 4: return MTransformationMatrix::kZXY;
			case 5: return MTransformationMatrix::kZYX;
			default: return MTransformationMatrix::kXYZ;
		}
	}
}

// ======================================================================

void MayaSceneBuilder::install(Messenger * newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void MayaSceneBuilder::remove()
{
	messenger = 0;
}

// ======================================================================
// Coordinate conversion: engine vector -> Maya vector
// Engine stores (-mayaX, mayaY, mayaZ), so we negate X back.
// ======================================================================

void MayaSceneBuilder::engineVectorToMaya(const float engineVec[3], double &outX, double &outY, double &outZ)
{
	outX = static_cast<double>(-engineVec[0]);
	outY = static_cast<double>(engineVec[1]);
	outZ = static_cast<double>(engineVec[2]);
}

// ======================================================================
// Coordinate conversion: engine quaternion -> Maya euler angles
//
// The exporter does:
//   qx = Quaternion(euler.x, unitX)
//   qy = Quaternion(-euler.y, unitY)
//   qz = Quaternion(-euler.z, unitZ)
//   engineQuat = compose(qx, qy, qz) in order based on euler.order
//
// To invert: we convert the engine quaternion to a Maya MQuaternion,
// then to MEulerRotation with the correct order. However, because the
// exporter negates Y and Z angles, we need to account for that.
//
// The engine quaternion in IFF is stored as (w,x,y,z).
// In our JointData it's stored as (x,y,z,w) after swizzle during read.
// ======================================================================

void MayaSceneBuilder::engineQuatToMayaEuler(const float eq[4], int rotationOrder, double &outRx, double &outRy, double &outRz)
{
	// eq layout: [x, y, z, w]
	// MQuaternion constructor: MQuaternion(x, y, z, w)
	MQuaternion mq(eq[0], eq[1], eq[2], eq[3]);
	MEulerRotation euler = mq.asEulerRotation();
	euler.reorderIt(jroToMayaOrder(rotationOrder));

	// The exporter negated Y and Z angles, so we negate them back
	outRx = euler.x;
	outRy = -euler.y;
	outRz = -euler.z;
}

// ======================================================================

MStatus MayaSceneBuilder::createJointHierarchy(
	const std::vector<JointData> & joints,
	const std::string & rootName,
	std::vector<MDagPath> & outJointPaths,
	MObject parentForRoot)
{
	MStatus status;
	outJointPaths.resize(joints.size());

	MObject rootParentObj = parentForRoot;
	if (rootParentObj.isNull())
	{
		// Create "master" group for re-export compatibility (exportKeyframeSkeletalAnimation expects it)
		MFnTransform masterFn;
		rootParentObj = masterFn.create(MObject::kNullObj, &status);
		if (!status)
		{
			MESSENGER_LOG_ERROR(("failed to create master group\n"));
			return status;
		}
		masterFn.setName("master", &status);
	}

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const JointData & jd = joints[i];

		MFnIkJoint jointFn;
		MObject parentObj = rootParentObj;
		if (jd.parentIndex >= 0 && jd.parentIndex < static_cast<int>(i))
		{
			parentObj = outJointPaths[static_cast<size_t>(jd.parentIndex)].node();
		}

		MObject jointObj = jointFn.create(parentObj, &status);
		if (!status)
		{
			MESSENGER_LOG_ERROR(("failed to create joint %s\n", jd.name.c_str()));
			return status;
		}

		// Root joint (parentIndex -1) must use "nodename__filename" format for exportSkeleton
		std::string jointName = jd.name;
		if (jd.parentIndex < 0 && !rootName.empty())
		{
			jointName = jd.name;
			jointName += "__";
			jointName += rootName;
		}
		jointFn.setName(MString(jointName.c_str()), &status);

		// Set rotation order FIRST so euler decomposition uses the correct order
		jointFn.setRotationOrder(jroToTransformOrder(jd.rotationOrder), false);

		// BPTR -> translation (undo engine X-negation)
		double tx, ty, tz;
		engineVectorToMaya(jd.bindTranslation, tx, ty, tz);
		jointFn.setTranslation(MVector(tx, ty, tz), MSpace::kTransform);

		// BPRO -> rotation (main rotate X/Y/Z)
		// The exporter reads MFnTransform::getRotation() and converts via
		// MayaConversions::convertRotation which negates Y,Z euler angles.
		// We invert that here.
		double rx, ry, rz;
		engineQuatToMayaEuler(jd.bindRotation, jd.rotationOrder, rx, ry, rz);
		MEulerRotation bindEuler(rx, ry, rz, jroToMayaOrder(jd.rotationOrder));
		jointFn.setRotation(bindEuler);

		// RPRE -> rotateOrientation (rotate axis)
		// The exporter reads MFnTransform::rotateOrientation() for this field.
		double preRx, preRy, preRz;
		engineQuatToMayaEuler(jd.preRotation, 0, preRx, preRy, preRz);
		MEulerRotation preEuler(preRx, preRy, preRz);
		jointFn.setRotateOrientation(preEuler.asQuaternion(), MSpace::kTransform, false);

		// RPST -> joint orient
		// The exporter reads MFnIkJoint::getOrientation() for this field.
		double postRx, postRy, postRz;
		engineQuatToMayaEuler(jd.postRotation, 0, postRx, postRy, postRz);
		MEulerRotation postEuler(postRx, postRy, postRz);
		jointFn.setOrientation(postEuler);

		MDagPath dagPath;
		status = jointFn.getPath(dagPath);
		if (!status)
		{
			MESSENGER_LOG_ERROR(("failed to get dag path for joint %s\n", jd.name.c_str()));
			return status;
		}

		outJointPaths[i] = dagPath;
	}

	return MS::kSuccess;
}

// ======================================================================

MStatus MayaSceneBuilder::createMesh(
	const std::vector<float> & positions,
	const std::vector<float> & normals,
	const std::vector<ShaderGroupData> & shaderGroups,
	const std::string & meshName,
	MDagPath & outMeshPath)
{
	UNREF(normals);
	MStatus status;

	const int vertexCount = static_cast<int>(positions.size()) / 3;

	MFloatPointArray vertexArray(vertexCount);
	for (int i = 0; i < vertexCount; ++i)
	{
		const size_t base = static_cast<size_t>(i) * 3;
		// Undo engine X-negation for positions
		vertexArray.set(i,
			-positions[base + 0],
			positions[base + 1],
			positions[base + 2]);
	}

	int totalTriangles = 0;
	for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
		totalTriangles += static_cast<int>(shaderGroups[sg].triangles.size());

	MIntArray polygonCounts(totalTriangles, 3);
	MIntArray polygonConnects;
	polygonConnects.setLength(static_cast<unsigned>(totalTriangles * 3));

	int triIdx = 0;
	for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
	{
		const ShaderGroupData & group = shaderGroups[sg];
		for (size_t t = 0; t < group.triangles.size(); ++t)
		{
			int pi0 = group.triangles[t].indices[0];
			int pi1 = group.triangles[t].indices[1];
			int pi2 = group.triangles[t].indices[2];

			if (!group.positionIndices.empty())
			{
				if (pi0 >= 0 && pi0 < static_cast<int>(group.positionIndices.size()))
					pi0 = group.positionIndices[static_cast<size_t>(pi0)];
				if (pi1 >= 0 && pi1 < static_cast<int>(group.positionIndices.size()))
					pi1 = group.positionIndices[static_cast<size_t>(pi1)];
				if (pi2 >= 0 && pi2 < static_cast<int>(group.positionIndices.size()))
					pi2 = group.positionIndices[static_cast<size_t>(pi2)];
			}

			polygonConnects[static_cast<unsigned>(triIdx * 3 + 0)] = pi0;
			polygonConnects[static_cast<unsigned>(triIdx * 3 + 1)] = pi1;
			polygonConnects[static_cast<unsigned>(triIdx * 3 + 2)] = pi2;
			++triIdx;
		}
	}

	MFnMesh meshFn;
	MObject meshObj = meshFn.create(
		vertexCount,
		totalTriangles,
		vertexArray,
		polygonCounts,
		polygonConnects,
		MObject::kNullObj,
		&status);

	UNREF(meshObj);

	if (!status)
	{
		MESSENGER_LOG_ERROR(("failed to create mesh %s\n", meshName.c_str()));
		return status;
	}

	meshFn.setName(MString(meshName.c_str()), &status);

	// Build combined UV array from all shader groups (setUVs overwrites, so we must combine first)
	MFloatArray uArray, vArray;
	int totalUVCount = 0;
	for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
	{
		const ShaderGroupData & group = shaderGroups[sg];
		totalUVCount += static_cast<int>(group.uvs.size());
	}
	if (totalUVCount > 0)
	{
		uArray.setLength(static_cast<unsigned>(totalUVCount));
		vArray.setLength(static_cast<unsigned>(totalUVCount));
		unsigned uvOffset = 0;
		for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
		{
			const ShaderGroupData & group = shaderGroups[sg];
			for (size_t u = 0; u < group.uvs.size(); ++u)
			{
				uArray[uvOffset + static_cast<unsigned>(u)] = group.uvs[u].u;
				// Undo the exporter's UV V-flip: exporter does v = 1 - mayaV
				vArray[uvOffset + static_cast<unsigned>(u)] = 1.0f - group.uvs[u].v;
			}
			uvOffset += static_cast<unsigned>(group.uvs.size());
		}
		meshFn.setUVs(uArray, vArray);
	}

	int faceOffset = 0;
	unsigned uvBaseOffset = 0;
	for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
	{
		const ShaderGroupData & group = shaderGroups[sg];
		if (!group.uvs.empty())
		{
			for (size_t t = 0; t < group.triangles.size(); ++t)
			{
				int faceId = faceOffset + static_cast<int>(t);
				for (int v = 0; v < 3; ++v)
				{
					int uvIdx = group.triangles[t].indices[v];
					if (uvIdx >= 0 && uvIdx < static_cast<int>(group.uvs.size()))
						meshFn.assignUV(faceId, v, static_cast<int>(uvBaseOffset) + uvIdx);
				}
			}
		}
		faceOffset += static_cast<int>(group.triangles.size());
		uvBaseOffset += static_cast<unsigned>(group.uvs.size());
	}

	status = meshFn.getPath(outMeshPath);
	return status;
}

// ======================================================================

MStatus MayaSceneBuilder::createSkinCluster(
	const MDagPath & meshPath,
	const std::vector<MDagPath> & jointPaths,
	const std::vector<std::string> & transformNames,
	const std::vector<int> & weightHeaders,
	const std::vector<SkinWeight> & weightData)
{
	MStatus status;

	// Mesh transformNames use raw skeleton names (e.g. "root"); Maya root joints use "nodename__filename"
	std::map<std::string, size_t> jointNameToIndex;
	for (size_t j = 0; j < jointPaths.size(); ++j)
	{
		MFnDagNode dagFn(jointPaths[j]);
		const std::string name(dagFn.name().asChar());
		jointNameToIndex[name] = j;
		// Alias: "root__kaadu" -> also index as "root" so mesh transform names match
		std::string::size_type dbl = name.find("__");
		if (dbl != std::string::npos && dbl > 0)
		{
			const std::string baseName = name.substr(0, dbl);
			if (jointNameToIndex.find(baseName) == jointNameToIndex.end())
				jointNameToIndex[baseName] = j;
		}
	}

	std::map<int, size_t> transformToJoint;
	for (size_t t = 0; t < transformNames.size(); ++t)
	{
		std::map<std::string, size_t>::const_iterator it = jointNameToIndex.find(transformNames[t]);
		if (it != jointNameToIndex.end())
			transformToJoint[static_cast<int>(t)] = it->second;
	}

	MString melCmd = "skinCluster -tsb ";
	for (size_t j = 0; j < jointPaths.size(); ++j)
	{
		MFnDagNode dagFn(jointPaths[j]);
		melCmd += dagFn.fullPathName();
		melCmd += " ";
	}
	{
		MFnDagNode meshDagFn(meshPath);
		melCmd += meshDagFn.fullPathName();
	}

	MStringArray melResult;
	status = MGlobal::executeCommand(melCmd, melResult);
	if (!status)
	{
		MESSENGER_LOG_ERROR(("failed to create skin cluster via MEL\n"));
		return status;
	}

	if (melResult.length() == 0)
		return MS::kFailure;

	MSelectionList selList;
	selList.add(melResult[0]);
	MObject skinClusterObj;
	selList.getDependNode(0, skinClusterObj);

	MFnSkinCluster skinFn(skinClusterObj, &status);
	if (!status)
		return status;

	const int vertexCount = static_cast<int>(weightHeaders.size());
	int weightOffset = 0;

	MDagPath meshShapePath = meshPath;
	meshShapePath.extendToShape();

	MDagPathArray influencePaths;
	unsigned int influenceCount = skinFn.influenceObjects(influencePaths, &status);
	UNREF(influenceCount);

	for (int v = 0; v < vertexCount; ++v)
	{
		const int numWeights = weightHeaders[static_cast<size_t>(v)];
		MIntArray influenceIndices;
		MDoubleArray weights;
		double weightSum = 0.0;

		for (int w = 0; w < numWeights; ++w)
		{
			const SkinWeight & sw = weightData[static_cast<size_t>(weightOffset + w)];
			std::map<int, size_t>::const_iterator it = transformToJoint.find(sw.transformIndex);
			if (it != transformToJoint.end())
			{
				influenceIndices.append(static_cast<int>(it->second));
				const double wval = static_cast<double>(sw.weight);
				weights.append(wval);
				weightSum += wval;
			}
		}

		if (influenceIndices.length() > 0)
		{
			// Normalize weights when some were dropped (missing joints) so they sum to 1
			if (weightSum > 0.0 && std::fabs(weightSum - 1.0) > 1e-6)
			{
				for (unsigned int i = 0; i < weights.length(); ++i)
					weights[i] /= weightSum;
			}

			MFnSingleIndexedComponent singleVertComp;
			MObject singleVert = singleVertComp.create(MFn::kMeshVertComponent);
			singleVertComp.addElement(v);
			skinFn.setWeights(meshShapePath, singleVert, influenceIndices, weights, true);
		}

		weightOffset += numWeights;
	}

	return MS::kSuccess;
}

// ======================================================================

MStatus MayaSceneBuilder::createBlendShapes(
	const MDagPath & meshPath,
	const std::vector<BlendTargetData> & blendTargets)
{
	if (blendTargets.empty())
		return MS::kSuccess;

	MStatus status;
	UNREF(meshPath);

	for (size_t bt = 0; bt < blendTargets.size(); ++bt)
	{
		MESSENGER_LOG(("  blend target: %s (%d position deltas)\n",
			blendTargets[bt].name.c_str(),
			static_cast<int>(blendTargets[bt].positionIndices.size())));
	}

	return MS::kSuccess;
}

// ======================================================================

MStatus MayaSceneBuilder::createHardpoints(
	const std::vector<HardpointData> & hardpoints,
	const std::map<std::string, MDagPath> & parentJointToPathMap,
	const std::string & meshName,
	MObject defaultParentForEmpty)
{
	MStatus status;

	for (size_t hp = 0; hp < hardpoints.size(); ++hp)
	{
		const HardpointData & hd = hardpoints[hp];

		MObject parentObj = defaultParentForEmpty;
		if (!hd.parentJoint.empty())
		{
			std::map<std::string, MDagPath>::const_iterator it = parentJointToPathMap.find(hd.parentJoint);
			if (it == parentJointToPathMap.end())
			{
				MESSENGER_LOG_WARNING(("  Hardpoint [%s]: parent joint [%s] not found in skeleton, skipping\n", hd.name.c_str(), hd.parentJoint.c_str()));
				continue;
			}
			parentObj = it->second.node();
		}

		MFnTransform transformFn;
		MObject hpObj = transformFn.create(parentObj, &status);
		UNREF(hpObj);
		if (!status)
			continue;

		// Use exporter-compatible naming "hp_meshname_hardpointname" for round-trip
		std::string hpNodeName = "hp_";
		hpNodeName += meshName;
		hpNodeName += "_";
		hpNodeName += hd.name;
		transformFn.setName(MString(hpNodeName.c_str()));

		// Undo engine coordinate transform for hardpoint position
		double hx, hy, hz;
		engineVectorToMaya(hd.position, hx, hy, hz);
		transformFn.setTranslation(MVector(hx, hy, hz), MSpace::kTransform);

		// Undo engine coordinate transform for hardpoint rotation
		double hrx, hry, hrz;
		engineQuatToMayaEuler(hd.rotation, 0, hrx, hry, hrz);
		MEulerRotation hpEuler(hrx, hry, hrz);
		transformFn.setRotation(hpEuler.asQuaternion(), MSpace::kTransform);

		// Create a small cube as visual indicator (hpViz_ prefix so exporter ignores it)
		{
			std::string vizName = "hpViz_";
			vizName += meshName;
			vizName += "_";
			vizName += hd.name;
			const double cubeSize = 0.05;
			MString melCmd = "polyCube -w ";
			melCmd += cubeSize;
			melCmd += " -h ";
			melCmd += cubeSize;
			melCmd += " -d ";
			melCmd += cubeSize;
			melCmd += " -n \"";
			melCmd += vizName.c_str();
			melCmd += "\"";
			if (MGlobal::executeCommand(melCmd))
			{
				MSelectionList sel;
				MGlobal::getActiveSelectionList(sel);
				if (sel.length() > 0)
				{
					MDagPath cubePath;
					sel.getDagPath(0, cubePath);
					if (!cubePath.hasFn(MFn::kTransform))
						cubePath.pop();
					MFnDagNode hpDagFn(transformFn.object());
					MString parentCmd = "parent \"";
					parentCmd += cubePath.fullPathName();
					parentCmd += "\" \"";
					parentCmd += hpDagFn.fullPathName();
					parentCmd += "\"";
					IGNORE_RETURN(MGlobal::executeCommand(parentCmd));
				}
			}
		}
	}

	return MS::kSuccess;
}

// ======================================================================

MStatus MayaSceneBuilder::createMaterial(
	const std::string & shaderName,
	const std::string & texturePath,
	MObject & outShadingGroup)
{
	MStatus status;

	MString melCmd = "shadingNode -asShader lambert -name \"";
	melCmd += shaderName.c_str();
	melCmd += "\"";

	MStringArray result;
	status = MGlobal::executeCommand(melCmd, result);
	if (!status || result.length() == 0)
		return MS::kFailure;

	MString shaderNodeName = result[0];

	melCmd = "sets -renderable true -noSurfaceShader true -empty -name \"";
	melCmd += shaderNodeName;
	melCmd += "SG\"";
	status = MGlobal::executeCommand(melCmd, result);
	if (!status || result.length() == 0)
		return MS::kFailure;

	MString sgName = result[0];

	melCmd = "connectAttr -f ";
	melCmd += shaderNodeName;
	melCmd += ".outColor ";
	melCmd += sgName;
	melCmd += ".surfaceShader";
	IGNORE_RETURN(MGlobal::executeCommand(melCmd));

	if (!texturePath.empty())
	{
		// Maya 8 does not display DDS correctly (shows rainbow static). Convert DDS to TGA for proper display.
		std::string pathToUse = texturePath;
		const std::string tgaPath = DdsToTgaConverter::convertToTga(texturePath);
		if (!tgaPath.empty())
			pathToUse = tgaPath;

		melCmd = "shadingNode -asTexture file -name \"";
		melCmd += shaderName.c_str();
		melCmd += "_tex\"";
		status = MGlobal::executeCommand(melCmd, result);
		if (status && result.length() > 0)
		{
			MString texNodeName = result[0];

			// Use forward slashes and escape backslashes for MEL (backslash is escape char)
			std::string safePath = pathToUse;
			for (std::string::size_type i = 0; i < safePath.size(); ++i)
				if (safePath[i] == '\\')
					safePath[i] = '/';

			melCmd = "setAttr -type \"string\" ";
			melCmd += texNodeName;
			melCmd += ".fileTextureName \"";
			melCmd += safePath.c_str();
			melCmd += "\"";
			IGNORE_RETURN(MGlobal::executeCommand(melCmd));

			melCmd = "connectAttr -f ";
			melCmd += texNodeName;
			melCmd += ".outColor ";
			melCmd += shaderNodeName;
			melCmd += ".color";
			IGNORE_RETURN(MGlobal::executeCommand(melCmd));
		}
	}

	MSelectionList selList;
	selList.add(sgName);
	selList.getDependNode(0, outShadingGroup);

	return MS::kSuccess;
}

// ======================================================================

static bool isMayaDefaultShader(const std::string &name)
{
	std::string lower;
	lower.reserve(name.size());
	for (size_t i = 0; i < name.size(); ++i)
		lower += static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
	return (lower == "initialshadinggroup" || lower == "lambert1" || lower == "phong1" ||
		lower.find("initialshadinggroup") != std::string::npos);
}

static bool shaderFileExists(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (f) { fclose(f); return true; }
	// Try with .sht extension (Iff/TreeFile may add it)
	std::string withExt = path;
	if (withExt.size() < 4 || withExt.compare(withExt.size() - 4, 4, ".sht") != 0)
		withExt += ".sht";
	f = fopen(withExt.c_str(), "rb");
	if (f) { fclose(f); return true; }
	return false;
}

static std::string resolveShaderPath(const std::string &shaderTemplateName, const std::string &inputFilePath)
{
	std::string normalizedInput = inputFilePath;
	for (std::string::size_type i = 0; i < normalizedInput.size(); ++i)
	{
		if (normalizedInput[i] == '\\')
			normalizedInput[i] = '/';
	}

	std::string baseDir;

	const char *envExportRoot = getenv("TITAN_EXPORT_ROOT");
	if (envExportRoot && envExportRoot[0])
	{
		baseDir = envExportRoot;
		if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '/')
			baseDir.push_back('/');
	}
	else
	{
		const char *envDataRoot = getenv("TITAN_DATA_ROOT");
		if (envDataRoot && envDataRoot[0])
		{
			baseDir = envDataRoot;
			if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '/')
				baseDir.push_back('/');
		}
		else
		{
			std::string::size_type cgPos = normalizedInput.find("compiled/game/");
			if (cgPos != std::string::npos)
				baseDir = normalizedInput.substr(0, cgPos + 14);
		}
	}

	if (baseDir.empty())
		return std::string();

	std::string resolved = baseDir + shaderTemplateName;
	for (std::string::size_type i = 0; i < resolved.size(); ++i)
	{
		if (resolved[i] == '\\')
			resolved[i] = '/';
	}

	return resolved;
}

// ----------------------------------------------------------------------

static std::string sanitizeMayaName(const std::string &name)
{
	std::string result;
	result.reserve(name.size());
	for (size_t i = 0; i < name.size(); ++i)
	{
		char c = name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
			result.push_back(c);
		else
			result.push_back('_');
	}
	if (!result.empty() && result[0] >= '0' && result[0] <= '9')
		result = "_" + result;
	return result;
}

// ======================================================================

MStatus MayaSceneBuilder::assignMaterials(
	const MDagPath & meshPath,
	const std::vector<ShaderGroupData> & shaderGroups,
	const std::string & inputFilePath)
{
	MStatus status;

	if (shaderGroups.empty())
		return MS::kSuccess;

	int faceOffset = 0;

	for (size_t sgIdx = 0; sgIdx < shaderGroups.size(); ++sgIdx)
	{
		const ShaderGroupData & group = shaderGroups[sgIdx];
		const int faceCount = static_cast<int>(group.triangles.size());

		if (faceCount == 0)
		{
			faceOffset += faceCount;
			continue;
		}

		std::string shaderName;
		{
			std::string rawName = group.shaderTemplateName;
			std::string::size_type lastSlash = rawName.find_last_of("/\\");
			if (lastSlash != std::string::npos)
				rawName = rawName.substr(lastSlash + 1);
			std::string::size_type dotPos = rawName.find_last_of('.');
			if (dotPos != std::string::npos)
				rawName = rawName.substr(0, dotPos);
			shaderName = sanitizeMayaName(rawName);
		}

		if (shaderName.empty())
			shaderName = "material";

		MString sgNodeName;
		bool materialExists = false;

		{
			MString checkCmd = "objExists \"";
			checkCmd += shaderName.c_str();
			checkCmd += "SG\"";
			int exists = 0;
			if (MGlobal::executeCommand(checkCmd, exists) && exists)
				materialExists = true;
		}

		if (!materialExists)
		{
			std::string resolvedShaderFile = resolveShaderPath(group.shaderTemplateName, inputFilePath);
			bool useDefault = isMayaDefaultShader(group.shaderTemplateName) ||
				resolvedShaderFile.empty() ||
				!shaderFileExists(resolvedShaderFile);

			if (!useDefault)
			{
				MString importCmd = "importShader -i \"";
				importCmd += resolvedShaderFile.c_str();
				importCmd += "\"";

				MStatus importStatus = MGlobal::executeCommand(importCmd, true, true);
				if (importStatus)
				{
					MESSENGER_LOG(("  imported shader [%s] -> [%s]\n", group.shaderTemplateName.c_str(), shaderName.c_str()));
				}
				else
				{
					MESSENGER_LOG_WARNING(("  failed to import shader [%s], using default material\n", resolvedShaderFile.c_str()));
					useDefault = true;
				}
			}
			if (useDefault)
			{
				MObject dummySG;
				createMaterial(shaderName, std::string(), dummySG);
				MESSENGER_LOG(("  using default material for [%s]\n", group.shaderTemplateName.c_str()));
			}
		}

		sgNodeName = shaderName.c_str();
		sgNodeName += "SG";

		MFnDagNode meshDagFn(meshPath);
		MString meshFullPath = meshDagFn.fullPathName();

		MString assignCmd = "sets -e -forceElement ";
		assignCmd += sgNodeName;
		assignCmd += " ";

		if (shaderGroups.size() == 1)
		{
			assignCmd += meshFullPath;
		}
		else
		{
			assignCmd += meshFullPath;
			assignCmd += ".f[";
			assignCmd += faceOffset;
			assignCmd += ":";
			assignCmd += (faceOffset + faceCount - 1);
			assignCmd += "]";
		}

		status = MGlobal::executeCommand(assignCmd);
		if (!status)
		{
			MESSENGER_LOG_WARNING(("  failed to assign material [%s] to faces [%d:%d]\n",
				shaderName.c_str(), faceOffset, faceOffset + faceCount - 1));
		}

		faceOffset += faceCount;
	}

	// Force viewport refresh so textures display (Maya suspends redraws during scripts)
	IGNORE_RETURN(MGlobal::executeCommand("refresh -force"));

	return MS::kSuccess;
}

// ======================================================================

namespace
{
	static void disconnectExistingAnimation(MPlug &plug)
	{
		if (!plug.isConnected())
			return;
		MPlugArray sources;
		MStatus st;
		plug.connectedTo(sources, true, false, &st);
		if (!st || sources.length() == 0)
			return;
		MDGModifier modifier;
		for (unsigned i = 0; i < sources.length(); ++i)
			modifier.disconnect(sources[i], plug);
		modifier.doIt();
	}
}

// ======================================================================

MStatus MayaSceneBuilder::setKeyframes(
	const MDagPath & jointPath,
	const std::string & attribute,
	const std::vector<AnimKeyframe> & keyframes,
	float fps)
{
	MStatus status;

	MFnDependencyNode depFn(jointPath.node());
	MPlug plug = depFn.findPlug(MString(attribute.c_str()), &status);
	if (!status)
		return status;

	disconnectExistingAnimation(plug);

	MFnAnimCurve animCurveFn;
	animCurveFn.create(plug, MFnAnimCurve::kAnimCurveTL, NULL, &status);
	if (!status)
		return status;

	for (size_t k = 0; k < keyframes.size(); ++k)
	{
		double timeVal = static_cast<double>(keyframes[k].frame) / static_cast<double>(fps);
		MTime time(timeVal, MTime::kSeconds);
		animCurveFn.addKeyframe(time, static_cast<double>(keyframes[k].value));
	}

	return MS::kSuccess;
}

// ======================================================================

MStatus MayaSceneBuilder::setRotationKeyframes(
	const MDagPath & jointPath,
	const std::vector<QuatKeyframe> & keyframes,
	float fps)
{
	MStatus status;

	MFnDependencyNode depFn(jointPath.node());
	MPlug rxPlug = depFn.findPlug("rotateX", &status);
	MPlug ryPlug = depFn.findPlug("rotateY", &status);
	MPlug rzPlug = depFn.findPlug("rotateZ", &status);
	if (!status)
		return status;

	disconnectExistingAnimation(rxPlug);
	disconnectExistingAnimation(ryPlug);
	disconnectExistingAnimation(rzPlug);

	MFnAnimCurve rxCurve, ryCurve, rzCurve;
	rxCurve.create(rxPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);
	ryCurve.create(ryPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);
	rzCurve.create(rzPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);

	for (size_t k = 0; k < keyframes.size(); ++k)
	{
		const QuatKeyframe & qk = keyframes[k];
		double timeVal = static_cast<double>(qk.frame) / static_cast<double>(fps);
		MTime time(timeVal, MTime::kSeconds);

		MQuaternion q(qk.rotation[0], qk.rotation[1], qk.rotation[2], qk.rotation[3]);
		MEulerRotation euler = q.asEulerRotation();

		// Undo the Y,Z negation from the exporter's coordinate conversion
		rxCurve.addKeyframe(time, euler.x);
		ryCurve.addKeyframe(time, -euler.y);
		rzCurve.addKeyframe(time, -euler.z);
	}

	return MS::kSuccess;
}

// ======================================================================
// ANS stores rotation as delta from bind pose: delta = current * conjugate(bindPose).
// So current = delta * bindPose. We read bind pose from joint, multiply, then set.
// ======================================================================

MStatus MayaSceneBuilder::setRotationKeyframesFromDeltas(
	const MDagPath & jointPath,
	const std::vector<QuatKeyframe> & deltaKeyframes,
	float fps)
{
	MStatus status;

	MFnIkJoint jointFn(jointPath.node());
	MQuaternion bindQuat;
	jointFn.getRotation(bindQuat, MSpace::kTransform);

	MFnDependencyNode depFn(jointPath.node());
	MPlug rxPlug = depFn.findPlug("rotateX", &status);
	MPlug ryPlug = depFn.findPlug("rotateY", &status);
	MPlug rzPlug = depFn.findPlug("rotateZ", &status);
	if (!status)
		return status;

	disconnectExistingAnimation(rxPlug);
	disconnectExistingAnimation(ryPlug);
	disconnectExistingAnimation(rzPlug);

	MFnAnimCurve rxCurve, ryCurve, rzCurve;
	rxCurve.create(rxPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);
	ryCurve.create(ryPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);
	rzCurve.create(rzPlug, MFnAnimCurve::kAnimCurveTA, NULL, &status);

	for (size_t k = 0; k < deltaKeyframes.size(); ++k)
	{
		const QuatKeyframe & qk = deltaKeyframes[k];
		double timeVal = static_cast<double>(qk.frame) / static_cast<double>(fps);
		MTime time(timeVal, MTime::kSeconds);

		// Delta from file (engine quat x,y,z,w)
		MQuaternion deltaQ(qk.rotation[0], qk.rotation[1], qk.rotation[2], qk.rotation[3]);
		// Final = delta * bindPose (export: delta = current * conj(bind), so current = delta * bind)
		MQuaternion finalQ = deltaQ * bindQuat;
		MEulerRotation euler = finalQ.asEulerRotation();

		// Undo Y,Z negation from exporter's coordinate conversion
		rxCurve.addKeyframe(time, euler.x);
		ryCurve.addKeyframe(time, -euler.y);
		rzCurve.addKeyframe(time, -euler.z);
	}

	return MS::kSuccess;
}

// ======================================================================
// ANS stores translation as delta from bind pose. final = bindPose + delta.
// negateDelta: true for translateX (engine negates X).
// ======================================================================

MStatus MayaSceneBuilder::setKeyframesFromDeltas(
	const MDagPath & jointPath,
	const std::string & attribute,
	const std::vector<AnimKeyframe> & deltaKeyframes,
	float fps,
	double bindPoseValue,
	bool negateDelta)
{
	MStatus status;

	MFnDependencyNode depFn(jointPath.node());
	MPlug plug = depFn.findPlug(MString(attribute.c_str()), &status);
	if (!status)
		return status;

	disconnectExistingAnimation(plug);

	MFnAnimCurve animCurveFn;
	animCurveFn.create(plug, MFnAnimCurve::kAnimCurveTL, NULL, &status);
	if (!status)
		return status;

	for (size_t k = 0; k < deltaKeyframes.size(); ++k)
	{
		double deltaVal = static_cast<double>(deltaKeyframes[k].value);
		if (negateDelta)
			deltaVal = -deltaVal;
		double finalVal = bindPoseValue + deltaVal;
		double timeVal = static_cast<double>(deltaKeyframes[k].frame) / static_cast<double>(fps);
		MTime time(timeVal, MTime::kSeconds);
		animCurveFn.addKeyframe(time, finalVal);
	}

	return MS::kSuccess;
}

// ======================================================================
