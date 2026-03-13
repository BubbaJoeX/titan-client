// ======================================================================
//
// ImportSkeleton.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportSkeleton.h"

#include "MayaSceneBuilder.h"
#include "Messenger.h"
#include "PluginMain.h"
#include "SetDirectoryCommand.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFnDagNode.h"
#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MSelectionList.h"

#include <string>
#include <vector>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_SKTM = TAG(S,K,T,M);
	const Tag TAG_SLOD = TAG(S,L,O,D);
	const Tag TAG_PRNT = TAG(P,R,N,T);
	const Tag TAG_RPRE = TAG(R,P,R,E);
	const Tag TAG_RPST = TAG(R,P,S,T);
	const Tag TAG_BPTR = TAG(B,P,T,R);
	const Tag TAG_BPRO = TAG(B,P,R,O);
	const Tag TAG_JROR = TAG(J,R,O,R);

	const Tag TAG_V000 = TAG(0,0,0,0);
	const Tag TAG_V001 = TAG(0,0,0,1);
	const Tag TAG_V002 = TAG(0,0,0,2);

	// ------------------------------------------------------------------

	bool readSingleSkeleton(Iff &iff, std::vector<MayaSceneBuilder::JointData> &joints)
	{
		iff.enterForm(TAG_SKTM);

		const Tag versionTag = iff.getCurrentName();
		const bool isV1 = (versionTag == TAG_V001);
		const bool isV2 = (versionTag == TAG_V002);

		if (!isV1 && !isV2)
		{
			MESSENGER_LOG_ERROR(("unsupported SKTM version\n"));
			return false;
		}

		iff.enterForm(versionTag);

		// INFO
		iff.enterChunk(TAG_INFO);
		const int32 jointCount = iff.read_int32();
		iff.exitChunk(TAG_INFO);

		joints.resize(static_cast<size_t>(jointCount));

		// NAME
		iff.enterChunk(TAG_NAME);
		for (int i = 0; i < jointCount; ++i)
		{
			char *s = iff.read_string();
			joints[static_cast<size_t>(i)].name = s;
			delete [] s;
		}
		iff.exitChunk(TAG_NAME);

		// PRNT
		iff.enterChunk(TAG_PRNT);
		for (int i = 0; i < jointCount; ++i)
			joints[static_cast<size_t>(i)].parentIndex = iff.read_int32();
		iff.exitChunk(TAG_PRNT);

		// RPRE (pre-multiply rotation) - stored as w,x,y,z in IFF
		iff.enterChunk(TAG_RPRE);
		for (int i = 0; i < jointCount; ++i)
		{
			MayaSceneBuilder::JointData &jd = joints[static_cast<size_t>(i)];
			const float w = iff.read_float();
			const float x = iff.read_float();
			const float y = iff.read_float();
			const float z = iff.read_float();
			jd.preRotation[0] = x;
			jd.preRotation[1] = y;
			jd.preRotation[2] = z;
			jd.preRotation[3] = w;
		}
		iff.exitChunk(TAG_RPRE);

		// RPST (post-multiply rotation) - stored as w,x,y,z in IFF
		iff.enterChunk(TAG_RPST);
		for (int i = 0; i < jointCount; ++i)
		{
			MayaSceneBuilder::JointData &jd = joints[static_cast<size_t>(i)];
			const float w = iff.read_float();
			const float x = iff.read_float();
			const float y = iff.read_float();
			const float z = iff.read_float();
			jd.postRotation[0] = x;
			jd.postRotation[1] = y;
			jd.postRotation[2] = z;
			jd.postRotation[3] = w;
		}
		iff.exitChunk(TAG_RPST);

		// BPTR (bind-pose translation) - stored as x,y,z in IFF
		iff.enterChunk(TAG_BPTR);
		for (int i = 0; i < jointCount; ++i)
		{
			MayaSceneBuilder::JointData &jd = joints[static_cast<size_t>(i)];
			jd.bindTranslation[0] = iff.read_float();
			jd.bindTranslation[1] = iff.read_float();
			jd.bindTranslation[2] = iff.read_float();
		}
		iff.exitChunk(TAG_BPTR);

		// BPRO (bind-pose rotation) - stored as w,x,y,z in IFF
		iff.enterChunk(TAG_BPRO);
		for (int i = 0; i < jointCount; ++i)
		{
			MayaSceneBuilder::JointData &jd = joints[static_cast<size_t>(i)];
			const float w = iff.read_float();
			const float x = iff.read_float();
			const float y = iff.read_float();
			const float z = iff.read_float();
			jd.bindRotation[0] = x;
			jd.bindRotation[1] = y;
			jd.bindRotation[2] = z;
			jd.bindRotation[3] = w;
		}
		iff.exitChunk(TAG_BPRO);

		// Version-specific chunks
		if (isV1)
		{
			// v0001 may have an extra BPMJ chunk
			if (!iff.atEndOfForm())
			{
				iff.enterChunk();
				iff.exitChunk();
			}
			for (int i = 0; i < jointCount; ++i)
				joints[static_cast<size_t>(i)].rotationOrder = 0;
		}
		else
		{
			// v0002 has JROR chunk
			iff.enterChunk(TAG_JROR);
			for (int i = 0; i < jointCount; ++i)
				joints[static_cast<size_t>(i)].rotationOrder = static_cast<int>(iff.read_uint32());
			iff.exitChunk(TAG_JROR);
		}

		iff.exitForm(versionTag);
		iff.exitForm(TAG_SKTM);

		return true;
	}
}

// ======================================================================

void ImportSkeleton::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportSkeleton::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportSkeleton::creator()
{
	return new ImportSkeleton();
}

// ======================================================================

ImportSkeleton::ImportSkeleton()
: MPxCommand()
{
}

// ======================================================================

MStatus ImportSkeleton::doIt(const MArgList &args)
{
	MStatus status;

	MESSENGER_LOG(("ImportSkeleton: begin\n"));

	// -- parse arguments: expect -i <filename>
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
	MESSENGER_LOG(("ImportSkeleton: opening file [%s]\n", filename.c_str()));

	// -- open the IFF
	Iff iff(filename.c_str());

	// Determine top-level form: SKTM (single) or SLOD (LOD)
	const Tag topTag = iff.getCurrentName();

	if (topTag == TAG_SKTM)
	{
		// Single skeleton
		std::vector<MayaSceneBuilder::JointData> joints;
		if (!readSingleSkeleton(iff, joints))
			return MS::kFailure;

		std::string rootName = filename;
		{
			const std::string::size_type lastSlash = rootName.find_last_of("\\/");
			if (lastSlash != std::string::npos)
				rootName = rootName.substr(lastSlash + 1);
			const std::string::size_type dot = rootName.find_last_of('.');
			if (dot != std::string::npos)
				rootName = rootName.substr(0, dot);
		}

		MESSENGER_LOG(("ImportSkeleton: creating joint hierarchy [%s] with %d joints\n", rootName.c_str(), static_cast<int>(joints.size())));

		std::vector<MDagPath> outJointPaths;
		status = MayaSceneBuilder::createJointHierarchy(joints, rootName, outJointPaths);
		MESSENGER_REJECT_STATUS(!status, ("failed to create joint hierarchy\n"));

		MESSENGER_LOG(("ImportSkeleton: complete, created %u joints\n", static_cast<unsigned>(outJointPaths.size())));
	}
	else if (topTag == TAG_SLOD)
	{
		// LOD skeleton: FORM SLOD -> FORM 0000 -> INFO (int16 levelCount) + N x FORM SKTM
		iff.enterForm(TAG_SLOD);
		iff.enterForm(TAG_V000);

		iff.enterChunk(TAG_INFO);
		const int16 levelCount = iff.read_int16();
		iff.exitChunk(TAG_INFO);

		MESSENGER_LOG(("ImportSkeleton: LOD skeleton with %d levels\n", static_cast<int>(levelCount)));

		std::string baseName = filename;
		{
			const std::string::size_type lastSlash = baseName.find_last_of("\\/");
			if (lastSlash != std::string::npos)
				baseName = baseName.substr(lastSlash + 1);
			const std::string::size_type dot = baseName.find_last_of('.');
			if (dot != std::string::npos)
				baseName = baseName.substr(0, dot);
		}

		// Create LOD group structure for re-export: lodGroup -> l0, l1, l2... (each with skeleton)
		MObject lodGroupParent;
		{
			MString melCmd = "group -em -n \"";
			melCmd += baseName.c_str();
			melCmd += "\"";
			MStringArray result;
			status = MGlobal::executeCommand(melCmd, result);
			MESSENGER_REJECT_STATUS(!status, ("failed to create LOD group\n"));

			// Add lodGroup shape so exporter recognizes it as kLodGroup
			melCmd = "createNode lodGroup -p \"";
			melCmd += result[0];
			melCmd += "\"";
			IGNORE_RETURN(MGlobal::executeCommand(melCmd));

			MSelectionList sel;
			sel.add(result[0]);
			MObject lodGroupObj;
			sel.getDependNode(0, lodGroupObj);
			lodGroupParent = lodGroupObj;
		}

		for (int16 level = 0; level < levelCount; ++level)
		{
			std::vector<MayaSceneBuilder::JointData> joints;
			if (!readSingleSkeleton(iff, joints))
				return MS::kFailure;

			// Create l0, l1, l2... transform under LOD group
			char lodLevelName[32];
			sprintf(lodLevelName, "l%d", static_cast<int>(level));
			MString melCmd = "createNode transform -n \"";
			melCmd += lodLevelName;
			melCmd += "\" -p \"";
			melCmd += baseName.c_str();
			melCmd += "\"";
			MStringArray lodResult;
			status = MGlobal::executeCommand(melCmd, lodResult);
			MESSENGER_REJECT_STATUS(!status, ("failed to create LOD level %d transform\n", static_cast<int>(level)));

			MSelectionList lodSel;
			lodSel.add(lodResult[0]);
			MObject lodLevelObj;
			lodSel.getDependNode(0, lodLevelObj);

			char levelSuffix[32];
			sprintf(levelSuffix, "_l%d", static_cast<int>(level));
			std::string rootName = baseName + levelSuffix;

			MESSENGER_LOG(("ImportSkeleton: creating LOD level %d [%s] with %d joints\n",
				static_cast<int>(level), rootName.c_str(), static_cast<int>(joints.size())));

			std::vector<MDagPath> outJointPaths;
			status = MayaSceneBuilder::createJointHierarchy(joints, rootName, outJointPaths, lodLevelObj);
			MESSENGER_REJECT_STATUS(!status, ("failed to create joint hierarchy for LOD level %d\n", static_cast<int>(level)));
		}

		iff.exitForm(TAG_V000);
		iff.exitForm(TAG_SLOD);

		MESSENGER_LOG(("ImportSkeleton: LOD skeleton import complete\n"));
	}
	else
	{
		MESSENGER_LOG_ERROR(("ImportSkeleton: unrecognized top-level form, expected SKTM or SLOD\n"));
		return MS::kFailure;
	}

	return MS::kSuccess;
}

// ======================================================================
