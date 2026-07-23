// ======================================================================
//
// SwgCuiAvatarSelection.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAvatarSelection.h"

#include "Archive/ByteStream.h"
#include "UIButton.h"
#include "UIBaseObject.h"
#include "UICheckbox.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UITable.h"
#include "UITableModelDefault.h"
#include "UIText.h"
#include "UIUtils.h"
#include "UnicodeUtils.h"
#include "clientGame/ConfigClientGame.h"
#include "clientGame/ConnectionManager.h"
#include "clientGame/CreatureController.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientGame/RoadmapManager.h"
#include "clientUserInterface/CuiCachedAvatarManager.h"
#include "clientUserInterface/CuiAnimationManager.h"
#include "clientUserInterface/CuiLoadingManager.h"
#include "clientUserInterface/CuiLoginManager.h"
#include "clientUserInterface/CuiLoginManagerAvatarInfo.h"
#include "clientUserInterface/CuiLoginManagerClusterInfo.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiStringIds.h"
#include "clientUserInterface/CuiStringIdsServer.h"
#include "clientUserInterface/CuiTransition.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedFoundation/ApplicationVersion.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Branch.h"
#include "sharedFoundation/Production.h"
#include "sharedMathArchive/VectorArchive.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/ClientCentralMessages.h"
#include "sharedNetworkMessages/CommandChannelMessages.h"
#include "sharedNetworkMessages/DeleteCharacterMessage.h"
#include "sharedNetworkMessages/DeleteCharacterReplyMessage.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Object.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "swgClientUserInterface/SwgCuiAvatarCreationHelper.h"
#include "swgClientUserInterface/SwgCuiDeleteAvatarConfirmation.h"
#include "swgClientUserInterface/SwgCuiMediatorTypes.h"
#include "swgClientUserInterface/SwgCuiSceneSelection.h"
#include "unicodeArchive/UnicodeArchive.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

//-----------------------------------------------------------------

namespace
{
	namespace UnnamedMessages
	{
		const char * const GameConnectionOpened             = "GameConnectionOpened";
	}

	namespace Properties
	{
		const UILowerString AvatarNetworkId    = UILowerString ("AvatarNetworkId");
		const UILowerString ClusterId          = UILowerString ("ClusterId");
		const UILowerString DefaultViewerPitch = UILowerString ("DefaultViewerPitch");
		const UILowerString DefaultViewerYaw   = UILowerString ("DefaultViewerYaw");
	}

	bool s_autoSelectedAvatar = false;
	CuiLoginManagerAvatarInfo   s_avatarToDelete;

	const Unicode::String s_unlockedSlotCharacterSuffix = Unicode::narrowToWide(" \\#FF0000[UNLOCKED SLOT]");
	char const * const s_slotPlaceholderAppearance = "appearance/target_dummy.sat";
	char const * const s_slotHologramShader = "shader/ui_membrane.sht";


	Unicode::String CreateTooltipText (const CuiLoginManagerAvatarInfo & avatarInfo)
	{
		Unicode::String tooltipText;
		if (avatarInfo.characterLevel > 0)
		{
			const StringId levelId ("ui_charsheet", "level");
			tooltipText = levelId.localize () + Unicode::narrowToWide (" ");
			Unicode::String tmpStr1;
			UIUtils::FormatLong (tmpStr1, avatarInfo.characterLevel);
			tooltipText+= tmpStr1;
		}

		if (!avatarInfo.characterSkillTemplate.empty ())
		{
			const std::string roadmapName = "title_" + RoadmapManager::getRoadmapNameForTemplateName (avatarInfo.characterSkillTemplate);
			const StringId roadmapNameStringId ("ui_roadmap", roadmapName.c_str());
			tooltipText += Unicode::narrowToWide (" ");
			tooltipText += roadmapNameStringId.localize ();
		}

		return tooltipText;
	}

	// SWG Source Change 2021 - Aconite
	// Remove closed server check process for obvious reasons
	/*
	// must be in ascending sorted order (and no dupes either)!!!
	const uint32 ms_closedServerIds[] = {  4, // Corbantis
										   9, // Kauri
										  10, // Lowca
										  13, // Intrepid 
										  14, // Kettemoor
										  15, // Naritus
										  16, // Scylla
										  18, // Valcyn
										  19, // Tempest
										  26, // Tarquinas
										  27, // Wanderhome
										  36, // Europe-Infinity
										  40, // Japan-Katana
										  41  // Japan-Harla										   
										 };

	const int ms_closedServerTotal = sizeof(ms_closedServerIds) / sizeof(ms_closedServerIds[0]);

	bool isClosedServer(uint32 serverId)
	{
#if PRODUCTION != 1
		for (int i = 1; i < ms_closedServerTotal; ++i)
		{
			FATAL((ms_closedServerIds[i-1] >= ms_closedServerIds[i]), ("ms_closedServerIds must be in ascending sorted order (and no dupes either) - ms_closedServerIds[%d]=%lu >= ms_closedServerIds[%d]=%lu", (i-1), ms_closedServerIds[i-1], i, ms_closedServerIds[i]));
		}
#endif

		return std::binary_search(&ms_closedServerIds[0], &ms_closedServerIds[ms_closedServerTotal], serverId);
	}
	*/
}

//----------------------------------------------------------------------

SwgCuiAvatarSelection::SwgCuiAvatarSelection (UIPage & page) :
CuiMediator              ("SwgCuiAvatarSelection", page),
UIEventCallback          (),
MessageDispatch::Receiver(),
m_okButton               (0),
m_cancelButton           (0),
m_createButton           (0),
m_deleteButton           (0),
m_avatarNameText         (0),
m_avatarDetailsText      (0),
m_noCharactersText       (0),
m_actionPage             (0),
m_table                  (0),
m_objectViewer           (0),
m_avatarViewerCount      (0),
m_selectedAvatarIndex    (-1),
m_carouselPosition       (0.0f),
m_carouselTargetPosition (0.0f),
m_hoveredAvatarIndex     (-1),
m_lastLayoutSize         (0, 0),
m_messageBox             (0),
m_messageBoxDeleteWait   (0),
m_messageBoxLoginWait    (0),
m_waitingLoginForDelete  (false),
m_waitingLogin           (false),
m_waitingLoginForSelect  (false),
m_waitingLoginForCreate  (false),
m_autoConnected          (false),
m_proceed                (false),
m_callback               (new MessageDispatch::Callback),
m_selectedAvatar         (new CuiLoginManagerAvatarInfo),
m_waitingDeletion        (false),
m_deletingAvatar         (new CuiLoginManagerAvatarInfo),
m_refreshingCharacterList (false),
m_waitingForConnection   (false),
m_dropFromCluster        (false),
m_waitingForClusterId    (0),
m_connectionTimeout      (0.0f),
m_connectingToGame       (false),
m_avatarPopulateFirstTime (true),
m_deleteAvatarConfirmationPage(NULL),
m_deleteAvatarConfirmationMediator(NULL),
m_waitForConnectionRetry(false),
m_hasAlreadyRetriedConnection(false),
m_hideClosed (NULL),
m_showAvailableSlots (NULL),
m_showAvailableSlotsEnabled (false),
m_moreSlotsText (NULL),
m_realAvatarCount (0),
m_slotPlaceholderCount (0),
m_slotClusterId (0),
m_placeholderAssetChecked (false),
m_placeholderAssetAvailable (false),
m_welcomeInitialized (false),
m_welcomeComplete (false),
m_welcomeElapsed (0.0f),
m_welcomeFullText (),
m_lastSlotDiagnosticClusterId (0),
m_lastSlotDiagnosticCount (-1),
m_lastSlotDiagnosticRendered (-1)
{
	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		m_avatarViewers[i] = 0;
		m_avatarLabels[i] = 0;
		m_avatarCreatures[i] = 0;
		m_hoverAnimationPlayed[i] = false;
		m_slotPlaceholders[i] = 0;

		char viewerName[32];
		char labelName[32];
		_snprintf (viewerName, sizeof (viewerName), "avatarViewer%d", i);
		_snprintf (labelName, sizeof (labelName), "avatarLabel%d", i);

		UIWidget * widget = 0;
		getCodeDataObject (TUIWidget, widget, viewerName, false);
		m_avatarViewers[i] = dynamic_cast<CuiWidget3dObjectListViewer *>(widget);
		getCodeDataObject (TUIText, m_avatarLabels[i], labelName, false);

		if (m_avatarViewers[i])
		{
			m_avatarViewers[i]->SetLocalTooltip (CuiStringIds::tooltip_viewer_3d_controls.localize ());
			m_avatarViewers[i]->SetPropertyFloat (Properties::DefaultViewerPitch, m_avatarViewers[i]->getCameraPitch ());
			m_avatarViewers[i]->SetPropertyFloat (Properties::DefaultViewerYaw, m_avatarViewers[i]->getCameraYaw ());
			m_avatarViewers[i]->setRotationSlowsToStop (true);
			m_avatarViewers[i]->setIgnoreMouseWheel (true);
			m_avatarViewers[i]->SetVisible (false);
		}

		if (m_avatarLabels[i])
			m_avatarLabels[i]->SetVisible (false);
	}

	m_objectViewer = m_avatarViewers[0];
	if (!m_objectViewer)
	{
		UIWidget * widget = 0;
		getCodeDataObject (TUIWidget, widget, "ViewerWidget");
		m_objectViewer = NON_NULL (dynamic_cast<CuiWidget3dObjectListViewer *>(widget));
		m_avatarViewers[0] = m_objectViewer;
	}

	getCodeDataObject (TUIText,       m_avatarNameText, "textName");
	getCodeDataObject (TUIText,       m_avatarDetailsText, "textDetails", false);
	getCodeDataObject (TUIText,       m_noCharactersText, "textNoCharacters", false);
	getCodeDataObject (TUIPage,       m_actionPage, "pageActions", false);
	getCodeDataObject (TUIButton,     m_okButton,       "buttonNext");
	getCodeDataObject (TUIButton,     m_cancelButton,   "buttonPrev");
	getCodeDataObject (TUIButton,     m_createButton,   "buttonCreate");
	getCodeDataObject (TUIButton,     m_deleteButton,   "buttonDelete");
	getCodeDataObject (TUICheckbox,   m_hideClosed,     "checkHideClosed");
	getCodeDataObject (TUICheckbox,   m_showAvailableSlots, "checkShowAvailableSlots", false);
	getCodeDataObject (TUIText,       m_moreSlotsText, "textMoreSlots", false);

	registerMediatorObject(*m_hideClosed, true);
	m_hideClosed->SetChecked(CuiPreferences::getHideCharactersOnClosedGalaxies());
	if (m_showAvailableSlots)
	{
		bool showAvailableSlots = false;
		IGNORE_RETURN (CuiSettings::loadBoolean (getMediatorDebugName (), "showAvailableSlots", showAvailableSlots));
		m_showAvailableSlotsEnabled = showAvailableSlots;
		m_showAvailableSlots->SetChecked (showAvailableSlots);
		registerMediatorObject (*m_showAvailableSlots, true);
	}

	getCodeDataObject (TUIPage,       m_deleteAvatarConfirmationPage, "deleteConfirmation");
	m_deleteAvatarConfirmationPage->SetVisible(false);
	m_deleteAvatarConfirmationMediator = new SwgCuiDeleteAvatarConfirmation(*m_deleteAvatarConfirmationPage);
	m_deleteAvatarConfirmationMediator->fetch();

	getCodeDataObject (TUITable,      m_table,          "table");

	m_table->SetVisible         (false);

	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));
	model->ClearData   ();
	m_table->SelectRow (-1);

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (m_avatarViewers[i])
			registerMediatorObject (*m_avatarViewers[i], true);
	}
	registerMediatorObject (getPage (), true);
}

//-----------------------------------------------------------------

SwgCuiAvatarSelection::~SwgCuiAvatarSelection ()
{
	delete m_deletingAvatar;
	m_deletingAvatar = 0;

	delete m_callback;
	m_callback = 0;

	m_okButton      = 0;
	m_cancelButton  = 0;
	m_createButton  = 0;
	m_deleteButton  = 0;
	m_table         = 0;
	m_hideClosed    = 0;

	m_avatarNameText = 0;
	m_avatarDetailsText = 0;
	m_noCharactersText = 0;
	m_actionPage = 0;
	m_showAvailableSlots = 0;
	m_moreSlotsText = 0;

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (m_avatarViewers[i])
			m_avatarViewers[i]->clearObjects ();
		delete m_slotPlaceholders[i];
		m_slotPlaceholders[i] = 0;
		m_avatarViewers[i] = 0;
		m_avatarLabels[i] = 0;
		m_avatarCreatures[i] = 0;
	}
	m_objectViewer = 0;
	m_messageBox = 0;
	m_messageBoxDeleteWait = 0;

	delete m_selectedAvatar;
	m_selectedAvatar = 0;

	m_messageBoxLoginWait = 0;

	m_deleteAvatarConfirmationMediator->release();
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::performActivate ()
{
	SwgCuiAvatarCreationHelper::setCreatingJedi (false);

	s_avatarToDelete.clear ();

	m_connectingToGame       = false;
	m_connectionTimeout      = 0.0f;
	m_avatarNameText->Clear ();
	if (m_avatarDetailsText)
		m_avatarDetailsText->Clear ();
	if (m_noCharactersText)
		m_noCharactersText->SetVisible (false);
	m_selectedAvatarIndex = -1;
	m_carouselPosition = 0.0f;
	m_carouselTargetPosition = 0.0f;
	m_hoveredAvatarIndex = -1;
	m_lastLayoutSize = UISize (0, 0);

	m_table->SelectRow (-1);

	GameNetwork::setAcceptSceneCommand (false);
	
	m_dropFromCluster       = false;
	m_waitingForConnection  = false;
	m_proceed               = false;
	m_waitingDeletion       = false;
	m_waitingLoginForDelete = false;
	m_waitingLoginForSelect = false;
	m_waitingLoginForCreate = false;
	m_waitingLogin          = false;

	m_deletingAvatar->clear ();

	CuiLoginManager::setAllPingsDisabled ();

	m_callback->connect (*this, &SwgCuiAvatarSelection::onClusterConnection,      static_cast<CuiLoginManager::Messages::ClusterConnection *>     (0));
	m_callback->connect (*this, &SwgCuiAvatarSelection::onAvatarListChanged,      static_cast<CuiLoginManager::Messages::AvatarListChanged*>      (0));
	m_callback->connect (*this, &SwgCuiAvatarSelection::onAvailableCharacterSlotsChanged, static_cast<CuiLoginManager::Messages::AvailableCharacterSlotsChanged*> (0));
	m_callback->connect (*this, &SwgCuiAvatarSelection::onClusterStatusChanged,   static_cast<CuiLoginManager::Messages::ClusterStatusChanged*>   (0));
	m_callback->connect (*this, &SwgCuiAvatarSelection::onDeleteAvatarConfirmation, static_cast<SwgCuiDeleteAvatarConfirmation::Message::DeleteAvatarConfirmation*> (0));

	setPointerInputActive  (true);
	setKeyboardInputActive (true);

	m_okButton->AddCallback      (this);
	m_cancelButton->AddCallback  (this);
	m_createButton->AddCallback  (this);
	m_deleteButton->AddCallback  (this);
	m_table->AddCallback         (this);

	connectToMessage (UnnamedMessages::GameConnectionOpened);
	connectToMessage (DeleteCharacterReplyMessage::MessageType);
	connectToMessage (CuiLoadingManager::Messages::FullscreenLoadingDisabled);
	connectToMessage (Game::Messages::SCENE_CHANGED);

	{
		for (int i = 0; i < MaxAvatarViewers; ++i)
		{
			if (!m_avatarViewers[i])
				continue;
			float f = 0.0f;
			if (m_avatarViewers[i]->GetPropertyFloat (Properties::DefaultViewerPitch, f))
				m_avatarViewers[i]->setCameraPitch (f);
			if (m_avatarViewers[i]->GetPropertyFloat (Properties::DefaultViewerYaw, f))
				m_avatarViewers[i]->setCameraYaw (f, true);
		}
	}

	m_createButton->SetEnabled (true);
	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (m_avatarViewers[i])
			m_avatarViewers[i]->setPaused (false);
	}

	{
		UIText* text;

		bool useExitText = !Game::getSinglePlayer() && (CuiLoginManager::getSessionIdKey () && !ConfigClientGame::getEnableAdminLogin());

		getCodeDataObject (TUIText,       text, "backTextOnPrev");
		text->SetVisible(!useExitText);

		getCodeDataObject (TUIText,       text, "exitTextOnPrev");
		text->SetVisible(useExitText);
	}


	//-- reconnect if needed
//	if (!GameNetwork::isConnectedToLoginServer ())
//		reconnectLoginServer (false);
	clearCharacterList ();
	refreshList        (false);
	ensureViewerBehindChrome ();

	m_table->SetEnabled        (true);
	getPage ().SetFocus        ();

	setIsUpdating (true);

	CuiTransition::signalTransitionReady (CuiMediatorTypes::AvatarSelection);
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::performDeactivate ()
{
	disconnectFromMessage (UnnamedMessages::GameConnectionOpened);
	disconnectFromMessage (DeleteCharacterReplyMessage::MessageType);
	disconnectFromMessage (CuiLoadingManager::Messages::FullscreenLoadingDisabled);
	disconnectFromMessage (Game::Messages::SCENE_CHANGED);

	m_autoConnected = true;

	m_callback->disconnect (*this, &SwgCuiAvatarSelection::onAvatarListChanged,      static_cast<CuiLoginManager::Messages::AvatarListChanged*>      (0));
	m_callback->disconnect (*this, &SwgCuiAvatarSelection::onAvailableCharacterSlotsChanged, static_cast<CuiLoginManager::Messages::AvailableCharacterSlotsChanged*> (0));
	m_callback->disconnect (*this, &SwgCuiAvatarSelection::onClusterConnection,      static_cast<CuiLoginManager::Messages::ClusterConnection *>     (0));
	m_callback->disconnect (*this, &SwgCuiAvatarSelection::onClusterStatusChanged,   static_cast<CuiLoginManager::Messages::ClusterStatusChanged*>   (0));
	m_callback->disconnect (*this, &SwgCuiAvatarSelection::onDeleteAvatarConfirmation, static_cast<SwgCuiDeleteAvatarConfirmation::Message::DeleteAvatarConfirmation*> (0));

	setIsUpdating (false);

	clearCharacterList ();

	if (m_messageBox)
		m_messageBox->closeMessageBox ();
	if (m_messageBoxDeleteWait)
		m_messageBoxDeleteWait->closeMessageBox ();
	if (m_messageBoxLoginWait)
		m_messageBoxLoginWait->closeMessageBox ();

	m_messageBox           = 0;
	m_messageBoxDeleteWait = 0;
	m_messageBoxLoginWait  = 0;

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (m_avatarViewers[i])
			m_avatarViewers[i]->setPaused (true);
	}

	disconnectAll();

	m_okButton->RemoveCallback      (this);
	m_cancelButton->RemoveCallback  (this);
	m_createButton->RemoveCallback  (this);
	m_deleteButton->RemoveCallback  (this);
	m_table->RemoveCallback         (this);

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (m_avatarViewers[i])
			m_avatarViewers[i]->clearObjects ();
	}
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::refreshList (bool updateSelection)
{
	const long oldRowSelected = m_table->GetLastSelectedRow ();

	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));

	CuiLoginManager::AvatarInfoVector aiv;

	{
		struct RefreshCharacterListScope
		{
			bool & m_flag;
			explicit RefreshCharacterListScope (bool & flag) : m_flag (flag) { m_flag = true; }
			~RefreshCharacterListScope () { m_flag = false; }
		} const refreshScope (m_refreshingCharacterList);

		m_table->SelectRow (-1);
		model->ClearData ();

		typedef CuiLoginManager::AvatarInfoVector AvatarInfoVector;

		CuiLoginManager::getAvatarInfo  (aiv);

		AvatarInfoVector::const_iterator it;

		bool hasAvatarOnClosedServers = false;
		for (it = aiv.begin (); it != aiv.end (); ++it)
		{
			const CuiLoginManagerAvatarInfo & avatarInfo = *it;
			//if (isClosedServer(avatarInfo.clusterId))
			//{
			//	hasAvatarOnClosedServers = true;
			//	break;
			//}
		}

		m_hideClosed->SetVisible(hasAvatarOnClosedServers);
		
		for (it = aiv.begin (); it != aiv.end (); ++it)
		{
			const CuiLoginManagerAvatarInfo & avatarInfo = *it;
			addAvatar (avatarInfo);

			if (!isActive ())
				break;
		}

		m_table->Link ();
	}
	
	//-- if autoconnecting, don't waste time updating the player model
	if (updateSelection || m_table->GetLastSelectedRow () < 0)
	{
		if (!autoConnectOk ())
		{
			if (oldRowSelected >= 0 && model->GetRowCount () > 0)
				m_table->SelectRow (std::min (oldRowSelected, static_cast<long>(model->GetRowCount () - 1L)));
			else if (model->GetRowCount () > 0)
				m_table->SelectRow (0);
		}
	}
	
	if (m_table->GetLastSelectedRow () < 0)
	{
		if (!aiv.empty () && autoConnectOk () && ConfigClientGame::getAutoConnectToCentralServer ())
			m_createButton->Press ();
		else if (!aiv.empty ())
			m_table->SelectRow (0);
	}

	populateAvatarViewers ();
	updateAvatarSelection ();

	CuiCachedAvatarManager::saveCharacterList ();

	m_avatarPopulateFirstTime = false;

	ensureViewerBehindChrome ();
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::ensureViewerBehindChrome ()
{
	int const count = (m_avatarViewerCount > 0) ? m_avatarViewerCount : 1;
	bool moved[MaxAvatarViewers] = { false };

	// Move nearest-to-farthest to Bottom. Each later (farther) viewer is inserted
	// underneath the previous viewers, leaving the front of the arc on top.
	for (int layer = 0; layer < count; ++layer)
	{
		int bestIndex = -1;
		float bestDistance = 1000.0f;
		for (int i = 0; i < count; ++i)
		{
			if (moved[i] || !m_avatarViewers[i])
				continue;

			float offset = static_cast<float>(i) - m_carouselPosition;
			if (m_avatarViewerCount > 1)
			{
				float const halfCount = static_cast<float>(m_avatarViewerCount) * 0.5f;
				while (offset > halfCount)
					offset -= static_cast<float>(m_avatarViewerCount);
				while (offset < -halfCount)
					offset += static_cast<float>(m_avatarViewerCount);
			}

			float const distance = fabsf (offset);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestIndex = i;
			}
		}

		if (bestIndex >= 0)
		{
			moved[bestIndex] = true;
			CuiWidget3dObjectListViewer * const viewer = m_avatarViewers[bestIndex];
			UIBaseObject * const p = viewer->GetParent ();
			if (p && p->IsA (TUIPage))
				IGNORE_RETURN (static_cast<UIPage *>(p)->MoveChild (viewer, UIBaseObject::Bottom));
		}
	}
}

//----------------------------------------------------------------------

Object * SwgCuiAvatarSelection::getSlotPlaceholder (int index)
{
	if (index < 0 || index >= MaxAvatarViewers)
		return 0;

	if (!m_placeholderAssetChecked)
	{
		m_placeholderAssetChecked = true;
		m_placeholderAssetAvailable = TreeFile::exists (s_slotPlaceholderAppearance);
		WARNING (!m_placeholderAssetAvailable, ("Character selection placeholder appearance [%s] is unavailable; using label-only fallback", s_slotPlaceholderAppearance));
	}

	if (!m_placeholderAssetAvailable)
		return 0;

	if (!m_slotPlaceholders[index])
	{
		Object * const placeholder = new Object;
		placeholder->setAppearance (AppearanceTemplateList::createAppearance (s_slotPlaceholderAppearance));
		m_slotPlaceholders[index] = placeholder;
	}

	return m_slotPlaceholders[index];
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::beginWelcomeText ()
{
	if (!m_noCharactersText || m_welcomeInitialized)
		return;

	Unicode::String createLabel;
	m_createButton->GetText (createLabel);
	m_welcomeFullText = Unicode::narrowToWide ("Welcome to SWG: Titan. Press *Create Character* to start your adventure.");
	m_welcomeElapsed = 0.0f;
	m_welcomeComplete = false;
	m_welcomeInitialized = true;
	m_noCharactersText->SetLocalText (Unicode::emptyString);
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::completeWelcomeText ()
{
	if (m_noCharactersText && m_welcomeInitialized && !m_welcomeComplete)
	{
		m_welcomeComplete = true;
		m_noCharactersText->SetLocalText (m_welcomeFullText);
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::updateSlotSelectionDisplay ()
{
	m_avatarNameText->SetLocalText (Unicode::narrowToWide ("Available Character Slot"));
	if (m_avatarDetailsText)
		m_avatarDetailsText->SetLocalText (Unicode::narrowToWide ("Available Character Slot"));
	m_okButton->SetEnabled (true);
	m_deleteButton->SetEnabled (false);
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::populateAvatarViewers (bool preserveCarouselState)
{
	int const previousRealAvatarCount = m_realAvatarCount;
	int const previousViewerCount = m_avatarViewerCount;
	int const previousSelectedIndex = m_selectedAvatarIndex;
	uint32 const previousSlotClusterId = m_slotClusterId;
	float const previousCarouselPosition = m_carouselPosition;
	float const previousCarouselTarget = m_carouselTargetPosition;

	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));
	m_realAvatarCount = std::min (static_cast<int>(model->GetRowCount ()), static_cast<int>(MaxAvatarViewers));
	bool const preserveRealViewers = preserveCarouselState && previousRealAvatarCount == m_realAvatarCount;
	m_avatarViewerCount = m_realAvatarCount;
	m_slotPlaceholderCount = 0;

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		CuiWidget3dObjectListViewer * const viewer = m_avatarViewers[i];
		if (preserveRealViewers && i < m_realAvatarCount)
		{
			if (viewer)
			{
				viewer->setUseOverrideShader ("", false);
				viewer->SetVisible (true);
			}
			continue;
		}

		m_avatarCreatures[i] = 0;
		m_hoverAnimationPlayed[i] = false;

		if (viewer)
		{
			viewer->clearObjects ();
			viewer->setUseOverrideShader ("", false);
			viewer->SetVisible (i < m_realAvatarCount);
		}
		if (m_avatarLabels[i])
		{
			m_avatarLabels[i]->Clear ();
			m_avatarLabels[i]->SetVisible (i < m_realAvatarCount);
		}

		if (i >= m_realAvatarCount || !viewer)
			continue;

		UIData const * const cellData = model->GetCellDataVisual (i, 0);
		if (!cellData)
			continue;

		std::string networkIdString;
		long clusterId = 0;
		cellData->GetPropertyNarrow (Properties::AvatarNetworkId, networkIdString);
		cellData->GetPropertyLong (Properties::ClusterId, clusterId);

		CreatureObject * const avatar = CuiLoginManager::getAvatarCreature (static_cast<uint32>(clusterId), NetworkId (networkIdString));
		m_avatarCreatures[i] = avatar;
		if (avatar)
		{
			avatar->setAnimationMood ("ui");
			viewer->addObject (*avatar);
			viewer->setCameraForceTarget (true);
			viewer->recomputeZoom ();
			viewer->setCameraForceTarget (false);
		}

		UIString displayName;
		cellData->GetProperty (UITableModelDefault::DataProperties::Value, displayName);
		if (m_avatarLabels[i])
			m_avatarLabels[i]->SetLocalText (displayName);
	}

	int selectedRow = m_table->GetLastSelectedRow ();
	if (selectedRow < 0 || selectedRow >= m_realAvatarCount)
		selectedRow = 0;

	m_slotClusterId = 0;
	if (preserveCarouselState && previousSelectedIndex >= previousRealAvatarCount && previousSlotClusterId != 0)
		m_slotClusterId = previousSlotClusterId;
	else if (m_realAvatarCount > 0)
	{
		UIData const * const selectedData = model->GetCellDataVisual (selectedRow, 0);
		long clusterId = 0;
		if (selectedData)
			selectedData->GetPropertyLong (Properties::ClusterId, clusterId);
		m_slotClusterId = static_cast<uint32>(clusterId);
	}
	else
		m_slotClusterId = CuiLoginManager::getFirstClusterWithAvailableSlots ();

	int const authoritativeAvailable = std::max (0, CuiLoginManager::getAvailableCharacterSlots (m_slotClusterId));
	bool const showAvailable = m_showAvailableSlots && m_showAvailableSlotsEnabled;
	int const welcomePlaceholderCount = (m_realAvatarCount == 0) ? 1 : 0;
	// The server count is already net remaining capacity. Only cap it by the
	// number of viewers left after real avatars and the mandatory welcome dummy.
	int const requestedAvailableCount = showAvailable ? authoritativeAvailable : 0;
	int const availableDisplayCount = std::min (requestedAvailableCount, MaxAvatarViewers - m_realAvatarCount - welcomePlaceholderCount);
	m_slotPlaceholderCount = welcomePlaceholderCount + std::max (0, availableDisplayCount);

	for (int slot = 0; slot < m_slotPlaceholderCount; ++slot)
	{
		int const viewerIndex = m_realAvatarCount + slot;
		CuiWidget3dObjectListViewer * const viewer = m_avatarViewers[viewerIndex];
		if (!viewer)
			continue;

		viewer->SetVisible (true);
		if (TreeFile::exists (s_slotHologramShader))
			viewer->setUseOverrideShader (s_slotHologramShader, true);

		Object * const placeholder = getSlotPlaceholder (slot);
		if (placeholder)
		{
			viewer->addObject (*placeholder);
			viewer->setCameraForceTarget (true);
			viewer->recomputeZoom ();
			viewer->setCameraForceTarget (false);
		}

		if (m_avatarLabels[viewerIndex])
		{
			m_avatarLabels[viewerIndex]->SetLocalText (Unicode::narrowToWide ((m_realAvatarCount == 0 && slot == 0) ? "CREATE A CHARACTER" : "AVAILABLE CHARACTER SLOT"));
			m_avatarLabels[viewerIndex]->SetVisible (true);
		}
	}

	m_avatarViewerCount = m_realAvatarCount + m_slotPlaceholderCount;
	int const hiddenAvailableCount = std::max (0, requestedAvailableCount - availableDisplayCount);
	if (showAvailable &&
		(m_lastSlotDiagnosticClusterId != m_slotClusterId ||
		 m_lastSlotDiagnosticCount != authoritativeAvailable ||
		 m_lastSlotDiagnosticRendered != availableDisplayCount))
	{
		DEBUG_REPORT_LOG(true, ("Character selection slots cluster %lu authoritative %d rendered %d real avatars %d viewer cap %d\n",
			m_slotClusterId, authoritativeAvailable, availableDisplayCount, m_realAvatarCount, MaxAvatarViewers));
		m_lastSlotDiagnosticClusterId = m_slotClusterId;
		m_lastSlotDiagnosticCount = authoritativeAvailable;
		m_lastSlotDiagnosticRendered = availableDisplayCount;
	}
	if (m_moreSlotsText)
	{
		if (hiddenAvailableCount > 0)
		{
			char buffer[64];
			_snprintf (buffer, sizeof (buffer), "%d more slots available", hiddenAvailableCount);
			m_moreSlotsText->SetLocalText (Unicode::narrowToWide (buffer));
			m_moreSlotsText->SetVisible (true);
		}
		else
			m_moreSlotsText->SetVisible (false);
	}

	m_okButton->SetEnabled (m_avatarViewerCount > 0);
	m_deleteButton->SetEnabled (m_realAvatarCount > 0);
	m_createButton->SetEnabled (true);
	if (m_noCharactersText)
		m_noCharactersText->SetVisible (m_realAvatarCount == 0);

	if (m_realAvatarCount == 0)
		beginWelcomeText ();
	else
	{
		m_welcomeInitialized = false;
		m_welcomeComplete = false;
	}

	if (preserveCarouselState && m_avatarViewerCount > 0 && previousSelectedIndex >= 0)
	{
		int selectedIndex = previousSelectedIndex;
		bool const selectedWasPlaceholder = previousSelectedIndex >= previousRealAvatarCount;
		if (selectedWasPlaceholder && previousSelectedIndex >= m_avatarViewerCount)
		{
			selectedIndex = 0;
			if (m_realAvatarCount > 0)
			{
				float bestDistance = static_cast<float>(previousViewerCount + 1);
				for (int realIndex = 0; realIndex < m_realAvatarCount; ++realIndex)
				{
					float distance = fabsf (static_cast<float>(realIndex - previousSelectedIndex));
					if (previousViewerCount > 1)
						distance = std::min (distance, static_cast<float>(previousViewerCount) - distance);
					if (distance < bestDistance)
					{
						bestDistance = distance;
						selectedIndex = realIndex;
					}
				}
			}
		}
		else if (!selectedWasPlaceholder)
			selectedIndex = std::min (selectedRow, m_realAvatarCount - 1);

		selectedIndex = std::max (0, std::min (selectedIndex, m_avatarViewerCount - 1));
		m_selectedAvatarIndex = selectedIndex;

		m_refreshingCharacterList = true;
		m_table->SelectRow (selectedIndex < m_realAvatarCount ? selectedIndex : -1);
		m_refreshingCharacterList = false;

		int const previousTargetIndex = static_cast<int>(floorf (previousCarouselTarget + 0.5f));
		if (previousTargetIndex >= 0 && previousTargetIndex < m_avatarViewerCount)
		{
			m_carouselPosition = previousCarouselPosition;
			m_carouselTargetPosition = previousCarouselTarget;
		}
		else
		{
			m_carouselPosition = previousCarouselPosition;
			m_carouselTargetPosition = static_cast<float>(selectedIndex);
			if (m_avatarViewerCount > 1)
			{
				float const halfCount = static_cast<float>(m_avatarViewerCount) * 0.5f;
				while (m_carouselTargetPosition - m_carouselPosition > halfCount)
					m_carouselTargetPosition -= static_cast<float>(m_avatarViewerCount);
				while (m_carouselTargetPosition - m_carouselPosition < -halfCount)
					m_carouselTargetPosition += static_cast<float>(m_avatarViewerCount);
			}
		}
	}
	else
	{
		m_selectedAvatarIndex = (m_avatarViewerCount > 0) ? selectedRow : -1;
		m_carouselPosition = static_cast<float>(selectedRow);
		m_carouselTargetPosition = m_carouselPosition;
	}

	layoutAvatarViewers ();
	ensureViewerBehindChrome ();
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::layoutAvatarViewers ()
{
	int const count = (m_avatarViewerCount > 0) ? m_avatarViewerCount : 1;
	UISize const pageSize = getPage ().GetSize ();
	long const availableWidth = std::max (640L, pageSize.x);
	long const centerX = availableWidth / 2L;
	long const nameY = std::max (300L, pageSize.y - 232L);
	m_avatarNameText->SetLocation ((availableWidth - m_avatarNameText->GetWidth ()) / 2L, nameY);
	if (m_avatarDetailsText)
		m_avatarDetailsText->SetLocation ((availableWidth - m_avatarDetailsText->GetWidth ()) / 2L, nameY + 30L);
	if (m_noCharactersText)
		m_noCharactersText->SetLocation ((availableWidth - m_noCharactersText->GetWidth ()) / 2L, std::max (180L, nameY - 180L));
	if (m_actionPage)
		m_actionPage->SetLocation ((availableWidth - m_actionPage->GetWidth ()) / 2L, std::max (390L, pageSize.y - 172L));

	float const frontWidth = static_cast<float>(std::min (430L, std::max (300L, availableWidth * 2L / 5L)));
	float const top = static_cast<float>(std::max (54L, pageSize.y / 12L));
	float const frontHeight = static_cast<float>(std::max (250L, nameY - static_cast<long>(top) - 8L));
	float const horizontalRadius = static_cast<float>(availableWidth) * 0.39f;
	float const maximumArcRadians = 1.35f;
	float const maximumDepth = 1.0f - cosf (maximumArcRadians);

	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		CuiWidget3dObjectListViewer * const viewer = m_avatarViewers[i];
		UIText * const label = m_avatarLabels[i];
		if (!viewer || i >= count)
			continue;

		float offset = static_cast<float>(i) - m_carouselPosition;
		if (m_avatarViewerCount > 1)
		{
			float const halfCount = static_cast<float>(m_avatarViewerCount) * 0.5f;
			while (offset > halfCount)
				offset -= static_cast<float>(m_avatarViewerCount);
			while (offset < -halfCount)
				offset += static_cast<float>(m_avatarViewerCount);
		}

		float const angle = tanhf (offset * 0.38f) * maximumArcRadians;
		float const depth = std::max (0.0f, std::min (1.0f, (1.0f - cosf (angle)) / maximumDepth));
		float const scale = 1.0f - depth * 0.48f;
		float const width = frontWidth * scale;
		float const height = frontHeight * scale;
		float const xCenter = static_cast<float>(centerX) + sinf (angle) * horizontalRadius;
		float const y = top + depth * 72.0f;
		long const xLocation = static_cast<long>(floorf (xCenter - width * 0.5f + 0.5f));
		long const yLocation = static_cast<long>(floorf (y + 0.5f));
		long const viewerWidth = static_cast<long>(floorf (width + 0.5f));
		long const viewerHeight = static_cast<long>(floorf (height + 0.5f));

		viewer->SetLocation (xLocation, yLocation);
		viewer->SetSize (UISize (viewerWidth, viewerHeight));
		viewer->SetOpacity (1.0f - depth * 0.52f);

		if (label)
		{
			label->SetLocation (xLocation, yLocation + viewerHeight - 24L);
			label->SetSize (UISize (viewerWidth, 22L));
			label->SetOpacity (1.0f - depth * 0.58f);
			label->SetVisible (m_avatarViewerCount == 0 || fabsf (offset) > 0.55f);
		}
	}

	m_lastLayoutSize = pageSize;
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::rotateCarousel (int steps)
{
	if (m_avatarViewerCount <= 1 || steps == 0)
		return;

	int const maximumQueuedSteps = MaxAvatarViewers;
	steps = std::max (-maximumQueuedSteps, std::min (maximumQueuedSteps, steps));

	float const queuedDelta = m_carouselTargetPosition - m_carouselPosition;
	float const requestedDelta = queuedDelta + static_cast<float>(steps);
	float const boundedDelta = std::max (-static_cast<float>(maximumQueuedSteps), std::min (static_cast<float>(maximumQueuedSteps), requestedDelta));
	m_carouselTargetPosition = m_carouselPosition + boundedDelta;
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::advanceCarousel (float deltaTimeSecs)
{
	if (m_avatarViewerCount <= 0)
		return;

	float const difference = m_carouselTargetPosition - m_carouselPosition;
	float const easing = 1.0f - expf (-10.0f * std::max (0.0f, deltaTimeSecs));
	if (fabsf (difference) > 0.001f)
		m_carouselPosition += difference * easing;
	else
		m_carouselPosition = m_carouselTargetPosition;

	int centeredIndex = static_cast<int>(floorf (m_carouselPosition + 0.5f));
	centeredIndex %= m_avatarViewerCount;
	if (centeredIndex < 0)
		centeredIndex += m_avatarViewerCount;

	if (centeredIndex != m_selectedAvatarIndex)
	{
		m_selectedAvatarIndex = centeredIndex;
		if (centeredIndex < m_realAvatarCount)
			m_table->SelectRow (centeredIndex);
		else
		{
			m_refreshingCharacterList = true;
			m_table->SelectRow (-1);
			m_refreshingCharacterList = false;
			updateSlotSelectionDisplay ();
		}
		ensureViewerBehindChrome ();
	}

	if (fabsf (difference) > 0.001f || getPage ().GetSize () != m_lastLayoutSize)
		layoutAvatarViewers ();

	if (fabsf (m_carouselTargetPosition - m_carouselPosition) <= 0.001f &&
		m_showAvailableSlots && m_showAvailableSlotsEnabled &&
		m_selectedAvatarIndex >= 0 && m_selectedAvatarIndex < m_realAvatarCount)
	{
		UITableModelDefault * const model = safe_cast<UITableModelDefault *>(m_table->GetTableModel ());
		UIData const * const selectedData = model ? model->GetCellDataVisual (m_selectedAvatarIndex, 0) : 0;
		long clusterId = 0;
		if (selectedData && selectedData->GetPropertyLong (Properties::ClusterId, clusterId) && static_cast<uint32>(clusterId) != m_slotClusterId)
		{
			populateAvatarViewers (true);
			updateAvatarSelection ();
		}
	}
}

//----------------------------------------------------------------------

//-- temporarily just add avatars in non-alphabetical order...

void SwgCuiAvatarSelection::addAvatar (const CuiLoginManagerAvatarInfo & avatarInfo)
{
	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));

	{
		const int numRows = model->GetRowCount ();

		for (int i = 0; i < numRows; ++i)
		{
			UIData * const data = model->GetCellDataLogical (i, 0);
			NOT_NULL (data);

			std::string networkIdStr;
			long        clusterId = 0;

			if (data->GetPropertyNarrow  (Properties::AvatarNetworkId, networkIdStr) && data->GetPropertyLong    (Properties::ClusterId, clusterId))			
			{
				//-- avatar is already in the table
				if (avatarInfo.clusterId == static_cast<uint32>(clusterId) && avatarInfo.networkId == NetworkId (networkIdStr))
				{
					Unicode::String tooltipText = CreateTooltipText (avatarInfo);
					if (!tooltipText.empty ())
					{
						data->SetProperty (UITableModelDefault::DataProperties::LocalTooltip, tooltipText);
					}
					return;
				}
			}
		}
	}

	const std::string narrowName (Unicode::wideToNarrow (avatarInfo.name));
	const CuiLoginManagerClusterInfo * const clusterInfo = CuiLoginManager::findClusterInfo (avatarInfo.clusterId);

	WARNING (!clusterInfo, ("Unable to load cluster info for cluster %d, requested by avatar [%s], %s", avatarInfo.clusterId, narrowName.c_str (), avatarInfo.networkId.getValueString ().c_str ()));

	//if (m_hideClosed->IsChecked() && isClosedServer(avatarInfo.clusterId))
	//	return;

	Unicode::String avatarDisplayName = avatarInfo.name;
	if (avatarInfo.characterType == static_cast<int>(EnumerateCharacterId_Chardata::CT_jedi))
		avatarDisplayName += s_unlockedSlotCharacterSuffix;

	Unicode::String clusterDisplayName = clusterInfo ? Unicode::narrowToWide (clusterInfo->name) : Unicode::emptyString;
	Unicode::String planetDisplayName = avatarInfo.planetName.empty () ? Unicode::emptyString : StringId ("planet_n", avatarInfo.planetName).localize ();
	Unicode::String statusDisplayStr = Unicode::emptyString;

	if (clusterInfo)
	{
		if (clusterInfo->up)
		{
			if (clusterInfo->loading)
				statusDisplayStr = CuiStringIdsServer::server_loading.localize ();
			else if (clusterInfo->locked)
			{
				if(clusterInfo->isAdmin)
				{
					Unicode::String lockedFlag = u"\\#00ff00 (God Mode)";
					statusDisplayStr = CuiStringIdsServer::server_locked.localize() + lockedFlag;
				}
				else
				{
					statusDisplayStr = CuiStringIdsServer::server_locked.localize();
				}
			}
			else if (clusterInfo->restricted)
				statusDisplayStr = CuiStringIdsServer::server_restricted.localize ();
			else if (clusterInfo->isFull)
				statusDisplayStr = CuiStringIdsServer::server_full.localize ();
			else
				statusDisplayStr = CuiStringIdsServer::server_online.localize ();
		}
		else
			statusDisplayStr = CuiStringIdsServer::server_offline.localize ();
		if(clusterInfo->isAdmin && clusterInfo->isSecret)
		{
			Unicode::String secretFlag = u"\\#ff00ff (Secret)";
			statusDisplayStr += secretFlag;
		}
	}
	
	if (clusterInfo && !clusterInfo->branch.empty() && clusterInfo->isAdmin)
	{
		static const Unicode::String::value_type *s_colorRed     = u"\\#ff0000";
		static const Unicode::String::value_type *s_colorGreen   = u"\\#00ff00";
		static const Unicode::String::value_type *s_colorBlue    = u"\\#0000ff";
		static const Unicode::String::value_type *s_colorMagenta = u"\\#ff00ff";
		static const Unicode::String::value_type *s_colorYellow  = u"\\#ffff00";
		static const Unicode::String::value_type *s_colorCyan    = u"\\#00ffff";
		static const Unicode::String::value_type *s_colorWhite   = u"\\#ffffff";
		static const Unicode::String::value_type *s_colorBlack   = u"\\#000000";

		Unicode::String color = s_colorRed; // red is the default color

		if (clusterInfo->netVersionMatch == true)
		{
			if (clusterInfo->branch == Branch().getBranchName())
			{
				if (clusterInfo->version == (uint)atoi( ApplicationVersion::getPublicVersion()))
				{
					color = s_colorGreen;
				}
				else
				{
					color = s_colorMagenta;
				}
			}
			else
			{
				color = s_colorYellow;
			}
		}

		//avatarDisplayName = color + avatarDisplayName;
		clusterDisplayName = color + clusterDisplayName + u" [Test]";
		planetDisplayName = color + planetDisplayName;
		statusDisplayStr = color + statusDisplayStr;
	}

	UIData * const d = model->AppendCell (0, narrowName.c_str (), avatarDisplayName);
	d->SetPropertyNarrow  (Properties::AvatarNetworkId,                 avatarInfo.networkId.getValueString ());
	d->SetPropertyLong    (Properties::ClusterId,                       static_cast<long>(avatarInfo.clusterId));

	Unicode::String tooltipText = CreateTooltipText(avatarInfo);

	if(!tooltipText.empty())
	{
		d->SetProperty  (UITableModelDefault::DataProperties::LocalTooltip, tooltipText);
	}

	model->AppendCell (1, narrowName.c_str (), clusterDisplayName);

	model->AppendCell (2, narrowName.c_str (), planetDisplayName);

	model->AppendCell (3, narrowName.c_str (), statusDisplayStr);

	
	std::string avatarNameToUse;
	uint32        launcherClusterId  = ConfigClientGame::getLauncherClusterId  ();
	{
		const std::string & launcherAvatarName = ConfigClientGame::getLauncherAvatarName ();
		
		if (!launcherAvatarName.empty ())
		{
			avatarNameToUse = launcherAvatarName;
			const size_t splitpos = avatarNameToUse.find (" (");
			if (splitpos != std::string::npos)
				avatarNameToUse = avatarNameToUse.substr (0, splitpos);
		}
		else
		{
			avatarNameToUse   = ConfigClientGame::getAvatarName ();
			launcherClusterId = CuiLoginManager::findClusterId (ConfigClientGame::getCentralServerName ());
		}
	}
	
	if (autoConnectOk () && !avatarNameToUse.empty () && m_table->GetLastSelectedRow () < 0)
	{
		if (!_stricmp (avatarNameToUse.c_str (), narrowName.c_str ()))
		{
			if (launcherClusterId == 0 || launcherClusterId == avatarInfo.clusterId)
			{
				if (autoConnectOk () || !s_autoSelectedAvatar)
					m_table->SelectRow (static_cast<long>(model->GetRowCount ()) - 1L);
				
				//-----------------------------------------------------------------
				//-- see if we can autoconnect with this avatar
				
				if (autoConnectOk ())
				{
					m_okButton->Press ();
					ConfigClientGame::setNextAutoConnectToGameServer (false);
				}
			}
		}

		s_autoSelectedAvatar = true;
	}
} //lint !e429 //d not a leak

//----------------------------------------------------------------------

bool SwgCuiAvatarSelection::autoConnectOk () const
{
	if (ConfigClientGame::getNextAutoConnectToGameServer ())
		return true;

	if (!m_autoConnected)
	{
		if (ConfigClientGame::getAutoConnectToGameServer ())
			return true;
		
		if (!ConfigClientGame::getLauncherAvatarName ().empty ())
			return true;
	}

	return false;
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::OnButtonPressed(UIWidget *Context)
{
	if (Context == m_cancelButton)
	{
		if (Game::getSinglePlayer ())
		{
			CuiTransition::startTransition (CuiMediatorTypes::AvatarSelection, CuiMediatorTypes::SceneSelection);
		}
		else
		{
			if (CuiLoginManager::getSessionIdKey () && !ConfigClientGame::getEnableAdminLogin())
			{
				deactivate ();
				CuiManager::terminateIoWin ();
				return;
			}
			else
			{
				CuiTransition::startTransition(CuiMediatorTypes::AvatarSelection, CuiMediatorTypes::LoginScreen);
			}
		}
	}

	//----------------------------------------------------------------------

	else if (Context == m_okButton)
	{
		if (m_selectedAvatarIndex >= m_realAvatarCount && m_selectedAvatarIndex < m_avatarViewerCount)
			handleCreate ();
		else
			requestAvatarSelection ();
	}

	//----------------------------------------------------------------------

	else if (Context == m_createButton)
	{
		handleCreate ();
	}
	else if (Context == m_deleteButton)
	{
		requestAvatarDeletion ();
	}

} //lint !e818 //stfu noob

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::OnGenericSelectionChanged (UIWidget * context)
{
	if (context == m_table)
	{
		if (m_refreshingCharacterList)
			return;

		//-- if autoconnecting, don't waste time updating the player model
		if (!autoConnectOk ())
			updateAvatarSelection ();
	}
} //lint !e818 //stfu noob

//-----------------------------------------------------------------

int SwgCuiAvatarSelection::findViewerIndex (UIWidget const * widget) const
{
	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		if (widget == m_avatarViewers[i])
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::selectAvatarIndex (int index)
{
	if (m_avatarViewerCount <= 0)
		return;

	if (index < 0)
		index = m_avatarViewerCount - 1;
	else if (index >= m_avatarViewerCount)
		index = 0;

	int targetIndex = static_cast<int>(floorf (m_carouselTargetPosition + 0.5f));
	targetIndex %= m_avatarViewerCount;
	if (targetIndex < 0)
		targetIndex += m_avatarViewerCount;

	int delta = index - targetIndex;
	if (delta > m_avatarViewerCount / 2)
		delta -= m_avatarViewerCount;
	else if (delta < -(m_avatarViewerCount / 2))
		delta += m_avatarViewerCount;

	rotateCarousel (delta);
	getPage ().SetFocus ();
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::playHoverAnimation (int index)
{
	if (index < 0 || index >= m_avatarViewerCount || m_hoverAnimationPlayed[index])
		return;

	CreatureObject * const avatar = m_avatarCreatures[index];
	if (!avatar)
		return;

	static char const * const animations[] =
	{
		"cough_polite",
		"nod_head_once",
		"pose_proudly",
		"rub_chin_thoughtful",
		"scratch_head"
	};

	std::string const id = avatar->getNetworkId ().getValueString ();
	unsigned int hash = 2166136261u;
	for (std::string::const_iterator it = id.begin (); it != id.end (); ++it)
		hash = (hash ^ static_cast<unsigned char>(*it)) * 16777619u;

	m_hoverAnimationPlayed[index] = true;
	IGNORE_RETURN (CuiAnimationManager::attemptPlayEmote (*avatar, 0, animations[hash % (sizeof (animations) / sizeof (animations[0]))]));
}

//-----------------------------------------------------------------

bool SwgCuiAvatarSelection::OnMessage (UIWidget * context, const UIMessage & msg)
{
	int const viewerIndex = findViewerIndex (context);
	if (m_realAvatarCount == 0 && m_welcomeInitialized && !m_welcomeComplete &&
		(msg.Type == UIMessage::KeyDown || (msg.Type == UIMessage::LeftMouseDown && (context == &getPage () || viewerIndex >= 0))))
	{
		completeWelcomeText ();
		return false;
	}

	if (msg.Type == UIMessage::MouseWheel && (context == &getPage () || viewerIndex >= 0))
	{
		rotateCarousel (-static_cast<int>(msg.Data));
		return false;
	}

	if (viewerIndex >= 0)
	{
		if (msg.Type == UIMessage::LeftMouseDown)
		{
			selectAvatarIndex (viewerIndex);
			return false;
		}
		if (msg.Type == UIMessage::MouseEnter)
		{
			m_hoveredAvatarIndex = viewerIndex;
			if (fabsf (m_carouselTargetPosition - m_carouselPosition) < 0.01f)
				playHoverAnimation (viewerIndex);
		}
		else if (msg.Type == UIMessage::MouseExit)
		{
			if (m_hoveredAvatarIndex == viewerIndex)
				m_hoveredAvatarIndex = -1;
			if (fabsf (m_carouselTargetPosition - m_carouselPosition) < 0.01f)
			{
				m_hoverAnimationPlayed[viewerIndex] = false;
				if (m_avatarCreatures[viewerIndex])
					m_avatarCreatures[viewerIndex]->setAnimationMood ("ui");
			}
		}
	}

	if (context == &getPage () && msg.Type == UIMessage::KeyDown && m_avatarViewerCount > 0)
	{
		if (msg.Keystroke == UIMessage::LeftArrow)
		{
			rotateCarousel (-1);
			return false;
		}
		if (msg.Keystroke == UIMessage::RightArrow)
		{
			rotateCarousel (1);
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------

void SwgCuiAvatarSelection::updateAvatarSelection ()
{
	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));

	const int row = m_table->GetLastSelectedRow ();
	const UIData * const cellData = (row >= 0) ? model->GetCellDataVisual (row, 0) : 0;
	if (row < 0 || !cellData)
	{
		if (m_selectedAvatarIndex >= m_realAvatarCount && m_selectedAvatarIndex < m_avatarViewerCount)
		{
			updateSlotSelectionDisplay ();
			layoutAvatarViewers ();
			return;
		}
		m_selectedAvatarIndex = -1;
		m_avatarNameText->Clear ();
		if (m_avatarDetailsText)
			m_avatarDetailsText->Clear ();
		m_okButton->SetEnabled (false);
		m_deleteButton->SetEnabled (false);
		layoutAvatarViewers ();
		return;
	}

	m_avatarNameText->Clear ();

	UIString selectionName;
	cellData->GetProperty (UITableModelDefault::DataProperties::Value, selectionName);
	WARNING (selectionName.empty (), ("Empty selection name"));

	m_avatarNameText->SetLocalText (selectionName);
	m_selectedAvatarIndex = std::min (row, m_avatarViewerCount - 1);

	if (m_avatarDetailsText)
	{
		Unicode::String details;
		for (int column = 1; column <= 3; ++column)
		{
			UIData const * const detailCell = model->GetCellDataVisual (row, column);
			UIString value;
			if (detailCell && detailCell->GetProperty (UITableModelDefault::DataProperties::Value, value) && !value.empty ())
			{
				if (!details.empty ())
					details += Unicode::narrowToWide ("   |   ");
				details += value;
			}
		}
		m_avatarDetailsText->SetLocalText (details);
	}

	std::string networkIdStr;
	if (!cellData->GetPropertyNarrow (Properties::AvatarNetworkId, networkIdStr))
		WARNING (true, ("Can't get networkid"));
	const NetworkId networkId (networkIdStr);

	long l_clusterId = 0;
	if (!cellData->GetPropertyLong (Properties::ClusterId, l_clusterId))
		WARNING (true, ("Can't get clusterid"));

	const uint32 clusterId = static_cast<uint32>(l_clusterId);

	CreatureObject * const avatar = CuiLoginManager::getAvatarCreature (clusterId, networkId);

	if (!avatar)
	{
		WARNING (true, ("no avatar?"));

		CuiMessageBox::createInfoBox (CuiStringIdsServer::server_err_avatar_not_found.localize ());
		m_table->SelectRow (-1);
	}
	else
	{
		const CuiLoginManagerAvatarInfo * const avatarInfo = CuiLoginManager::findAvatarInfo (clusterId, avatar->getLocalizedName ());

		if (avatarInfo)
		{
			UIData * const planetCellData = model->GetCellDataVisual (row, 2);
			if (planetCellData)
			{
				if (avatarInfo->planetName.empty ())
					planetCellData->SetProperty (UITableModelDefault::DataProperties::Value, Unicode::emptyString);
				else
					planetCellData->SetProperty (UITableModelDefault::DataProperties::Value, StringId ("planet_n", avatarInfo->planetName).localize ());
			}

			UIData * const nameCellData = model->GetCellDataVisual (row, 0);
			if (nameCellData)
			{
				Unicode::String tooltipText = CreateTooltipText (*avatarInfo);
				if (!tooltipText.empty ())
				{
					nameCellData->SetProperty(UITableModelDefault::DataProperties::LocalTooltip, tooltipText);
				}
			}
		}
	}

	m_okButton->SetEnabled (avatar != 0);
	m_deleteButton->SetEnabled (avatar != 0);
	layoutAvatarViewers ();
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::update (float deltaTimeSecs)
{
	CuiMediator::update (deltaTimeSecs);

	advanceCarousel (deltaTimeSecs);
	if (m_realAvatarCount == 0 && m_welcomeInitialized && !m_welcomeComplete && m_noCharactersText)
	{
		m_welcomeElapsed += std::max (0.0f, deltaTimeSecs);
		size_t const characterCount = static_cast<size_t>(m_welcomeElapsed * 30.0f);
		if (characterCount >= m_welcomeFullText.size ())
			completeWelcomeText ();
		else
			m_noCharactersText->SetLocalText (m_welcomeFullText.substr (0, characterCount));
	}

	if (m_dropFromCluster)
	{
		WARNING (true, ("SwgCuiAvatarSelection dropping cluster connection"));
		CuiLoginManager::disconnectFromCluster ();
		m_dropFromCluster = false;
		return;
	}

	if (m_connectingToGame && !m_waitForConnectionRetry)
	{
		static const float TIMEOUT_CONNECTING_TO_GAME = ConfigClientGame::getConnectionTimeout();
		m_connectionTimeout += deltaTimeSecs;

		if (m_connectionTimeout > TIMEOUT_CONNECTING_TO_GAME)
		{
			//-- this should trigger a callback to this class
			CuiLoadingManager::setFullscreenLoadingEnabled (false);
			CuiMessageBox::createInfoBox (CuiStringIdsServer::server_timeout_gameserver.localize ());
		}

		// SWG Source Addition 2021 - Aconite
		// Catch when a player has recently crashed and is trying shortly thereafter to login to
		// their character again but the connection server has refused to load the character because
		// it is still authoritative so we need to make the request for a second time to reset
		// everything since we can't use the back button or escape and our only options are to close
		// the client entirely or wait for the connection request to timeout
		if (m_connectionTimeout > 8 && !GameNetwork::isConnectedToConnectionServer() && CuiLoadingManager::isLoadingScreenVisible() && !m_hasAlreadyRetriedConnection)
		{
			m_waitForConnectionRetry = true;
			m_hasAlreadyRetriedConnection = true;
			m_connectionTimeout = 0;
			requestAvatarSelection();
		}
		return;
	}

	if (m_proceed)
	{
		UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));
		const int row = m_table->GetLastSelectedRow ();

		Unicode::String currentlySelectedCharacter;
		if (model->GetValueAtText (row, 0, currentlySelectedCharacter) && !currentlySelectedCharacter.empty())
		{
			std::string::size_type const pos = currentlySelectedCharacter.find(s_unlockedSlotCharacterSuffix);
			if (pos != std::string::npos)
				currentlySelectedCharacter = currentlySelectedCharacter.substr(0, pos);

			std::string const currentlySelectedCharacterName = Unicode::wideToNarrow (currentlySelectedCharacter);
			ConfigClientGame::setAvatarName        (currentlySelectedCharacterName);
			ConfigClientGame::setCentralServerName (CuiLoginManager::getConnectedClusterName ());
			LocalMachineOptionManager::save ();
			ConfigClientGame::setNextAutoConnectToGameServer (false);

			UIData * const d = model->GetCellDataVisual (row, 0);
			std::string networkIdStr;
			d->GetPropertyNarrow  (Properties::AvatarNetworkId, networkIdStr);
			const NetworkId avatarId (networkIdStr);

			const SelectCharacter s (avatarId);
			GameNetwork::send (s, true);

			//set the destination planet, so loading screen can contextualize
			if(m_selectedAvatar)
			{
				CuiLoadingManager::setPlanetName(m_selectedAvatar->planetName);
			}

			CuiLoadingManager::setFullscreenLoadingEnabled (true);
			CuiLoadingManager::setFullscreenBackButtonEnabled (true);
			CuiLoadingManager::setFullscreenLoadingPercent (-1);
			CuiLoadingManager::setFullscreenLoadingString  (CuiStringIdsServer::server_connecting_game.localize ());
			m_connectionTimeout      = 0.0f;
			m_connectingToGame       = true;

			CuiLoginManager::setAllPingsDisabled ();
			const uint32 clusterId = CuiLoginManager::getConnectedClusterId ();
			if (clusterId)
				CuiLoginManager::setPingEnabled (clusterId, true);

			GameNetwork::setAcceptSceneCommand (true);

			m_autoConnected = true;
		}
		else
		{
			CuiMessageBox::createInfoBox (CuiStringIdsServer::server_err_no_character_selected.localize ());
		}

		m_proceed = false;
		m_waitForConnectionRetry = false;
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::onClusterConnection       (bool b)
{
	if (m_messageBox)
		m_messageBox->closeMessageBox ();

	if (m_messageBoxDeleteWait)
		m_messageBoxDeleteWait->closeMessageBox ();

	if (b)
	{
		if (!m_waitingForConnection)
		{
			WARNING (true, ("SwgCuiAvatarSelection received unexpected cluster connection, dropping."));
			m_dropFromCluster = true;
		}
		else
		{
			m_waitingForConnection = false;
			const uint32 clusterId = CuiLoginManager::getConnectedClusterId ();

			if (clusterId != m_waitingForClusterId)
			{
				WARNING (true, ("SwgCuiAvatarSelection received cluster connection to the wrong cluster [%d], wanted [%d], dropping.", clusterId, m_waitingForClusterId));
				m_dropFromCluster = true;
			}
			else
				m_proceed = true;
		}
	}
	else
	{
		CuiMessageBox::createInfoBox (CuiStringIdsServer::server_cluster_login_failed.localize ());
		m_selectedAvatar->clear ();
		updateAvatarSelection ();
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::onSceneChanged (bool)
{
	if (Game::getScene ())
	{
		deactivate();
		CuiLoginManager::purgeCreatures ();
		GameNetwork::disconnectLoginServer ();
	}
}

//----------------------------------------------------------------------

/**
* We've been authenticated & connected
*/

void SwgCuiAvatarSelection::onAvatarListChanged (bool)
{
	if (m_waitingLogin)
	{
		if (m_messageBoxLoginWait)
			m_messageBoxLoginWait->closeMessageBox ();

		m_waitingLogin = false;
	}

	if (m_waitingLoginForDelete)
	{
		if (m_messageBoxLoginWait)
			m_messageBoxLoginWait->closeMessageBox ();

		m_waitingLoginForSelect = false;
		m_waitingLoginForDelete = false;
		m_waitingLoginForCreate = false;
		performDelete ();
	}
	else if (m_waitingLoginForSelect)
	{
		m_waitingLoginForSelect = false;
		m_waitingLoginForCreate = false;
		m_waitingLoginForDelete = false;
		requestAvatarSelection ();
	}
	else if (m_waitingLoginForCreate)
	{
		m_waitingLoginForSelect = false;
		m_waitingLoginForDelete = false;
		m_waitingLoginForCreate = false;
		handleCreate ();
	}
	else if (isActive ())
	{
		refreshList (false);
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::onAvailableCharacterSlotsChanged (bool)
{
	if (isActive ())
	{
		populateAvatarViewers (true);
		updateAvatarSelection ();
	}
}

//-----------------------------------------------------------------------

void SwgCuiAvatarSelection::receiveMessage(const MessageDispatch::Emitter &, const MessageDispatch::MessageBase & message)
{
	if(message.isType(UnnamedMessages::GameConnectionOpened))
	{
		CuiLoadingManager::setFullscreenLoadingString  (CuiStringIdsServer::server_loading_scene.localize ());
		return;
	}

	else if (message.isType (DeleteCharacterReplyMessage::MessageType))
	{
		Archive::ReadIterator ri = NON_NULL (safe_cast<const GameNetworkMessage *>(&message))->getByteStream().begin();
		const DeleteCharacterReplyMessage delReply (ri);

		if (m_waitingDeletion)
		{
			if (m_messageBoxDeleteWait)
				m_messageBoxDeleteWait->closeMessageBox ();

			if (m_messageBoxLoginWait)
				m_messageBoxLoginWait->closeMessageBox ();

			if (delReply.getResultCode () == DeleteCharacterReplyMessage::rc_OK)
			{
				CuiMessageBox::createInfoBox (CuiStringIdsServer::server_avatar_deleted.localize ());

				CuiLoginManager::removeAvatarFromList (*m_deletingAvatar);
				refreshList (true);
				CuiCachedAvatarManager::saveCharacterList ();
			}
			else
				CuiMessageBox::createInfoBox (CuiStringIdsServer::server_avatar_deleted_failed.localize ());

			m_deleteButton->SetEnabled (true);

			m_deletingAvatar->clear ();
			m_waitingDeletion = false;
		}
	}

	//----------------------------------------------------------------------

	else if (message.isType (CuiLoadingManager::Messages::FullscreenLoadingDisabled))
	{
		GameNetwork::setAcceptSceneCommand (false);
		m_table->SetEnabled (true);
		getPage ().SetFocus ();
		m_connectingToGame = false;
		updateAvatarSelection ();
//		reconnectLoginServer (false);
	}

	//----------------------------------------------------------------------

	else if (message.isType (Game::Messages::SCENE_CHANGED))
	{
		onSceneChanged (true);
	}

	//----------------------------------------------------------------------
	
	const CuiMessageBox::BoxMessage * const abm = dynamic_cast<const CuiMessageBox::BoxMessage *>(&message);
	
	if (abm)
	{
		if (abm->getMessageBox () == m_messageBoxDeleteWait)
		{
			m_messageBoxDeleteWait = 0;
		}
		
		else if (abm->getMessageBox () == m_messageBoxLoginWait)
		{
			m_messageBoxLoginWait = 0;
			
			if (message.isType (CuiMessageBox::Messages::COMPLETED))
			{
				const CuiMessageBox::CompletedMessage * const cm = NON_NULL (dynamic_cast<const CuiMessageBox::CompletedMessage *>(abm));
				
				//-- user closed it
				if (cm->getButtonType () != CuiMessageBox::GBT_None)
				{
					m_waitingLoginForDelete = false;
					m_waitingLogin          = false;
					m_waitingLoginForSelect = false;
					m_waitingLoginForCreate = false;
					m_table->SetEnabled (true);
					getPage ().SetFocus ();
					m_connectingToGame = false;
				}
			}
		}

		else if (abm->getMessageBox () == m_messageBox)
		{
			if (message.isType (CuiMessageBox::Messages::CLOSED))
			{
				m_connectingToGame = false;
				m_table->SetEnabled (true);
				getPage ().SetFocus ();
				m_messageBox = 0;
				//			m_waitingDeletion = false;
			}
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::clearCharacterList ()
{
	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));
	model->ClearData ();

	m_avatarViewerCount = 0;
	m_realAvatarCount = 0;
	m_slotPlaceholderCount = 0;
	m_selectedAvatarIndex = -1;
	m_carouselPosition = 0.0f;
	m_carouselTargetPosition = 0.0f;
	for (int i = 0; i < MaxAvatarViewers; ++i)
	{
		m_avatarCreatures[i] = 0;
		m_hoverAnimationPlayed[i] = false;
		if (m_avatarViewers[i])
		{
			m_avatarViewers[i]->clearObjects ();
			m_avatarViewers[i]->setUseOverrideShader ("", false);
			m_avatarViewers[i]->SetVisible (false);
		}
		if (m_avatarLabels[i])
			m_avatarLabels[i]->SetVisible (false);
	}
	if (m_moreSlotsText)
		m_moreSlotsText->SetVisible (false);
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::requestAvatarSelection ()
{
	if (getCurrentlySelectedAvatar ())
	{
		ConfigClientGame::setNextAutoConnectToGameServer (false);
		m_autoConnected = true;

		const CuiLoginManagerClusterInfo * const clusterInfo = CuiLoginManager::findClusterInfo (m_selectedAvatar->clusterId);

		if (clusterInfo)
		{
			if (m_waitingLoginForCreate || m_waitingLoginForDelete)
				return;

			//-- keep waiting
			if (m_waitingLogin)
			{
				m_waitingLoginForSelect = true;
				return;
			}

			m_waitingLoginForSelect = false;

			const uint32 clusterId = CuiLoginManager::getConnectedClusterId ();
			if (clusterId != 0)
			{
				if (clusterId != clusterInfo->id)
				{
					WARNING (true, ("SwgCuiAvatarSelection::requestAvatarSelection already connected to [%d], wants [%d], dropping", clusterId, clusterInfo->id));
					CuiLoginManager::disconnectFromCluster ();
				}
				else
				{
					m_proceed = true;
					m_table->SetEnabled	(false);
					m_connectingToGame = false;
					return;
				}
			}
			
			if (!GameNetwork::isConnectedToLoginServer ())
			{ 
				reconnectLoginServer (false);
				m_waitingLoginForSelect = true;
				return;
			}
			
			CuiLoginManager::connectToCluster (*clusterInfo);

			if (m_messageBox)
				m_messageBox->closeMessageBox ();

			if (!m_hasAlreadyRetriedConnection) {
				m_messageBox = CuiMessageBox::createMessageBox(CuiStringIdsServer::server_connecting_central.localize());
				m_messageBox->setRunner(true);
				m_messageBox->connectToMessages(*this);
			}
			
			m_waitingForConnection = true;
			m_waitingForClusterId = clusterInfo->id;
		}
		else
			WARNING (true, ("no cluster"));
	}
}


//----------------------------------------------------------------------

void SwgCuiAvatarSelection::requestAvatarDeletion  ()
{
	if (getCurrentlySelectedAvatar (false))
	{
		if (m_messageBox)
			m_messageBox->closeMessageBox ();

		s_avatarToDelete = *m_selectedAvatar;

		NOT_NULL (m_selectedAvatar);

		const CuiLoginManager::ClusterInfo * const clusterInfo = CuiLoginManager::findClusterInfo (s_avatarToDelete.clusterId);

		if (!clusterInfo)
			return;
		
		m_connectingToGame = false;
		m_deleteAvatarConfirmationMediator->setAvatarInfo(s_avatarToDelete);
		m_deleteAvatarConfirmationMediator->activate();
	}
}

//----------------------------------------------------------------------

bool SwgCuiAvatarSelection::getCurrentlySelectedAvatar (bool checkCluster)
{
	UITableModelDefault * const model = NON_NULL (safe_cast<UITableModelDefault *>(m_table->GetTableModel ()));
	const int row = m_table->GetLastSelectedRow ();

	Unicode::String currentlySelectedCharacter;
	if (model->GetValueAtText (row, 0, currentlySelectedCharacter) && !currentlySelectedCharacter.empty())
	{
		std::string::size_type const pos = currentlySelectedCharacter.find(s_unlockedSlotCharacterSuffix);
		if (pos != std::string::npos)
			currentlySelectedCharacter = currentlySelectedCharacter.substr(0, pos);

		const UIData * const selectedData = model->GetCellDataVisual (row, 0);

		if (!selectedData)
		{
			CuiMessageBox::createInfoBox (CuiStringIdsServer::server_err_no_character_selected.localize ());
			return false;
		}
		
		long l_clusterId = 0;
		if (!selectedData->GetPropertyLong (Properties::ClusterId, l_clusterId))
			WARNING (true, ("No clusterid"));

		const uint32 clusterId = static_cast<uint32>(l_clusterId);

		if (checkCluster)
		{						
			const CuiLoginManagerClusterInfo * const clusterInfo = CuiLoginManager::findClusterInfo (clusterId);
			
			if (!clusterInfo)
				WARNING (true, ("no clusterinfo"));
			else
			{
				if (!clusterInfo->up)
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_connection_unavailable.localize ());
					return false;
				}
				else if (clusterInfo->loading)
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_connection_loading.localize ());
					return false;
				}
				else if (clusterInfo->locked && !clusterInfo->isAdmin)
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_connection_locked.localize ());
					return false;
				}
				// Players should be allowed to login with existing characters
				// but not create new characters. The cluster list will still say 
				// restricted but players can login with existing characters
				/*
				else if (clusterInfo->restricted)
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_connection_restricted.localize ());
					return false;
				}
				*/
				else if (clusterInfo->isFull && !clusterInfo->isAdmin && !autoConnectOk())
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_cluster_full.localize ());
					return false;
				}
				else if (clusterInfo->getHost().empty () || clusterInfo->getPort() == 0)
				{
					CuiMessageBox::createInfoBox (CuiStringIdsServer::server_cluster_address_missing.localize ());
					return false;
				}
			}			
		}

		const CuiLoginManagerAvatarInfo * const avatarInfo = CuiLoginManager::findAvatarInfo (clusterId, currentlySelectedCharacter);
		
		if (!avatarInfo)
			WARNING (true, ("No avatar info"));
		else
		{
			*m_selectedAvatar  = *avatarInfo;
			return true;
		}

	}

	CuiMessageBox::createInfoBox (CuiStringIdsServer::server_err_no_character_selected.localize ());

	return false;
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::performDelete ()
{
	m_waitingDeletion = false;
	
	if (s_avatarToDelete.clusterId == 0 || s_avatarToDelete.networkId == NetworkId::cms_invalid)
		CuiMessageBox::createInfoBox (CuiStringIdsServer::server_err_no_character_selected.localize ());
	else
	{
		if (!GameNetwork::isConnectedToLoginServer ())
			reconnectLoginServer (true);
		else
		{
			const DeleteCharacterMessage delMsg (s_avatarToDelete.clusterId, s_avatarToDelete.networkId);
			GameNetwork::sendToLoginServer (delMsg, true);
			
			if (m_messageBoxDeleteWait)
				m_messageBoxDeleteWait->closeMessageBox ();
			
			m_messageBoxDeleteWait = CuiMessageBox::createMessageBox (CuiStringIdsServer::server_wait_avatar_delete.localize ());
			m_messageBoxDeleteWait->setRunner (true);
			m_messageBoxDeleteWait->connectToMessages (*this);
			
			m_waitingDeletion = true;
			*m_deletingAvatar = *m_selectedAvatar;
			
			m_deleteButton->SetEnabled (false);

			s_avatarToDelete.clear ();
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::reconnectLoginServer (bool forDelete)
{
	m_waitingLoginForSelect = false;
	m_waitingLoginForCreate = false;
	
	GameNetwork::disconnectConnectionServer ();
	GameNetwork::disconnectLoginServer      ();
	
	if (m_messageBoxLoginWait)
		m_messageBoxLoginWait->closeMessageBox ();
	
	const char* const sessionId = CuiLoginManager::getSessionIdKey ();
	
	//-- station connection
	if (sessionId)
	{
		//-- @todo request new session key so we can login
		GameNetwork::setUserPassword    (sessionId);
		GameNetwork::connectLoginServer (ConfigClientGame::getLoginServerAddress(), ConfigClientGame::getLoginServerPort());
		
		m_messageBoxLoginWait = CuiMessageBox::createMessageBox (CuiStringIdsServer::server_connecting_login.localize ());
		m_messageBoxLoginWait->setRunner (true);
		m_messageBoxLoginWait->connectToMessages (*this);
		
	}
	
	//-- direct login server connection, no station api
	else
	{
		// hook up to the loginserver
		GameNetwork::connectLoginServer (ConfigClientGame::getLoginServerAddress(), ConfigClientGame::getLoginServerPort());
		
		m_messageBoxLoginWait = CuiMessageBox::createMessageBox (CuiStringIdsServer::server_connecting_login.localize ());
		m_messageBoxLoginWait->setRunner (true);
		m_messageBoxLoginWait->connectToMessages (*this);
	}

	m_waitingLoginForDelete = forDelete;
	m_waitingLogin          = !forDelete;
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::onClusterStatusChanged (bool)
{
	refreshList (false);
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::onDeleteAvatarConfirmation(CuiLoginManagerAvatarInfo const &info)
{
	// Delete this character!

	s_avatarToDelete = info;

	performDelete();
}

//----------------------------------------------------------------------

void SwgCuiAvatarSelection::handleCreate ()
{
	if (m_waitingLoginForSelect || m_waitingLoginForDelete)
		return;
	
	//-- keep waiting
	if (m_waitingLogin)
	{
		m_waitingLoginForCreate = true;
		return;
	}
	
	m_waitingLoginForCreate = false;
		
	if (!GameNetwork::isConnectedToLoginServer ())
	{ 
		reconnectLoginServer (false);
		m_waitingLoginForCreate = true;
		return;
	}

	CuiTransition::startTransition(CuiMediatorTypes::AvatarSelection, CuiMediatorTypes::ClusterSelection);
	//CuiTransition::startTransition(CuiMediatorTypes::AvatarSelection, CuiMediatorTypes::AvatarSimple);
}

void SwgCuiAvatarSelection::OnCheckboxSet( UIWidget *context )
{
	if (context == m_showAvailableSlots)
	{
		m_showAvailableSlotsEnabled = true;
		CuiSettings::saveBoolean (getMediatorDebugName (), "showAvailableSlots", true);
		CuiSettings::save ();
		populateAvatarViewers (true);
		updateAvatarSelection ();
		return;
	}

	clearCharacterList();
	refreshList(true);

	CuiPreferences::setHideCharactersOnClosedGalaxies(true);
}

void SwgCuiAvatarSelection::OnCheckboxUnset( UIWidget *context )
{
	if (context == m_showAvailableSlots)
	{
		m_showAvailableSlotsEnabled = false;
		CuiSettings::saveBoolean (getMediatorDebugName (), "showAvailableSlots", false);
		CuiSettings::save ();
		populateAvatarViewers (true);
		updateAvatarSelection ();
		return;
	}

	clearCharacterList();
	refreshList(true);

	CuiPreferences::setHideCharactersOnClosedGalaxies(false);
}

// ======================================================================
