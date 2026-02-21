// ==================================================================
//
// ShowAirspeederPanelMessage.cpp
// Copyright 2024
//
// ==================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/ShowAirspeederPanelMessage.h"

// ==================================================================

const char* const ShowAirspeederPanelMessage::cms_name = "ShowAirspeederPanelMessage";

// ==================================================================

ShowAirspeederPanelMessage::ShowAirspeederPanelMessage(bool show) :
	GameNetworkMessage("ShowAirspeederPanelMessage"),
	m_show(show)
{
	addVariable(m_show);
}

// ------------------------------------------------------------------

ShowAirspeederPanelMessage::ShowAirspeederPanelMessage(Archive::ReadIterator& source) :
	GameNetworkMessage("ShowAirspeederPanelMessage"),
	m_show(false)
{
	addVariable(m_show);
	unpack(source);
}

// ------------------------------------------------------------------

ShowAirspeederPanelMessage::~ShowAirspeederPanelMessage()
{
}

// ------------------------------------------------------------------

bool ShowAirspeederPanelMessage::getShow() const
{
	return m_show.get();
}

// ==================================================================
