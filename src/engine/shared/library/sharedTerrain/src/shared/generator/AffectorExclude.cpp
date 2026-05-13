//
// AffectorExclude.cpp
// asommers 9-30-2000
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#include "sharedTerrain/FirstSharedTerrain.h"
#include "sharedTerrain/AffectorExclude.h"

#include "sharedFile/Iff.h"

//-------------------------------------------------------------------
//
// AffectorExclude
//
AffectorExclude::AffectorExclude () :
	TerrainGenerator::Affector (TAG_AEXC, TGAT_exclude)
{
}

//-------------------------------------------------------------------

AffectorExclude::~AffectorExclude ()
{
}

//-------------------------------------------------------------------

unsigned AffectorExclude::getAffectedMaps() const
{
	return TGM_exclude;
}

//-------------------------------------------------------------------

void AffectorExclude::affect (const float /*worldX*/, const float /*worldZ*/, const int x, const int z, const float amount, const TerrainGenerator::GeneratorChunkData& generatorChunkData) const
{
	if (amount > 0.f)
		generatorChunkData.excludeMap->setData (x, z, true);
}

//-------------------------------------------------------------------

void AffectorExclude::load (Iff& iff)
{
	switch (iff.getCurrentName ())
	{
	case TAG_0000:
		load_0000 (iff);
		break;

	default:
		{
			char tagBuffer [5];
			ConvertTagToString (iff.getCurrentName (), tagBuffer);

			char buffer [128];
			iff.formatLocation (buffer, sizeof (buffer));
			DEBUG_FATAL (true, ("invalid AffectorExclude version %s/%s", buffer, tagBuffer));
		}
		break;
	}
}

//-------------------------------------------------------------------

void AffectorExclude::load_0000 (Iff& iff)
{
	iff.enterForm (TAG_0000);

		//-- load the base data
		LayerItem::load (iff);

		//-- Recover from truncated Generator IFF: dangling bytes before the sibling DATA chunk
		// would otherwise assert in getFirstTag() when probing the next chunk header.
		iff.discardIncompleteTrailingIFFBlockHeadersInCurrentForm ();

		if (iff.enterChunk (TAG_DATA, true))
			iff.exitChunk (TAG_DATA, true);

		iff.discardIncompleteTrailingIFFBlockHeadersInCurrentForm ();

	iff.exitForm (TAG_0000, true);
}

//-------------------------------------------------------------------

void AffectorExclude::save (Iff& iff) const
{
	iff.insertForm (TAG_0000);

		//-- save the base
		LayerItem::save (iff);

		//-- save specific data
		iff.insertChunk (TAG_DATA);
		iff.exitChunk (TAG_DATA);

	iff.exitForm (TAG_0000);
}

//-------------------------------------------------------------------

