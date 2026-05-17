// ======================================================================
//
// ClaimManipulateStateMessage.h
//
// Tells the client whether the local player may drop/manipulate in the
// open-world claim footprint they are standing in.
//
// ======================================================================

#ifndef INCLUDED_ClaimManipulateStateMessage_H
#define INCLUDED_ClaimManipulateStateMessage_H

// ======================================================================

#include "sharedNetworkMessages/GameNetworkMessage.h"

// ======================================================================

class ClaimManipulateStateMessage : public GameNetworkMessage
{
public:
	static char const * const cms_name;

	ClaimManipulateStateMessage(bool canManipulate);
	explicit ClaimManipulateStateMessage(Archive::ReadIterator & source);
	~ClaimManipulateStateMessage();

	bool getCanManipulate() const;

private:
	Archive::AutoVariable<unsigned char> m_canManipulate;

	ClaimManipulateStateMessage(ClaimManipulateStateMessage const &);
	ClaimManipulateStateMessage & operator=(ClaimManipulateStateMessage const &);
};

// ======================================================================

#endif
