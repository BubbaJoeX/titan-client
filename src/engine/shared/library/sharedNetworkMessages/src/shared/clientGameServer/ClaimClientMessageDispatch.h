// ======================================================================
//
// ClaimClientMessageDispatch.h
//
// Decode claim network messages for clientGame without including message
// class bodies (avoids Archive template instantiation in clientGame TUs).
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

	bool decodeManipulateState(Archive::ReadIterator & source);
	void decodeFootprintSync(Archive::ReadIterator & source, NetworkId & markerIdOut, float & centerXOut, float & centerYOut, float & centerZOut, float & radiusMetersOut, bool & activeOut);
}

// ======================================================================

#endif
