// ======================================================================
//
// StaffRankDataTable.h
// Staff titles keyed by account admin level (0-50), from staff_ranks.iff.
//
// ======================================================================

#ifndef INCLUDED_StaffRankDataTable_H
#define INCLUDED_StaffRankDataTable_H

#include <string>

// ======================================================================

class StaffRankDataTable
{
public:
	static void install();
	static void remove();

	/** Title for an admin level; clamped to 0..max. Non-staff (0) is PLAYER. */
	static std::string getRankTitle(int adminLevel);

	/** Highest level defined in the table (typically 50). */
	static int getMaxDefinedLevel();

private:
	StaffRankDataTable();
	StaffRankDataTable(StaffRankDataTable const &);
	StaffRankDataTable &operator=(StaffRankDataTable const &);
};

// ======================================================================

#endif // INCLUDED_StaffRankDataTable_H
