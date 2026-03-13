// ======================================================================
//
// ImportPob.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportPob.h"

#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"
#include "sharedMath/IndexedTriangleList.h"
#include "sharedMath/Vector.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MFnTransform.h"
#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MSelectionList.h"
#include "maya/MStatus.h"

#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_PRTO = TAG(P,R,T,O);
	const Tag TAG_PRTS = TAG(P,R,T,S);
	const Tag TAG_PRTL = TAG(P,R,T,L);
	const Tag TAG_IDTL = TAG(I,D,T,L);
	const Tag TAG_CELS = TAG(C,E,L,S);
	const Tag TAG_CELL = TAG(C,E,L,L);
	const Tag TAG_CRC  = TAG3(C,R,C);
	const Tag TAG_PGRF = TAG(P,G,R,F);
	const Tag TAG_NULL = TAG(N,U,L,L);

	static int readPortalIndexFromPrtl(Iff &iff)
	{
		Tag chunkTag = iff.getCurrentName();
		iff.enterChunk(chunkTag);
		int portalIndex = -1;
		if (chunkTag == TAG_0005)
		{
			iff.read_bool8();
			iff.read_bool8();
			portalIndex = iff.read_int32();
			iff.read_bool8();
			iff.read_int32();
			iff.read_string();
			iff.read_bool8();
			for (int m = 0; m < 16; ++m)
				iff.read_float();
		}
		else if (chunkTag == TAG(0,0,0,4))
		{
			iff.read_bool8();
			portalIndex = iff.read_int32();
			iff.read_bool8();
			iff.read_int32();
			iff.read_string();
			iff.read_bool8();
			for (int m = 0; m < 16; ++m)
				iff.read_float();
		}
		else if (chunkTag == TAG(0,0,0,3))
		{
			iff.read_bool8();
			portalIndex = iff.read_int32();
			iff.read_bool8();
			iff.read_int32();
			iff.read_string();
		}
		else if (chunkTag == TAG(0,0,0,2))
		{
			iff.read_bool8();
			portalIndex = iff.read_int32();
			iff.read_bool8();
			iff.read_int32();
		}
		else if (chunkTag == TAG(0,0,0,1))
		{
			portalIndex = iff.read_int32();
			iff.read_bool8();
			iff.read_int32();
		}
		iff.exitChunk(chunkTag);
		return portalIndex;
	}

	static MStatus createPortalMesh(const IndexedTriangleList &itl, const char *portalName, MObject parentObj, MDagPath &outPath)
	{
		const std::vector<Vector> &verts = itl.getVertices();
		const std::vector<int> &indices = itl.getIndices();
		if (verts.empty() || indices.size() < 3)
			return MS::kFailure;

		std::vector<float> positions;
		positions.reserve(verts.size() * 3);
		for (size_t i = 0; i < verts.size(); ++i)
		{
			positions.push_back(-verts[i].x);
			positions.push_back(verts[i].y);
			positions.push_back(verts[i].z);
		}
		std::vector<float> normals(positions.size(), 0.0f);
		for (size_t i = 0; i < normals.size(); i += 3)
			normals[i + 1] = 1.0f;

		MayaSceneBuilder::ShaderGroupData sg;
		sg.shaderTemplateName = "shader/placeholder";
		for (size_t t = 0; t + 2 < indices.size(); t += 3)
		{
			MayaSceneBuilder::TriangleData tri;
			tri.indices[0] = indices[t];
			tri.indices[1] = indices[t + 1];
			tri.indices[2] = indices[t + 2];
			sg.triangles.push_back(tri);
		}

		std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
		MStatus st = MayaSceneBuilder::createMesh(positions, normals, groups, portalName, outPath);
		if (!st)
			return st;

		MObject shapeObj = outPath.node();
		MFnDependencyNode shapeDepFn(shapeObj);
		MString cmd = "addAttr -ln portal -at bool ";
		cmd += shapeDepFn.name();
		MGlobal::executeCommand(cmd);
		cmd = "setAttr \"";
		cmd += shapeDepFn.name();
		cmd += ".portal\" 1";
		MGlobal::executeCommand(cmd);

		MDagPath transformPath = outPath;
		transformPath.pop(1);
		MFnDagNode parentFn(parentObj);
		std::string parentCmd = "parent \"";
		parentCmd += transformPath.fullPathName().asChar();
		parentCmd += "\" \"";
		parentCmd += parentFn.fullPathName().asChar();
		parentCmd += "\"";
		MGlobal::executeCommand(MString(parentCmd.c_str()));

		return MS::kSuccess;
	}
}

// ======================================================================

static std::string resolveTreeFilePath(const std::string &treeFilePath, const std::string &inputFilename)
{
	if (treeFilePath.empty())
		return std::string();

	std::string normalizedInput = inputFilename;
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
		return treeFilePath;

	std::string path = treeFilePath;
	if (path.find("appearance/") != 0 && path.find("appearance\\") != 0)
		path = std::string("appearance/") + path;

	std::string resolved = baseDir + path;
	for (std::string::size_type i = 0; i < resolved.size(); ++i)
	{
		if (resolved[i] == '\\')
			resolved[i] = '/';
	}

	return resolved;
}

// ======================================================================

void ImportPob::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportPob::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportPob::creator()
{
	return new ImportPob;
}

// ======================================================================

ImportPob::ImportPob()
{
}

// ======================================================================

MStatus ImportPob::doIt(const MArgList &args)
{
	MESSENGER_INDENT;

	MStatus status;

	std::string filename;

	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get argument count\n"));

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
	}

	MESSENGER_REJECT_STATUS(filename.empty(), ("no filename specified, use -i <filename>\n"));

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportPob: opening [%s]\n", filename.c_str()));

	Iff iff(filename.c_str());

	Tag topForm = iff.getCurrentName();
	if (topForm != TAG_PRTO)
	{
		MESSENGER_LOG_ERROR(("ImportPob: expected FORM PRTO but found different top-level form in [%s]\n", filename.c_str()));
		return MS::kFailure;
	}

	iff.enterForm(TAG_PRTO);

	Tag versionTag = iff.getCurrentName();
	int version = -1;
	if (versionTag == TAG_0000) version = 0;
	else if (versionTag == TAG_0001) version = 1;
	else if (versionTag == TAG_0002) version = 2;
	else if (versionTag == TAG_0003) version = 3;
	else if (versionTag == TAG_0004) version = 4;

	MESSENGER_REJECT_STATUS(version < 0, ("ImportPob: unsupported PRTO version\n"));

	iff.enterForm(versionTag);

	int numberOfPortals = 0;
	int numberOfCells = 0;

	iff.enterChunk(TAG_DATA);
	{
		numberOfPortals = iff.read_int32();
		numberOfCells = iff.read_int32();
	}
	iff.exitChunk(TAG_DATA);

	MESSENGER_LOG(("ImportPob: %d portals, %d cells\n", numberOfPortals, numberOfCells));

	// Parse PRTS form (portal geometry)
	std::vector<IndexedTriangleList *> portalGeometries;
	portalGeometries.resize(static_cast<size_t>(numberOfPortals), 0);
	iff.enterForm(TAG_PRTS);
	{
		for (int i = 0; i < numberOfPortals; ++i)
		{
			if (version == 4)
			{
				if (iff.getCurrentName() == TAG_IDTL)
				{
					portalGeometries[static_cast<size_t>(i)] = new IndexedTriangleList(iff);
				}
			}
			else
			{
				iff.enterChunk(TAG_PRTL);
				const int numVerts = iff.read_int32();
				std::vector<Vector> verts(static_cast<size_t>(numVerts));
				for (int j = 0; j < numVerts; ++j)
					verts[j] = iff.read_floatVector();
				iff.exitChunk(TAG_PRTL);
				IndexedTriangleList *itl = new IndexedTriangleList();
				if (numVerts >= 3)
				{
					std::vector<int> indices;
					for (int j = 1; j + 1 < numVerts; ++j)
					{
						indices.push_back(0);
						indices.push_back(j);
						indices.push_back(j + 1);
					}
					itl->addIndexedTriangleList(&verts[0], numVerts, &indices[0], static_cast<int>(indices.size()));
				}
				portalGeometries[static_cast<size_t>(i)] = itl;
			}
		}
	}
	iff.exitForm(TAG_PRTS);

	// Derive root name from filename
	std::string rootName = filename;
	{
		const std::string::size_type lastSlash = rootName.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			rootName = rootName.substr(lastSlash + 1);
		const std::string::size_type dotPos = rootName.find_last_of('.');
		if (dotPos != std::string::npos)
			rootName = rootName.substr(0, dotPos);
	}

	// Create root group
	MFnTransform rootFn;
	MObject rootObj = rootFn.create(MObject::kNullObj, &status);
	MESSENGER_REJECT_STATUS(!status, ("failed to create POB root transform\n"));

	rootFn.setName(MString(rootName.c_str()));

	// Create cell transforms (r0, r1, r2...) under root for MayaHierarchy export
	std::vector<MObject> cellTransforms;
	cellTransforms.resize(static_cast<size_t>(numberOfCells));
	for (int i = 0; i < numberOfCells; ++i)
	{
		char cellName[32];
		sprintf(cellName, "r%d", i);
		MFnTransform cellFn;
		cellTransforms[static_cast<size_t>(i)] = cellFn.create(rootObj, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to create cell transform r%d\n", i));
		cellFn.setName(MString(cellName));
	}

	// Parse CELS and import each cell's appearance
	iff.enterForm(TAG_CELS);
	{
		for (int i = 0; i < numberOfCells; ++i)
		{
			iff.enterForm(TAG_CELL);

			Tag cellVersionTag = iff.getCurrentName();
			int cellVersion = (cellVersionTag == TAG_0001) ? 1 : (cellVersionTag == TAG_0002) ? 2 :
				(cellVersionTag == TAG_0003) ? 3 : (cellVersionTag == TAG_0004) ? 4 : (cellVersionTag == TAG_0005) ? 5 : 0;

			iff.enterForm(cellVersionTag);

			std::string cellName;
			std::string appearanceName;
			int32 cellPortalCount = 0;

			iff.enterChunk(TAG_DATA);
			{
				cellPortalCount = iff.read_int32();
				iff.read_bool8(); // canSeeWorldCell
				if (cellVersion >= 5)
					cellName = iff.read_string();
				else
				{
					char buf[32];
					sprintf(buf, "cell_%d", i);
					cellName = buf;
				}
				appearanceName = iff.read_string();

				if (cellVersion >= 2)
				{
					bool hasFloor = iff.read_bool8();
					if (hasFloor)
						iff.read_string(); // floorName
				}
			}
			iff.exitChunk(TAG_DATA);

			// Skip collision extent (NULL form or extent form) - only in 0004 and 0005
			if (cellVersion >= 4 && iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm())
			{
				iff.enterForm();
				iff.exitForm();
			}

			// Parse PRTL forms (one per portal) to get portal indices
			std::vector<int> cellPortalIndices;
			for (int32 p = 0; p < cellPortalCount && iff.getNumberOfBlocksLeft() > 0; ++p)
			{
				if (iff.isCurrentForm() && iff.getCurrentName() == TAG_PRTL)
				{
					iff.enterForm(TAG_PRTL);
					int portalIdx = readPortalIndexFromPrtl(iff);
					iff.exitForm(TAG_PRTL);
					if (portalIdx >= 0 && portalIdx < numberOfPortals)
						cellPortalIndices.push_back(portalIdx);
				}
				else if (iff.isCurrentForm())
				{
					iff.enterForm();
					iff.exitForm();
				}
			}

			// Skip LGHT chunk - in 0003, 0004, 0005
			if (cellVersion >= 3 && iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm())
			{
				iff.enterChunk();
				iff.exitChunk();
			}

			iff.exitForm(cellVersionTag);
			iff.exitForm(TAG_CELL);

			if (!appearanceName.empty())
			{
				std::string resolvedPath = resolveTreeFilePath(appearanceName, filename);

				// Use full path (root|r0) so parent resolves correctly
				char parentCellPathBuf[128];
				sprintf(parentCellPathBuf, "%s|r%d", rootName.c_str(), i);
				std::string parentCellPath = parentCellPathBuf;

				MESSENGER_LOG(("ImportPob: cell [%s] appearance [%s] -> [%s]\n",
					cellName.c_str(), appearanceName.c_str(), resolvedPath.c_str()));

				MString cmd = "importStaticMesh -i \"";
				cmd += resolvedPath.c_str();
				cmd += "\" -parent \"";
				cmd += parentCellPath.c_str();
				cmd += "\"";

				status = MGlobal::executeCommand(cmd, true, true);
				if (!status)
				{
					MESSENGER_LOG_WARNING(("ImportPob: failed to import cell appearance [%s]\n", resolvedPath.c_str()));
				}
			}

			// Create portals group and portal meshes for this cell
			if (!cellPortalIndices.empty())
			{
				MObject cellObj = cellTransforms[static_cast<size_t>(i)];
				MFnTransform cellFn(cellObj);
				MObject portalsObj;
				{
					MString cmd = "createNode transform -n \"portals\" -p \"";
					cmd += cellFn.fullPathName();
					cmd += "\"";
					MGlobal::executeCommand(cmd);
					MSelectionList sel;
					MGlobal::getActiveSelectionList(sel);
					if (sel.length() > 0)
						sel.getDependNode(0, portalsObj);
				}
				if (!portalsObj.isNull())
				{
					for (size_t p = 0; p < cellPortalIndices.size(); ++p)
					{
						int geomIdx = cellPortalIndices[p];
						if (geomIdx >= 0 && geomIdx < numberOfPortals && portalGeometries[static_cast<size_t>(geomIdx)])
						{
							char portalName[32];
							sprintf(portalName, "p%d", static_cast<int>(p));
							MDagPath meshPath;
							if (createPortalMesh(*portalGeometries[static_cast<size_t>(geomIdx)], portalName, portalsObj, meshPath) == MS::kSuccess)
								MESSENGER_LOG(("  Created portal %s in cell r%d\n", portalName, i));
						}
					}
				}
			}
		}
	}
	iff.exitForm(TAG_CELS);

	// Free portal geometries
	for (size_t g = 0; g < portalGeometries.size(); ++g)
	{
		if (portalGeometries[g])
			delete portalGeometries[g];
	}

	// Skip PGRF form if present
	if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_PGRF)
	{
		iff.enterForm(TAG_PGRF);
		iff.exitForm(TAG_PGRF);
	}

	// Skip CRC chunk
	if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_CRC)
	{
		iff.enterChunk(TAG_CRC);
		iff.exitChunk(TAG_CRC);
	}

	iff.exitForm(versionTag);
	iff.exitForm(TAG_PRTO);

	MESSENGER_LOG(("ImportPob: done\n"));

	return MS::kSuccess;
}

// ======================================================================
