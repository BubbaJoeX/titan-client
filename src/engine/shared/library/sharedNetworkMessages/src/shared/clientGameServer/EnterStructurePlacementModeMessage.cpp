// ==================================================================
//
// EnterStructurePlacementModeMessage.cpp
// Copyright 2002 Sony Online Entertainment
//
// ==================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/EnterStructurePlacementModeMessage.h"

// ==================================================================

const char* const EnterStructurePlacementModeMessage::cms_name = "EnterStructurePlacementModeMessage";

// ==================================================================

EnterStructurePlacementModeMessage::EnterStructurePlacementModeMessage (const NetworkId& deedNetworkId, const std::string& structureSharedObjectTemplateName, float const claimFootprintRadiusMeters) :
	GameNetworkMessage ("EnterStructurePlacementModeMessage"),
	m_deedNetworkId (deedNetworkId),
	m_structureSharedObjectTemplateName (structureSharedObjectTemplateName),
	m_claimFootprintRadiusMeters (claimFootprintRadiusMeters)
{
	addVariable (m_deedNetworkId);
	addVariable (m_structureSharedObjectTemplateName);
	addVariable (m_claimFootprintRadiusMeters);
}

// ------------------------------------------------------------------

EnterStructurePlacementModeMessage::EnterStructurePlacementModeMessage (Archive::ReadIterator& source) :
	GameNetworkMessage ("EnterStructurePlacementModeMessage"),
	m_deedNetworkId (),
	m_structureSharedObjectTemplateName (),
	m_claimFootprintRadiusMeters (-1.f)
{
	addVariable (m_deedNetworkId);
	addVariable (m_structureSharedObjectTemplateName);
	addVariable (m_claimFootprintRadiusMeters);
	unpack (source);
}

// ------------------------------------------------------------------

EnterStructurePlacementModeMessage::~EnterStructurePlacementModeMessage ()
{
}

// ------------------------------------------------------------------

const NetworkId& EnterStructurePlacementModeMessage::getDeedNetworkId () const
{
	return m_deedNetworkId.get ();
}

// ------------------------------------------------------------------

const std::string& EnterStructurePlacementModeMessage::getStructureSharedObjectTemplateName () const
{
	return m_structureSharedObjectTemplateName.get ();
}

// ------------------------------------------------------------------

float EnterStructurePlacementModeMessage::getClaimFootprintRadiusMeters () const
{
	return m_claimFootprintRadiusMeters.get ();
}

// ==================================================================
