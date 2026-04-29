// ======================================================================
//
// DeveloperWaterLevelMessage.h
// Server -> client: full list of local water table patches.
//
// ======================================================================

#ifndef INCLUDED_DeveloperWaterLevelMessage_H
#define INCLUDED_DeveloperWaterLevelMessage_H

#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "sharedTerrain/LocalWaterTablePatchArchive.h"
#include "Archive/AutoDeltaByteStream.h"

#include <vector>

class DeveloperWaterLevelMessage : public GameNetworkMessage
{
public:

	explicit DeveloperWaterLevelMessage (std::vector<LocalWaterTablePatch> const & patches);
	explicit DeveloperWaterLevelMessage (Archive::ReadIterator & source);
	virtual ~DeveloperWaterLevelMessage ();

	std::vector<LocalWaterTablePatch> getPatches () const;

	/** Legacy single-float helper when code expects one global delta; prefers the lone patch's delta. */
	float getDeltaMeters () const;

	static char const * const cms_name;

private:

	Archive::AutoArray<LocalWaterTablePatch> m_patches;
};

#endif
