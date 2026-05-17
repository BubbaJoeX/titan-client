// ======================================================================
//
// ClientClaimManipulateState.h
//
// Notifies UI when the server changes open-claim manipulate permission.
// UI registers a callback from clientUserInterface (no clientGame->UI dep).
//
// ======================================================================

#ifndef INCLUDED_ClientClaimManipulateState_H
#define INCLUDED_ClientClaimManipulateState_H

// ======================================================================

class ClientClaimManipulateState
{
public:
	typedef void (*NotifyFunc)(bool canManipulate);

	static void setNotifyFunc(NotifyFunc func);
	static void notify(bool canManipulate);

private:
	static NotifyFunc ms_notifyFunc;
};

// ======================================================================

#endif
