#ifndef SWGMAYAEDITOR_SWGTRANSLATORNAMES_H
#define SWGMAYAEDITOR_SWGTRANSLATORNAMES_H

/**
 * Maya file translators: short ASCII ids for registration (MEL/VG `file -import -type "SwgSat"`).
 * Do not use Maya reserved translator names (e.g. SAT_ATF = ACIS solid, not SWG .sat).
 *
 * filter() must follow Maya's documented form (see MPxFileTranslator::filter): wildcard patterns like *.obj.
 * The devkit Alembic sample returns "*.abc" only. Strings such as "Label (*.ext)" can break the Maya 2026
 * import dialog (blank "Files of type" rows). Use plain patterns; pick the type via this name in MEL if needed.
 */
namespace swg_translator
{
	// Registration / -type (alphanumeric + underscore only)
	inline constexpr char const kTypeMgn[] = "SwgMgn";
	inline constexpr char const kTypeMsh[] = "SwgMsh";
	inline constexpr char const kTypeSkt[] = "SwgSkt";
	inline constexpr char const kTypeAns[] = "SwgAns";
	inline constexpr char const kTypeFlr[] = "SwgFlr";
	inline constexpr char const kTypeSat[] = "SwgSat";
	inline constexpr char const kTypePob[] = "SwgPob";
	inline constexpr char const kTypeDds[] = "SwgDds";
	inline constexpr char const kTypeLod[] = "SwgLod";
	inline constexpr char const kTypeLmg[] = "SwgLmg";
	inline constexpr char const kTypeLsb[] = "SwgLsb";

	// MPxFileTranslator::filter() — patterns only (Maya 2026 / Qt file dialog)
	inline constexpr char const kFilterMgn[] = "*.mgn";
	inline constexpr char const kFilterMsh[] = "*.msh *.apt";
	inline constexpr char const kFilterSkt[] = "*.skt *.lod";
	inline constexpr char const kFilterAns[] = "*.ans";
	inline constexpr char const kFilterFlr[] = "*.flr";
	inline constexpr char const kFilterSat[] = "*.sat";
	inline constexpr char const kFilterPob[] = "*.pob";
	inline constexpr char const kFilterDds[] = "*.dds";
	inline constexpr char const kFilterLod[] = "*.lod";
	inline constexpr char const kFilterLmg[] = "*.lmg";
	inline constexpr char const kFilterLsb[] = "*.lsb";
}

#endif
