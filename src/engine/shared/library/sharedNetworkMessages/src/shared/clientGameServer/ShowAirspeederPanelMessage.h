// ==================================================================
//
// ShowAirspeederPanelMessage.h
// Copyright 2024
//
// ==================================================================

#ifndef	INCLUDED_ShowAirspeederPanelMessage_H
#define	INCLUDED_ShowAirspeederPanelMessage_H

// ==================================================================

#include "Archive/AutoDeltaByteStream.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"

// ==================================================================

class ShowAirspeederPanelMessage : public GameNetworkMessage
{
public:

	explicit ShowAirspeederPanelMessage(bool show);
	explicit ShowAirspeederPanelMessage(Archive::ReadIterator &source);
	virtual ~ShowAirspeederPanelMessage();

	bool getShow() const;

public:

	static const char* const cms_name;

private:

	Archive::AutoVariable<bool> m_show;
};

// ==================================================================

#endif
