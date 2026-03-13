// ======================================================================
//
// ImportLodMesh.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportLodMesh.h"

#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"
#include "sharedMath/IndexedTriangleList.h"
#include "sharedMath/Vector.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MGlobal.h"
#include "maya/MSelectionList.h"

#include <string>
#include <vector>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_MLOD = TAG(M,L,O,D);
	const Tag TAG_DTLA = TAG(D,T,L,A);
	const Tag TAG_APPR = TAG(A,P,P,R);
	const Tag TAG_PIVT = TAG(P,I,V,T);
	const Tag TAG_CHLD = TAG(C,H,L,D);
	const Tag TAG_RADR = TAG(R,A,D,R);
	const Tag TAG_TEST = TAG(T,E,S,T);
	const Tag TAG_WRIT = TAG(W,R,I,T);

	static void skipForm(Iff &iff)
	{
		iff.enterForm();
		while (iff.getNumberOfBlocksLeft() > 0)
		{
			if (iff.isCurrentForm())
				skipForm(iff);
			else
			{
				iff.enterChunk();
				iff.exitChunk();
			}
		}
		iff.exitForm();
	}
}

// ======================================================================

void ImportLodMesh::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportLodMesh::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportLodMesh::creator()
{
	return new ImportLodMesh();
}

// ======================================================================

ImportLodMesh::ImportLodMesh()
{
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

MStatus ImportLodMesh::doIt(const MArgList &args)
{
	MStatus status;

	MESSENGER_LOG(("ImportLodMesh: begin\n"));

	//-- parse arguments: expect -i <filename>
	std::string filename;
	{
		const unsigned argCount = args.length(&status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get arg count\n"));

		bool haveFilename = false;

		for (unsigned i = 0; i < argCount; ++i)
		{
			MString argName = args.asString(i, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get arg [%u]\n", i));

			if (argName == "-i")
			{
				MESSENGER_REJECT_STATUS(haveFilename, ("-i specified multiple times\n"));

				MString argValue = args.asString(i + 1, &status);
				MESSENGER_REJECT_STATUS(!status, ("failed to get filename after -i\n"));

				filename = argValue.asChar();
				++i;
				haveFilename = true;
			}
			else
			{
				MESSENGER_LOG_ERROR(("unknown argument [%s]\n", argName.asChar()));
				return MS::kFailure;
			}
		}

		MESSENGER_REJECT_STATUS(!haveFilename, ("missing required -i <filename> argument\n"));
	}

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportLodMesh: opening file [%s]\n", filename.c_str()));

	//-- open the IFF and detect format
	Iff iff(filename.c_str());
	Tag topForm = iff.getCurrentName();

	std::vector<std::string> detailLevelPaths;
	IndexedTriangleList *radarShape = 0;
	IndexedTriangleList *testShape = 0;

	if (topForm == TAG_DTLA)
	{
		//-- DTLA format: full detail appearance with radar/test shapes
		iff.enterForm(TAG_DTLA);
		Tag versionTag = iff.getCurrentName();
		iff.enterForm(versionTag);

		while (iff.getNumberOfBlocksLeft() > 0)
		{
			if (iff.isCurrentForm())
			{
				Tag formTag = iff.getCurrentName();
				if (formTag == TAG_APPR)
				{
					iff.enterForm(TAG_APPR);
					while (iff.getNumberOfBlocksLeft() > 0)
					{
						if (iff.isCurrentForm())
							skipForm(iff);
						else
						{
							iff.enterChunk();
							iff.exitChunk();
						}
					}
					iff.exitForm(TAG_APPR);
				}
				else if (formTag == TAG_RADR)
				{
					iff.enterForm(TAG_RADR);
					if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_INFO)
					{
						iff.enterChunk(TAG_INFO);
						int hasRadar = iff.read_int32();
						iff.exitChunk(TAG_INFO);
						if (hasRadar && iff.getNumberOfBlocksLeft() > 0)
						{
							radarShape = new IndexedTriangleList(iff);
						}
					}
					iff.exitForm(TAG_RADR);
				}
				else if (formTag == TAG_TEST)
				{
					iff.enterForm(TAG_TEST);
					if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_INFO)
					{
						iff.enterChunk(TAG_INFO);
						int hasTest = iff.read_int32();
						iff.exitChunk(TAG_INFO);
						if (hasTest && iff.getNumberOfBlocksLeft() > 0)
						{
							testShape = new IndexedTriangleList(iff);
						}
					}
					iff.exitForm(TAG_TEST);
				}
				else if (formTag == TAG_DATA)
				{
					iff.enterForm(TAG_DATA);
					while (iff.getNumberOfBlocksLeft() > 0)
					{
						if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_CHLD)
						{
							iff.enterChunk(TAG_CHLD);
							iff.read_int32();
							char *childName = iff.read_string();
							detailLevelPaths.push_back(childName);
							delete [] childName;
							iff.exitChunk(TAG_CHLD);
						}
						else if (iff.isCurrentForm())
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
					iff.exitForm(TAG_DATA);
				}
				else
				{
					skipForm(iff);
				}
			}
			else
			{
				iff.enterChunk();
				iff.exitChunk();
			}
		}

		iff.exitForm(versionTag);
		iff.exitForm(TAG_DTLA);
		MESSENGER_LOG(("ImportLodMesh: DTLA format, %d LOD levels, radar=%s test=%s\n",
			static_cast<int>(detailLevelPaths.size()),
			radarShape ? "yes" : "no", testShape ? "yes" : "no"));
	}
	else if (topForm == TAG_MLOD)
	{
		//-- MLOD format: simple list of LOD paths
		iff.enterForm(TAG_MLOD);
		iff.enterForm(TAG_0000);

		int16 detailLevelCount = 0;
		iff.enterChunk(TAG_INFO);
		detailLevelCount = iff.read_int16();
		iff.exitChunk(TAG_INFO);

		for (int16 i = 0; i < detailLevelCount; ++i)
		{
			iff.enterChunk(TAG_NAME);
			char *s = iff.read_string();
			detailLevelPaths.push_back(s);
			delete [] s;
			iff.exitChunk(TAG_NAME);
		}

		iff.exitForm(TAG_0000);
		iff.exitForm(TAG_MLOD);
		MESSENGER_LOG(("ImportLodMesh: MLOD format, %d detail levels\n", static_cast<int>(detailLevelPaths.size())));
	}
	else
	{
		MESSENGER_LOG_ERROR(("ImportLodMesh: unknown format (expected MLOD or DTLA)\n"));
		return MS::kFailure;
	}

	//-- Create LOD group structure for re-export: lodGroup -> l0, l1, l2... (each with mesh)
	std::string baseName = filename;
	{
		const std::string::size_type lastSlash = baseName.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			baseName = baseName.substr(lastSlash + 1);
		const std::string::size_type dot = baseName.find_last_of('.');
		if (dot != std::string::npos)
			baseName = baseName.substr(0, dot);
	}

	if (!detailLevelPaths.empty())
	{
		// Create LOD group with lodGroup shape for exporter recognition
		MString melCmd = "group -em -n \"";
		melCmd += baseName.c_str();
		melCmd += "\"";
		MStringArray lodGroupResult;
		status = MGlobal::executeCommand(melCmd, lodGroupResult);
		MESSENGER_REJECT_STATUS(!status, ("failed to create LOD group\n"));

		melCmd = "createNode lodGroup -p \"";
		melCmd += lodGroupResult[0];
		melCmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(melCmd));

		// Import each LOD level and parent under l0, l1, l2...
		for (int16 i = 0; i < static_cast<int16>(detailLevelPaths.size()) && i < 10; ++i)
		{
			const std::string &relativePath = detailLevelPaths[static_cast<size_t>(i)];
			const std::string resolvedPath = resolveTreeFilePath(relativePath, filename);

			MESSENGER_LOG(("ImportLodMesh: importing LOD %d -> [%s] (resolved: [%s])\n",
				static_cast<int>(i), relativePath.c_str(), resolvedPath.c_str()));

			// Create l0, l1, l2... transform
			char lodLevelName[32];
			sprintf(lodLevelName, "l%d", static_cast<int>(i));
			melCmd = "createNode transform -n \"";
			melCmd += lodLevelName;
			melCmd += "\" -p \"";
			melCmd += lodGroupResult[0];
			melCmd += "\"";
			IGNORE_RETURN(MGlobal::executeCommand(melCmd));

			// Import mesh
			MString importCmd = "importSkeletalMesh -i \"";
			importCmd += resolvedPath.c_str();
			importCmd += "\"";
			status = MGlobal::executeCommand(importCmd, true, true);
			if (!status)
			{
				MESSENGER_LOG_WARNING(("ImportLodMesh: failed to import LOD [%s]\n", resolvedPath.c_str()));
				continue;
			}

			// Parent the imported mesh under lX (mesh name from path)
			std::string meshName = relativePath;
			{
				const std::string::size_type lastSlash = meshName.find_last_of("/\\");
				if (lastSlash != std::string::npos)
					meshName = meshName.substr(lastSlash + 1);
				const std::string::size_type dotPos = meshName.find_last_of('.');
				if (dotPos != std::string::npos)
					meshName = meshName.substr(0, dotPos);
			}
			melCmd = "parent \"";
			melCmd += meshName.c_str();
			melCmd += "\" \"";
			melCmd += lodLevelName;
			melCmd += "\"";
			IGNORE_RETURN(MGlobal::executeCommand(melCmd));
		}

		//-- create radar and test meshes under l0 if present (DTLA format)
		if ((radarShape || testShape) && !detailLevelPaths.empty())
		{
			std::string l0PathStr = std::string(lodGroupResult[0].asChar()) + "|l0";
			MSelectionList sel;
			MGlobal::getSelectionListByName(MString(l0PathStr.c_str()), sel);
			if (sel.length() > 0)
			{
				MDagPath l0DagPath;
				sel.getDagPath(0, l0DagPath);
				MObject l0Obj = l0DagPath.node();

				auto createShapeMesh = [&](IndexedTriangleList *shape, const char *name) -> bool
				{
					if (!shape) return false;
					const std::vector<Vector> &verts = shape->getVertices();
					const std::vector<int> &indices = shape->getIndices();
					if (verts.empty() || indices.size() < 3) return false;

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
					MDagPath meshPath;
					MStatus st = MayaSceneBuilder::createMesh(positions, normals, groups, name, meshPath);
					if (!st) return false;

					std::string parentCmd = "parent \"";
					parentCmd += meshPath.fullPathName().asChar();
					parentCmd += "\" \"";
					parentCmd += l0PathStr;
					parentCmd += "\"";
					IGNORE_RETURN(MGlobal::executeCommand(MString(parentCmd.c_str())));
					return true;
				};

				if (radarShape)
				{
					if (createShapeMesh(radarShape, "radar"))
						MESSENGER_LOG(("  Created radar shape under l0\n"));
					delete radarShape;
					radarShape = 0;
				}
				if (testShape)
				{
					if (createShapeMesh(testShape, "test"))
						MESSENGER_LOG(("  Created test shape under l0\n"));
					delete testShape;
					testShape = 0;
				}
			}
		}
		if (radarShape) delete radarShape;
		if (testShape) delete testShape;
	}

	MESSENGER_LOG(("ImportLodMesh: done, imported %d detail levels\n", static_cast<int>(detailLevelPaths.size())));

	return MS::kSuccess;
}

// ======================================================================
