// ======================================================================
//
// LocalWaterTablePatchArchive.h
// Archive read/write for LocalWaterTablePatch — required before
// Archive::AutoArray<LocalWaterTablePatch> instantiates pack/unpack.
//
// ======================================================================

#ifndef INCLUDED_LocalWaterTablePatchArchive_H
#define INCLUDED_LocalWaterTablePatchArchive_H

#include "sharedTerrain/TerrainWaterLevelDeveloperDelta.h"
#include "Archive/Archive.h"

namespace Archive
{
	inline void get (ReadIterator & source, LocalWaterTablePatch & target)
	{
		get (source, target.centerX);
		get (source, target.centerZ);
		get (source, target.radius);
		get (source, target.deltaMeters);
	}

	inline void put (ByteStream & target, LocalWaterTablePatch const & source)
	{
		put (target, source.centerX);
		put (target, source.centerZ);
		put (target, source.radius);
		put (target, source.deltaMeters);
	}
}

#endif
