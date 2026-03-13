// ======================================================================
//
// ExportStaticMesh.cpp
// Todd Fiala
//
// copyright 1999, Bootprint Entertainment
//
// ======================================================================

//precompiled header includes
#include "FirstMayaExporter.h"

//module includes
#include "ExportStaticMesh.h"

//engine shared includes
#include "sharedFile/Iff.h"
#include "sharedDebug/PerformanceTimer.h"

//local MayaExporter includes
#if !NO_DATABASE
#include "DatabaseImporter.h"
#endif
#include "ExportArgs.h"
#include "ExporterLog.h"
#include "ExportManager.h"
#include "MayaHierarchy.h"
#include "MayaUtility.h"
#include "MayaMeshReader.h"
#include "Messenger.h"
#include "PluginMain.h"
#include "SetDirectoryCommand.h"
#include "StaticMeshBuilder.h"

//maya includes
#include "maya/MAnimControl.h"
#include "maya/MArgList.h"
#include "maya/MCommandResult.h"
#include "maya/MFileIo.h"
#include "maya/MGlobal.h"
#include "maya/MIntArray.h"
#include "maya/MSelectionList.h"
#include "maya/MTime.h"

// STL/system includes
#include <stdio.h>
#include <stdarg.h>
#include <map>
#include <sstream>

// ======================================================================

static Messenger *messenger;

// ======================================================================

namespace ExportStaticMeshNamespace
{
	const int c_seondsPerMinute = 60;
	const int c_secondsPerHour = 3600;
}

using namespace ExportStaticMeshNamespace;

// ======================================================================

void *ExportStaticMesh::creator ()
{
	return new ExportStaticMesh;
}

// ----------------------------------------------------------------------

void ExportStaticMesh::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ExportStaticMesh::remove(void)
{
	messenger = 0;
}

// ----------------------------------------------------------------------

bool ExportStaticMesh::processArguments(
	const MArgList &args,
	MDagPath *targetDagPath,
	bool &interactive,
	bool &lock,
	bool &unlock,
	bool &showViewerAfterExport,
	bool &fixPobCrc
	)
{

	MESSENGER_INDENT;
	MStatus  status;

	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT(!status, ("failed to get args length\n"));

	//-- handle each argument
	bool haveNode            = false;
	bool isInteractive       = false;
	interactive              = false;
	lock                     = false;
	unlock                   = false;
	showViewerAfterExport    = false;

	// always non-silent unless the silent arg is present
	messenger->endSilentExport();

	// NOTE: interactive or autoexport argument needs to come first

	for (unsigned argIndex = 0; argIndex < argCount; ++argIndex)
	{
		MString argName = args.asString(argIndex, &status);
		MESSENGER_REJECT(!status, ("failed to get arg [%u] as string\n", argIndex));

		IGNORE_RETURN( argName.toLowerCase() );

		if (argName == ExportArgs::cs_interactiveArgName)
		{
			MESSENGER_REJECT(argIndex != 0, ("%s must be first argument", ExportArgs::cs_interactiveArgName));
			isInteractive = true;
			interactive   = true;
		}
		else if (argName == ExportArgs::cs_showViewerAfterExport)
		{
			showViewerAfterExport = true;
		}
		else if (argName == ExportArgs::cs_lockArgName)
		{
			lock = true;
		}
		else if (argName == ExportArgs::cs_unlockArgName)
		{
			unlock = true;
		}
		else if (argName == ExportArgs::cs_nodeArgName)
		{
			if (!isInteractive)
			{
				//--handle node argument
				MESSENGER_REJECT(haveNode, ("node argument specified multiple times\n"));

				MString nodeName = args.asString(argIndex + 1, &status);
				MESSENGER_REJECT(!status, ("failed to get node name argument\n"));

				// search for node in Maya scene
				MSelectionList  nodeList;
				status = MGlobal::getSelectionListByName(nodeName, nodeList);
				MESSENGER_REJECT(!status, ("MGlobal::getSelectionListByName failure"));

				MESSENGER_REJECT(nodeList.length() < 1, ("no scene nodes match specified export node [%s]\n", nodeName.asChar()));
				MESSENGER_REJECT(nodeList.length() > 1, ("multiple nodes match specified export node [%s]\n", nodeName.asChar()));

				status = nodeList.getDagPath(0, *targetDagPath);
				MESSENGER_REJECT(!status, ("failed to get dag path for export node [%s]\n", nodeName.asChar()));

				// fixup argIndex
				++argIndex;
				haveNode = true;
			}
		}
		else if (argName == ExportArgs::cs_silentArgName)
		{
			// silent mode - disable message box feedback (logs messages instead)
			messenger->startSilentExport();
		}
		else if (argName == ExportArgs::cs_fixPobCrc)
		{
			fixPobCrc = true;
		}
		else
		{
			MESSENGER_LOG_ERROR(("unknown argument [%s]\n", argName.asChar()));
			return false;
		}
	}

	//-- handle interactive node selection
	if (isInteractive)
	{
		MSelectionList nodeList;
		status = MGlobal::getActiveSelectionList(nodeList);
		MESSENGER_REJECT(!status,("failed to get active selection list\n"))

		// we only support export of one skeleton template into a skeleton template file
		MESSENGER_REJECT(nodeList.length() != 1, ("must have exactly one node specified, currently [%u]\n", nodeList.length()));

		status = nodeList.getDagPath(0, *targetDagPath);
		MESSENGER_REJECT(!status, ("failed to get dag path for selected node\n"));

		haveNode = true;
	}
	MESSENGER_REJECT(!isInteractive && !haveNode, ("neither joint node (-node) nor (-interactive) was specified\n"));
	return MStatus();
}

// ----------------------------------------------------------------------

MStatus ExportStaticMesh::doIt(const MArgList &args)
{
	PerformanceTimer exportTimer;
	exportTimer.start();

	messenger->clearWarningsAndErrors();

	MESSENGER_INDENT;

	MStatus status;
	MDagPath  targetDagPath;

	MString arg;
	if(args.length())
		IGNORE_RETURN(args.get(0, arg));

	//-- 
	const char *const     shaderTemplateReferenceDir = SetDirectoryCommand::getDirectoryString(SHADER_TEMPLATE_REFERENCE_DIR_INDEX);
	const char *const     appearanceWriteDir         = SetDirectoryCommand::getDirectoryString(APPEARANCE_WRITE_DIR_INDEX);
	const char *const     shaderTemplateWriteDir     = SetDirectoryCommand::getDirectoryString(SHADER_TEMPLATE_WRITE_DIR_INDEX);
	const char *const     effectReferenceDir         = SetDirectoryCommand::getDirectoryString(EFFECT_REFERENCE_DIR_INDEX);
	const char *const     textureReferenceDir        = SetDirectoryCommand::getDirectoryString(TEXTURE_REFERENCE_DIR_INDEX);
	const char *const     textureWriteDir            = SetDirectoryCommand::getDirectoryString(TEXTURE_WRITE_DIR_INDEX);
	const char *const     author                     = SetDirectoryCommand::getDirectoryString(AUTHOR_INDEX);

	bool interactive           = false;
	bool lock                  = false;
	bool unlock                = false;
	bool showViewerAfterExport = false;
	bool fixPobCrc             = false;

	const bool processSuccess = processArguments(
		args, 
		&targetDagPath, 
		interactive, 
		lock, 
		unlock, 
		showViewerAfterExport,
		fixPobCrc
	);
	MESSENGER_REJECT_STATUS(!processSuccess, ("argument processing failed\n"));

	//-- find out what is selected
	MSelectionList        transformList;
	IGNORE_RETURN(transformList.add(targetDagPath));

	//-- install the exporter log
	ExporterLog::install (messenger);
	ExporterLog::setAuthor (author);

	//get, store the base directory
	std::string baseDir = textureWriteDir;
	std::string::size_type pos = baseDir.find_last_of("texture");
	FATAL(pos == static_cast<unsigned int>(std::string::npos), ("malformed filename in ExportStaticMesh::doIt"));
	baseDir = baseDir.substr(0, pos-strlen("texture"));
	baseDir += "\\";
	ExporterLog::setBaseDir(baseDir);

	ExporterLog::setSourceFilename (MFileIO::currentFile().asChar());
	ExporterLog::setMayaCommand("exportStaticMesh");

	//-- setup the hierarchy
	MayaHierarchy hierarchy (messenger);

	hierarchy.setAppearanceWriteDir (appearanceWriteDir);
	hierarchy.setShaderTemplateWriteDir (shaderTemplateWriteDir);
	hierarchy.setShaderTemplateReferenceDir (shaderTemplateReferenceDir);
	hierarchy.setEffectReferenceDir (effectReferenceDir);
	hierarchy.setTextureReferenceDir (textureReferenceDir);
	hierarchy.setTextureWriteDir (textureWriteDir);

	//do the actual export to disk (the slow part)
	MESSENGER_REJECT_STATUS(!hierarchy.build (transformList), ("failed to build hierarchy\n"));

	//load the log file
	MString nodeName = hierarchy.getBaseName();
	std::string shortLogFilename = nodeName.asChar();
	shortLogFilename += ".log";
	char buffer[1024];
	MObject selectedNode;
	status = transformList.getDependNode(0, selectedNode);
	const bool gnnResult = MayaUtility::getNodeName(selectedNode, buffer, 1024);
	MESSENGER_REJECT_STATUS(!gnnResult, ("getNodeName() failed\n"));
	std::string reexportArguments = ExportArgs::cs_nodeArgName.asChar();
	reexportArguments += " ";
	reexportArguments += buffer;
	ExporterLog::setMayaExportOptions(reexportArguments);
	IGNORE_RETURN(ExporterLog::loadLogFile(shortLogFilename));
	std::string newLogFilename = SetDirectoryCommand::getDirectoryString(LOG_DIR_INDEX);
	newLogFilename += shortLogFilename;

	//check for duplicate names (if there are dupes, maya returns a '|' as part of the node name)
	if(shortLogFilename.find("|") != std::string::npos) //lint !e737 !e650 std::string::npos isn't same signage as std::string's find(), sigh
	{
		std::string errorMsg = "duplicate node name ";
		errorMsg += shortLogFilename;
		errorMsg += " found, correct and re-export";
		MESSENGER_LOG_ERROR((errorMsg.c_str()));
		if (interactive)
		{
			MESSENGER_MESSAGE_BOX(NULL, errorMsg.c_str(), "Error!", MB_OK);
		}
	}

	hierarchy.dump ();
	bool writeSuccess = hierarchy.write (true);
	if (!writeSuccess)
	{
		if (interactive)
			MESSENGER_MESSAGE_BOX(NULL, "Local export failed, see output window", "Error!", MB_OK);
	}

	ExporterLog::setAssetGroup("Local export, N/A");
	ExporterLog::setAssetName("Local export, N/A");
	IGNORE_RETURN(ExporterLog::writeStaticMesh (newLogFilename.c_str(), interactive));
	ExporterLog::remove();

	messenger->printWarningsAndErrors();

	exportTimer.stop();
	const int exportTime = static_cast<int>(exportTimer.getElapsedTime());
	const int hours = exportTime / c_secondsPerHour;
	const int minutes = (exportTime - (hours * c_secondsPerHour)) / c_seondsPerMinute;
	const int seconds = exportTime - (hours * c_secondsPerHour) - (minutes * c_seondsPerMinute);
	MESSENGER_LOG(("Total export time: %2ih %2im %2is\n", hours, minutes, seconds));

	MESSENGER_LOG(("MemoryManager %lu/%lu=bytes %d/%d=allocs\n", MemoryManager::getCurrentNumberOfBytesAllocated(), MemoryManager::getMaximumNumberOfBytesAllocated(), MemoryManager::getCurrentNumberOfAllocations(), MemoryManager::getMaximumNumberOfAllocations()));

    std::stringstream text;
    text << "Export Complete: " << nodeName.asChar() << std::endl;
    messenger->getWarningAndErrorText(text);
    text << std::ends;

	MESSENGER_MESSAGE_BOX(NULL,text.str().c_str(),"Export",MB_OK);
	
	if (showViewerAfterExport)
	{
		std::stringstream assetPathAndFilename;
		assetPathAndFilename << SetDirectoryCommand::getDirectoryString(APPEARANCE_WRITE_DIR_INDEX)  << nodeName.asChar() << ".apt";
		ExportManager::LaunchViewer(assetPathAndFilename.str());

	}

	return MS::kSuccess;
}

// ======================================================================
