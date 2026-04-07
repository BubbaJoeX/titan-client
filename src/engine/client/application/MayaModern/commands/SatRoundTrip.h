#ifndef SWGMAYAEDITOR_SATROUNDTRIP_H
#define SWGMAYAEDITOR_SATROUNDTRIP_H

#include "Tag.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class Iff; // sharedFile/Iff.h in .cpp

/**
 * LATX: maps skeleton template path (.skt) -> logical animation table (.lat) for ash controllers.
 * LDTB: per-LOD distance bands (linear min/max in world units); client squares them for culling.
 * SFSK: "must use soft skinning" flag on SkeletalAppearance2.
 * APAG: "always play action generator animations" flag.
 */
/** Preserves SMAT tail blocks in file order for round-trip. */
struct SatTrailingItem
{
	/** false: IFF chunk with tag + payload bytes; true: nested FORM copied as a complete root IFF (one FORM). */
	bool isForm = false;
	Tag tag{};
	std::vector<std::uint8_t> payload;
};

struct SatRoundTripData
{
	std::string versionTag;
	std::string infoExtraString;
	bool createAnimationController = false;
	std::vector<std::string> meshGenerators;
	std::vector<std::pair<std::string, std::string>> skeletonTemplates;
	std::vector<std::pair<std::string, std::string>> latx;
	std::vector<std::pair<float, float>> lodDistances;
	bool hasSoftSkinningChunk = false;
	std::uint8_t softSkinning = 0;
	bool hasApagChunk = false;
	std::uint8_t apag = 0;
	std::vector<SatTrailingItem> trailing;
};

namespace sat_round_trip
{
/** After SKTI exit; iff still inside SMAT version form. Parses LATX, LDTB, SFSK, APAG, then any remaining chunks / nested FORMs (preserved in order). */
bool parseTailAfterSkti(Iff& iff, SatRoundTripData& out);

/** Build SMAT IFF (entire file) from round-trip data. */
bool writeSmatFile(const char* filePath, const SatRoundTripData& data);

std::string serializePayload(const SatRoundTripData& data);
bool deserializePayload(const std::string& s, SatRoundTripData& out);

/** Create a locator-style transform holding storable string attribute `swgSmatRoundTrip` for export. */
bool createMetadataNode(const std::string& mayaBasename, const std::string& payloadUtf8);
} // namespace sat_round_trip

#endif
