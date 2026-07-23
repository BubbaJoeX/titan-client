#ifndef SwgCuiAvatarSelection_H
#define SwgCuiAvatarSelection_H

//-----------------------------------------------------------------

#include "sharedMessageDispatch/Receiver.h"
#include "UIEventCallback.h"
#include "Unicode.h"
#include "clientUserInterface/CuiMediator.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkId.h"

class CreatureObject;
class Object;
class CuiMessageBox;
class CuiWidget3dObjectListViewer;
class UIButton;
class UICheckbox;
class UIDataSource;
class UIList;
class UIPage;
class UIText;
class UITable;
class CuiLoginManagerAvatarInfo;
class CuiLoginManagerClusterInfo;
class SwgCuiDeleteAvatarConfirmation;

namespace MessageDispatch
{
	class Callback;
}

//-----------------------------------------------------------------

/**
*
*/
class SwgCuiAvatarSelection :
public CuiMediator,
public UIEventCallback,
public MessageDispatch::Receiver
{
public:

	explicit                   SwgCuiAvatarSelection     (UIPage & page);

	//-- PS UI callbacks
	void                       OnButtonPressed           ( UIWidget *context );
	void                       OnGenericSelectionChanged ( UIWidget *context );
	bool                       OnMessage                 ( UIWidget *context, const UIMessage & msg );
	void					   OnCheckboxSet             ( UIWidget *context );
	void					   OnCheckboxUnset           ( UIWidget *context );

	void                       receiveMessage            (const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message);

	void                       clearCharacterList        ();

	void                       onClusterConnection       (bool b);
	void                       onStartScene              (bool b);

	void                       onSceneChanged            (bool);
	void                       onAvatarListChanged       (bool b);
	void                       onClusterStatusChanged	 (bool b);
	void                       onDeleteAvatarConfirmation(CuiLoginManagerAvatarInfo const &info);

	void                       update                    (float deltaTimeSecs);

protected:

	void                       performActivate           ();
	void                       performDeactivate         ();

private:
	                          ~SwgCuiAvatarSelection      ();
	                           SwgCuiAvatarSelection      (const SwgCuiAvatarSelection &);
	SwgCuiAvatarSelection &    operator=                  (const SwgCuiAvatarSelection &);

	void                       refreshList                (bool updateSelection);

	void                       addAvatar                  (const CuiLoginManagerAvatarInfo & avatarInfo);

	void                       updateAvatarSelection      ();
	void                       requestAvatarSelection     ();
	void                       requestAvatarDeletion      ();
	bool                       getCurrentlySelectedAvatar (bool checkCluster = true);
	void                       performDelete              ();
	bool                       autoConnectOk              () const;

	void                       reconnectLoginServer       (bool forDelete);
	void                       handleCreate               ();

	void                       ensureViewerBehindChrome   ();
	void                       layoutAvatarViewers        ();
	void                       advanceCarousel           (float deltaTimeSecs);
	void                       rotateCarousel            (int steps);
	void                       populateAvatarViewers      ();
	void                       selectAvatarIndex          (int index);
	void                       playHoverAnimation         (int index);
	int                        findViewerIndex             (UIWidget const * widget) const;
	Object *                   getSlotPlaceholder         (int index);
	void                       updateSlotSelectionDisplay ();
	void                       beginWelcomeText           ();
	void                       completeWelcomeText        ();

private:
	enum
	{
		MaxAvatarViewers = 10
	};

	UIButton *                 m_okButton;
	UIButton *                 m_cancelButton;

	UIButton *                 m_createButton;
	UIButton *                 m_deleteButton;

	UIText *                   m_avatarNameText;
	UIText *                   m_avatarDetailsText;
	UIText *                   m_noCharactersText;
	UIPage *                   m_actionPage;

	UITable *                  m_table;

	CuiWidget3dObjectListViewer * m_objectViewer;
	CuiWidget3dObjectListViewer * m_avatarViewers[MaxAvatarViewers];
	UIText *                      m_avatarLabels[MaxAvatarViewers];
	CreatureObject *              m_avatarCreatures[MaxAvatarViewers];
	int                           m_avatarViewerCount;
	int                           m_selectedAvatarIndex;
	float                         m_carouselPosition;
	float                         m_carouselTargetPosition;
	int                           m_hoveredAvatarIndex;
	bool                          m_hoverAnimationPlayed[MaxAvatarViewers];
	UISize                        m_lastLayoutSize;

	CuiMessageBox *            m_messageBox;
	CuiMessageBox *            m_messageBoxDeleteWait;
	CuiMessageBox *            m_messageBoxLoginWait;

	bool                       m_waitingLoginForDelete;
	bool                       m_waitingLogin;
	bool                       m_waitingLoginForSelect;
	bool                       m_waitingLoginForCreate;

	bool                       m_autoConnected;

	bool                       m_proceed;

	MessageDispatch::Callback *       m_callback;

	CuiLoginManagerAvatarInfo *       m_selectedAvatar;
	bool                              m_waitingDeletion;
	CuiLoginManagerAvatarInfo *       m_deletingAvatar;

	bool                              m_refreshingCharacterList;

	bool                              m_waitingForConnection;
	bool                              m_dropFromCluster;
	uint32                            m_waitingForClusterId;

	float                             m_connectionTimeout;
	bool                              m_connectingToGame;

	bool                              m_avatarPopulateFirstTime;

	UIPage *                          m_deleteAvatarConfirmationPage;
	SwgCuiDeleteAvatarConfirmation *  m_deleteAvatarConfirmationMediator;

	UICheckbox *					  m_hideClosed;
	UICheckbox *                      m_showAvailableSlots;
	UIText *                          m_moreSlotsText;
	Object *                          m_slotPlaceholders[MaxAvatarViewers];
	int                               m_realAvatarCount;
	int                               m_slotPlaceholderCount;
	uint32                            m_slotClusterId;
	bool                              m_placeholderAssetChecked;
	bool                              m_placeholderAssetAvailable;
	bool                              m_welcomeInitialized;
	bool                              m_welcomeComplete;
	float                             m_welcomeElapsed;
	Unicode::String                   m_welcomeFullText;

	bool                              m_waitForConnectionRetry;
	bool                              m_hasAlreadyRetriedConnection;
};

//-----------------------------------------------------------------

#endif

//-----------------------------------------------------------------
