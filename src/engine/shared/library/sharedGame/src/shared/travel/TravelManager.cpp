// ======================================================================
//
// TravelManager.cpp
// asommers
//
// copyright 2002, sony online entertainment
//
// ======================================================================

#include "sharedGame/FirstSharedGame.h"
#include "sharedGame/TravelManager.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFile/Iff.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedUtility/DataTable.h"
#include "sharedUtility/DataTableColumnType.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>
#include <climits>

// ----------------------------------------------------------------------

// Release builds strip DataTable::getStringValue / getIntValue DEBUG_FATAL checks. Wrong column types
// (e.g. int in col 0) make getStringValue read a string pointer from int cell memory -> 0xC0000005 with no message.
// Validate DTI! schema with FATAL (always in Release) before any cell access.
namespace
{
void validateTravelDataTableForLoad (DataTable const &table, char const *pathForErrors)
{
	int const nCols = table.getNumColumns ();
	if (nCols < 2)
	{
		FATAL (true, ("TravelManager: '%s' must have at least 2 columns (row planet names in column 0, int costs in column 1+); found %d columns.",
		              pathForErrors, nCols));
	}

	DataTableColumnType::DataType const bt0 = table.getDataTypeForColumn (0).getBasicType ();
	if (bt0 != DataTableColumnType::DT_String)
	{
		FATAL (true, ("TravelManager: '%s' column 0 must be string (planet name per row). Basic type is %d, not String. "
		              "getStringValue(0,row) in Release does not type-check; non-string data causes undefined behavior and access violations. "
		              "Re-export datatables/travel/travel.iff with column 0 as type 's'.",
		              pathForErrors, static_cast<int> (bt0)));
	}

	for (int c = 1; c < nCols; ++c)
	{
		DataTableColumnType::DataType const bt = table.getDataTypeForColumn (c).getBasicType ();
		if (bt != DataTableColumnType::DT_Int)
		{
			FATAL (true, ("TravelManager: '%s' column %d must be int (travel cost to the planet in the column header). Basic type is %d. "
			              "getIntValue loads costs as int; use type 'i' for all cost columns in the DTI!.",
			              pathForErrors, c, static_cast<int> (bt)));
		}
	}
}
} // namespace

// ======================================================================

namespace TravelManagerNamespace
{
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	typedef std::vector<std::string> PlanetNameList;
	typedef std::map<std::string, int> PlanetNameToPlanetIndexMap;
	typedef std::map<std::pair<int, int>, int> SingleHopRouteList; // <<planet1, planet2>, cost> (planet1 ***IS ALWAYS <=*** planet2)
	typedef std::map<std::pair<int, int>, std::pair<int, std::vector<int> > > AnyHopLeastCostRouteList; // <<planet1, planet2>, <cost, <list of planets along the route>>> (planet1 ***IS ALWAYS <=*** planet2)

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void remove ();
	void load (const char* fileName);
	bool getPlanetIndex (const std::string& planetName, int& planetIndex);
	bool getPlanetSingleHopCost(int planetIndex1, int planetIndex2, int& planetCost);
	bool getPlanetAnyHopLeastCost(int planetIndex1, int planetIndex2, int& planetCost);

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	bool           ms_installed;
	bool           ms_debugReport;
	PlanetNameList ms_planetNameList;
	PlanetNameToPlanetIndexMap ms_planetNameToPlanetIndexList;
	SingleHopRouteList ms_singleHopRouteList;
	AnyHopLeastCostRouteList ms_anyHopLeastCostRouteList;

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
}

using namespace TravelManagerNamespace;

// ======================================================================
// STATIC PUBLIC TravelManager
// ======================================================================

void TravelManager::install ()
{
	InstallTimer const installTimer("TravelManager::install ");

	DEBUG_FATAL (ms_installed, ("TravelManager::install: already installed"));
	ms_installed = true;

	load ("datatables/travel/travel.iff");

	DebugFlags::registerFlag (ms_debugReport, "SharedGame", "reportTravelManager");
	ExitChain::add (TravelManagerNamespace::remove, "TravelManagerNamespace::remove");
}

// ----------------------------------------------------------------------

int TravelManager::getNumberOfPlanets ()
{
	return static_cast<int> (ms_planetNameToPlanetIndexList.size ());
}

// ----------------------------------------------------------------------

bool TravelManager::getPlanetName (int planetIndex, std::string& planetName)
{
	if (planetIndex < 0 || planetIndex >= getNumberOfPlanets ())
	{
		DEBUG_WARNING (true, ("TravelManager::getPlanetName: planetIndex out of range %i >= %i\n", planetIndex, getNumberOfPlanets ()));
		return false;
	}

	planetName = ms_planetNameList [static_cast<uint> (planetIndex)];
	return true;
}

// ----------------------------------------------------------------------

bool TravelManager::getPlanetSingleHopCost (const std::string& planetName1, const std::string& planetName2, int& planetCost)
{
	int planetIndex1(0);
	if (!getPlanetIndex (planetName1, planetIndex1))
	{
		DEBUG_WARNING (true, ("TravelManager::getPlanetSingleHopCost: planet %s does not exist\n", planetName1.c_str ()));
		return false;
	}

	int planetIndex2(0);
	if (!getPlanetIndex (planetName2, planetIndex2))
	{
		DEBUG_WARNING (true, ("TravelManager::getPlanetSingleHopCost: planet %s does not exist\n", planetName2.c_str ()));
		return false;
	}

	return TravelManagerNamespace::getPlanetSingleHopCost(planetIndex1, planetIndex2, planetCost);
}

// ----------------------------------------------------------------------

bool TravelManager::getPlanetAnyHopLeastCost (const std::string& planetName1, const std::string& planetName2, int& planetCost)
{
	int planetIndex1(0);
	if (!getPlanetIndex (planetName1, planetIndex1))
	{
		DEBUG_WARNING (true, ("TravelManager::getPlanetAnyHopLeastCost: planet %s does not exist\n", planetName1.c_str ()));
		return false;
	}

	int planetIndex2(0);
	if (!getPlanetIndex (planetName2, planetIndex2))
	{
		DEBUG_WARNING (true, ("TravelManager::getPlanetAnyHopLeastCost: planet %s does not exist\n", planetName2.c_str ()));
		return false;
	}

	return TravelManagerNamespace::getPlanetAnyHopLeastCost(planetIndex1, planetIndex2, planetCost);
}

// ----------------------------------------------------------------------

bool TravelManager::getAdjacentPlanets (const std::string& planetName, std::set<std::string>& planetList)
{
	int planetIndex(0);
	if (!getPlanetIndex (planetName, planetIndex))
	{
		DEBUG_WARNING (true, ("TravelManager::getAdjacentPlanets: planet %s does not exist\n", planetName.c_str ()));
		return false;
	}

	planetList.clear ();
	planetList.insert (planetName);

	SingleHopRouteList::const_iterator iter = ms_singleHopRouteList.begin ();
	for (; iter != ms_singleHopRouteList.end (); ++iter)
	{
		if (iter->first.first == planetIndex)
			planetList.insert (ms_planetNameList [static_cast<uint> (iter->first.second)]);
		else if (iter->first.second == planetIndex)
			planetList.insert (ms_planetNameList [static_cast<uint> (iter->first.first)]);
	}

	return true;
}

// ======================================================================
// STATIC PRIVATE TravelManager
// ======================================================================

void TravelManagerNamespace::remove ()
{
	DEBUG_FATAL (!ms_installed, ("TravelManager::install: not installed"));
	ms_installed = false;

	DebugFlags::unregisterFlag (ms_debugReport);
}

// ----------------------------------------------------------------------

void TravelManagerNamespace::load (const char* fileName)
{
	ms_planetNameList.clear ();
	ms_planetNameToPlanetIndexList.clear ();
	ms_singleHopRouteList.clear ();
	ms_anyHopLeastCostRouteList.clear ();

	Iff iff;
	if (iff.open (fileName, true))
	{
		DataTable dataTable;
		dataTable.load (iff);
		validateTravelDataTableForLoad (dataTable, fileName);

		//-- setup planet names
		int const numberOfColumns = dataTable.getNumColumns ();
		// nCols<2 should be impossible: validate required >=2.
		FATAL (numberOfColumns < 2, (
			"TravelManager::load: internal error — getNumColumns()=%d after validate (expected >=2).",
			numberOfColumns));
		// x64: interleaved std::map<string,int> insert + push_back per column hit an AV for some builds after
		// the ~7th header. Defer the map: fill the vector
		// (reserve avoids realloc + string moves that can interact badly with mixed map growth), O(n) duplicate
		// scan, then one batch to build the name->index table. (Lists were cleared at start of load().)
		ms_planetNameList.reserve (static_cast<size_t> (numberOfColumns - 1));
		int col(0);
		for (col = 1; col < numberOfColumns; ++col)
		{
			// Copy: avoid any edge-case with refs into DataTable while we fill our own vector.
			std::string const planetName = dataTable.getColumnName (col);
			for (size_t pi = 0; pi < ms_planetNameList.size (); ++pi)
			{
				if (ms_planetNameList[pi] == planetName)
					FATAL (true, ("TravelManagerNamespace::load: duplicate planet %s in column %i", planetName.c_str (), col));
			}
			FATAL (ms_planetNameList.size () > static_cast<size_t> (INT_MAX), ("TravelManagerNamespace::load: too many route columns in travel table (>%d).", INT_MAX));
			ms_planetNameList.push_back (planetName);
		}
		for (size_t pi = 0; pi < ms_planetNameList.size (); ++pi)
			ms_planetNameToPlanetIndexList[ms_planetNameList[pi]] = static_cast<int> (pi);

		//-- setup "single hop" routes
		const int numberOfRows = dataTable.getNumRows ();
		int row(0);
		for (row = 0; row < numberOfRows; ++row)
		{
			const std::string planetName1 = dataTable.getStringValue (0, row);
			int planetIndex1(0);
			if (!getPlanetIndex (planetName1, planetIndex1))
				FATAL (true, ("TravelManagerNamespace::load: planet %s in column 0 could not be matched to a column header (row %i).", planetName1.c_str (), row));
			
			for (col = 1; col < numberOfColumns; ++col)
			{
				const int cost = dataTable.getIntValue (col, row);
				if (cost != 0)
				{
					const std::string planetName2 = dataTable.getColumnName (col);
					FATAL (cost < 0, (
						"TravelManagerNamespace::load: negative travel cost %i from row [%s] to [%s] (travel.iff). "
						"Non-positive costs break Dijkstra and can crash; use positive credits or 0 for no route.",
						cost, planetName1.c_str (), planetName2.c_str ()));
					int planetIndex2(0);
					if (!getPlanetIndex (planetName2, planetIndex2))
						FATAL (true, ("TravelManagerNamespace::load: column header planet %s could not be matched to row planet names (row %i).", planetName2.c_str (), row));

					int dummy(0);
					if (!TravelManager::getPlanetSingleHopCost (planetName1, planetName2, dummy))
					{
						// Canonical key uses (min,max) indices; do not swap planetIndex1 — it must stay this row's
						// planet for the rest of the inner loop (swap here caused wrong edges + OOB in Dijkstra).
						int lo = planetIndex1;
						int hi = planetIndex2;
						if (hi < lo)
							std::swap (lo, hi);

						ms_singleHopRouteList[std::make_pair (lo, hi)] = cost;

						DEBUG_REPORT_LOG (ms_debugReport, ("Added single hop travel route %s <--> %s costing %i\n", planetName1.c_str (), planetName2.c_str (), cost));
					}
				}
			}
		}

		// setup "least cost" routes, some of which may involve multiple hops
		int routeCost;
		size_t const planetCount = ms_planetNameToPlanetIndexList.size ();
		FATAL (planetCount > static_cast<size_t> (INT_MAX), ("TravelManagerNamespace::load: planet count exceeds %d; travel table is invalid.", INT_MAX));
		// after FATAL, planetCount <= INT_MAX; MSVC C4267 still flags size_t->int
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
#endif
		int const numberOfPlanets = static_cast<int> (planetCount);
		FATAL (
			static_cast<int> (ms_planetNameList.size ()) != numberOfPlanets,
			("TravelManagerNamespace::load: internal error — ms_planetNameList.size()=%d planetIndexMap.size()=%d.",
			 static_cast<int> (ms_planetNameList.size ()), numberOfPlanets));
		int indexPlanet1, indexPlanet2;
		// x64: std::priority_queue< pair<ll,int>, greater<> > Dijk hit 0xC5 after 040. Use O(n^2) per source (no heap);
		// one Dijkstra run per source fills all (source,t) with t>source.
		int const nPlanetsDijk = numberOfPlanets;
		static long long const kDijkInf = (1LL << 50);
		std::vector<char> djkUsed (static_cast<size_t> (nPlanetsDijk), 0);
		std::vector<long long> djkDist (static_cast<size_t> (nPlanetsDijk), kDijkInf);
		std::vector<int> djkPar (static_cast<size_t> (nPlanetsDijk), -1);
		for (indexPlanet1 = 0; indexPlanet1 < numberOfPlanets; ++indexPlanet1)
		{
			int const source = indexPlanet1;
			ms_anyHopLeastCostRouteList[std::make_pair (source, source)] = std::make_pair (0, std::vector<int> ());

			std::fill (djkDist.begin (), djkDist.end (), kDijkInf);
			std::fill (djkPar.begin (), djkPar.end (), -1);
			std::fill (djkUsed.begin (), djkUsed.end (), 0);
			djkDist[static_cast<size_t> (source)] = 0;
			for (int djkR = 0; djkR < nPlanetsDijk; ++djkR)
			{
				int u = -1;
				long long bestU = kDijkInf;
				for (int v = 0; v < nPlanetsDijk; ++v)
				{
					if (!djkUsed[static_cast<size_t> (v)] && djkDist[static_cast<size_t> (v)] < bestU)
					{
						bestU = djkDist[static_cast<size_t> (v)];
						u = v;
					}
				}
				if (u < 0 || bestU >= kDijkInf)
					break;
				djkUsed[static_cast<size_t> (u)] = 1;
				for (int w = 0; w < nPlanetsDijk; ++w)
				{
					if (u == w)
						continue;
					if (!getPlanetSingleHopCost (u, w, routeCost))
						continue;
					FATAL (routeCost < 0, (
						"TravelManagerNamespace::load: negative single-hop cost in graph (u=%i w=%i cost=%i); travel.iff data invalid.",
						u, w, routeCost));
					long long const dNext = bestU + static_cast<long long> (routeCost);
					FATAL (
						routeCost > 0 && dNext < bestU,
						("TravelManagerNamespace::load: Dijkstra distance overflow in relax (u=%i w=%i routeCost=%i).",
						 u, w, routeCost));
					if (dNext < djkDist[static_cast<size_t> (w)])
					{
						djkDist[static_cast<size_t> (w)] = dNext;
						djkPar[static_cast<size_t> (w)] = u;
					}
				}
			}

			for (indexPlanet2 = source + 1; indexPlanet2 < numberOfPlanets; ++indexPlanet2)
			{
				int const target = indexPlanet2;
				if (djkDist[static_cast<size_t> (target)] >= kDijkInf)
				{
					FATAL (true, ("TravelManagerNamespace::load: couldn't find least cost travel route from %s to %s", ms_planetNameList[static_cast<size_t> (source)].c_str (), ms_planetNameList[static_cast<size_t> (target)].c_str ()));
				}
				FATAL (djkDist[static_cast<size_t> (target)] > static_cast<long long> (INT_MAX), (
					"TravelManagerNamespace::load: Dijkstra total cost for %s to %s exceeds INT_MAX; graph data may be corrupt.",
					ms_planetNameList[static_cast<size_t> (source)].c_str (), ms_planetNameList[static_cast<size_t> (target)].c_str ()));
				FATAL (djkDist[static_cast<size_t> (target)] < 0, (
					"TravelManagerNamespace::load: negative Dijkstra distance for %s to %s (non-negative edge costs required).",
					ms_planetNameList[static_cast<size_t> (source)].c_str (), ms_planetNameList[static_cast<size_t> (target)].c_str ()));
				int const bestTotalCost = static_cast<int> (djkDist[static_cast<size_t> (target)]);

				std::vector<int> djkPath;
				djkPath.reserve (static_cast<size_t> (nPlanetsDijk + 1));
				{
					int c = target;
					int steps = 0;
					while (c != -1)
					{
						FATAL (c < 0 || c >= nPlanetsDijk, (
							"TravelManagerNamespace::load: Dijkstra parent walk OOB c=%i nPlanets=%i (source=%s target=%s).",
							c, nPlanetsDijk,
							ms_planetNameList[static_cast<size_t> (source)].c_str (),
							ms_planetNameList[static_cast<size_t> (target)].c_str ()));
						djkPath.push_back (c);
						++steps;
						FATAL (steps > nPlanetsDijk + 1, (
							"TravelManagerNamespace::load: Dijkstra parent chain cycle or corrupt parent pointers (source=%s target=%s steps=%i). "
							"Usually caused by negative edge weights or bad travel.iff — see earlier validation.",
							ms_planetNameList[static_cast<size_t> (source)].c_str (),
							ms_planetNameList[static_cast<size_t> (target)].c_str (),
							steps));
						c = djkPar[static_cast<size_t> (c)];
					}
				}
				if (djkPath.empty () || djkPath.back () != source)
					FATAL (true, ("TravelManagerNamespace::load: Dijkstra path rebuild failed (internal) for %s to %s", ms_planetNameList[static_cast<size_t> (source)].c_str (), ms_planetNameList[static_cast<size_t> (target)].c_str ()));
				std::reverse (djkPath.begin (), djkPath.end ());
				{
					int const pathFront = djkPath.front ();
					int const pathBack = djkPath.back ();
					if (pathFront != source || pathBack != target)
						FATAL (true, (
							"TravelManagerNamespace::load: Dijkstra path endpoints mismatch: source=%d target=%d front=%d back=%d size=%u.",
							source, target, pathFront, pathBack, static_cast<unsigned int> (djkPath.size ())));
				}
				std::vector<int> routeList;
				for (size_t pi = 1; pi + 1 < djkPath.size (); ++pi)
					routeList.push_back (djkPath[pi]);
				{
					for (std::vector<int>::const_iterator rli = routeList.begin (); rli != routeList.end (); ++rli)
					{
						int const h = *rli;
						FATAL (h < 0 || h >= nPlanetsDijk, (
							"TravelManagerNamespace::load: Dijkstra route has invalid planet index %i (nPlanets=%i).", h, nPlanetsDijk));
					}
				}

				if (ms_debugReport)
				{
					std::string trRoute = ms_planetNameList[static_cast<size_t> (source)];
					for (std::vector<int>::const_iterator hIt = routeList.begin (); hIt != routeList.end (); ++hIt)
					{
						trRoute += " <--> ";
						trRoute += ms_planetNameList[static_cast<size_t> (*hIt)];
					}
					trRoute += " <--> ";
					trRoute += ms_planetNameList[static_cast<size_t> (target)];

					DEBUG_REPORT_LOG (ms_debugReport, ("Added least cost travel route %s costing %i\n", trRoute.c_str (), bestTotalCost));
					if (getPlanetSingleHopCost (source, target, routeCost) && (routeCost > bestTotalCost))
					{
						DEBUG_REPORT_LOG (ms_debugReport, (
							"Single hop travel route %s <--> %s (%i) cost ***MORE*** than least cost travel route %s (%i)\n",
							ms_planetNameList[static_cast<size_t> (source)].c_str (), ms_planetNameList[static_cast<size_t> (target)].c_str (), routeCost, trRoute.c_str (), bestTotalCost));
					}
				}
				{
					std::pair<int, std::vector<int>> hopEntry (bestTotalCost, std::move (routeList));
					ms_anyHopLeastCostRouteList[std::make_pair (source, target)] = std::move (hopEntry);
				}
			}
		}

		// sanity: every planet pair resolvable (single map lookup per pair; avoids redundant string->index work)
		for (indexPlanet1 = 0; indexPlanet1 < numberOfPlanets; ++indexPlanet1)
		{
			for (indexPlanet2 = 0; indexPlanet2 < numberOfPlanets; ++indexPlanet2)
			{
				if (!getPlanetAnyHopLeastCost (indexPlanet1, indexPlanet2, routeCost))
					FATAL (true, (
						"TravelManagerNamespace::load: couldn't find least cost travel route from %s to %s (indices %i,%i).",
						ms_planetNameList[static_cast<size_t> (indexPlanet1)].c_str (),
						ms_planetNameList[static_cast<size_t> (indexPlanet2)].c_str (),
						indexPlanet1, indexPlanet2));
			}
		}
	}
	// else: original build only used OutputDebugString; open failure left tables empty
}

// ----------------------------------------------------------------------

bool TravelManagerNamespace::getPlanetIndex (const std::string& planetName, int& planetIndex)
{
	PlanetNameToPlanetIndexMap::const_iterator iterFind = ms_planetNameToPlanetIndexList.find(planetName);
	if (iterFind == ms_planetNameToPlanetIndexList.end())
		return false;

	planetIndex = iterFind->second;
	return true;
}

// ----------------------------------------------------------------------

bool TravelManagerNamespace::getPlanetSingleHopCost(int planetIndex1, int planetIndex2, int& planetCost)
{
	if (planetIndex2 < planetIndex1)
		std::swap (planetIndex1, planetIndex2);

	SingleHopRouteList::const_iterator iterFind = ms_singleHopRouteList.find(std::make_pair(planetIndex1, planetIndex2));
	if (iterFind != ms_singleHopRouteList.end())
	{
		planetCost = iterFind->second;
		return true;
	}

	return false;
}

// ----------------------------------------------------------------------

bool TravelManagerNamespace::getPlanetAnyHopLeastCost(int planetIndex1, int planetIndex2, int& planetCost)
{
	if (planetIndex2 < planetIndex1)
		std::swap (planetIndex1, planetIndex2);

	AnyHopLeastCostRouteList::const_iterator iterFind = ms_anyHopLeastCostRouteList.find(std::make_pair(planetIndex1, planetIndex2));
	if (iterFind != ms_anyHopLeastCostRouteList.end())
	{
		planetCost = iterFind->second.first;
		return true;
	}

	return false;
}

// ======================================================================
