// ======================================================================
//
// ImportStaticMesh.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportStaticMesh.h"

#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFnDagNode.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MSelectionList.h"
#include "maya/MStatus.h"

#include <string>
#include <vector>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_APT  = TAG3(A,P,T);
	const Tag TAG_MESH = TAG(M,E,S,H);
	const Tag TAG_DTLA = TAG(D,T,L,A);
	const Tag TAG_CHLD = TAG(C,H,L,D);
	const Tag TAG_SPS  = TAG3(S,P,S);
	const Tag TAG_CNT  = TAG3(C,N,T);
	const Tag TAG_VTXA = TAG(V,T,X,A);
	const Tag TAG_INDX = TAG(I,N,D,X);
	const Tag TAG_SIDX = TAG(S,I,D,X);
	const Tag TAG_APPR = TAG(A,P,P,R);
	const Tag TAG_HPTS = TAG(H,P,T,S);
	const Tag TAG_HPNT = TAG(H,P,N,T);
	const Tag TAG_FLOR = TAG(F,L,O,R);
}

// ======================================================================

static std::string resolveTreeFilePath(const std::string &treeFilePath, const std::string &inputFilename)
{
	std::string baseDir;

	const char *envExportRoot = getenv("TITAN_EXPORT_ROOT");
	if (envExportRoot && envExportRoot[0])
	{
		baseDir = envExportRoot;
		if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '\\' && baseDir[baseDir.size() - 1] != '/')
			baseDir.push_back('/');
	}
	else
	{
		const char *envDataRoot = getenv("TITAN_DATA_ROOT");
		if (envDataRoot && envDataRoot[0])
		{
			baseDir = envDataRoot;
			if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '\\' && baseDir[baseDir.size() - 1] != '/')
				baseDir.push_back('/');
		}
		else
		{
			std::string normalizedInput = inputFilename;
			for (std::string::size_type i = 0; i < normalizedInput.size(); ++i)
			{
				if (normalizedInput[i] == '\\')
					normalizedInput[i] = '/';
			}

			std::string::size_type cgPos = normalizedInput.find("compiled/game/");
			if (cgPos != std::string::npos)
			{
				baseDir = normalizedInput.substr(0, cgPos + 14);
			}
			else
			{
				std::string::size_type firstSlash = treeFilePath.find_first_of("/\\");
				if (firstSlash != std::string::npos)
				{
					std::string firstComponent = "/" + treeFilePath.substr(0, firstSlash + 1);
					std::string::size_type pos = normalizedInput.find(firstComponent);
					if (pos != std::string::npos)
						baseDir = normalizedInput.substr(0, pos + 1);
				}
			}

			if (baseDir.empty())
			{
				const std::string::size_type lastSlash = normalizedInput.find_last_of('/');
				if (lastSlash != std::string::npos)
					baseDir = normalizedInput.substr(0, lastSlash + 1);
			}
		}
	}

	std::string resolved = baseDir + treeFilePath;

	for (std::string::size_type i = 0; i < resolved.size(); ++i)
	{
		if (resolved[i] == '\\')
			resolved[i] = '/';
	}

	return resolved;
}

// ======================================================================

void ImportStaticMesh::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportStaticMesh::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportStaticMesh::creator()
{
	return new ImportStaticMesh();
}

// ======================================================================

ImportStaticMesh::ImportStaticMesh()
{
}

// ======================================================================

MStatus ImportStaticMesh::doIt(const MArgList &args)
{
	MESSENGER_INDENT;

	MStatus status;

	//-- parse arguments: -i <filename>
	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get argument count\n"));
	MESSENGER_REJECT_STATUS(argCount < 2, ("usage: importStaticMesh -i <filename> [-parent <transform>]\n"));

	std::string filename;
	std::string parentName;

	for (unsigned i = 0; i < argCount; ++i)
	{
		MString arg = args.asString(i, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get argument %u\n", i));

		if (arg == "-i" && (i + 1) < argCount)
		{
			MString val = args.asString(i + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get filename argument\n"));
			filename = val.asChar();
			++i;
		}
		else if (arg == "-parent" && (i + 1) < argCount)
		{
			MString val = args.asString(i + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get parent argument\n"));
			parentName = val.asChar();
			++i;
		}
	}

	MESSENGER_REJECT_STATUS(filename.empty(), ("no filename specified, use -i <filename>\n"));

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportStaticMesh: opening [%s]\n", filename.c_str()));

	//-- open the IFF file and determine if it's an APT redirect or direct MESH
	std::string meshFilename = filename;
	bool isRedirect = false;

	{
		Iff probeIff(filename.c_str());
		if (probeIff.getCurrentName() == TAG_APT)
		{
			probeIff.enterForm(TAG_APT);
			probeIff.enterForm(TAG_0000);

			probeIff.enterChunk(TAG_NAME);
			{
				std::string redirectPath;
				probeIff.read_string(redirectPath);
				meshFilename = redirectPath;
			}
			probeIff.exitChunk(TAG_NAME);

			probeIff.exitForm(TAG_0000);
			probeIff.exitForm(TAG_APT);

			MESSENGER_LOG(("  APT redirect -> [%s]\n", meshFilename.c_str()));

			meshFilename = resolveTreeFilePath(meshFilename, filename);
			MESSENGER_LOG(("  resolved path -> [%s]\n", meshFilename.c_str()));
			isRedirect = true;
		}
	}

	UNREF(isRedirect);

	//-- if the resolved file is a DTLA (detail/LOD appearance), extract the first child mesh path
	{
		Iff probeIff2(meshFilename.c_str());
		Tag topForm = probeIff2.getCurrentName();

		if (topForm == TAG_DTLA)
		{
			MESSENGER_LOG(("  file is DTLA (detail LOD), extracting child meshes\n"));

			probeIff2.enterForm(TAG_DTLA);
			Tag versionTag = probeIff2.getCurrentName();
			probeIff2.enterForm(versionTag);

			// skip forms/chunks until we find FORM DATA which contains the CHLD entries
			while (probeIff2.getNumberOfBlocksLeft() > 0)
			{
				if (probeIff2.isCurrentForm() && probeIff2.getCurrentName() == TAG_DATA)
					break;

				if (probeIff2.isCurrentForm())
				{
					probeIff2.enterForm();
					probeIff2.exitForm();
				}
				else
				{
					probeIff2.enterChunk();
					probeIff2.exitChunk();
				}
			}

			if (probeIff2.getNumberOfBlocksLeft() > 0 && probeIff2.isCurrentForm() && probeIff2.getCurrentName() == TAG_DATA)
			{
				probeIff2.enterForm(TAG_DATA);

				if (probeIff2.getNumberOfBlocksLeft() > 0)
				{
					probeIff2.enterChunk(TAG_CHLD);
					{
						const int32 childId = probeIff2.read_int32();
						UNREF(childId);
						char *childName = probeIff2.read_string();

						std::string childPath = std::string("appearance/") + childName;
						delete [] childName;

						MESSENGER_LOG(("  DTLA child[0] -> [%s]\n", childPath.c_str()));
						meshFilename = resolveTreeFilePath(childPath, meshFilename);
						MESSENGER_LOG(("  resolved child path -> [%s]\n", meshFilename.c_str()));
					}
					probeIff2.exitChunk(TAG_CHLD);
				}

				probeIff2.exitForm(TAG_DATA);
			}

			probeIff2.exitForm(versionTag);
			probeIff2.exitForm(TAG_DTLA);
		}
	}

	Iff iff(meshFilename.c_str());

	Tag meshTopForm = iff.getCurrentName();
	if (meshTopForm != TAG_MESH)
	{
		MESSENGER_LOG_ERROR(("ImportStaticMesh: expected FORM MESH but found [%c%c%c%c] in [%s]\n",
			static_cast<char>((meshTopForm >> 24) & 0xFF),
			static_cast<char>((meshTopForm >> 16) & 0xFF),
			static_cast<char>((meshTopForm >> 8) & 0xFF),
			static_cast<char>(meshTopForm & 0xFF),
			meshFilename.c_str()));
		return MS::kFailure;
	}

	//-- enter FORM MESH / FORM 0005
	iff.enterForm(TAG_MESH);
	iff.enterForm(TAG_0005);

	//-- read APPR form if present (appearance: extents, hardpoints, floors)
	std::vector<MayaSceneBuilder::HardpointData> hardpoints;
	std::string floorReferencePath;

	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_APPR)
	{
		iff.enterForm(TAG_APPR);
		iff.enterForm(TAG_0003);

		while (iff.getNumberOfBlocksLeft() > 0)
		{
			if (iff.isCurrentForm())
			{
				const Tag formTag = iff.getCurrentName();

				if (formTag == TAG_FLOR)
				{
					iff.enterForm(TAG_FLOR);
					if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DATA)
					{
						iff.enterChunk(TAG_DATA);
						bool hasFloors = iff.read_bool8();
						if (hasFloors)
						{
							floorReferencePath = iff.read_string();
						}
						iff.exitChunk(TAG_DATA);
					}
					iff.exitForm(TAG_FLOR);
				}
				else if (formTag == TAG_HPTS)
				{
					iff.enterForm(TAG_HPTS);

					while (iff.getNumberOfBlocksLeft() > 0)
					{
						if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_HPNT)
						{
							iff.enterChunk(TAG_HPNT);
							{
								MayaSceneBuilder::HardpointData hp;

								// HPNT contains a 4x4 transform matrix (16 floats) + name string
								float matrix[16];
								for (int m = 0; m < 16; ++m)
									matrix[m] = iff.read_float();

								std::string hpName;
								iff.read_string(hpName);
								hp.name = hpName;
								hp.parentJoint = "";

								// Extract position from matrix column 3
								hp.position[0] = matrix[12];
								hp.position[1] = matrix[13];
								hp.position[2] = matrix[14];

								// Extract rotation as identity for now (matrix -> quat conversion)
								hp.rotation[0] = 0.0f;
								hp.rotation[1] = 0.0f;
								hp.rotation[2] = 0.0f;
								hp.rotation[3] = 1.0f;

								hardpoints.push_back(hp);
							}
							iff.exitChunk(TAG_HPNT);
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

					iff.exitForm(TAG_HPTS);
				}
				else
				{
					iff.enterForm();
					iff.exitForm();
				}
			}
			else
			{
				iff.enterChunk();
				iff.exitChunk();
			}
		}

		iff.exitForm(TAG_0003);
		iff.exitForm(TAG_APPR);
	}

	//-- skip forms/chunks until we find FORM SPS
	while (iff.getNumberOfBlocksLeft() > 0)
	{
		if (iff.isCurrentForm() && iff.getCurrentName() == TAG_SPS)
			break;

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

	MESSENGER_REJECT_STATUS(iff.getNumberOfBlocksLeft() == 0, ("could not find FORM SPS in mesh file\n"));

	//-- enter FORM SPS / FORM 0001
	iff.enterForm(TAG_SPS);
	iff.enterForm(TAG_0001);

	//-- read CNT chunk (shader count)
	int32 shaderCount = 0;
	iff.enterChunk(TAG_CNT);
	{
		shaderCount = iff.read_int32();
	}
	iff.exitChunk(TAG_CNT);

	MESSENGER_LOG(("  shader count: %d\n", static_cast<int>(shaderCount)));

	std::vector<float> allPositions;
	std::vector<float> allNormals;
	std::vector<MayaSceneBuilder::ShaderGroupData> shaderGroups;

	//-- read each shader group (form tag = ConvertIntToTag(shaderIndex+1))
	for (int32 shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		const Tag shaderFormTag = ConvertIntToTag(shaderIndex + 1);
		iff.enterForm(shaderFormTag);

		MayaSceneBuilder::ShaderGroupData sg;

		//-- read shader NAME
		iff.enterChunk(TAG_NAME);
		{
			std::string shaderName;
			iff.read_string(shaderName);
			sg.shaderTemplateName = shaderName;
		}
		iff.exitChunk(TAG_NAME);

		MESSENGER_LOG(("  shader %d: [%s]\n", static_cast<int>(shaderIndex), sg.shaderTemplateName.c_str()));

		//-- read INFO chunk (primitive count per shader)
		int32 primitiveCount = 0;
		iff.enterChunk(TAG_INFO);
		{
			primitiveCount = iff.read_int32();
		}
		iff.exitChunk(TAG_INFO);

		//-- read each primitive
		for (int32 primIndex = 0; primIndex < primitiveCount; ++primIndex)
		{
			iff.enterForm(TAG_0001);

			//-- primitive INFO
			bool hasIndices = false;
			iff.enterChunk(TAG_INFO);
			{
				iff.read_int32();                     // primitiveType
				hasIndices = (iff.read_int8() != 0);  // hasIndices
				iff.read_int8();                      // hasSortedIndices
			}
			iff.exitChunk(TAG_INFO);

			//-- FORM VTXA / FORM 0003
			iff.enterForm(TAG_VTXA);
			iff.enterForm(TAG_0003);

			uint32 formatFlags = 0;
			int32  vertexCount = 0;

			iff.enterChunk(TAG_INFO);
			{
				formatFlags = iff.read_uint32();
				vertexCount = iff.read_int32();
			}
			iff.exitChunk(TAG_INFO);

			const bool hasPosition   = (formatFlags & 0x00000001) != 0;
			const bool hasTransform  = (formatFlags & 0x00000002) != 0;
			const bool hasNormal     = (formatFlags & 0x00000004) != 0;
			const bool hasPointSize  = (formatFlags & 0x00000008) != 0;
			const bool hasColor0     = (formatFlags & 0x00000010) != 0;
			const bool hasColor1     = (formatFlags & 0x00000020) != 0;
			const int texCoordSetCount = static_cast<int>((formatFlags >> 8) & 0x0F);

			const int basePositionIndex = static_cast<int>(allPositions.size()) / 3;
			const int baseVertexIndex = static_cast<int>(sg.positionIndices.size());

			//-- read DATA chunk (interleaved vertex data)
			iff.enterChunk(TAG_DATA);
			{
				for (int32 v = 0; v < vertexCount; ++v)
				{
					float px = 0.0f, py = 0.0f, pz = 0.0f;
					float nx = 0.0f, ny = 0.0f, nz = 0.0f;
					float tu = 0.0f, tv = 0.0f;
					uint32 color0 = 0xFFFFFFFF;

					if (hasPosition)
					{
						px = iff.read_float();
						py = iff.read_float();
						pz = iff.read_float();
					}

					if (hasTransform)
						iff.read_float();

					if (hasNormal)
					{
						nx = iff.read_float();
						ny = iff.read_float();
						nz = iff.read_float();
					}

					if (hasPointSize)
						iff.read_float();

					if (hasColor0)
						color0 = iff.read_uint32();

					if (hasColor1)
						iff.read_uint32();

					for (int tc = 0; tc < texCoordSetCount; ++tc)
					{
						const int dim = static_cast<int>(((formatFlags >> (12 + tc * 2)) & 0x03)) + 1;

						if (tc == 0 && dim >= 2)
						{
							tu = iff.read_float();
							tv = iff.read_float();
							for (int d = 2; d < dim; ++d)
								iff.read_float();
						}
						else
						{
							for (int d = 0; d < dim; ++d)
								iff.read_float();
						}
					}

					allPositions.push_back(px);
					allPositions.push_back(py);
					allPositions.push_back(pz);

					allNormals.push_back(nx);
					allNormals.push_back(ny);
					allNormals.push_back(nz);

					sg.positionIndices.push_back(basePositionIndex + static_cast<int>(v));

					if (texCoordSetCount > 0)
					{
						MayaSceneBuilder::UVData uv;
						uv.u = tu;
						uv.v = tv;
						sg.uvs.push_back(uv);
					}

					if (hasColor0)
						sg.vertexColors.push_back(color0);
				}
			}
			iff.exitChunk(TAG_DATA);

			iff.exitForm(TAG_0003);
			iff.exitForm(TAG_VTXA);

			//-- read INDX chunk (uint16 indices)
			if (hasIndices && iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_INDX)
			{
				iff.enterChunk(TAG_INDX);
				{
					const int32 indexCount = iff.read_int32();
					const int32 triangleCount = indexCount / 3;

					for (int32 t = 0; t < triangleCount; ++t)
					{
						MayaSceneBuilder::TriangleData tri;
						// Store local indices (into shader group's vertex/UV array) for correct UV assignment
						tri.indices[0] = baseVertexIndex + static_cast<int>(iff.read_uint16());
						tri.indices[1] = baseVertexIndex + static_cast<int>(iff.read_uint16());
						tri.indices[2] = baseVertexIndex + static_cast<int>(iff.read_uint16());
						sg.triangles.push_back(tri);
					}

					const int32 remainder = indexCount - (triangleCount * 3);
					for (int32 r = 0; r < remainder; ++r)
						iff.read_uint16();
				}
				iff.exitChunk(TAG_INDX);
			}

			//-- skip SIDX chunk if present
			if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_SIDX)
			{
				iff.enterChunk(TAG_SIDX);
				iff.exitChunk(TAG_SIDX);
			}

			//-- skip any remaining blocks in this primitive form
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

			iff.exitForm(TAG_0001);
		}

		shaderGroups.push_back(sg);

		iff.exitForm(shaderFormTag);
	}

	iff.exitForm(TAG_0001);
	iff.exitForm(TAG_SPS);
	iff.exitForm(TAG_0005);
	iff.exitForm(TAG_MESH);

	MESSENGER_LOG(("  Read %d shader groups, %d total vertices, %d hardpoints\n",
		static_cast<int>(shaderGroups.size()),
		static_cast<int>(allPositions.size()) / 3,
		static_cast<int>(hardpoints.size())));

	//-- derive mesh name from filename
	std::string meshName = meshFilename;
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
	status = MayaSceneBuilder::createMesh(allPositions, allNormals, shaderGroups, meshName, meshPath);
	MESSENGER_REJECT_STATUS(!status, ("failed to create mesh [%s]\n", meshName.c_str()));

	MESSENGER_LOG(("ImportStaticMesh: created mesh [%s]\n", meshName.c_str()));

	//-- assign materials from shader groups
	status = MayaSceneBuilder::assignMaterials(meshPath, shaderGroups, filename);
	if (status)
		MESSENGER_LOG(("  Assigned %d materials\n", static_cast<int>(shaderGroups.size())));
	else
		MESSENGER_LOG_WARNING(("  Failed to assign materials\n"));

	//-- create hardpoints as child transforms of the mesh
	if (!hardpoints.empty())
	{
		std::vector<MDagPath> emptyJointPaths;
		std::vector<std::string> emptyJointNames;
		status = MayaSceneBuilder::createHardpoints(hardpoints, emptyJointPaths, emptyJointNames);
		if (status)
			MESSENGER_LOG(("  Created %d hardpoints\n", static_cast<int>(hardpoints.size())));
	}

	//-- create collision group with floor if floor reference present
	if (!floorReferencePath.empty())
	{
		MObject meshTransformObj = meshPath.transform(&status);
		if (status)
		{
			MFnDagNode meshDagFn(meshTransformObj);
			MObject collisionObj;
			{
				MString cmd = "createNode transform -n \"collision\" -p \"";
				cmd += meshDagFn.fullPathName();
				cmd += "\"";
				MGlobal::executeCommand(cmd);
				MSelectionList sel;
				MGlobal::getActiveSelectionList(sel);
				if (sel.length() > 0)
				{
					sel.getDependNode(0, collisionObj);
				}
			}
			if (!collisionObj.isNull())
			{
				MFnDagNode collisionDagFn(collisionObj);
				MString cmd = "polyPlane -w 1 -h 1 -sx 1 -sy 1 -n \"floor0\" -p \"";
				cmd += collisionDagFn.fullPathName();
				cmd += "\"";
				status = MGlobal::executeCommand(cmd);
				if (status)
				{
					MSelectionList sel;
					MGlobal::getActiveSelectionList(sel);
					if (sel.length() > 0)
					{
						MObject floorObj;
						sel.getDependNode(0, floorObj);
						MDagPath floorPath;
						MDagPath::getAPathTo(floorObj, floorPath);
						floorPath.extendToShape();
						MObject floorShapeObj = floorPath.node();
						MFnDependencyNode shapeDepFn(floorShapeObj);
						cmd = "addAttr -ln external_reference -dt string ";
						cmd += shapeDepFn.name();
						MGlobal::executeCommand(cmd);
						cmd = "setAttr \"";
						cmd += shapeDepFn.name();
						cmd += ".external_reference\" -type \"string\" \"";
						cmd += floorReferencePath.c_str();
						cmd += "\"";
						MGlobal::executeCommand(cmd);
						MESSENGER_LOG(("  Created collision|floor0 with external_reference [%s]\n", floorReferencePath.c_str()));
					}
				}
			}
		}
	}

	//-- parent mesh under specified transform (e.g. for POB cell hierarchy)
	if (!parentName.empty())
	{
		std::string cmd = "parent \"";
		cmd += meshPath.fullPathName().asChar();
		cmd += "\" \"";
		cmd += parentName;
		cmd += "\"";
		status = MGlobal::executeCommand(MString(cmd.c_str()), true, true);
		if (status)
			MESSENGER_LOG(("  Parented mesh under [%s]\n", parentName.c_str()));
		else
			MESSENGER_LOG_WARNING(("  Failed to parent mesh under [%s]\n", parentName.c_str()));
	}

	MESSENGER_LOG(("ImportStaticMesh: done\n"));

	return MStatus(MStatus::kSuccess);
}

// ======================================================================
