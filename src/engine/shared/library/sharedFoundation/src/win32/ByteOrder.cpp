// ======================================================================
//
// ByteOrder.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/ByteOrder.h"

// ======================================================================

#include <stdlib.h>

ulong ntohl(ulong netLong)
{
	return _byteswap_ulong(netLong);
}

ulong htonl(ulong hostLong)
{
	return _byteswap_ulong(hostLong);
}

ushort ntohs(ushort netShort)
{
	return _byteswap_ushort(netShort);
}

ushort htons(ushort hostShort)
{
	return _byteswap_ushort(hostShort);
}

// ======================================================================
