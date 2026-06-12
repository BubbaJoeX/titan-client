// ======================================================================
//
// ClaimClientMessageDispatch.h
//
// Decode claim network messages for clientGame without pulling Archive
// NetworkId operators into clientGame translation units.
//
// ======================================================================

#ifndef INCLUDED_ClaimClientMessageDispatch_H
#define INCLUDED_ClaimClientMessageDispatch_H

// ======================================================================

namespace Archive
{
	class ReadIterator;
}

class NetworkId;

namespace ClaimClientMessageDispatch
{
	extern char const * const cms_claimFootprintSync;
	extern char const * const cms_claimManipulateState;
	extern char const * const cms_sceneEndBaselines;

	bool decodeManipulateState(Archive::ReadIterator & source);
	void decodeFootprintSync(Archive::ReadIterator & source, NetworkId & markerIdOut, float & centerXOut, float & centerYOut, float & centerZOut, float & radiusMetersOut, bool & activeOut);
	void decodeSceneEndBaselines(Archive::ReadIterator & source, NetworkId & networkIdOut);
}

// ======================================================================

#endif
