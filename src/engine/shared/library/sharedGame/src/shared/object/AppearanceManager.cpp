// ======================================================================
// 
// AppearanceManager.cpp
// Copyright Sony Online Entertainment, Inc.
//
// ======================================================================

#include "sharedGame/FirstSharedGame.h"
#include "sharedGame/AppearanceManager.h"

#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/Report.h"
#include "sharedFile/Iff.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/PersistentCrcString.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/LessPointerComparator.h"
#include "sharedFoundation/PointerDeleter.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableColumnType.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

// Release: wrong DTI! column types make getStringValue read int bits as char* -> 0xC0000005 (same class as travel.iff).
namespace
{
inline void amInstallTrace (char const *msg)
{
	REPORT_LOG (true, ("AppearanceManager: %s\n", msg ? msg : ""));
#if defined(_WIN32)
	char line[640];
	_snprintf_s (
		line, sizeof (line), _TRUNCATE,
		"[Titan] AppearanceManager: %s\r\n",
		msg ? msg : "");
	::OutputDebugStringA (line);
#endif
}

void validateAppearanceDataTableForLoad (DataTable const &table, char const *pathForErrors)
{
	int const nCols = table.getNumColumns ();
	if (nCols < 2)
	{
		FATAL (true, ("AppearanceManager: '%s' must have at least 2 columns (source name in column 0, appearance entries in columns 1+); found %d columns.",
		              pathForErrors, nCols));
	}
	for (int c = 0; c < nCols; ++c)
	{
		DataTableColumnType::DataType const bt = table.getDataTypeForColumn (c).getBasicType ();
		if (bt != DataTableColumnType::DT_String)
		{
			FATAL (true, ("AppearanceManager: '%s' column %d must be string ('s' in DTI!). Basic type is %d. "
			              "getStringValue in Release does not type-check; fix appearance_table.iff export.",
			              pathForErrors, c, static_cast<int> (bt)));
		}
	}
}
} // namespace

// ======================================================================

namespace AppearanceManagerNamespace
{
	typedef std::set<CrcString const *, LessPointerComparator> AppearanceTemplateNameSet;
	AppearanceTemplateNameSet ms_appearanceTemplateNameSet;

	typedef std::vector<CrcString const *> CrcStringVector;
	typedef std::map<CrcString const *, CrcStringVector *, LessPointerComparator> ObjectTemplateAppearanceTemplateMap;
	ObjectTemplateAppearanceTemplateMap ms_objectTemplateAppearanceTemplateMap;

	bool ms_installed;
	bool ms_verboseWarnings;

	// appearance_table.iff is very large; x64 Release has crashed in ~DataTable when the load was
	// stack-scoped. Hold one heap DataTable for process lifetime (never destroy) like skufree manifest.
	DataTable *ms_appearanceTableHeld = 0;

	void remove();

#ifdef _DEBUG
	void regressionTest();
#endif

	class PointerDeleterPair
	{
	public:

		template<typename FirstType, typename SecondType>
		void operator()(std::pair<FirstType, SecondType> &pairArgument) const
		{
			delete pairArgument.first;
			delete pairArgument.second;
		}
	};
}

using namespace AppearanceManagerNamespace;	

// ======================================================================

void AppearanceManager::install()
{
	amInstallTrace ("install() entered");

	InstallTimer const installTimer("AppearanceManager::install");

	DEBUG_FATAL(ms_installed, ("AppearanceManagerNamespace::install: already installed"));
	ms_installed = true;

	ms_verboseWarnings = ConfigFile::getKeyBool("SharedGame/AppearanceManager", "verboseWarnings", false);

	char const * const appearanceTableFileName = "datatables/appearance/appearance_table.iff";

	amInstallTrace ("before Iff::open(appearance_table.iff) optional=true + FATAL if fail (Release-safe)");

	Iff iff;
	// optional=true: Iff does not DEBUG_FATAL internally when missing; we FATAL below with a clear message.
	bool const iffOpened = iff.open (appearanceTableFileName, true);
	amInstallTrace (iffOpened ? "Iff::open returned true" : "Iff::open returned false");
	FATAL (!iffOpened, ("AppearanceManager::install: could not open [%s]. Verify TreeFile path and archive.", appearanceTableFileName));

	FATAL (ms_appearanceTableHeld != 0, ("AppearanceManager::install: internal error, appearance table already held."));

	amInstallTrace ("before new DataTable");
	ms_appearanceTableHeld = new DataTable;
	amInstallTrace ("before DataTable::load(Iff)");
	ms_appearanceTableHeld->load (iff);
	amInstallTrace ("after DataTable::load, before Iff::close");
	iff.close ();
	amInstallTrace ("after Iff::close, before validateAppearanceDataTableForLoad");

	DataTable &dataTable = *ms_appearanceTableHeld;
	validateAppearanceDataTableForLoad (dataTable, appearanceTableFileName);

	amInstallTrace ("after validateAppearanceDataTableForLoad");

	int const numberOfColumns = dataTable.getNumColumns ();

	int const numberOfRows = dataTable.getNumRows ();
	REPORT_LOG (true, ("AppearanceManager: appearance_table.iff loaded rows=%d cols=%d (DataTable heap-held)\n", numberOfRows, numberOfColumns));
	amInstallTrace ("counts retrieved; starting row loop");

	for (int row = 0; row < numberOfRows; ++row)
	{
		// getStringValue() returns const char*; do not bind through std::string& — MSVC/x64 + Release
		// has seen bad temporaries/lifetime edge cases in this hot loop.
		char const *const sourceStr = dataTable.getStringValue(0, row);
		TemporaryCrcString const crcSourceName(sourceStr, true);

		//-- Look up the source name
		CrcStringVector * crcStringVector = 0;
		{
			ObjectTemplateAppearanceTemplateMap::iterator iter = ms_objectTemplateAppearanceTemplateMap.find(&crcSourceName);
			if (iter != ms_objectTemplateAppearanceTemplateMap.end())
			{
				DEBUG_WARNING(true, ("AppearanceManager::install(%s): duplicate entry found for %s", appearanceTableFileName, crcSourceName.getString()));
				continue;
			}
			else
			{
				crcStringVector = new CrcStringVector();
				crcStringVector->reserve(static_cast<size_t>(numberOfColumns));

				ms_objectTemplateAppearanceTemplateMap.insert(std::make_pair(new PersistentCrcString(crcSourceName), crcStringVector));
			}
		}

		//-- Read in the column data
		for (int column = 1; column < numberOfColumns; ++column)
		{
			char const *const fileStr = dataTable.getStringValue(column, row);

#ifdef _DEBUG
			if (strstr(fileStr, ".sat") != 0 || strstr(fileStr, ".apt") != 0)
				DEBUG_WARNING(ms_verboseWarnings && !TreeFile::exists(fileStr), ("AppearanceManager::install(%s): [%s] is not a valid entry for row %d column %s because the file does not exist", appearanceTableFileName, fileStr, row, dataTable.getColumnName(column).c_str()));
			else
				DEBUG_WARNING(
					(fileStr[0] != '\0' && strcmp(fileStr, ":block") != 0 && strcmp(fileStr, ":default") != 0 && strcmp(fileStr, ":hide") != 0),
					("AppearanceManager::install(%s): [%s] is not a valid entry for row %d column %s", appearanceTableFileName, fileStr, row, dataTable.getColumnName(column).c_str()));
#endif

			TemporaryCrcString const crcFileName(fileStr, true);
			
			CrcString const * appearanceTemplateName = 0;
			AppearanceTemplateNameSet::iterator iter = ms_appearanceTemplateNameSet.find(static_cast<CrcString const *>(&crcFileName));
			if (iter != ms_appearanceTemplateNameSet.end())
				appearanceTemplateName = *iter;
			else
			{
				appearanceTemplateName = new PersistentCrcString(crcFileName);

				ms_appearanceTemplateNameSet.insert(appearanceTemplateName);
			}

			crcStringVector->push_back(appearanceTemplateName);
		}
	}

	REPORT_LOG (true, ("AppearanceManager: appearance map build complete\n"));
	amInstallTrace ("appearance map build complete");

#ifdef _DEBUG
	regressionTest();
#endif

	amInstallTrace ("before ExitChain::add(AppearanceManager::remove)");
	ExitChain::add(AppearanceManagerNamespace::remove, "AppearanceManagerNamespace::remove");
	amInstallTrace ("install() returning");
}

// ----------------------------------------------------------------------

void AppearanceManagerNamespace::remove()
{
	DEBUG_FATAL(!ms_installed, ("AppearanceManagerNamespace::remove: not installed"));
	ms_installed = false;

	// Leak the held appearance DataTable (do not run ~DataTable on that slab). Clear so a rare
	// re-install would not FATAL; a second load would replace the pointer and leak the first slab.
	ms_appearanceTableHeld = 0;

	std::for_each(ms_appearanceTemplateNameSet.begin(), ms_appearanceTemplateNameSet.end(), PointerDeleter());
	ms_appearanceTemplateNameSet.clear();

	std::for_each(ms_objectTemplateAppearanceTemplateMap.begin(), ms_objectTemplateAppearanceTemplateMap.end(), PointerDeleterPair());
	ms_objectTemplateAppearanceTemplateMap.clear();
}

// ----------------------------------------------------------------------

//bool AppearanceManager::exists(CrcString const & sourceName)
bool AppearanceManager::isAppearanceManaged(std::string const &fileName)
{
	TemporaryCrcString const crcFileName(fileName.c_str(), true);
	return ms_objectTemplateAppearanceTemplateMap.find(&crcFileName) != ms_objectTemplateAppearanceTemplateMap.end();
}

// ----------------------------------------------------------------------

//bool AppearanceManager::getAppearanceName(CrcString const & sourceName, int const column, CrcString const * & fileName)
bool AppearanceManager::getAppearanceName(std::string &targetName, std::string const &sourceName, int const sourceColumn)
{
	TemporaryCrcString const crcSourceName(sourceName.c_str(), true);

	//-- Look up the source name
	ObjectTemplateAppearanceTemplateMap::iterator iter = ms_objectTemplateAppearanceTemplateMap.find(&crcSourceName);
	if (iter == ms_objectTemplateAppearanceTemplateMap.end())
		return false;

	//-- Source name exists, so the column must exist
	CrcStringVector const & crcStringVector = *iter->second;
	int const column = sourceColumn - 1;
	int const numberOfColumns = static_cast<int>(crcStringVector.size());
	if (column < 0 || column >= numberOfColumns)
	{
		DEBUG_FATAL(true, ("AppearanceManager::getAppearanceName(%s): invalid column %d/%d", sourceName.c_str(), column, numberOfColumns));
		return false;
	}

	targetName = crcStringVector[static_cast<size_t>(column)]->getString();

	return true;
}

// ----------------------------------------------------------------------

#ifdef _DEBUG

void AppearanceManagerNamespace::regressionTest()
{
	DEBUG_FATAL(!AppearanceManager::isAppearanceManaged("shared_aakuan_belt"), ("AppearanceManagerNamespace::regressionTest: FAILED - shared_aakuan_belt not found"));

	std::string targetName;
	DEBUG_FATAL(!AppearanceManager::getAppearanceName(targetName, "shared_aakuan_belt", 1) || targetName != std::string("appearance/belt_s05_m.sat"), ("AppearanceManagerNamespace::regressionTest: shared_aakuan_belt Male Human appearance is supposed to be appearance/belt_s05_m.sat, but is %s", targetName.c_str()));
}

#endif

// ======================================================================
