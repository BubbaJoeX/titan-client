//===================================================================
//
// SetupSharedUtility.cpp
// asommers
//
// copyright 2002, sony online entertainment
//
//===================================================================

#include "sharedUtility/FirstSharedUtility.h"
#include "sharedUtility/SetupSharedUtility.h"

#include "sharedFile/FileManifest.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedUtility/CachedFileManager.h"
#include "sharedUtility/Callback.h"
#include "sharedUtility/ConfigSharedUtility.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableColumnType.h"
#include "sharedUtility/DataTableManager.h"
#include "sharedFoundation/Fatal.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "sharedUtility/LocationManager.h"
#include "sharedUtility/PooledString.h"
#include "sharedUtility/WorldSnapshotReaderWriter.h"
#include "sharedDebug/InstallTimer.h"

#if defined(_WIN32)
#include <windows.h>
#include <cstdio>
#define SETUP_SHARED_UTILITY_TRACE(msg) do { ::OutputDebugStringA("[Titan] " msg "\r\n"); } while (0)
#else
#define SETUP_SHARED_UTILITY_TRACE(msg) do { } while (0)
#endif

// NOTE: Do not wrap installFileManifestEntries in SEH that swallows AVs. MSVC will not run C++
// destructors or the function tail, leaving tables in m_tables and corrupting the heap.
// skufree is Iff-loaded into a heap DataTable kept for process lifetime (~DataTable omitted) because
// x64 Release crashed destroying large manifest tables after the ingest loop.

//===================================================================

namespace
{
	bool ms_installed;

	// skufree.iff is large (~40k+ rows). x64 Release has crashed in DataTable::~DataTable (cell teardown)
	// immediately after installFileManifestEntries finishes. Hold one heap-allocated DataTable for the
	// process lifetime so we never destroy that table on this path; memory is reclaimed at exit.
	DataTable *s_skufreeManifestTableHeld = 0;

	void validateManifestSkuFreeTable (DataTable const &table, char const *pathForErrors)
	{
		int const nCols = table.getNumColumns ();
		FATAL (nCols < 3, ("SetupSharedUtility: '%s' expected at least 3 columns (fileName, sceneId, fileSize); found %d.",
		                   pathForErrors, nCols));

		int const colFile  = table.findColumnNumber ("fileName");
		int const colScene = table.findColumnNumber ("sceneId");
		int const colSize  = table.findColumnNumber ("fileSize");
		FATAL (colFile < 0, ("SetupSharedUtility: '%s' missing column [fileName] (see skufree.tab header).", pathForErrors));
		FATAL (colScene < 0, ("SetupSharedUtility: '%s' missing column [sceneId].", pathForErrors));
		FATAL (colSize < 0, ("SetupSharedUtility: '%s' missing column [fileSize].", pathForErrors));

		DataTableColumnType::DataType const tFile = table.getDataTypeForColumn (colFile).getBasicType ();
		DataTableColumnType::DataType const tScene = table.getDataTypeForColumn (colScene).getBasicType ();
		DataTableColumnType::DataType const tSize = table.getDataTypeForColumn (colSize).getBasicType ();
		FATAL (tFile != DataTableColumnType::DT_String,
		       ("SetupSharedUtility: '%s' column fileName must be string (DTI 's'); type=%d.", pathForErrors, static_cast<int> (tFile)));
		FATAL (tScene != DataTableColumnType::DT_String,
		       ("SetupSharedUtility: '%s' column sceneId must be string; type=%d.", pathForErrors, static_cast<int> (tScene)));
		FATAL (tSize != DataTableColumnType::DT_Int,
		       ("SetupSharedUtility: '%s' column fileSize must be int (DTI 'i'); type=%d.", pathForErrors, static_cast<int> (tSize)));
	}
}

//===================================================================

SetupSharedUtility::Data::Data () :
	m_allowFileCaching(false)
{
}

//===================================================================

void SetupSharedUtility::install(SetupSharedUtility::Data const & data)
{
	// ODS is intentionally not routed through Report (same rationale as InstallTimer) so we
	// can see where access violations land during utility setup.
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: enter (before InstallTimer)");
	InstallTimer const installTimer("SetupSharedUtility::install");

	DEBUG_FATAL (ms_installed, ("SetupSharedUtility::install already installed"));
	ms_installed = true;

	ConfigSharedUtility::install();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after ConfigSharedUtility::install");

	CurrentUserOptionManager::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after CurrentUserOptionManager::install");
	LocalMachineOptionManager::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after LocalMachineOptionManager::install");
	DataTableManager::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after DataTableManager::install");
	WorldSnapshotReaderWriter::Node::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after WorldSnapshotReaderWriter::Node::install");
	Callback::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after Callback::install");
	CachedFileManager::install(data.m_allowFileCaching);
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after CachedFileManager::install");
	PooledString::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after PooledString::install");
	LocationManager::install ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after LocationManager::install");
	installFileManifestEntries ();
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after installFileManifestEntries");

	ExitChain::add (SetupSharedUtility::remove, "SetupSharedUtility");
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::install: after ExitChain::add, leave");
}

//-------------------------------------------------------------------

void SetupSharedUtility::remove ()
{
	DEBUG_FATAL (!ms_installed, ("SetupSharedUtility::remove not installed"));
	ms_installed = false;
}

//-------------------------------------------------------------------

void SetupSharedUtility::setupGameData(Data & data)
{
	data.m_allowFileCaching = false;
}

//-------------------------------------------------------------------

void SetupSharedUtility::setupToolData(Data & data)
{
	data.m_allowFileCaching = false;
}

//-------------------------------------------------------------------

void SetupSharedUtility::installFileManifestEntries ()
{
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::installFileManifestEntries: enter");
	// read in the datatable entries for sharedFile/FileManifest.cpp
	std::string datatableName = FileManifest::getDatatableName ();

#if defined(_WIN32)
	{
		char buf[512];
		_snprintf_s (buf, sizeof (buf), _TRUNCATE, "[Titan] installFileManifestEntries: datatable=%s\r\n", datatableName.c_str ());
		::OutputDebugStringA (buf);
	}
#endif

	FATAL (!TreeFile::exists (datatableName.c_str ()), ("%s could not be found. Are your paths set up correctly?", datatableName.c_str ()));
	FATAL (s_skufreeManifestTableHeld != 0, ("SetupSharedUtility::installFileManifestEntries: already executed (held table exists)."));

	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::installFileManifestEntries: Iff open + heap DataTable held for process life (no ~DataTable on skufree)");

	Iff iff;
	FATAL (!iff.open (datatableName.c_str (), true), ("SetupSharedUtility::installFileManifestEntries: Iff::open failed for [%s].", datatableName.c_str ()));

	s_skufreeManifestTableHeld = new DataTable;
	s_skufreeManifestTableHeld->load (iff);
	iff.close ();

	DataTable &manifestDatatable = *s_skufreeManifestTableHeld;

	validateManifestSkuFreeTable (manifestDatatable, datatableName.c_str ());
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::installFileManifestEntries: after validateManifestSkuFreeTable");

	int const colFile  = manifestDatatable.findColumnNumber ("fileName");
	int const colScene = manifestDatatable.findColumnNumber ("sceneId");
	int const colSize  = manifestDatatable.findColumnNumber ("fileSize");
	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::installFileManifestEntries: column indices ok, before getNumRows");

	int const numRows = manifestDatatable.getNumRows ();
#if defined(_WIN32)
	{
		char buf[128];
		_snprintf_s (buf, sizeof (buf), _TRUNCATE, "[Titan] installFileManifestEntries: numRows=%d (begin row loop)\r\n", numRows);
		::OutputDebugStringA (buf);
	}
#endif

	for (int i = 0; i < numRows; ++i)
	{
		char const *const fileStr = manifestDatatable.getStringValue (colFile, i);
		char const *const sceneStr = manifestDatatable.getStringValue (colScene, i);
		int const fileSize = manifestDatatable.getIntValue (colSize, i);

		if (fileStr && fileStr[0] != '\0')
			FileManifest::addStoredManifestEntry (fileStr, sceneStr ? sceneStr : "", fileSize);
		else
			DEBUG_WARNING (true, ("SetupSharedUtility::installFileManifestEntries(): empty filename row %i\n", i));
	}

	SETUP_SHARED_UTILITY_TRACE("SetupSharedUtility::installFileManifestEntries: row loop done (skufree DataTable left allocated; no destructor)");
}
//===================================================================
