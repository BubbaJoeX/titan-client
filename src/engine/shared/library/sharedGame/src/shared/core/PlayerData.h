//========================================================================
//
// PlayerData.h
//
// copyright 2001 Sony Online Entertainment
//
//========================================================================

#ifndef INCLUDED_PlayerData_H
#define INCLUDED_PlayerData_H

namespace PlayerDataPriviledgedTitle
{
	// DO NOT DELETE OR CHANGE THE ORDER OF THESE ENUMS,
	// BECAUSE THEY ARE PERSISTED IN THE DB; ADD NEW ONES
	// AT THE BOTTOM
	enum
	{
		NormalPlayer = 0,
		CustomerServiceRepresentative = 1,
		Developer = 2,
		Warden = 3,
		QualityAssurance = 4,
	};

	// /object setAdminTitle <1-50>: stored privileged title = base + level (avoids collision with legacy 1-4).
	int const kStaffRankDisplayEncodedBase = 16;

	inline bool isStaffRankDisplayEncodedTitle(int8 v)
	{
		return v >= static_cast<int8>(kStaffRankDisplayEncodedBase + 1)
			&& v <= static_cast<int8>(kStaffRankDisplayEncodedBase + 50);
	}

	inline int staffRankDisplayLevelFromEncodedTitle(int8 v)
	{
		return static_cast<int>(v) - kStaffRankDisplayEncodedBase;
	}
}	// namespace PlayerDataPriviledgedTitle

//========================================================================

#endif	// INCLUDED_PlayerData_H
