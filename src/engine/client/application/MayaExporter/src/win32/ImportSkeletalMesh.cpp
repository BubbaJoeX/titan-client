// ======================================================================
//
// ImportSkeletalMesh.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportSkeletalMesh.h"

#include "MayaCompoundString.h"
#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFnDagNode.h"
#include "maya/MItDag.h"

#include <map>
#include <set>
#include <string>
#include <vector>

// ======================================================================

const Tag TAG_BLT  = TAG3(B,L,T);
const Tag TAG_BLTS = TAG(B,L,T,S);
const Tag TAG_DOT3 = TAG(D,O,T,3);
const Tag TAG_DYN  = TAG3(D,Y,N);
const Tag TAG_FOZC = TAG(F,O,Z,C);
const Tag TAG_HPTS = TAG(H,P,T,S);
const Tag TAG_ITL  = TAG3(I,T,L);
const Tag TAG_NIDX = TAG(N,I,D,X);
const Tag TAG_NORM = TAG(N,O,R,M);
const Tag TAG_OITL = TAG(O,I,T,L);
const Tag TAG_OZC  = TAG3(O,Z,C);
const Tag TAG_OZN  = TAG3(O,Z,N);
const Tag TAG_PIDX = TAG(P,I,D,X);
const Tag TAG_POSN = TAG(P,O,S,N);
const Tag TAG_PRIM = TAG(P,R,I,M);
const Tag TAG_PSDT = TAG(P,S,D,T);
const Tag TAG_SKMG = TAG(S,K,M,G);
const Tag TAG_SKTM = TAG(S,K,T,M);
const Tag TAG_STAT = TAG(S,T,A,T);
const Tag TAG_TCSD = TAG(T,C,S,D);
const Tag TAG_TCSF = TAG(T,C,S,F);
const Tag TAG_TRTS = TAG(T,R,T,S);
const Tag TAG_TXCI = TAG(T,X,C,I);
const Tag TAG_TWDT = TAG(T,W,D,T);
const Tag TAG_TWHD = TAG(T,W,H,D);
const Tag TAG_VDCL = TAG(V,D,C,L);
const Tag TAG_XFNM = TAG(X,F,N,M);
const Tag TAG_ZTO  = TAG3(Z,T,O);

// ======================================================================

namespace
{
	Messenger * messenger;
}

// ======================================================================

void ImportSkeletalMesh::install(Messenger * newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportSkeletalMesh::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void * ImportSkeletalMesh::creator()
{
	return new ImportSkeletalMesh;
}

// ----------------------------------------------------------------------

ImportSkeletalMesh::ImportSkeletalMesh()
{
}

// ======================================================================

static void readHardpoints(Iff & iff, std::vector<MayaSceneBuilder::HardpointData> & hardpoints)
{
	const int16 count = iff.read_int16();

	for (int i = 0; i < static_cast<int>(count); ++i)
	{
		MayaSceneBuilder::HardpointData hp;

		std::string hpName;
		iff.read_string(hpName);
		hp.name = hpName;

		std::string parentName;
		iff.read_string(parentName);
		hp.parentJoint = parentName;

		const float qw = iff.read_float();
		const float qx = iff.read_float();
		const float qy = iff.read_float();
		const float qz = iff.read_float();
		hp.rotation[0] = qx;
		hp.rotation[1] = qy;
		hp.rotation[2] = qz;
		hp.rotation[3] = qw;

		hp.position[0] = iff.read_float();
		hp.position[1] = iff.read_float();
		hp.position[2] = iff.read_float();

		hardpoints.push_back(hp);
	}
}

// ======================================================================

MStatus ImportSkeletalMesh::doIt(const MArgList & args)
{
	MESSENGER_INDENT;

	MStatus status;

	//-- parse arguments
	std::string inputFilename;
	std::string skeletonFilename;

	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get args length\n"));

	for (unsigned argIndex = 0; argIndex < argCount; ++argIndex)
	{
		MString argName = args.asString(argIndex, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get arg [%u] as string\n", argIndex));

		if (argName == "-i" || argName == "-input")
		{
			++argIndex;
			MESSENGER_REJECT_STATUS(argIndex >= argCount, ("-i requires a filename argument\n"));
			MString val = args.asString(argIndex, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get filename arg\n"));
			inputFilename = val.asChar();
		}
		else if (argName == "-skeleton")
		{
			++argIndex;
			MESSENGER_REJECT_STATUS(argIndex >= argCount, ("-skeleton requires a filename argument\n"));
			MString val = args.asString(argIndex, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get skeleton filename arg\n"));
			skeletonFilename = val.asChar();
		}
		else
		{
			MESSENGER_LOG_WARNING(("unknown argument [%s], ignoring\n", argName.asChar()));
		}
	}

	MESSENGER_REJECT_STATUS(inputFilename.empty(), ("no input file specified. Use -i <filename>\n"));

	inputFilename = resolveImportPath(inputFilename);
	if (!skeletonFilename.empty())
		skeletonFilename = resolveImportPath(skeletonFilename);
	MESSENGER_LOG(("ImportSkeletalMesh: reading [%s]\n", inputFilename.c_str()));

	//-- open IFF
	Iff iff(inputFilename.c_str());

	const Tag topFormTag = iff.getCurrentName();
	if (topFormTag != TAG_SKMG)
	{
		MESSENGER_LOG_ERROR(("ImportSkeletalMesh: expected FORM SKMG but found different top-level form in [%s]\n", inputFilename.c_str()));
		return MS::kFailure;
	}

	iff.enterForm(TAG_SKMG);

	const Tag versionTag = iff.getCurrentName();
	iff.enterForm(versionTag);

	//-- read INFO chunk
	int32 maxTransformsPerVertex   = 0;
	int32 maxTransformsPerShader   = 0;
	int32 skeletonTemplateCount    = 0;
	int32 transformNameCount       = 0;
	int32 positionCount            = 0;
	int32 transformWeightDataCount = 0;
	int32 normalCount              = 0;
	int32 perShaderDataCount       = 0;
	int32 blendTargetCount         = 0;
	int16 occlusionZoneNameCount   = 0;
	int16 occlusionZoneCombCount   = 0;
	int16 zonesThisOccludesCount   = 0;
	int16 occlusionLayer           = 0;

	iff.enterChunk(TAG_INFO);
	{
		maxTransformsPerVertex   = iff.read_int32();
		maxTransformsPerShader   = iff.read_int32();
		skeletonTemplateCount    = iff.read_int32();
		transformNameCount       = iff.read_int32();
		positionCount            = iff.read_int32();
		transformWeightDataCount = iff.read_int32();
		normalCount              = iff.read_int32();
		perShaderDataCount       = iff.read_int32();
		blendTargetCount         = iff.read_int32();
		occlusionZoneNameCount   = iff.read_int16();
		occlusionZoneCombCount   = iff.read_int16();
		zonesThisOccludesCount   = iff.read_int16();
		occlusionLayer           = iff.read_int16();
	}
	iff.exitChunk(TAG_INFO);

	UNREF(maxTransformsPerVertex);
	UNREF(maxTransformsPerShader);
	UNREF(occlusionZoneNameCount);
	UNREF(occlusionZoneCombCount);
	UNREF(zonesThisOccludesCount);
	UNREF(occlusionLayer);

	MESSENGER_LOG(("  INFO: positions=%d normals=%d transforms=%d shaders=%d blendTargets=%d\n",
		positionCount, normalCount, transformNameCount, perShaderDataCount, blendTargetCount));

	//-- read SKTM chunk (skeleton template names)
	std::vector<std::string> skeletonTemplateNames;
	iff.enterChunk(TAG_SKTM);
	{
		for (int32 i = 0; i < skeletonTemplateCount; ++i)
		{
			std::string name;
			iff.read_string(name);
			skeletonTemplateNames.push_back(name);
		}
	}
	iff.exitChunk(TAG_SKTM);

	//-- read XFNM chunk (transform names)
	std::vector<std::string> transformNames;
	iff.enterChunk(TAG_XFNM);
	{
		for (int32 i = 0; i < transformNameCount; ++i)
		{
			std::string name;
			iff.read_string(name);
			transformNames.push_back(name);
		}
	}
	iff.exitChunk(TAG_XFNM);

	//-- read POSN chunk (positions as x,y,z floats)
	std::vector<float> positions;
	positions.reserve(static_cast<size_t>(positionCount) * 3);
	iff.enterChunk(TAG_POSN);
	{
		for (int32 i = 0; i < positionCount; ++i)
		{
			positions.push_back(iff.read_float());
			positions.push_back(iff.read_float());
			positions.push_back(iff.read_float());
		}
	}
	iff.exitChunk(TAG_POSN);

	//-- read TWHD chunk (weight count per vertex)
	std::vector<int> weightHeaders;
	weightHeaders.reserve(static_cast<size_t>(positionCount));
	iff.enterChunk(TAG_TWHD);
	{
		for (int32 i = 0; i < positionCount; ++i)
			weightHeaders.push_back(static_cast<int>(iff.read_int32()));
	}
	iff.exitChunk(TAG_TWHD);

	//-- read TWDT chunk (transform index + weight pairs)
	std::vector<MayaSceneBuilder::SkinWeight> weightData;
	weightData.reserve(static_cast<size_t>(transformWeightDataCount));
	iff.enterChunk(TAG_TWDT);
	{
		for (int32 i = 0; i < transformWeightDataCount; ++i)
		{
			MayaSceneBuilder::SkinWeight sw;
			sw.transformIndex = static_cast<int>(iff.read_int32());
			sw.weight         = iff.read_float();
			weightData.push_back(sw);
		}
	}
	iff.exitChunk(TAG_TWDT);

	//-- read NORM chunk (normals as x,y,z floats)
	std::vector<float> normals;
	normals.reserve(static_cast<size_t>(normalCount) * 3);
	iff.enterChunk(TAG_NORM);
	{
		for (int32 i = 0; i < normalCount; ++i)
		{
			normals.push_back(iff.read_float());
			normals.push_back(iff.read_float());
			normals.push_back(iff.read_float());
		}
	}
	iff.exitChunk(TAG_NORM);

	//-- skip DOT3 chunk if present
	if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DOT3)
	{
		iff.enterChunk(TAG_DOT3);
		iff.exitChunk(TAG_DOT3);
	}

	//-- read HPTS form if present (hardpoints)
	std::vector<MayaSceneBuilder::HardpointData> staticHardpoints;
	std::vector<MayaSceneBuilder::HardpointData> dynamicHardpoints;

	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_HPTS)
	{
		iff.enterForm(TAG_HPTS);

		while (iff.getNumberOfBlocksLeft() > 0)
		{
			const Tag chunkTag = iff.getCurrentName();

			if (chunkTag == TAG_STAT)
			{
				iff.enterChunk(TAG_STAT);
				readHardpoints(iff, staticHardpoints);
				iff.exitChunk(TAG_STAT);
			}
			else if (chunkTag == TAG_DYN)
			{
				iff.enterChunk(TAG_DYN);
				readHardpoints(iff, dynamicHardpoints);
				iff.exitChunk(TAG_DYN);
			}
			else
			{
				iff.enterChunk();
				iff.exitChunk();
			}
		}

		iff.exitForm(TAG_HPTS);
	}

	//-- read BLTS form if present (blend targets)
	std::vector<MayaSceneBuilder::BlendTargetData> blendTargets;

	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_BLTS)
	{
		iff.enterForm(TAG_BLTS);

		while (iff.getNumberOfBlocksLeft() > 0)
		{
			iff.enterForm(TAG_BLT);

			MayaSceneBuilder::BlendTargetData bt;

			//-- BLT INFO
			iff.enterChunk(TAG_INFO);
			{
				const int32 btPosCount  = iff.read_int32();
				const int32 btNormCount = iff.read_int32();
				UNREF(btNormCount);

				std::string variableName;
				iff.read_string(variableName);
				bt.name = variableName;

				bt.positionIndices.reserve(static_cast<size_t>(btPosCount));
				bt.positionDeltas.reserve(static_cast<size_t>(btPosCount) * 3);
			}
			iff.exitChunk(TAG_INFO);

			//-- BLT POSN
			if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_POSN)
			{
				iff.enterChunk(TAG_POSN);
				while (iff.getChunkLengthLeft(1) > 0)
				{
					const int32 idx = iff.read_int32();
					const float dx  = iff.read_float();
					const float dy  = iff.read_float();
					const float dz  = iff.read_float();

					bt.positionIndices.push_back(static_cast<int>(idx));
					bt.positionDeltas.push_back(dx);
					bt.positionDeltas.push_back(dy);
					bt.positionDeltas.push_back(dz);
				}
				iff.exitChunk(TAG_POSN);
			}

			//-- BLT NORM
			if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_NORM)
			{
				iff.enterChunk(TAG_NORM);
				while (iff.getChunkLengthLeft(1) > 0)
				{
					const int32 idx = iff.read_int32();
					const float dx  = iff.read_float();
					const float dy  = iff.read_float();
					const float dz  = iff.read_float();

					bt.normalIndices.push_back(static_cast<int>(idx));
					bt.normalDeltas.push_back(dx);
					bt.normalDeltas.push_back(dy);
					bt.normalDeltas.push_back(dz);
				}
				iff.exitChunk(TAG_NORM);
			}

			//-- skip remaining BLT sub-chunks (DOT3, HPTS, etc.)
			while (iff.getNumberOfBlocksLeft() > 0)
			{
				if (iff.isCurrentForm())
				{
					iff.enterForm();
					iff.exitForm();
				}
				else
				{
					iff.enterChunk();
					iff.exitChunk();
				}
			}

			blendTargets.push_back(bt);

			iff.exitForm(TAG_BLT);
		}

		iff.exitForm(TAG_BLTS);
	}

	//-- skip OZN, FOZC, OZC, ZTO chunks
	while (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm())
	{
		const Tag chunkTag = iff.getCurrentName();
		if (chunkTag == TAG_OZN || chunkTag == TAG_FOZC || chunkTag == TAG_OZC || chunkTag == TAG_ZTO)
		{
			iff.enterChunk();
			iff.exitChunk();
		}
		else
		{
			break;
		}
	}

	//-- read per-shader data (PSDT forms)
	std::vector<MayaSceneBuilder::ShaderGroupData> shaderGroups;

	for (int32 shaderIndex = 0; shaderIndex < perShaderDataCount; ++shaderIndex)
	{
		iff.enterForm(TAG_PSDT);

		MayaSceneBuilder::ShaderGroupData sg;

		//-- NAME chunk (shader template name)
		iff.enterChunk(TAG_NAME);
		{
			std::string shaderName;
			iff.read_string(shaderName);
			sg.shaderTemplateName = shaderName;
		}
		iff.exitChunk(TAG_NAME);

		//-- PIDX chunk (position indices)
		int32 shaderVertexCount = 0;
		iff.enterChunk(TAG_PIDX);
		{
			shaderVertexCount = iff.read_int32();
			sg.positionIndices.reserve(static_cast<size_t>(shaderVertexCount));
			for (int32 i = 0; i < shaderVertexCount; ++i)
				sg.positionIndices.push_back(static_cast<int>(iff.read_int32()));
		}
		iff.exitChunk(TAG_PIDX);

		//-- NIDX chunk (normal indices, optional)
		if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_NIDX)
		{
			iff.enterChunk(TAG_NIDX);
			{
				sg.normalIndices.reserve(static_cast<size_t>(shaderVertexCount));
				for (int32 i = 0; i < shaderVertexCount; ++i)
					sg.normalIndices.push_back(static_cast<int>(iff.read_int32()));
			}
			iff.exitChunk(TAG_NIDX);
		}

		//-- skip DOT3 chunk if present
		if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DOT3)
		{
			iff.enterChunk(TAG_DOT3);
			iff.exitChunk(TAG_DOT3);
		}

		//-- skip VDCL chunk if present
		if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_VDCL)
		{
			iff.enterChunk(TAG_VDCL);
			iff.exitChunk(TAG_VDCL);
		}

		//-- TXCI chunk (texture coordinate set info)
		int32 texCoordSetCount = 0;
		std::vector<int32> texCoordDimensionality;

		if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_TXCI)
		{
			iff.enterChunk(TAG_TXCI);
			{
				texCoordSetCount = iff.read_int32();
				texCoordDimensionality.reserve(static_cast<size_t>(texCoordSetCount));
				for (int32 i = 0; i < texCoordSetCount; ++i)
					texCoordDimensionality.push_back(iff.read_int32());
			}
			iff.exitChunk(TAG_TXCI);
		}

		//-- TCSF form (texture coordinate set data)
		if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TCSF)
		{
			iff.enterForm(TAG_TCSF);

			// Read only the first UV set with dimensionality >= 2
			bool firstSetRead = false;

			for (int32 setIndex = 0; setIndex < texCoordSetCount; ++setIndex)
			{
				iff.enterChunk(TAG_TCSD);

				const int32 dim = (setIndex < static_cast<int32>(texCoordDimensionality.size())) ? texCoordDimensionality[static_cast<size_t>(setIndex)] : 2;

				if (!firstSetRead && dim >= 2)
				{
					sg.uvs.reserve(static_cast<size_t>(shaderVertexCount));
					for (int32 vi = 0; vi < shaderVertexCount; ++vi)
					{
						MayaSceneBuilder::UVData uv;
						uv.u = iff.read_float();
						uv.v = iff.read_float();

						// skip extra dimensions beyond 2
						for (int32 d = 2; d < dim; ++d)
							iff.read_float();

						sg.uvs.push_back(uv);
					}
					firstSetRead = true;
				}
				else
				{
					// skip this set
					for (int32 vi = 0; vi < shaderVertexCount; ++vi)
					{
						for (int32 d = 0; d < dim; ++d)
							iff.read_float();
					}
				}

				iff.exitChunk(TAG_TCSD);
			}

			iff.exitForm(TAG_TCSF);
		}

		//-- PRIM form (triangle primitives)
		if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_PRIM)
		{
			iff.enterForm(TAG_PRIM);

			//-- INFO chunk (primitive count)
			iff.enterChunk(TAG_INFO);
			{
				iff.read_int32();
			}
			iff.exitChunk(TAG_INFO);

			//-- read primitives (ITL or OITL)
			while (iff.getNumberOfBlocksLeft() > 0)
			{
				const Tag primTag = iff.getCurrentName();

				if (primTag == TAG_ITL)
				{
					iff.enterChunk(TAG_ITL);
					{
						const int32 triCount = iff.read_int32();
						for (int32 t = 0; t < triCount; ++t)
						{
							MayaSceneBuilder::TriangleData tri;
							tri.indices[0] = static_cast<int>(iff.read_int32());
							tri.indices[1] = static_cast<int>(iff.read_int32());
							tri.indices[2] = static_cast<int>(iff.read_int32());
							sg.triangles.push_back(tri);
						}
					}
					iff.exitChunk(TAG_ITL);
				}
				else if (primTag == TAG_OITL)
				{
					iff.enterChunk(TAG_OITL);
					{
						const int32 triCount = iff.read_int32();
						for (int32 t = 0; t < triCount; ++t)
						{
							iff.read_int16(); // occlusion zone combination index
							MayaSceneBuilder::TriangleData tri;
							tri.indices[0] = static_cast<int>(iff.read_int32());
							tri.indices[1] = static_cast<int>(iff.read_int32());
							tri.indices[2] = static_cast<int>(iff.read_int32());
							sg.triangles.push_back(tri);
						}
					}
					iff.exitChunk(TAG_OITL);
				}
				else
				{
					if (iff.isCurrentForm())
					{
						iff.enterForm();
						iff.exitForm();
					}
					else
					{
						iff.enterChunk();
						iff.exitChunk();
					}
				}
			}

			iff.exitForm(TAG_PRIM);
		}

		shaderGroups.push_back(sg);

		iff.exitForm(TAG_PSDT);
	}

	//-- skip TRTS form if present
	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TRTS)
	{
		iff.enterForm(TAG_TRTS);
		iff.exitForm(TAG_TRTS);
	}

	iff.exitForm(versionTag);
	iff.exitForm(TAG_SKMG);

	MESSENGER_LOG(("  Read %d shader groups, %d blend targets, %d static hardpoints, %d dynamic hardpoints\n",
		static_cast<int>(shaderGroups.size()),
		static_cast<int>(blendTargets.size()),
		static_cast<int>(staticHardpoints.size()),
		static_cast<int>(dynamicHardpoints.size())));

	//-- derive mesh name from filename
	std::string meshName = inputFilename;
	{
		const std::string::size_type lastSlash = meshName.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			meshName = meshName.substr(lastSlash + 1);

		const std::string::size_type dotPos = meshName.find_last_of('.');
		if (dotPos != std::string::npos)
			meshName = meshName.substr(0, dotPos);
	}

	//-- create the mesh
	MDagPath meshPath;
	status = MayaSceneBuilder::createMesh(positions, normals, shaderGroups, meshName, meshPath);
	MESSENGER_REJECT_STATUS(!status, ("failed to create mesh\n"));

	MESSENGER_LOG(("  Created mesh [%s]\n", meshName.c_str()));

	//-- assign materials from shader groups
	status = MayaSceneBuilder::assignMaterials(meshPath, shaderGroups, inputFilename);
	if (status)
		MESSENGER_LOG(("  Assigned %d materials\n", static_cast<int>(shaderGroups.size())));
	else
		MESSENGER_LOG_WARNING(("  Failed to assign materials\n"));

	//-- create skin cluster and/or hardpoints when we have transforms or hardpoints
	const bool needJoints = !transformNames.empty() || !staticHardpoints.empty() || !dynamicHardpoints.empty();
	if (needJoints)
	{
		std::vector<MDagPath> jointPaths;

		// Build joint name -> DAG path map by iterating joints in the scene.
		// Mesh XFNM stores transform names as short names (e.g. "lthigh", "root") - the exporter
		// uses MayaCompoundString::getComponentString(0) to derive these from Maya joint names
		// like "lthigh_all_b_l0" or "root__all_b_l0". We must use the same logic for matching.
		// Include joints from ALL skeleton templates the mesh references (e.g. all_b, hum_f_face).
		std::map<std::string, MDagPath> jointMap;
		{
			std::set<std::string> skeletonFilters;
			for (size_t i = 0; i < skeletonTemplateNames.size(); ++i)
			{
				std::string filter = skeletonTemplateNames[i];
				const std::string::size_type lastSlash = filter.find_last_of("/\\");
				if (lastSlash != std::string::npos)
					filter = filter.substr(lastSlash + 1);
				const std::string::size_type dot = filter.find_last_of('.');
				if (dot != std::string::npos)
					filter = filter.substr(0, dot);
				if (!filter.empty())
					skeletonFilters.insert(filter);
			}

			MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
			for (; !dagIt.isDone(); dagIt.next())
			{
				MDagPath dagPath;
				dagIt.getPath(dagPath);
				MFnDagNode dagFn(dagPath);
				const MString mayaName(dagFn.name());

				// If we have skeleton filters, only use joints from those skeletons' hierarchies
				if (!skeletonFilters.empty())
				{
					MString fullPath = dagPath.fullPathName();
					std::string pathStr(fullPath.asChar());
					bool inFilter = false;
					for (std::set<std::string>::const_iterator it = skeletonFilters.begin(); it != skeletonFilters.end(); ++it)
					{
						if (pathStr.find(*it) != std::string::npos)
						{
							inFilter = true;
							break;
						}
					}
					if (!inFilter)
						continue;
				}

				// Use same short name as exporter: getComponentString(0) from MayaCompoundString
				MayaCompoundString compoundName(mayaName);
				const MString gameTransformName = (compoundName.getComponentCount() > 0) ? compoundName.getComponentString(0) : mayaName;
				const std::string shortName(gameTransformName.asChar());

				if (jointMap.find(shortName) == jointMap.end())
					jointMap[shortName] = dagPath;
			}
		}

		// Populate jointPaths from mesh transform names - MUST preserve exact order and index
		// so weight transform indices map correctly to skin cluster influences.
		jointPaths.reserve(transformNames.size());
		int missingCount = 0;
		for (size_t i = 0; i < transformNames.size(); ++i)
		{
			std::map<std::string, MDagPath>::const_iterator jit = jointMap.find(transformNames[i]);
			if (jit != jointMap.end())
				jointPaths.push_back(jit->second);
			else
			{
				++missingCount;
				MESSENGER_LOG_WARNING(("  Skin cluster: no joint for transform [%s] (index %u)\n", transformNames[i].c_str(), static_cast<unsigned>(i)));
			}
		}
		if (missingCount > 0)
			MESSENGER_LOG_WARNING(("  Skin cluster: %d transforms have no matching joint - weights will be dropped\n", missingCount));

		if (jointPaths.empty() && !transformNames.empty())
		{
			MESSENGER_LOG_WARNING(("  No matching joints found in scene for skin cluster (mesh expects %d transforms)\n", static_cast<int>(transformNames.size())));
		}
		else
		{
			// Log skeleton hierarchy for skin binding (compare with ImportAnimation output)
			std::string skinHint;
			if (!jointPaths.empty())
			{
				skinHint = jointPaths[0].fullPathName().asChar();
				std::string::size_type p = skinHint.find('|', 1);
				if (p != std::string::npos) skinHint = skinHint.substr(0, p);
			}
			MESSENGER_LOG(("  Skin cluster: skeleton [%s], %d influences\n",
				skinHint.c_str(), static_cast<int>(jointPaths.size())));

			status = MayaSceneBuilder::createSkinCluster(meshPath, jointPaths, transformNames, weightHeaders, weightData);
			if (status)
				MESSENGER_LOG(("  Created skin cluster with %d transforms\n", static_cast<int>(transformNames.size())));
			else
				MESSENGER_LOG_WARNING(("  Could not create skin cluster via MEL\n"));
		}

		//-- create hardpoints if present (use jointMap for parent lookup so hardpoints can attach to any joint)
		std::vector<MayaSceneBuilder::HardpointData> allHardpoints;
		allHardpoints.insert(allHardpoints.end(), staticHardpoints.begin(), staticHardpoints.end());
		allHardpoints.insert(allHardpoints.end(), dynamicHardpoints.begin(), dynamicHardpoints.end());

		if (!allHardpoints.empty() && !jointMap.empty())
		{
			status = MayaSceneBuilder::createHardpoints(allHardpoints, jointMap, meshName);
			if (status)
			{
				MESSENGER_LOG(("  Created %d hardpoints\n", static_cast<int>(allHardpoints.size())));
			}
			else
			{
				MESSENGER_LOG_WARNING(("  Could not create hardpoints\n"));
			}
		}
	}

	//-- create blend shapes if present
	if (!blendTargets.empty())
	{
		status = MayaSceneBuilder::createBlendShapes(meshPath, blendTargets);
		if (status)
		{
			MESSENGER_LOG(("  Created %d blend shapes\n", static_cast<int>(blendTargets.size())));
		}
		else
		{
			MESSENGER_LOG_WARNING(("  Could not create blend shapes\n"));
		}
	}

	MESSENGER_LOG(("ImportSkeletalMesh: done.\n"));

	return MS::kSuccess;
}

// ======================================================================
