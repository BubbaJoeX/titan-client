// ======================================================================
//
// DeveloperWaterLevelMessage.cpp
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/DeveloperWaterLevelMessage.h"

// ----------------------------------------------------------------------

char const * const DeveloperWaterLevelMessage::cms_name = "DeveloperWaterLevelMessage";

DeveloperWaterLevelMessage::DeveloperWaterLevelMessage (std::vector<LocalWaterTablePatch> const & patches) :
	GameNetworkMessage (DeveloperWaterLevelMessage::cms_name),
	m_patches ()
{
	addVariable (m_patches);
	m_patches.set (patches);
}

DeveloperWaterLevelMessage::DeveloperWaterLevelMessage (Archive::ReadIterator & source) :
	GameNetworkMessage (DeveloperWaterLevelMessage::cms_name),
	m_patches ()
{
	addVariable (m_patches);
	unpack (source);
}

DeveloperWaterLevelMessage::~DeveloperWaterLevelMessage ()
{
}

std::vector<LocalWaterTablePatch> DeveloperWaterLevelMessage::getPatches () const
{
	return m_patches.get ();
}

float DeveloperWaterLevelMessage::getDeltaMeters () const
{
	std::vector<LocalWaterTablePatch> const v = getPatches ();
	if (v.size () == 1)
		return v[0].deltaMeters;
	return 0.f;
}
