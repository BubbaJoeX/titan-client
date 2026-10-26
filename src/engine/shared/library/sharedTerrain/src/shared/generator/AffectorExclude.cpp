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
	TerrainGenerator::Affector (TAG_AEXC, TGAT_exclude),
	m_fine (false)
{
}

//-------------------------------------------------------------------

AffectorExclude::~AffectorExclude ()
{
}

//-------------------------------------------------------------------

unsigned AffectorExclude::getAffectedMaps() const
{
	//-- fine exclusion also moves vertex positions onto the edge
	return m_fine ? (TGM_exclude | TGM_vertexPosition) : TGM_exclude;
}

//-------------------------------------------------------------------

bool AffectorExclude::isFine () const
{
	return m_fine;
}

//-------------------------------------------------------------------

void AffectorExclude::setFine (const bool fine)
{
	m_fine = fine;
}

//-------------------------------------------------------------------

void AffectorExclude::affect (const float /*worldX*/, const float /*worldZ*/, const int x, const int z, const float amount, const TerrainGenerator::GeneratorChunkData& generatorChunkData) const
{
	//-- fine exclusion is applied per tile once all layers ran
	if (amount > 0.f && !m_fine)
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

	case TAG_0001:
		load_0001 (iff);
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

void AffectorExclude::load_0001 (Iff& iff)
{
	iff.enterForm (TAG_0001);

		LayerItem::load (iff);

		iff.enterChunk (TAG_DATA);
			m_fine = iff.read_int32 () != 0;
		iff.exitChunk (TAG_DATA, true);

	iff.exitForm (TAG_0001, true);
}

//-------------------------------------------------------------------

void AffectorExclude::save (Iff& iff) const
{
	//-- stock version unless the cut is fine
	const Tag version = m_fine ? TAG_0001 : TAG_0000;
	iff.insertForm (version);

		//-- save the base
		LayerItem::save (iff);

		//-- save specific data
		iff.insertChunk (TAG_DATA);
			if (m_fine)
				iff.insertChunkData (static_cast<int32> (1));
		iff.exitChunk (TAG_DATA);

	iff.exitForm (version);
}

//-------------------------------------------------------------------

