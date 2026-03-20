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

#include "sharedCollision/BoxExtent.h"
#include "sharedCollision/CylinderExtent.h"
#include "sharedCollision/CompositeExtent.h"
#include "sharedCollision/ExtentList.h"
#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"
#include "sharedMath/Quaternion.h"
#include "sharedMath/Transform.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFn.h"
#include "maya/MFnDagNode.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MSelectionList.h"
#include "maya/MStatus.h"

#include <map>
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
	const Tag TAG_MLOD = TAG(M,L,O,D);
	const Tag TAG_SPS  = TAG3(S,P,S);
	const Tag TAG_CNT  = TAG3(C,N,T);
	const Tag TAG_VTXA = TAG(V,T,X,A);
	const Tag TAG_INDX = TAG(I,N,D,X);
	const Tag TAG_SIDX = TAG(S,I,D,X);
	const Tag TAG_APPR = TAG(A,P,P,R);
	const Tag TAG_HPTS = TAG(H,P,T,S);
	const Tag TAG_HPNT = TAG(H,P,N,T);
	const Tag TAG_FLOR = TAG(F,L,O,R);

	static void createExtentMayaNodes(Extent *extent, MObject collisionParent, int &extentIndex)
	{
		if (!extent)
			return;

		BoxExtent *boxExt = dynamic_cast<BoxExtent *>(extent);
		if (boxExt)
		{
			const Vector &minV = boxExt->getMin();
			const Vector &maxV = boxExt->getMax();
			float width = maxV.x - minV.x;
			float height = maxV.y - minV.y;
			float depth = maxV.z - minV.z;
			float cx = (minV.x + maxV.x) * 0.5f;
			float cy = (minV.y + maxV.y) * 0.5f;
			float cz = (minV.z + maxV.z) * 0.5f;
			if (width < 0.01f) width = 0.01f;
			if (height < 0.01f) height = 0.01f;
			if (depth < 0.01f) depth = 0.01f;

			char name[32];
			sprintf(name, "extent%d", extentIndex++);
			MFnDagNode collisionFn(collisionParent);
			MString cmd = "polyCube -w ";
			cmd += width;
			cmd += " -h ";
			cmd += height;
			cmd += " -d ";
			cmd += depth;
			cmd += " -n \"";
			cmd += name;
			cmd += "\"";
			MGlobal::executeCommand(cmd);
			MSelectionList sel;
			MGlobal::getActiveSelectionList(sel);
			if (sel.length() > 0)
			{
				MDagPath cubePath;
				sel.getDagPath(0, cubePath);
				if (!cubePath.hasFn(MFn::kTransform))
					cubePath.pop();
				cmd = "move ";
				cmd += (-cx);
				cmd += " ";
				cmd += cy;
				cmd += " ";
				cmd += cz;
				cmd += " \"";
				cmd += cubePath.fullPathName();
				cmd += "\"";
				MGlobal::executeCommand(cmd);
				MString parentCmd = "parent \"";
				parentCmd += cubePath.fullPathName();
				parentCmd += "\" \"";
				parentCmd += collisionFn.fullPathName();
				parentCmd += "\"";
				MGlobal::executeCommand(parentCmd);
			}
			return;
		}

		CylinderExtent *cylExt = dynamic_cast<CylinderExtent *>(extent);
		if (cylExt)
		{
			const Cylinder &cyl = cylExt->getCylinder();
			float radius = cyl.getRadius();
			float height = cyl.getHeight();
			const Vector &base = cyl.getBase();
			if (radius < 0.01f) radius = 0.01f;
			if (height < 0.01f) height = 0.01f;

			char name[32];
			sprintf(name, "extent%d", extentIndex++);
			MFnDagNode collisionFn(collisionParent);
			MString cmd = "polyCylinder -r ";
			cmd += radius;
			cmd += " -h ";
			cmd += height;
			cmd += " -sx 8 -n \"";
			cmd += name;
			cmd += "\"";
			MGlobal::executeCommand(cmd);
			MSelectionList sel;
			MGlobal::getActiveSelectionList(sel);
			if (sel.length() > 0)
			{
				MDagPath cylPath;
				sel.getDagPath(0, cylPath);
				if (!cylPath.hasFn(MFn::kTransform))
					cylPath.pop();
				float cx = base.x;
				float cy = base.y + 0.5f * height;
				float cz = base.z;
				cmd = "move ";
				cmd += (-cx);
				cmd += " ";
				cmd += cy;
				cmd += " ";
				cmd += cz;
				cmd += " \"";
				cmd += cylPath.fullPathName();
				cmd += "\"";
				MGlobal::executeCommand(cmd);
				MString parentCmd = "parent \"";
				parentCmd += cylPath.fullPathName();
				parentCmd += "\" \"";
				parentCmd += collisionFn.fullPathName();
				parentCmd += "\"";
				MGlobal::executeCommand(parentCmd);
			}
			return;
		}

		CompositeExtent *compExt = dynamic_cast<CompositeExtent *>(extent);
		if (compExt)
		{
			for (int i = 0; i < compExt->getExtentCount(); ++i)
			{
				BaseExtent *child = compExt->getExtent(i);
				Extent *childExt = dynamic_cast<Extent *>(child);
				if (childExt)
					createExtentMayaNodes(childExt, collisionParent, extentIndex);
			}
		}
	}
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
			MESSENGER_LOG(("  file is DTLA (detail LOD), extracting l0 (highest detail) mesh\n"));

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

				// Iterate CHLD entries and select id 0 (l0 = highest detail)
				std::string l0Path;
				std::string firstPath;  // fallback if no id=0
				while (probeIff2.getNumberOfBlocksLeft() > 0)
				{
					if (!probeIff2.isCurrentForm() && probeIff2.getCurrentName() == TAG_CHLD)
					{
						probeIff2.enterChunk(TAG_CHLD);
						{
							const int32 childId = probeIff2.read_int32();
							char *childName = probeIff2.read_string();
							std::string childPath = std::string("appearance/") + childName;
							delete [] childName;

							if (firstPath.empty())
								firstPath = childPath;
							if (childId == 0)
							{
								l0Path = childPath;
								MESSENGER_LOG(("  DTLA l0 (id=0) -> [%s]\n", l0Path.c_str()));
							}
						}
						probeIff2.exitChunk(TAG_CHLD);
					}
					else if (probeIff2.isCurrentForm())
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

				std::string chosenPath = !l0Path.empty() ? l0Path : firstPath;
				if (!chosenPath.empty())
				{
					meshFilename = resolveTreeFilePath(chosenPath, meshFilename);
					MESSENGER_LOG(("  resolved path -> [%s]\n", meshFilename.c_str()));
				}

				probeIff2.exitForm(TAG_DATA);
			}

			probeIff2.exitForm(versionTag);
			probeIff2.exitForm(TAG_DTLA);
		}
		else if (topForm == TAG_MLOD)
		{
			//-- LMG (MLOD) format: extract l0 (first/highest detail) mesh path only
			MESSENGER_LOG(("  file is LMG (MLOD), extracting l0 (highest detail) mesh\n"));

			probeIff2.enterForm(TAG_MLOD);
			probeIff2.enterForm(TAG_0000);

			probeIff2.enterChunk(TAG_INFO);
			const int16 detailLevelCount = static_cast<int16>(probeIff2.read_int16());
			probeIff2.exitChunk(TAG_INFO);

			UNREF(detailLevelCount);
			if (detailLevelCount > 0)
			{
				probeIff2.enterChunk(TAG_NAME);
				char *l0Name = probeIff2.read_string();
				std::string l0Path = l0Name;
				delete [] l0Name;
				probeIff2.exitChunk(TAG_NAME);

				if (l0Path.find("appearance/") != 0 && l0Path.find("appearance\\") != 0)
					l0Path = std::string("appearance/") + l0Path;
				meshFilename = resolveTreeFilePath(l0Path, meshFilename);
				MESSENGER_LOG(("  LMG l0 -> [%s]\n", meshFilename.c_str()));
			}

			probeIff2.exitForm(TAG_0000);
			probeIff2.exitForm(TAG_MLOD);
		}
	}

	// When path has _l1, _l2, _l3 etc., use _l0 (highest detail) instead
	// Only match when _l + digits is followed by _ or . (avoids false matches like "l0oft" in ply_all_garage_r2_l0oft_mesh_r1)
	{
		std::string::size_type lp = meshFilename.rfind("_l");
		if (lp != std::string::npos && lp + 2 < meshFilename.size())
		{
			std::string::size_type end = lp + 2;
			while (end < meshFilename.size() && meshFilename[end] >= '0' && meshFilename[end] <= '9')
				++end;
			// Must be followed by _ or . (or end) to be a LOD suffix, not e.g. "l0oft"
			char next = (end < meshFilename.size()) ? meshFilename[end] : '\0';
			if (next == '_' || next == '.' || next == '\0')
			{
				std::string lodSuffix = meshFilename.substr(lp + 2, end - (lp + 2));
				if (lodSuffix != "0")
				{
					meshFilename = meshFilename.substr(0, lp + 2) + "0" + meshFilename.substr(end);
					MESSENGER_LOG(("  Forcing highest LOD: resolved to [%s]\n", meshFilename.c_str()));
				}
			}
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

	//-- enter FORM MESH / FORM version (0002, 0003, 0004, or 0005)
	iff.enterForm(TAG_MESH);
	Tag meshVersionTag = iff.getCurrentName();
	if (meshVersionTag != TAG_0002 && meshVersionTag != TAG_0003 && meshVersionTag != TAG_0004 && meshVersionTag != TAG_0005)
	{
		MESSENGER_LOG_ERROR(("ImportStaticMesh: unsupported MESH version [%c%c%c%c] in [%s] (expected 0002-0005)\n",
			static_cast<char>((meshVersionTag >> 24) & 0xFF),
			static_cast<char>((meshVersionTag >> 16) & 0xFF),
			static_cast<char>((meshVersionTag >> 8) & 0xFF),
			static_cast<char>(meshVersionTag & 0xFF),
			meshFilename.c_str()));
		iff.exitForm(TAG_MESH);
		return MS::kFailure;
	}
	iff.enterForm(meshVersionTag);

	//-- read APPR form if present (0004/0005 only; 0002/0003 have SPS first, extents/hardpoints after)
	std::vector<MayaSceneBuilder::HardpointData> hardpoints;
	std::string floorReferencePath;
	Extent *meshExtent = 0;
	Extent *meshCollisionExtent = 0;

	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_APPR)
	{
		iff.enterForm(TAG_APPR);
		iff.enterForm(TAG_0003);

		// APPR 0003 order: extent, collision extent, hardpoints, floors
		if (iff.getNumberOfBlocksLeft() > 0)
			meshExtent = ExtentList::create(iff);
		if (iff.getNumberOfBlocksLeft() > 0)
			meshCollisionExtent = ExtentList::create(iff);

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

								// HPNT contains 3x4 transform (12 floats) + name string (matches insertChunkFloatTransform)
								const Transform hpTransform = iff.read_floatTransform();
								std::string hpName;
								iff.read_string(hpName);
								hp.name = hpName;
								hp.parentJoint = "";

								const Vector &pos = hpTransform.getPosition_p();
								hp.position[0] = pos.x;
								hp.position[1] = pos.y;
								hp.position[2] = pos.z;

								const Quaternion q(hpTransform);
								hp.rotation[0] = static_cast<float>(q.x);
								hp.rotation[1] = static_cast<float>(q.y);
								hp.rotation[2] = static_cast<float>(q.z);
								hp.rotation[3] = static_cast<float>(q.w);

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

	//-- enter FORM SPS / FORM 0000 or 0001 (0002/0003 use 0000, 0004/0005 use 0001)
	iff.enterForm(TAG_SPS);
	Tag spsVersionTag = iff.getCurrentName();
	if (spsVersionTag != TAG_0000 && spsVersionTag != TAG_0001)
	{
		MESSENGER_LOG_ERROR(("ImportStaticMesh: unsupported SPS version [%c%c%c%c] in [%s]\n",
			static_cast<char>((spsVersionTag >> 24) & 0xFF),
			static_cast<char>((spsVersionTag >> 16) & 0xFF),
			static_cast<char>((spsVersionTag >> 8) & 0xFF),
			static_cast<char>(spsVersionTag & 0xFF),
			meshFilename.c_str()));
		iff.exitForm(TAG_SPS);
		iff.exitForm(meshVersionTag);
		iff.exitForm(TAG_MESH);
		return MS::kFailure;
	}
	if (spsVersionTag == TAG_0000)
	{
		MESSENGER_LOG_ERROR(("ImportStaticMesh: SPS 0000 format (mesh version 0002/0003) is not supported for import. Re-export the mesh with the current exporter to create version 0005.\n"));
		iff.exitForm(TAG_SPS);
		iff.exitForm(meshVersionTag);
		iff.exitForm(TAG_MESH);
		return MS::kFailure;
	}
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

	//-- read each shader group (use enterForm() to accept any form tag, like game loader)
	for (int32 shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		iff.enterForm();

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

			//-- FORM VTXA / FORM 0001, 0002, or 0003
			iff.enterForm(TAG_VTXA);
			const Tag vtxaVersionTag = iff.getCurrentName();
			iff.enterForm();

			uint32 formatFlags = 0;
			int32  vertexCount = 0;
			bool   hasPosition = true;
			bool   hasTransform = false;
			bool   hasNormal = false;
			bool   hasPointSize = false;
			bool   hasColor0 = false;
			bool   hasColor1 = false;
			int    texCoordSetCount = 0;

			iff.enterChunk(TAG_INFO);
			if (vtxaVersionTag == TAG_0001)
			{
				// VTXA 0001: numberOfVertices, numberOfUVSets, flags (1=transformed, 2=normal, 4=color0)
				vertexCount = iff.read_int32();
				texCoordSetCount = iff.read_int32();
				const uint32 flags = iff.read_uint32();
				hasPosition = true;
				hasTransform = (flags & 0x01) != 0;
				hasNormal = (flags & 0x02) != 0;
				hasColor0 = (flags & 0x04) != 0;
			}
			else
			{
				// VTXA 0002/0003: formatFlags, vertexCount
				formatFlags = iff.read_uint32();
				vertexCount = iff.read_int32();
				hasPosition   = (formatFlags & 0x00000001) != 0;
				hasTransform  = (formatFlags & 0x00000002) != 0;
				hasNormal     = (formatFlags & 0x00000004) != 0;
				hasPointSize  = (formatFlags & 0x00000008) != 0;
				hasColor0     = (formatFlags & 0x00000010) != 0;
				hasColor1     = (formatFlags & 0x00000020) != 0;
				texCoordSetCount = static_cast<int>((formatFlags >> 8) & 0x0F);
			}
			iff.exitChunk(TAG_INFO);

			const int basePositionIndex = static_cast<int>(allPositions.size()) / 3;
			const int baseVertexIndex = static_cast<int>(sg.positionIndices.size());

			iff.enterChunk(TAG_DATA);
			{
				for (int32 v = 0; v < vertexCount; ++v)
				{
					float px = 0.0f, py = 0.0f, pz = 0.0f;
					float nx = 0.0f, ny = 0.0f, nz = 0.0f;
					float tu = 0.0f, tv = 0.0f;
					uint32 color0 = 0xFFFFFFFF;

					if (vtxaVersionTag == TAG_0001)
					{
						// VTXA 0001 vertex: position, ooz?, normal?, color0?, texcoords
						px = iff.read_float();
						py = iff.read_float();
						pz = iff.read_float();
						if (hasTransform)
							iff.read_float();
						if (hasNormal)
						{
							nx = iff.read_float();
							ny = iff.read_float();
							nz = iff.read_float();
						}
						if (hasColor0)
						{
							const float a = iff.read_float();
							const float r = iff.read_float();
							const float g = iff.read_float();
							const float b = iff.read_float();
							color0 = (static_cast<uint32>(a * 255) << 24) | (static_cast<uint32>(r * 255) << 16) | (static_cast<uint32>(g * 255) << 8) | static_cast<uint32>(b * 255);
						}
						for (int tc = 0; tc < texCoordSetCount; ++tc)
						{
							tu = iff.read_float();
							tv = iff.read_float();
						}
					}
					else
					{
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

			iff.exitForm();
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

		iff.exitForm();
	}

	iff.exitForm(TAG_0001);
	iff.exitForm(TAG_SPS);
	iff.exitForm(meshVersionTag);
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

	//-- create hardpoints as child transforms of the mesh (static mesh: parentJoint empty, parent to mesh)
	if (!hardpoints.empty())
	{
		std::map<std::string, MDagPath> emptyJointMap;
		MObject meshParentObj = meshPath.transform(&status);
		if (!status)
			meshParentObj = MObject::kNullObj;
		status = MayaSceneBuilder::createHardpoints(hardpoints, emptyJointMap, meshName, meshParentObj);
		if (status)
			MESSENGER_LOG(("  Created %d hardpoints\n", static_cast<int>(hardpoints.size())));
	}

	//-- create collision group with floor and/or extent from APPR
	if (!floorReferencePath.empty() || meshCollisionExtent)
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

				if (!floorReferencePath.empty())
				{
					MString cmd = "polyPlane -w 1 -h 1 -sx 1 -sy 1 -n \"floor0\"";
					status = MGlobal::executeCommand(cmd);
					if (status)
					{
						MSelectionList sel;
						MGlobal::getActiveSelectionList(sel);
						if (sel.length() > 0)
						{
							MDagPath floorPath;
							sel.getDagPath(0, floorPath);
							if (!floorPath.hasFn(MFn::kTransform))
								floorPath.pop();
							MDagPath shapePath = floorPath;
							shapePath.extendToShape();
							std::string shapeFullPath = shapePath.fullPathName().asChar();
							MString addCmd = "addAttr -ln external_reference -dt string \"";
							addCmd += shapeFullPath.c_str();
							addCmd += "\"";
							MGlobal::executeCommand(addCmd);
							MString setCmd = "setAttr \"";
							setCmd += shapeFullPath.c_str();
							setCmd += ".external_reference\" -type \"string\" \"";
							setCmd += floorReferencePath.c_str();
							setCmd += "\"";
							MGlobal::executeCommand(setCmd);
							MString parentCmd = "parent \"";
							parentCmd += floorPath.fullPathName();
							parentCmd += "\" \"";
							parentCmd += collisionDagFn.fullPathName();
							parentCmd += "\"";
							MGlobal::executeCommand(parentCmd);
							MESSENGER_LOG(("  Created collision|floor0 with external_reference [%s]\n", floorReferencePath.c_str()));
						}
					}
				}

				if (meshCollisionExtent)
				{
					int extentIdx = 0;
					createExtentMayaNodes(meshCollisionExtent, collisionObj, extentIdx);
					ExtentList::release(meshCollisionExtent);
					meshCollisionExtent = 0;
					MESSENGER_LOG(("  Created %d extent primitives in collision\n", extentIdx));
				}
			}
		}
	}
	if (meshExtent)
		ExtentList::release(meshExtent);
	if (meshCollisionExtent)
		ExtentList::release(meshCollisionExtent);

	//-- parent mesh under specified transform (e.g. for POB cell hierarchy)
	// meshPath points to the shape; parent command requires the transform
	if (!parentName.empty())
	{
		MDagPath transformPath = meshPath;
		transformPath.pop();  // shape -> transform
		std::string cmd = "parent \"";
		cmd += transformPath.fullPathName().asChar();
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
