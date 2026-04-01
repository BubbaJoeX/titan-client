#ifndef SWGMAYAEDITOR_SWGTRANSLATORNAMES_H
#define SWGMAYAEDITOR_SWGTRANSLATORNAMES_H

/**
 * Maya file translators: short ASCII ids for registration (MEL/VG `file -import -type "SwgSat"`).
 * Do not use *, parentheses-only names, or Maya reserved types (e.g. SAT_ATF = ACIS solid, not SWG .sat).
 *
 * Human-readable lines for the file dialog come from filter() using kFilter* strings.
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

	// MPxFileTranslator::filter() — shown in "Files of type" next to the pattern
	inline constexpr char const kFilterMgn[] = "SWG skeletal mesh (*.mgn)";
	inline constexpr char const kFilterMsh[] = "SWG static mesh (*.msh *.apt)";
	inline constexpr char const kFilterSkt[] = "SWG skeleton (*.skt)";
	inline constexpr char const kFilterAns[] = "SWG animation (*.ans)";
	inline constexpr char const kFilterFlr[] = "SWG floor (*.flr)";
	inline constexpr char const kFilterSat[] = "SWG skeletal appearance (*.sat)";
	inline constexpr char const kFilterPob[] = "SWG portal object (*.pob)";
	inline constexpr char const kFilterDds[] = "SWG DDS texture (*.dds)";
}

#endif
