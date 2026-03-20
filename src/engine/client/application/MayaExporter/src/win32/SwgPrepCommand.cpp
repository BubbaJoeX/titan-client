// ======================================================================
//
// SwgPrepCommand.cpp
// Prepares Maya hierarchy for SWG asset export (POB, MSH, MGN).
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "SwgPrepCommand.h"

#include "Messenger.h"

#include "maya/MArgList.h"
#include "maya/MDagPath.h"
#include "maya/MFnDagNode.h"
#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MSelectionList.h"
#include "maya/MStatus.h"

#include <cstdio>

// ======================================================================

namespace
{
	Messenger *messenger;
	static const char *const ROOT_NAME = "swg_prep";
}

// ======================================================================

void SwgPrepCommand::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void SwgPrepCommand::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

SwgPrepCommand::SwgPrepCommand()
{
}

// ----------------------------------------------------------------------

void *SwgPrepCommand::creator()
{
	return new SwgPrepCommand();
}

// ----------------------------------------------------------------------

MStatus SwgPrepCommand::doIt(const MArgList &args)
{
	MStatus status;

	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get args length\n"));

	std::string format;

	for (unsigned argIndex = 0; argIndex < argCount; ++argIndex)
	{
		MString argName = args.asString(argIndex, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get arg [%u] as string\n", argIndex));

		argName.toLowerCase();

		if (argName == "-f" || argName == "-format")
		{
			MESSENGER_REJECT_STATUS(argIndex + 1 >= argCount, ("-f/-format requires a value (pob, msh, mgn)\n"));

			MString formatStr = args.asString(argIndex + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get format value\n"));

			formatStr.toLowerCase();
			format = formatStr.asChar();
			++argIndex;
		}
		else
		{
			MESSENGER_LOG_ERROR(("unknown argument [%s]\n", argName.asChar()));
			return MS::kFailure;
		}
	}

	MESSENGER_REJECT_STATUS(format.empty(), ("swgprep requires -f pob, -f msh, or -f mgn\n"));

	if (format == "pob")
		return prepPob();
	if (format == "msh")
		return prepMsh();
	if (format == "mgn")
		return prepMgn();

	MESSENGER_LOG_ERROR(("invalid format [%s]; use pob, msh, or mgn\n", format.c_str()));
	return MS::kFailure;
}

// ----------------------------------------------------------------------

MStatus SwgPrepCommand::prepPob()
{
	MStatus status;

	// root -> r0 (cell) -> mesh, portals, collision
	MString melCmd = "group -em -n \"";
	melCmd += ROOT_NAME;
	melCmd += "\"";
	MStringArray result;
	status = MGlobal::executeCommand(melCmd, result);
	MESSENGER_REJECT_STATUS(!status, ("failed to create POB root group\n"));

	MString rootPath = result[0];

	// Create r0 cell
	{
		MString cmd = "createNode transform -n \"r0\" -p \"";
		cmd += rootPath;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}

	// Get r0 path
	MString r0Path = rootPath + "|r0";

	// Create mesh, portals, collision under r0
	{
		MString cmd = "createNode transform -n \"mesh\" -p \"";
		cmd += r0Path;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}
	{
		MString cmd = "createNode transform -n \"portals\" -p \"";
		cmd += r0Path;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}
	{
		MString cmd = "createNode transform -n \"collision\" -p \"";
		cmd += r0Path;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}

	MESSENGER_LOG(("swgprep: created POB hierarchy %s -> r0 -> mesh, portals, collision\n", rootPath.asChar()));
	return MS::kSuccess;
}

// ----------------------------------------------------------------------

MStatus SwgPrepCommand::prepMsh()
{
	MStatus status;

	// root -> mesh (transform) -> collision (child of mesh)
	MString melCmd = "group -em -n \"";
	melCmd += ROOT_NAME;
	melCmd += "\"";
	MStringArray result;
	status = MGlobal::executeCommand(melCmd, result);
	MESSENGER_REJECT_STATUS(!status, ("failed to create MSH root group\n"));

	MString rootPath = result[0];

	// Create mesh transform
	{
		MString cmd = "createNode transform -n \"mesh\" -p \"";
		cmd += rootPath;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}

	MString meshPath = rootPath + "|mesh";

	// Create collision under mesh
	{
		MString cmd = "createNode transform -n \"collision\" -p \"";
		cmd += meshPath;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}

	MESSENGER_LOG(("swgprep: created MSH hierarchy %s -> mesh -> collision\n", rootPath.asChar()));
	return MS::kSuccess;
}

// ----------------------------------------------------------------------

MStatus SwgPrepCommand::prepMgn()
{
	MStatus status;

	// root -> lodGroup (with lodGroup shape) -> l0, l1, l2
	MString melCmd = "group -em -n \"";
	melCmd += ROOT_NAME;
	melCmd += "\"";
	MStringArray result;
	status = MGlobal::executeCommand(melCmd, result);
	MESSENGER_REJECT_STATUS(!status, ("failed to create MGN root group\n"));

	MString rootPath = result[0];

	// Create lodGroup transform with lodGroup shape
	melCmd = "createNode transform -n \"lodGroup\" -p \"";
	melCmd += rootPath;
	melCmd += "\"";
	IGNORE_RETURN(MGlobal::executeCommand(melCmd));

	MString lodGroupPath = rootPath + "|lodGroup";

	melCmd = "createNode lodGroup -p \"";
	melCmd += lodGroupPath;
	melCmd += "\"";
	IGNORE_RETURN(MGlobal::executeCommand(melCmd));

	// Create l0, l1, l2
	for (int i = 0; i < 3; ++i)
	{
		char lodName[16];
		sprintf(lodName, "l%d", i);
		MString cmd = "createNode transform -n \"";
		cmd += lodName;
		cmd += "\" -p \"";
		cmd += lodGroupPath;
		cmd += "\"";
		IGNORE_RETURN(MGlobal::executeCommand(cmd));
	}

	MESSENGER_LOG(("swgprep: created MGN hierarchy %s -> lodGroup -> l0, l1, l2\n", rootPath.asChar()));
	return MS::kSuccess;
}

// ======================================================================
