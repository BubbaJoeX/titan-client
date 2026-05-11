//======================================================================
//
// SwgCuiCinematicConversation.cpp
// KOTOR-style cinematic dialogue system for ground conversations
// Full camera control implementation
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCinematicConversation.h"

#include "clientGame/ClientObject.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/PlayerObject.h"
#include "clientGame/FreeCamera.h"
#include "clientObject/GameCamera.h"
#include "clientUserInterface/CuiConversationManager.h"
#include "clientUserInterface/CuiManager.h"
#include "swgClientUserInterface/SwgCuiHud.h"
#include "swgClientUserInterface/SwgCuiHudFactory.h"
#include "clientUserInterface/CuiObjectTextManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedNetworkMessages/MessageQueueNpcConversationCameraCommand.h"
#include "sharedCollision/CollisionEnums.h"
#include "sharedCollision/CollisionProperty.h"
#include "sharedCollision/CollisionWorld.h"
#include "sharedFoundation/Clock.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"

#include "UIBaseObject.h"
#include "UIButton.h"
#include "UIData.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UIText.h"
#include "UIWidget.h"
#include "UITypes.h"
#include "UIScrollbar.h"
#include "UnicodeUtils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cmath>

//======================================================================

namespace
{
	CellProperty const * getConversationCellForCamera(CreatureObject const * player, NetworkId const & npcId)
	{
		CellProperty const * cell = CellProperty::getWorldCellProperty();
		if (player && player->getParentCell())
			cell = player->getParentCell();
		else if (npcId.isValid())
		{
			Object const * npcObj = NetworkIdManager::getObjectById(npcId);
			if (npcObj && npcObj->getParentCell())
				cell = npcObj->getParentCell();
		}
		return cell;
	}

	bool isDialogueParticipantExtent(Object const * hit, CreatureObject const * player, NetworkId const & npcId)
	{
		if (!hit)
			return false;
		Object const * const root = hit->getRootParent();
		if (!root)
			return false;
		if (player && root == player->getRootParent())
			return true;
		if (!npcId.isValid())
			return false;
		Object const * const npcObj = NetworkIdManager::getObjectById(npcId);
		return npcObj && root == npcObj->getRootParent();
	}

	/// Scales face close-up distance / height from approximate standing height (~1.7m human = 1.0).
	float closeUpHeightScaleFromObject(Object const & obj, float horizontalRadius)
	{
		float vertSpan = 0.f;
		if (CollisionProperty const * const cp = obj.getCollisionProperty())
		{
			Sphere const s = cp->getBoundingSphere_w();
			vertSpan = std::max(vertSpan, 2.f * s.getRadius());
		}

		Vector const head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(obj);
		Vector const head_w = obj.rotateTranslate_o2w(head_o);
		float const baseY = obj.getPosition_w().y;
		vertSpan = std::max(vertSpan, std::max(0.35f, head_w.y - baseY) * 1.1f);

		if (vertSpan < 0.32f)
			vertSpan = std::max(vertSpan, horizontalRadius * 2.5f);

		float const refH = 1.7f;
		return std::max(0.88f, std::min(1.52f, vertSpan / refH));
	}

	/**
	 * Pull camera only when LOS passes through a dialogue participant's mesh (shorten onto their hull if needed).
	 * Terrain, buildings, cell hulls, and other statics are ignored — clipping through them is acceptable and
	 * avoids the camera snapping into geometry on exterior / city conversations.
	 */
	void resolveDesiredCameraAgainstCollision(NetworkId const & npcId, Vector const & pivot_w, Vector & ioDesiredCam_w)
	{
		CreatureObject const * const player = Game::getPlayerCreature();
		CellProperty const * const cell = getConversationCellForCamera(player, npcId);
		if (!cell)
			return;

		Vector radial = ioDesiredCam_w - pivot_w;
		float len = radial.magnitude();
		if (len < 0.04f)
			return;
		Vector const dir = radial * (1.0f / len);

		Object const * ignore = nullptr;
		Vector end = pivot_w + dir * len;

		for (int attempt = 0; attempt < 6; ++attempt)
		{
			float hitTime = 1.0f;
			Object const * hitObject = nullptr;
			QueryInteractionResult const r = CollisionWorld::queryInteraction(
				cell, pivot_w,
				cell, end,
				ignore,
				false,
				false,
				0.f,
				0.f,
				hitTime,
				hitObject);

			bool const solidHit =
				r == QIR_HitTerrain
				|| r == QIR_HitObjectExtent
				|| r == QIR_HitObjectFloor
				|| r == QIR_HitCellExtent
				|| r == QIR_HitCellFloor;

			if (!solidHit || hitTime >= 0.999f)
				return;

			if (hitObject && isDialogueParticipantExtent(hitObject, player, npcId))
			{
				ignore = hitObject->getRootParent();
				continue;
			}

			// Not a participant — keep desired camera (may clip static world).
			return;
		}
	}

	/** Look-at near upper torso so the player's full height fits the frame better than head-only CU. */
	Vector computePlayerReactionFramingLookAt_w()
	{
		CreatureObject * const player = Game::getPlayerCreature();
		if (!player)
			return Vector::zero;

		Vector head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*player);
		Vector head_w = player->rotateTranslate_o2w(head_o);
		head_w.y += 0.12f;

		CollisionProperty const * const cp = player->getCollisionProperty();
		if (!cp)
			return player->getPosition_w() * 0.42f + head_w * 0.58f;

		Sphere const s = cp->getBoundingSphere_w();
		Vector torso = s.getCenter();
		float const r = s.getRadius();
		if (r > 0.01f)
			torso.y += std::max(0.08f, 0.15f * r);
		return torso * 0.55f + head_w * 0.45f;
	}

	float computePlayerReactionCameraDistance(CreatureObject const * player)
	{
		float const kBaseCloseUpDistance = 1.2f;
		if (!player)
			return kBaseCloseUpDistance * 2.1f;
		float r = player->getAppearanceSphereRadius();
		if (CollisionProperty const * const cp = player->getCollisionProperty())
			r = std::max(r, cp->getBoundingSphere_w().getRadius());
		float const base = kBaseCloseUpDistance * 2.05f;
		return base + std::max(0.f, r - 0.48f) * 1.55f;
	}
	// FreeChaseCamera::alter() overwrites raw GameCamera transforms every frame while CI_freeChase is active.
	// Drive the god FreeCamera (CI_free) with pivot framing (same intent as the legacy npc conversation path).
	//
	// updateCameraFocus() eases m_currentCameraPos / LookAt each frame. Do not use FreeCamera's internal
	// interpolation here - setInterpolating(true) only converges ~30% per frame toward m_targetInfo, so when
	// the target moves every frame (our easing curve), the camera lags and scripted transitions appear broken.
	void alignFreeCameraToConversationCell(FreeCamera * const freeCamera, NetworkId const & npcId)
	{
		if (!freeCamera)
			return;

		CreatureObject * const player = Game::getPlayerCreature();
		Object * const npcObj = npcId.isValid() ? NetworkIdManager::getObjectById(npcId) : nullptr;

		CellProperty * cell = CellProperty::getWorldCellProperty();
		if (player && player->getParentCell())
			cell = const_cast<CellProperty *>(player->getParentCell());
		else if (npcObj && npcObj->getParentCell())
			cell = const_cast<CellProperty *>(npcObj->getParentCell());

		freeCamera->alignPivotWithCell(cell);
	}

	void applyShotToFreeCamera(FreeCamera * const freeCamera, NetworkId const & npcId, Vector const & camPos, Vector const & lookAt)
	{
		if (!freeCamera)
			return;

		alignFreeCameraToConversationCell(freeCamera, npcId);

		Vector resolvedCam = camPos;
		resolveDesiredCameraAgainstCollision(npcId, lookAt, resolvedCam);

		Vector const dv = lookAt - resolvedCam;
		float const dist = dv.magnitude();
		if (dist < 0.001f)
			return;

		freeCamera->setInterpolating(false);
		freeCamera->setMode(FreeCamera::M_pivot);
		freeCamera->setPivotPoint(lookAt);
		freeCamera->setPivotDistance(static_cast<real>(dist));
		freeCamera->setYaw(static_cast<real>(dv.theta()));
		freeCamera->setPitch(static_cast<real>(dv.phi()));
	}

}

//======================================================================

namespace SwgCuiCinematicConversationNamespace
{
	// Animation constants
	float const LETTERBOX_HEIGHT = 76.0f;
	float const LETTERBOX_ANIMATION_DURATION = 0.52f;
	float const DEFAULT_CAMERA_TRANSITION_DURATION = 0.44f;
	/// When the server omits duration on scripted look-at, avoid instant snaps through terrain.
	float const SCRIPTED_LOOKAT_DEFAULT_DURATION = 1.15f;
	float const SHOT_HOLD_TIME_MIN = 4.0f;
	float const SHOT_HOLD_TIME_MAX = 8.0f;
	// VO-style subtitle pacing (~220-260 wpm at English word lengths); scales slightly with line length.
	float const TYPEWRITER_CHARS_PER_SECOND_BASE = 36.0f;
	float const TYPEWRITER_CHARS_PER_SECOND_MIN = 26.0f;
	float const TYPEWRITER_CHARS_PER_SECOND_MAX = 44.0f;
	// Tighter than legacy close-up: face fills frame more like KOTOR dialog framing.
	float const CLOSE_UP_FACE_DISTANCE = 0.87f;
	float const CLOSE_UP_FACE_SIDE_OFFSET = 0.14f;
	float const CLOSE_UP_FACE_CAMERA_Y_BIAS = 0.05f;
	float const FACE_FOCUS_HEAD_Y_BIAS = 0.04f;
	/// Above this appearance/collision size we frame chest/torso (mobs) instead of face — avoids "nose-up" CU crop.
	float const HUMANOID_CLOSEUP_RADIUS_CAP = 0.78f;
	float const TYPEWRITER_PAUSE_AFTER_COMMA_SPACE = 1.0f;
	float const TYPEWRITER_PAUSE_AFTER_ELLIPSIS = 5.0f;
	float const TYPEWRITER_PAUSE_AFTER_SIX_DOTS = 6.0f;
	/// After player line finishes printing, hold reaction camera this long before NPC line + new responses.
	float const REACTION_POST_TYPEWRITER_BUFFER_SEC = 1.25f;
	/// Direct face framing for automatic dialogue; short cut (not the slower establishing / scripted transitions).
	float const FACE_DIALOGUE_TRANSITION_SEC = 0.17f;
	long const RESPONSE_ROW_STRIDE_PX = 23L;

	// Response prefix mappings
	struct PrefixMapping
	{
		char const * tag;
		SwgCuiCinematicConversation::ResponsePrefix prefix;
	};

	PrefixMapping const s_prefixMappings[] =
	{
		{ "[Agree]",      SwgCuiCinematicConversation::RP_Agree },
		{ "[Decline]",    SwgCuiCinematicConversation::RP_Decline },
		{ "[Persuade]",   SwgCuiCinematicConversation::RP_Persuade },
		{ "[Intimidate]", SwgCuiCinematicConversation::RP_Intimidate },
		{ "[Lie]",        SwgCuiCinematicConversation::RP_Lie },
		{ "[Question]",   SwgCuiCinematicConversation::RP_Question },
		{ "[Info]",       SwgCuiCinematicConversation::RP_Info },
		{ "[Attack]",     SwgCuiCinematicConversation::RP_Attack },
		{ nullptr,        SwgCuiCinematicConversation::RP_None }
	};
}

using namespace SwgCuiCinematicConversationNamespace;

//----------------------------------------------------------------------
// Static member initialization

bool SwgCuiCinematicConversation::ms_enabled = true;
bool SwgCuiCinematicConversation::ms_active = false;
SwgCuiCinematicConversation * SwgCuiCinematicConversation::ms_cameraCommandTarget = nullptr;

float const SwgCuiCinematicConversation::CLOSE_UP_DISTANCE = 1.2f;
float const SwgCuiCinematicConversation::MEDIUM_SHOT_DISTANCE = 2.5f;
float const SwgCuiCinematicConversation::OVER_SHOULDER_DISTANCE = 1.8f;
float const SwgCuiCinematicConversation::TWO_SHOT_DISTANCE = 3.5f;
float const SwgCuiCinematicConversation::HEAD_HEIGHT_OFFSET = 0.1f;
float const SwgCuiCinematicConversation::CAMERA_TRANSITION_SPEED = 1.0f;

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::isEnabled()
{
	return ms_enabled;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setEnabled(bool enabled)
{
	ms_enabled = enabled;
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::isActive()
{
	return ms_active;
}

//----------------------------------------------------------------------

CuiMediator * SwgCuiCinematicConversation::provideActiveInstanceForConversationManager()
{
	return (ms_active && ms_cameraCommandTarget) ? ms_cameraCommandTarget : nullptr;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::executeCloseFromConversationManager()
{
	SwgCuiCinematicConversation * const self = ms_cameraCommandTarget;
	if (!self)
		return;

	// Do not use closeThroughWorkspace() — CuiWorkspace::close() returns immediately when !isCloseable().
	if (self->CuiMediator::isActive())
		self->deactivate();
	else if (ms_active)
	{
		self->getPage().SetVisible(false);
		self->performDeactivate();
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::dispatchConversationKeystrokeFromManager(unsigned short const keystroke)
{
	SwgCuiCinematicConversation * const self = ms_cameraCommandTarget;
	if (!self || !ms_enabled)
		return;
	if (!self->CuiMediator::isActive())
		return;

	UIMessage msg;
	msg.Type = UIMessage::KeyDown;
	msg.Keystroke = keystroke;
	msg.Data = 0;
	self->OnMessage(&self->getPage(), msg);
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::scriptedLookAtOrbitPredicateThunk()
{
	if (!ms_cameraCommandTarget)
		return false;
	return ms_cameraCommandTarget->m_scriptedLookAtFramingActive
		&& ms_cameraCommandTarget->m_cameraControlActive
		&& !ms_cameraCommandTarget->m_cameraTransitioning;
}

//----------------------------------------------------------------------

float SwgCuiCinematicConversation::easeInOutCubic(float t)
{
	return t < 0.5f
		? 4.0f * t * t * t
		: 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static float easeInCubicLocal(float t)
{
	return t * t * t;
}

static float easeOutCubicLocal(float t)
{
	float const u = 1.0f - t;
	return 1.0f - u * u * u;
}

//----------------------------------------------------------------------

SwgCuiCinematicConversation::SwgCuiCinematicConversation(UIPage & page) :
CuiMediator("SwgCuiCinematicConversation", page),
UIEventCallback(),
m_callback(new MessageDispatch::Callback),
m_letterboxTop(nullptr),
m_letterboxBottom(nullptr),
m_dialoguePanel(nullptr),
m_npcNameText(nullptr),
m_npcMessageText(nullptr),
m_responsePanel(nullptr),
m_responseScrollbar(nullptr),
m_npcViewerPage(nullptr),
m_npcViewer(nullptr),
m_endConversationButton(nullptr),
m_responsePagePrevButton(nullptr),
m_responsePageNextButton(nullptr),
m_responsePageLabel(nullptr),
m_scriptedLookAtCameraHint(nullptr),
m_gmCinematicBadge(nullptr),
m_responseClonePrototype(nullptr),
m_cachedUILayoutValid(false),
m_cachedDialoguePanelSize(),
m_cachedDialoguePanelLocation(),
m_cachedResponsePanelSize(),
m_cachedResponsePanelLocation(),
m_cachedEndButtonLocation(),
m_targetNpcId(),
m_currentResponses(),
m_responsePageStart(0),
m_deferCloseUntilUpdate(false),
m_letterboxAnimationTime(0.0f),
m_letterboxTargetHeight(0.0f),
m_currentLetterboxHeight(0.0f),
m_letterboxAnimationStartHeight(0.0f),
m_letterboxAnimating(false),
m_hidePageAfterLetterboxOut(false),
m_cameraControlActive(false),
m_savedCameraView(0),
m_savedCameraTransform(),
m_currentCameraPos(),
m_currentCameraLookAt(),
m_targetCameraPos(),
m_targetCameraLookAt(),
m_startCameraPos(),
m_startCameraLookAt(),
m_cameraTransitionTime(0.0f),
m_cameraTransitionDuration(DEFAULT_CAMERA_TRANSITION_DURATION),
m_cameraTransitioning(false),
m_currentShotType(CST_CloseUp),
m_shotHoldTime(SHOT_HOLD_TIME_MIN),
m_timeSinceLastShotChange(0.0f),
	m_savedHudEnabled(true),
	m_deferHudRestoreUntilLetterboxOut(false),
	m_playerReactionHoldActive(false),
	m_playerReactionBeatPending(false),
	m_reactionPostLineBufferRemaining(-1.f),
	m_haveDeferredBranchResponses(false),
	m_deferredBranchResponses(),
	m_lastNpcMessageForCamera(),
	m_deferIncomingNpcSubtitle(false),
	m_deferredNpcSubtitle(),
	m_scriptedLookAtFramingActive(false),
	m_typewriterActive(false),
	m_typewriterFullText(),
	m_typewriterRevealLength(0),
	m_typewriterCharAccumulator(0.0f),
	m_typewriterCharsPerSecond(TYPEWRITER_CHARS_PER_SECOND_BASE),
	m_typewriterPauseRemaining(0.0f)
{
	// Get UI elements
	getCodeDataObject(TUIPage, m_letterboxTop, "letterboxTop");
	getCodeDataObject(TUIPage, m_letterboxBottom, "letterboxBottom");
	getCodeDataObject(TUIPage, m_dialoguePanel, "dialoguePanel");
	getCodeDataObject(TUIText, m_npcNameText, "npcName");
	getCodeDataObject(TUIText, m_npcMessageText, "npcMessage");
	getCodeDataObject(TUIPage, m_responsePanel, "responsePanel");
	getCodeDataObject(TUIScrollbar, m_responseScrollbar, "responseScrollbar", true);
	getCodeDataObject(TUIButton, m_endConversationButton, "endConversation");
	getCodeDataObject(TUIButton, m_responsePagePrevButton, "responsePagePrev", true);
	getCodeDataObject(TUIButton, m_responsePageNextButton, "responsePageNext", true);
	getCodeDataObject(TUIText, m_responsePageLabel, "responsePageLabel", true);
	getCodeDataObject(TUIText, m_scriptedLookAtCameraHint, "scriptedLookAtCameraHint", true);
	if (m_scriptedLookAtCameraHint)
	{
		m_scriptedLookAtCameraHint->SetLocalText(Unicode::narrowToWide(
			"Shift + mouse to pan   |   Shift + scroll to zoom"));
		m_scriptedLookAtCameraHint->SetVisible(false);
	}
	getCodeDataObject(TUIText, m_gmCinematicBadge, "gmCinematicBadge", true);
	if (m_gmCinematicBadge)
	{
		m_gmCinematicBadge->SetLocalText(Unicode::narrowToWide("*GM*"));
		m_gmCinematicBadge->SetPreLocalized(true);
		m_gmCinematicBadge->SetVisible(false);
	}

	// Get optional NPC viewer (KOTOR style has no portrait)
	UIBaseObject * viewerObj = nullptr;
	if (getCodeDataObject(TUIPage, m_npcViewerPage, "npcViewerPage", true))
	{
		viewerObj = m_npcViewerPage->GetChild("viewer");
		m_npcViewer = dynamic_cast<CuiWidget3dObjectListViewer *>(viewerObj);
		if (m_npcViewer)
		{
			m_npcViewer->setRotateSpeed(0.0f);
			m_npcViewer->setCameraLookAtCenter(true);
		}
	}

	// Response rows from .inc (response1..response6); extras are cloned at runtime from response6.
	for (int i = 0; i < BASE_RESPONSE_SLOTS; ++i)
	{
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "response%d", i + 1);

		ResponseSlot slot;
		slot.page = nullptr;
		slot.button = nullptr;
		slot.prefixText = nullptr;
		slot.text = nullptr;
		slot.ownedDuplicate = false;

		getCodeDataObject(TUIPage, slot.page, buffer, false);
		if (slot.page)
		{
			slot.button = dynamic_cast<UIButton *>(slot.page->GetChild("button"));
			slot.prefixText = dynamic_cast<UIText *>(slot.page->GetChild("prefix"));
			slot.text = dynamic_cast<UIText *>(slot.page->GetChild("text"));
			if (slot.button)
				registerMediatorObject(*slot.button, true);
			if (slot.prefixText)
				slot.prefixText->SetPreLocalized(true);
			if (slot.text)
				slot.text->SetPreLocalized(true);
		}

		m_responseSlots.push_back(slot);
	}

	if (m_responseSlots.size() >= static_cast<size_t>(BASE_RESPONSE_SLOTS) &&
		m_responseSlots[static_cast<size_t>(BASE_RESPONSE_SLOTS - 1)].page)
	{
		m_responseClonePrototype = m_responseSlots[static_cast<size_t>(BASE_RESPONSE_SLOTS - 1)].page;
	}

	if (m_endConversationButton)
	{
		registerMediatorObject(*m_endConversationButton, true);
	}
	if (m_responsePagePrevButton)
		registerMediatorObject(*m_responsePagePrevButton, true);
	if (m_responsePageNextButton)
		registerMediatorObject(*m_responsePageNextButton, true);

	if (m_dialoguePanel)
	{
		registerMediatorObject(*m_dialoguePanel, true);
	}

	if (m_npcNameText)
	{
		m_npcNameText->SetPreLocalized(true);
	}
	if (m_npcMessageText)
	{
		m_npcMessageText->SetPreLocalized(true);
	}

	registerMediatorObject(getPage(), true);

	// Initialize letterbox to hidden (size + fade — animated in on activate)
	if (m_letterboxTop)
	{
		UISize size = m_letterboxTop->GetSize();
		size.y = 0;
		m_letterboxTop->SetSize(size);
		m_letterboxTop->SetOpacity(0.0f);
		m_letterboxTop->SetBackgroundOpacity(0.0f);
	}
	if (m_letterboxBottom)
	{
		UISize size = m_letterboxBottom->GetSize();
		size.y = 0;
		m_letterboxBottom->SetSize(size);
		m_letterboxBottom->SetOpacity(0.0f);
		m_letterboxBottom->SetBackgroundOpacity(0.0f);
	}

	// So embedded/workspace close paths can dismiss this page (same pattern as SwgCuiTrade / Vendor).
	setState(MS_closeable);
	setState(MS_closeDeactivates);
}

//----------------------------------------------------------------------

SwgCuiCinematicConversation::~SwgCuiCinematicConversation()
{
	applyDeferredHudRestoreIfNeeded();

	releaseDynamicResponseSlots();

	if (ms_cameraCommandTarget == this)
	{
		CuiConversationManager::setCloseCinematicUiHandler(nullptr);
		CuiConversationManager::setActiveCinematicMediatorAccessor(nullptr);
		CuiConversationManager::setCinematicScriptedLookAtOrbitPredicate(nullptr);
		CuiConversationManager::setCinematicConversationKeystrokeHandler(nullptr);
		CuiConversationManager::setCameraCommandHandler(nullptr);
		ms_cameraCommandTarget = nullptr;
	}
	delete m_callback;
	m_callback = nullptr;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::performActivate()
{
	applyDeferredHudRestoreIfNeeded();

	setStickyVisible(true);
	CuiConversationManager::setCinematicConversationUiActive(true);

	CuiMediator::performActivate();

	cacheDialogueUILayout();

	ms_active = true;
	m_deferCloseUntilUpdate = false;

	m_savedHudEnabled = true;
	m_playerReactionHoldActive = false;
	m_playerReactionBeatPending = false;
	m_reactionPostLineBufferRemaining = -1.f;
	m_haveDeferredBranchResponses = false;
	m_deferredBranchResponses.clear();
	m_lastNpcMessageForCamera.clear();
	m_deferIncomingNpcSubtitle = false;
	m_deferredNpcSubtitle.clear();
	m_scriptedLookAtFramingActive = false;
	m_hidePageAfterLetterboxOut = false;
	m_deferHudRestoreUntilLetterboxOut = false;
	m_responsePageStart = 0;
	if (Game::getHudSceneType() == Game::ST_ground)
	{
		SwgCuiHud * const hud = SwgCuiHudFactory::findMediatorForCurrentHud();
		if (hud)
		{
			m_savedHudEnabled = hud->getHudEnabled();
			hud->setHudEnabled(false);
		}
	}

	// Connect to conversation manager signals
	m_callback->connect(*this, &SwgCuiCinematicConversation::onTargetChanged,
		static_cast<CuiConversationManager::Messages::TargetChanged *>(0));
	m_callback->connect(*this, &SwgCuiCinematicConversation::onResponsesChanged,
		static_cast<CuiConversationManager::Messages::ResponsesChanged *>(0));
	m_callback->connect(*this, &SwgCuiCinematicConversation::onConversationEnded,
		static_cast<CuiConversationManager::Messages::ConversationEnded *>(0));

	ms_cameraCommandTarget = this;
	CuiConversationManager::setCloseCinematicUiHandler(&SwgCuiCinematicConversation::executeCloseFromConversationManager);
	CuiConversationManager::setActiveCinematicMediatorAccessor(&SwgCuiCinematicConversation::provideActiveInstanceForConversationManager);
	CuiConversationManager::setCameraCommandHandler(&SwgCuiCinematicConversation::handleCameraCommand);
	CuiConversationManager::setCinematicScriptedLookAtOrbitPredicate(&SwgCuiCinematicConversation::scriptedLookAtOrbitPredicateThunk);
	CuiConversationManager::setCinematicConversationKeystrokeHandler(&SwgCuiCinematicConversation::dispatchConversationKeystrokeFromManager);

	// Start letterbox animation (slide + fade from screen top / bottom)
	m_letterboxAnimationStartHeight = m_currentLetterboxHeight;
	m_letterboxTargetHeight = LETTERBOX_HEIGHT;
	m_letterboxAnimating = true;
	m_letterboxAnimationTime = 0.0f;

	// Block player input during cinematic
	CuiManager::requestPointer(true);

	// Get current target
	CachedNetworkId const & targetId = CuiConversationManager::getTarget();
	if (targetId.isValid())
	{
		m_targetNpcId = targetId;
		setupNpcViewer();

		// Initialize camera control
		initializeCameraControl();
	}

	// Populate initial data
	setNpcMessage(CuiConversationManager::getLastMessage());

	CuiConversationManager::StringVector responses;
	CuiConversationManager::getResponses(responses);
	setResponses(responses);

	// Keyboard routing: IoWin Escape handling uses isCinematicConversationUiActive(); avoid
	// incrementKeyboardInputActiveCount here -- it forces IoWin to set retval on raw KeyDown and blocks stop().

	setEnabled(true);
	getPage().SetEnabled(true);
	getPage().SetFocus();
	if (m_dialoguePanel)
		m_dialoguePanel->SetFocus();

	// CuiMediator::update() (camera easing, letterbox) runs only when isUpdating() is true; the base
	// activate() path only enables that when m_maxRangeFromObject > 0, which this page does not set.
	setIsUpdating(true);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::performDeactivate()
{
	CuiConversationManager::setCloseCinematicUiHandler(nullptr);

	m_deferCloseUntilUpdate = false;

	m_scriptedLookAtFramingActive = false;

	if (m_scriptedLookAtCameraHint)
		m_scriptedLookAtCameraHint->SetVisible(false);
	m_typewriterFullText.clear();
	m_typewriterRevealLength = 0;
	m_typewriterCharAccumulator = 0.0f;
	m_typewriterPauseRemaining = 0.0f;
	m_deferIncomingNpcSubtitle = false;
	m_deferredNpcSubtitle.clear();
	m_playerReactionBeatPending = false;
	m_reactionPostLineBufferRemaining = -1.f;
	m_haveDeferredBranchResponses = false;
	m_deferredBranchResponses.clear();

	// Disconnect conversation signals before camera/HUD restore — avoids nested emits during teardown.
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onTargetChanged,
		static_cast<CuiConversationManager::Messages::TargetChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onResponsesChanged,
		static_cast<CuiConversationManager::Messages::ResponsesChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onConversationEnded,
		static_cast<CuiConversationManager::Messages::ConversationEnded *>(0));

	restoreDialogueUILayout();
	releaseDynamicResponseSlots();

	CuiConversationManager::setCinematicConversationKeystrokeHandler(nullptr);
	CuiConversationManager::setCinematicScriptedLookAtOrbitPredicate(nullptr);
	CuiConversationManager::setCameraCommandHandler(nullptr);
	ms_cameraCommandTarget = nullptr;

	ms_active = false;

	CuiConversationManager::setActiveCinematicMediatorAccessor(nullptr);

	CuiConversationManager::setCinematicConversationUiActive(false);

	m_deferHudRestoreUntilLetterboxOut = false;
	if (Game::getHudSceneType() == Game::ST_ground)
	{
		SwgCuiHud * const hud = SwgCuiHudFactory::findMediatorForCurrentHud();
		if (hud)
		{
			if (m_currentLetterboxHeight > 0.5f)
				m_deferHudRestoreUntilLetterboxOut = true;
			else
				hud->setHudEnabled(m_savedHudEnabled);
		}
	}

	setStickyVisible(false);

	// Restore camera control
	restoreCameraControl();

	// Release pointer
	CuiManager::requestPointer(false);

	// Parent deactivate() hid the page before this runs; letterbox bars need a visible page to render.
	m_hidePageAfterLetterboxOut = false;
	if (m_currentLetterboxHeight > 0.5f)
	{
		getPage().SetVisible(true);
		m_hidePageAfterLetterboxOut = true;
		m_letterboxAnimationStartHeight = m_currentLetterboxHeight;
		m_letterboxTargetHeight = 0.0f;
		m_letterboxAnimating = true;
		m_letterboxAnimationTime = 0.0f;
		setIsUpdating(true);
	}
	else
	{
		m_letterboxAnimating = false;
	}

	// Clear viewer
	if (m_npcViewer)
	{
		m_npcViewer->clearObjects();
	}

	CuiMediator::performDeactivate();

	if (!m_hidePageAfterLetterboxOut)
		setIsUpdating(false);
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::canActivateWhenWorkspaceDisabled () const
{
	return true;
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::shouldSurviveDisabledWorkspace () const
{
	return isActive();
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::OnMessage(UIWidget * /*context*/, UIMessage const & msg)
{
	if (!isActive())
		return true;
	if (msg.Type == UIMessage::KeyDown && msg.Keystroke == UIMessage::Escape)
	{
		CuiConversationManager::closeCinematicConversationFromInput();
		return true;
	}
	if (msg.Type == UIMessage::KeyDown && msg.Keystroke == UIMessage::Space)
	{
		if (m_cameraControlActive && isNpcLinePrintingLocked())
		{
			finishNpcMessageTypewriter();
			return true;
		}
	}
	if (msg.Type == UIMessage::KeyDown)
	{
		unsigned short const ks = msg.Keystroke;
		size_t const total = m_currentResponses.size();
		size_t const totalPages =
			(total == 0u)
				? 1u
				: (total + static_cast<size_t>(RESPONSES_PER_PAGE) - 1u) / static_cast<size_t>(RESPONSES_PER_PAGE);
		if (totalPages > 1u)
		{
			if (ks == static_cast<unsigned short>('-') || ks == static_cast<unsigned short>('_'))
			{
				changeResponsePage(-1);
				return true;
			}
			if (ks == static_cast<unsigned short>('=') || ks == static_cast<unsigned short>('+'))
			{
				changeResponsePage(1);
				return true;
			}
		}
		if (trySelectResponseByNumberKey(ks))
			return true;
	}
	return true;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::initializeCameraControl()
{
	GroundScene * const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
	if (!groundScene)
		return;

	// Save current camera state
	m_savedCameraView = groundScene->getCurrentView();

	GameCamera * const chaseCamera = groundScene->getCamera(GroundScene::CI_freeChase);
	if (chaseCamera)
		m_savedCameraTransform = chaseCamera->getTransform_o2w();

	groundScene->activateGodClientCamera();

	m_cameraControlActive = true;
	m_timeSinceLastShotChange = 0.0f;
	m_shotHoldTime = 0.0f;

	// Frame the conversation subjects immediately — do not seed from chase (player-forward), which reads as "player cam".
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const npcObj = NetworkIdManager::getObjectById(m_targetNpcId);

	Vector framingPos;
	Vector framingLookAt;

	if (player && npcObj)
	{
		if (useWideOpenFaceEstablishingShot())
			calculateWideOpenFaceShot(framingPos, framingLookAt);
		else
			calculateCloseUpShot(framingPos, framingLookAt);
		m_currentShotType = CST_CloseUp;
	}
	else if (npcObj)
	{
		calculateCloseUpShot(framingPos, framingLookAt);
		m_currentShotType = CST_CloseUp;
	}
	else if (chaseCamera)
	{
		m_currentCameraPos = chaseCamera->getPosition_w();
		Vector const forward = chaseCamera->getObjectFrameK_w();
		m_currentCameraLookAt = m_currentCameraPos + forward * 5.0f;
		m_currentShotType = CST_CloseUp;

		if (FreeCamera * const freeCamera = groundScene->getGodClientCamera())
			applyShotToFreeCamera(freeCamera, m_targetNpcId, m_currentCameraPos, m_currentCameraLookAt);
		return;
	}
	else
		return;

	m_currentCameraPos = framingPos;
	m_currentCameraLookAt = framingLookAt;
	m_targetCameraPos = framingPos;
	m_targetCameraLookAt = framingLookAt;
	m_startCameraPos = framingPos;
	m_startCameraLookAt = framingLookAt;
	m_cameraTransitioning = false;
	m_cameraTransitionTime = 0.f;

	// First subtitle still calls setNpcMessage() → CloseUp; we start from dual/NPC so that transition is dialogue-centric.

	if (FreeCamera * const freeCamera = groundScene->getGodClientCamera())
		applyShotToFreeCamera(freeCamera, m_targetNpcId, m_currentCameraPos, m_currentCameraLookAt);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::restoreCameraControl()
{
	if (!m_cameraControlActive)
		return;

	GroundScene * const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
	if (!groundScene)
		return;

	groundScene->deactivateGodClientCamera();

	// Restore the original camera view (clamp — saved index must stay valid if scene/view enums shift).
	int const numViews = groundScene->getNumberOfViews();
	if (numViews > 0)
	{
		int view = m_savedCameraView;
		if (view < 0 || view >= numViews)
			view = static_cast<int>(GroundScene::CI_freeChase);
		groundScene->setView(view);
	}

	m_cameraControlActive = false;
}

//----------------------------------------------------------------------

Vector SwgCuiCinematicConversation::computeNpcHeadPosition() const
{
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!targetObj)
		return Vector::zero;

	// Get NPC head position using the utility function
	Vector headPoint_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*targetObj);

	// Add a small offset to look slightly above the head for better framing
	headPoint_o.y += HEAD_HEIGHT_OFFSET;

	// Transform to world coordinates
	return targetObj->rotateTranslate_o2w(headPoint_o);
}

//----------------------------------------------------------------------

Vector SwgCuiCinematicConversation::computeNpcDialogueFramingPosition() const
{
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!targetObj)
		return Vector::zero;

	float r = targetObj->getAppearanceSphereRadius();
	CollisionProperty const * const cp = targetObj->getCollisionProperty();
	if (cp)
		r = std::max(r, cp->getBoundingSphere_w().getRadius());

	if (r <= HUMANOID_CLOSEUP_RADIUS_CAP)
		return computeNpcHeadPosition();

	// Large creatures / mobs: aim at upper torso so the whole actor fits in frame (head helper sits too high).
	Vector head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*targetObj);
	Vector chest_w = targetObj->rotateTranslate_o2w(head_o);
	chest_w.y += -0.38f * r;
	if (cp)
	{
		Vector const center = cp->getBoundingSphere_w().getCenter();
		chest_w = center * 0.58f + chest_w * 0.42f;
	}
	return chest_w;
}

//----------------------------------------------------------------------

Vector SwgCuiCinematicConversation::computeScriptedLookAtPoint(Object & targetObj) const
{
	CreatureObject * const creature = CreatureObject::asCreatureObject(&targetObj);
	if (creature)
	{
		Vector head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(targetObj);
		head_o.y += HEAD_HEIGHT_OFFSET;
		return targetObj.rotateTranslate_o2w(head_o);
	}

	CollisionProperty const * const cp = targetObj.getCollisionProperty();
	if (cp)
	{
		Sphere const & s = cp->getBoundingSphere_w();
		Vector lookAt = s.getCenter();
		float const r = s.getRadius();
		if (r > 0.001f)
		{
			// Aim below geometric center so tall props sit higher in the frame (clear bottom dialogue / letterbox).
			lookAt.y -= r * 0.55f;
		}
		return lookAt;
	}

	Vector head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(targetObj);
	head_o.y += HEAD_HEIGHT_OFFSET;
	return targetObj.rotateTranslate_o2w(head_o);
}

//----------------------------------------------------------------------

Vector SwgCuiCinematicConversation::computePlayerPosition() const
{
	CreatureObject * const player = Game::getPlayerCreature();
	if (!player)
		return Vector::zero;

	// Get player head position
	Vector headPoint_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*player);
	return player->rotateTranslate_o2w(headPoint_o);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateCloseUpShot(Vector & outCameraPos, Vector & outLookAt) const
{
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!targetObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	if (useWideOpenFaceEstablishingShot())
	{
		calculateWideOpenFaceShot(outCameraPos, outLookAt);
		return;
	}

	outLookAt = computeNpcDialogueFramingPosition();

	float distScale = 1.f;
	float horizR = 0.5f;
	{
		horizR = targetObj->getAppearanceSphereRadius();
		CollisionProperty const * const cp = targetObj->getCollisionProperty();
		if (cp)
			horizR = std::max(horizR, cp->getBoundingSphere_w().getRadius());
		if (horizR > HUMANOID_CLOSEUP_RADIUS_CAP)
			distScale = std::max(1.f, horizR / HUMANOID_CLOSEUP_RADIUS_CAP);
	}

	float const hScale = closeUpHeightScaleFromObject(*targetObj, horizR);

	// Camera sits on the conversation side of the face (toward the player), not behind the head.
	// Object K points where the creature faces; offset +K moves from the face toward who they're talking to.
	Vector npcForward = targetObj->getObjectFrameK_w();
	npcForward.y = 0.f;
	if (npcForward.magnitude() < 0.001f)
	{
		Vector towardPlayer = computePlayerPosition() - outLookAt;
		towardPlayer.y = 0.f;
		float const tp = towardPlayer.magnitude();
		if (tp > 0.001f)
			npcForward = towardPlayer / tp;
		else
			npcForward = Vector::unitZ;
	}
	else
		npcForward.normalize();

	Vector npcRight = targetObj->getObjectFrameI_w();
	npcRight.y = 0.f;
	if (npcRight.magnitude() > 0.001f)
		npcRight.normalize();

	float const faceDist = CLOSE_UP_FACE_DISTANCE * distScale * hScale;
	float const sideOff = CLOSE_UP_FACE_SIDE_OFFSET * std::min(distScale * hScale, 1.48f);
	float const yBias = CLOSE_UP_FACE_CAMERA_Y_BIAS * std::min(distScale * hScale, 1.38f);

	outCameraPos = outLookAt + npcForward * faceDist + npcRight * sideOff;
	outCameraPos.y = outLookAt.y + yBias;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculatePlayerFaceCloseUpShot(Vector & outCameraPos, Vector & outLookAt) const
{
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const npcObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!player || !npcObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	Vector head_o = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*player);
	outLookAt = player->rotateTranslate_o2w(head_o);
	outLookAt.y += FACE_FOCUS_HEAD_Y_BIAS;

	float distScale = 1.f;
	float horizR = 0.48f;
	{
		horizR = player->getAppearanceSphereRadius();
		if (CollisionProperty const * const cp = player->getCollisionProperty())
			horizR = std::max(horizR, cp->getBoundingSphere_w().getRadius());
		if (horizR > HUMANOID_CLOSEUP_RADIUS_CAP)
			distScale = std::max(1.f, horizR / HUMANOID_CLOSEUP_RADIUS_CAP);
	}

	float const hScale = closeUpHeightScaleFromObject(*player, horizR);

	Vector towardNpc = computeNpcDialogueFramingPosition() - outLookAt;
	towardNpc.y = 0.f;
	float tn = towardNpc.magnitude();
	if (tn < 0.001f)
	{
		Vector k = player->getObjectFrameK_w();
		k.y = 0.f;
		if (k.magnitude() > 0.001f)
			towardNpc = k * (1.0f / k.magnitude());
		else
			towardNpc = Vector::unitZ;
	}
	else
		towardNpc *= 1.0f / tn;

	Vector playerRight = player->getObjectFrameI_w();
	playerRight.y = 0.f;
	if (playerRight.magnitude() > 0.001f)
		playerRight.normalize();
	else
		playerRight = Vector::unitX;

	float const faceDist = CLOSE_UP_FACE_DISTANCE * distScale * hScale;
	float const sideOff = CLOSE_UP_FACE_SIDE_OFFSET * std::min(distScale * hScale, 1.48f);
	float const yBias = CLOSE_UP_FACE_CAMERA_Y_BIAS * std::min(distScale * hScale, 1.38f);

	outCameraPos = outLookAt + towardNpc * faceDist + playerRight * sideOff;
	outCameraPos.y = outLookAt.y + yBias;
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::useWideOpenFaceEstablishingShot() const
{
	if (CuiConversationManager::getAppearanceOverrideTemplateCrc() != 0)
		return true;

	Object const * const o = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!o)
		return false;

	char const * const tn = o->getObjectTemplateName();
	if (!tn)
		return false;

	return std::strstr(tn, "open_face") != nullptr
		|| std::strstr(tn, "Open_Face") != nullptr
		|| std::strstr(tn, "openface") != nullptr;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateWideOpenFaceShot(Vector & outCameraPos, Vector & outLookAt) const
{
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!player || !targetObj)
	{
		calculateMediumShot(outCameraPos, outLookAt);
		Vector pull = outCameraPos - outLookAt;
		pull.y = 0.f;
		float const h = pull.magnitude();
		if (h > 0.001f)
			outCameraPos = outCameraPos + pull * (1.45f / h);
		outCameraPos.y += 0.55f;
		outLookAt.y -= 0.15f;
		return;
	}

	calculateTwoShot(outCameraPos, outLookAt);

	Vector const playerPos = computePlayerPosition();
	Vector const npcPos = computeNpcDialogueFramingPosition();
	Vector mid = (playerPos + npcPos) * 0.5f;

	Vector radial = outCameraPos - mid;
	radial.y = 0.f;
	float const horiz = radial.magnitude();
	if (horiz > 0.001f)
		outCameraPos = outCameraPos + radial * (2.05f / horiz);

	Vector playerToNpc = npcPos - playerPos;
	playerToNpc.y = 0.f;
	float const sep = playerToNpc.magnitude();
	if (sep > 0.05f)
	{
		playerToNpc *= (1.0f / sep);
		Vector perp(-playerToNpc.z, 0.f, playerToNpc.x);
		float const pm = perp.magnitude();
		if (pm > 0.001f)
			outCameraPos = outCameraPos + perp * (0.62f / pm);
	}

	outLookAt = mid;
	outLookAt.y -= 0.2f;
	outCameraPos.y += 0.52f;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateMediumShot(Vector & outCameraPos, Vector & outLookAt) const
{
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!targetObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	Vector npcFraming = computeNpcDialogueFramingPosition();

	// Look slightly below dialogue framing (neck/chest on humans; mob framing is already torso-heavy).
	outLookAt = npcFraming;
	outLookAt.y -= 0.22f;

	Vector npcForward = targetObj->getObjectFrameK_w();
	npcForward.y = 0.f;
	if (npcForward.magnitude() < 0.001f)
	{
		Vector towardPlayer = computePlayerPosition() - outLookAt;
		towardPlayer.y = 0.f;
		float const tp = towardPlayer.magnitude();
		if (tp > 0.001f)
			npcForward = towardPlayer / tp;
		else
			npcForward = Vector::unitZ;
	}
	else
		npcForward.normalize();

	Vector npcRight = targetObj->getObjectFrameI_w();
	npcRight.y = 0.f;
	if (npcRight.magnitude() > 0.001f)
		npcRight.normalize();

	outCameraPos = outLookAt + npcForward * MEDIUM_SHOT_DISTANCE + npcRight * 0.5f;
	outCameraPos.y = outLookAt.y + 0.2f;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateOverShoulderShot(Vector & outCameraPos, Vector & outLookAt) const
{
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!player || !targetObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	// Look at NPC
	outLookAt = computeNpcDialogueFramingPosition();

	// Position camera behind and to the side of the player
	Vector playerPos = player->getPosition_w();
	Vector playerRight = player->getObjectFrameI_w();
	Vector playerUp = Vector::unitY;

	// Calculate direction from player to NPC
	Vector toNpc = outLookAt - playerPos;
	toNpc.y = 0.0f;
	toNpc.normalize();

	// Position camera over player's right shoulder
	outCameraPos = playerPos + playerRight * 0.6f + playerUp * 1.6f - toNpc * 0.5f;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateAxisDialogueShot(bool npcSpeaking, Vector & outCameraPos, Vector & outLookAt) const
{
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const npcObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!player || !npcObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	Vector const playerFraming = computePlayerReactionFramingLookAt_w();
	Vector const npcFraming = computeNpcDialogueFramingPosition();

	Vector pn = npcFraming - playerFraming;
	pn.y = 0.f;
	float const sep = pn.magnitude();
	if (sep < 0.08f)
	{
		if (npcSpeaking)
			calculateCloseUpShot(outCameraPos, outLookAt);
		else
		{
			outLookAt = playerFraming;
			Vector playerForward = player->getObjectFrameK_w();
			playerForward.y = 0.f;
			if (playerForward.magnitude() < 0.001f)
				playerForward = Vector::unitZ;
			else
				playerForward.normalize();
			float const camDist = computePlayerReactionCameraDistance(player);
			outCameraPos = outLookAt + playerForward * camDist;
			outCameraPos.y = outLookAt.y + 0.22f;
		}
		return;
	}
	pn *= 1.0f / sep;

	Vector perp(-pn.z, 0.f, pn.x);
	float const pm = perp.magnitude();
	if (pm > 0.001f)
		perp *= 1.0f / pm;
	else
		perp = Vector::unitX;

	Vector const mid = (playerFraming + npcFraming) * 0.5f;
	float const sideDist = std::max(2.0f, std::min(5.4f, sep * 0.48f + 1.48f));

	Vector const candPos[2] = { mid + perp * sideDist, mid - perp * sideDist };

	auto axisFacingScore = [&](Vector const & cam, Vector const & subject, Vector const & flatTowardOther) -> float
	{
		Vector vc = cam - subject;
		vc.y = 0.f;
		Vector vo = flatTowardOther;
		vo.y = 0.f;
		float const c1 = vc.magnitude();
		float const c2 = vo.magnitude();
		if (c1 < 0.06f || c2 < 0.06f)
			return -1.0e6f;
		vc *= 1.0f / c1;
		vo *= 1.0f / c2;
		return vc.dot(vo);
	};

	Vector const towardPlayerFlat = playerFraming - npcFraming;
	Vector const towardNpcFlat = npcFraming - playerFraming;

	if (npcSpeaking)
	{
		outLookAt = npcFraming;
		float const s0 = axisFacingScore(candPos[0], npcFraming, towardPlayerFlat);
		float const s1 = axisFacingScore(candPos[1], npcFraming, towardPlayerFlat);
		outCameraPos = s0 >= s1 ? candPos[0] : candPos[1];
	}
	else
	{
		outLookAt = playerFraming;
		float const s0 = axisFacingScore(candPos[0], playerFraming, towardNpcFlat);
		float const s1 = axisFacingScore(candPos[1], playerFraming, towardNpcFlat);
		outCameraPos = s0 >= s1 ? candPos[0] : candPos[1];
	}

	outCameraPos.y = mid.y + 0.42f;

	Vector pullTowardMid = mid - outCameraPos;
	pullTowardMid.y = 0.f;
	float const pullMag = pullTowardMid.magnitude();
	if (pullMag > 0.02f)
		outCameraPos = outCameraPos + pullTowardMid * (0.14f / pullMag);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::calculateTwoShot(Vector & outCameraPos, Vector & outLookAt) const
{
	CreatureObject * const player = Game::getPlayerCreature();
	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!player || !targetObj)
	{
		outCameraPos = m_currentCameraPos;
		outLookAt = m_currentCameraLookAt;
		return;
	}

	Vector playerPos = computePlayerPosition();
	Vector npcPos = computeNpcDialogueFramingPosition();

	// Look at the midpoint between player and NPC
	outLookAt = (playerPos + npcPos) * 0.5f;

	Vector playerToNpc = npcPos - playerPos;
	playerToNpc.y = 0.0f;
	float const separation = playerToNpc.normalize();

	Vector perpendicular(-playerToNpc.z, 0.0f, playerToNpc.x);
	float const perpMag = perpendicular.magnitude();
	if (perpMag > 0.001f)
		perpendicular /= perpMag;

	float const camDist = separation * 0.8f + TWO_SHOT_DISTANCE;

	Vector candA = outLookAt + perpendicular * camDist;
	Vector candB = outLookAt - perpendicular * camDist;
	candA.y = outLookAt.y + 0.3f;
	candB.y = outLookAt.y + 0.3f;

	// Pick the side of the line where both actors' facing hemispheres contain the camera (avoid backs of heads).
	Vector npcFwd = targetObj->getObjectFrameK_w();
	npcFwd.y = 0.f;
	if (npcFwd.magnitude() < 0.001f)
		npcFwd = playerToNpc;
	else
		npcFwd.normalize();

	Vector playerFwd = player->getObjectFrameK_w();
	playerFwd.y = 0.f;
	if (playerFwd.magnitude() < 0.001f)
		playerFwd = playerToNpc;
	else
		playerFwd.normalize();

	auto dualShotFaceScore = [&](Vector const & cam) -> float
	{
		Vector vn = cam - npcPos;
		vn.y = 0.f;
		Vector vp = cam - playerPos;
		vp.y = 0.f;
		float const dn = vn.magnitude();
		float const dp = vp.magnitude();
		if (dn < 0.05f || dp < 0.05f)
			return -1.0e6f;
		vn *= 1.0f / dn;
		vp *= 1.0f / dp;
		return std::min(vn.dot(npcFwd), vp.dot(playerFwd));
	};

	float const scoreA = dualShotFaceScore(candA);
	float const scoreB = dualShotFaceScore(candB);
	outCameraPos = scoreA >= scoreB ? candA : candB;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setCameraShot(CameraShotType shotType)
{
	setCameraShot(shotType, DEFAULT_CAMERA_TRANSITION_DURATION);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setCameraShot(CameraShotType shotType, float transitionDuration)
{
	m_currentShotType = shotType;

	Vector targetPos, targetLookAt;

	switch (shotType)
	{
	case CST_CloseUp:
		if (useWideOpenFaceEstablishingShot())
			calculateWideOpenFaceShot(targetPos, targetLookAt);
		else
			calculateCloseUpShot(targetPos, targetLookAt);
		break;
	case CST_MediumShot:
		calculateMediumShot(targetPos, targetLookAt);
		break;
	case CST_OverShoulder:
		calculateOverShoulderShot(targetPos, targetLookAt);
		break;
	case CST_TwoShot:
		calculateTwoShot(targetPos, targetLookAt);
		break;
	case CST_Reaction:
		calculatePlayerFaceCloseUpShot(targetPos, targetLookAt);
		break;
	}

	transitionCamera(targetPos, targetLookAt, transitionDuration);

	// Shot transition restarts this timer; hold duration is owned by dialogue state / scripted commands (no automatic cycling).
	m_timeSinceLastShotChange = 0.0f;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::transitionCamera(Vector const & targetPos, Vector const & targetLookAt, float duration)
{
	m_startCameraPos = m_currentCameraPos;
	m_startCameraLookAt = m_currentCameraLookAt;
	m_targetCameraPos = targetPos;
	m_targetCameraLookAt = targetLookAt;
	m_cameraTransitionTime = 0.0f;
	m_cameraTransitionDuration = std::max(duration, 0.001f);
	m_cameraTransitioning = true;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::update(float deltaTimeSecs)
{
	if (m_deferCloseUntilUpdate)
	{
		m_deferCloseUntilUpdate = false;
		// Runs after setTarget()/stop() returned. Use workspace close only — deactivateInWorkspace() calls
		// closeNextFrame(), which runs deactivate() synchronously AND schedules a second closeThroughWorkspace
		// next tick (MS_closeNextFrame), which still crashed on exit.
		if (isActive())
		{
			if (getContainingWorkspace())
				closeThroughWorkspace();
			else
				deactivate();
		}
		return;
	}

	CuiMediator::update(deltaTimeSecs);

	updateLetterbox(deltaTimeSecs);
	updateNpcMessageTypewriter(deltaTimeSecs);
	updateCameraFocus(deltaTimeSecs);
	updateScriptedLookAtCameraHint();
	updateGmCinematicBadge();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateLetterbox(float deltaTime)
{
	if (!m_letterboxAnimating)
		return;

	m_letterboxAnimationTime += deltaTime;
	float const tRaw = std::min(m_letterboxAnimationTime / LETTERBOX_ANIMATION_DURATION, 1.0f);

	bool const opening = m_letterboxTargetHeight > m_letterboxAnimationStartHeight + 0.5f;
	float const tPos = opening ? easeOutCubicLocal(tRaw) : easeInCubicLocal(tRaw);

	float const newHeight = m_letterboxAnimationStartHeight
		+ (m_letterboxTargetHeight - m_letterboxAnimationStartHeight) * tPos;

	float o = opening ? easeOutCubicLocal(std::min(1.f, tRaw * 1.2f)) : (1.0f - easeOutCubicLocal(tRaw));
	o = std::max(0.0f, std::min(1.0f, o));

	m_currentLetterboxHeight = newHeight;

	if (m_letterboxTop)
	{
		UISize size = m_letterboxTop->GetSize();
		size.y = static_cast<long>(newHeight);
		m_letterboxTop->SetSize(size);
		m_letterboxTop->SetOpacity(o);
		m_letterboxTop->SetBackgroundOpacity(o);
	}
	if (m_letterboxBottom)
	{
		UISize size = m_letterboxBottom->GetSize();
		size.y = static_cast<long>(newHeight);
		m_letterboxBottom->SetSize(size);
		m_letterboxBottom->SetOpacity(o);
		m_letterboxBottom->SetBackgroundOpacity(o);
	}

	if (m_letterboxAnimationTime >= LETTERBOX_ANIMATION_DURATION)
	{
		m_letterboxAnimating = false;
		m_currentLetterboxHeight = m_letterboxTargetHeight;
		if (m_letterboxTargetHeight <= 0.5f)
		{
			if (m_letterboxTop)
			{
				m_letterboxTop->SetOpacity(0.0f);
				m_letterboxTop->SetBackgroundOpacity(0.0f);
			}
			if (m_letterboxBottom)
			{
				m_letterboxBottom->SetOpacity(0.0f);
				m_letterboxBottom->SetBackgroundOpacity(0.0f);
			}
			if (m_hidePageAfterLetterboxOut)
			{
				m_hidePageAfterLetterboxOut = false;
				getPage().SetVisible(false);
				setIsUpdating(false);
				applyDeferredHudRestoreIfNeeded();
			}
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::applyDeferredHudRestoreIfNeeded()
{
	if (!m_deferHudRestoreUntilLetterboxOut)
		return;

	m_deferHudRestoreUntilLetterboxOut = false;

	if (Game::getHudSceneType() == Game::ST_ground)
	{
		SwgCuiHud * const hud = SwgCuiHudFactory::findMediatorForCurrentHud();
		if (hud)
			hud->setHudEnabled(m_savedHudEnabled);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateScriptedLookAtCameraHint()
{
	if (!m_scriptedLookAtCameraHint)
		return;

	bool const show = m_cameraControlActive && m_scriptedLookAtFramingActive;
	m_scriptedLookAtCameraHint->SetVisible(show);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateGmCinematicBadge()
{
	if (!m_gmCinematicBadge)
		return;

	bool const show = PlayerObject::isAdmin() && CuiConversationManager::isCinematicConversationUiActive();
	m_gmCinematicBadge->SetVisible(show);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::cacheDialogueUILayout()
{
	if (m_cachedUILayoutValid)
		return;

	if (m_dialoguePanel)
	{
		m_cachedDialoguePanelSize = m_dialoguePanel->GetSize();
		m_cachedDialoguePanelLocation = m_dialoguePanel->GetLocation();
	}
	if (m_responsePanel)
	{
		m_cachedResponsePanelSize = m_responsePanel->GetSize();
		m_cachedResponsePanelLocation = m_responsePanel->GetLocation();
	}
	if (m_endConversationButton)
		m_cachedEndButtonLocation = m_endConversationButton->GetLocation();

	m_cachedUILayoutValid = true;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::applyDialogueLayoutForResponseCount(size_t visibleRowsOnPage)
{
	cacheDialogueUILayout();
	if (!m_cachedUILayoutValid || !m_responsePanel)
		return;

	UISize viewportSize = m_cachedResponsePanelSize;
	m_responsePanel->SetSize(viewportSize);
	m_responsePanel->SetLocation(m_cachedResponsePanelLocation);

	long const rows = static_cast<long>(std::min(visibleRowsOnPage, static_cast<size_t>(RESPONSES_PER_PAGE)));
	long const contentHeight = rows * RESPONSE_ROW_STRIDE_PX;
	UISize scrollExtent = viewportSize;
	scrollExtent.y = std::max(viewportSize.y, contentHeight);
	m_responsePanel->SetScrollExtent(scrollExtent);

	m_responsePanel->SetScrollLocation(UIPoint(0L, 0L));

	updateResponseScrollbarVisibility();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateResponseAreaVisibility(bool visible)
{
	if (!m_responsePanel)
		return;
	if (UIWidget * const parent = dynamic_cast<UIWidget *>(m_responsePanel->GetParent()))
		parent->SetVisible(visible);
	if (m_responseScrollbar && !visible)
		m_responseScrollbar->SetVisible(false);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::restoreDialogueUILayout()
{
	if (!m_cachedUILayoutValid)
		return;

	if (m_responsePanel)
	{
		m_responsePanel->SetSize(m_cachedResponsePanelSize);
		m_responsePanel->SetScrollExtent(m_cachedResponsePanelSize);
		m_responsePanel->SetLocation(m_cachedResponsePanelLocation);
	}

	if (m_dialoguePanel)
	{
		m_dialoguePanel->SetSize(m_cachedDialoguePanelSize);
		m_dialoguePanel->SetScrollExtent(m_cachedDialoguePanelSize);
		m_dialoguePanel->SetLocation(m_cachedDialoguePanelLocation);
	}

	if (m_endConversationButton)
		m_endConversationButton->SetLocation(m_cachedEndButtonLocation);

	updateResponseScrollbarVisibility();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateResponseScrollbarVisibility()
{
	if (!m_responseScrollbar)
		return;
	// Pagination replaces vertical scrolling of the response list.
	m_responseScrollbar->SetVisible(false);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::ensureResponseSlotCount(size_t needed)
{
	if (!m_responsePanel || !m_responseClonePrototype)
		return;

	size_t const capped = std::min(needed, static_cast<size_t>(MAX_RESPONSE_SLOTS));
	while (m_responseSlots.size() < capped)
	{
		size_t const idx = m_responseSlots.size();

		UIPage * const row = dynamic_cast<UIPage *>(m_responseClonePrototype->DuplicateObject());
		if (!row)
			break;

		char nameBuf[64];
		snprintf(nameBuf, sizeof(nameBuf), "responseDyn%u", static_cast<unsigned>(idx + 1));
		row->SetName(nameBuf);

		long const y = static_cast<long>(idx) * RESPONSE_ROW_STRIDE_PX;
		row->SetLocation(UIPoint(0L, y));

		m_responsePanel->AddChild(row);

		ResponseSlot slot;
		slot.page = row;
		slot.button = dynamic_cast<UIButton *>(row->GetChild("button"));
		slot.prefixText = dynamic_cast<UIText *>(row->GetChild("prefix"));
		slot.text = dynamic_cast<UIText *>(row->GetChild("text"));
		slot.ownedDuplicate = true;
		if (slot.prefixText)
			slot.prefixText->SetPreLocalized(true);
		if (slot.text)
			slot.text->SetPreLocalized(true);
		if (slot.button)
			registerMediatorObject(*slot.button, true);
		m_responseSlots.push_back(slot);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::releaseDynamicResponseSlots()
{
	while (m_responseSlots.size() > static_cast<size_t>(BASE_RESPONSE_SLOTS))
	{
		ResponseSlot & s = m_responseSlots.back();
		if (s.button)
			unregisterMediatorObject(*s.button);
		if (s.page && s.ownedDuplicate && m_responsePanel)
		{
			m_responsePanel->RemoveChild(s.page);
			delete s.page;
		}
		m_responseSlots.pop_back();
	}
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::isNpcLinePrintingLocked() const
{
	if (!m_cameraControlActive)
		return false;
	if (m_typewriterFullText.empty())
		return false;
	if (m_typewriterRevealLength < m_typewriterFullText.length())
		return true;
	if (m_typewriterPauseRemaining > 0.001f)
		return true;
	return false;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateResponseAvailabilityForTypewriter()
{
	bool const allow = !isNpcLinePrintingLocked();
	size_t const visibleOnPage =
		(m_currentResponses.size() <= m_responsePageStart)
			? 0u
			: std::min(static_cast<size_t>(RESPONSES_PER_PAGE), m_currentResponses.size() - m_responsePageStart);
	for (size_t i = 0; i < m_responseSlots.size(); ++i)
	{
		ResponseSlot & slot = m_responseSlots[i];
		if (!slot.button)
			continue;
		bool const slotInUse =
			i < static_cast<size_t>(RESPONSES_PER_PAGE) && i < visibleOnPage && slot.page;
		bool const enable = allow && slotInUse;
		slot.button->SetEnabled(enable);
		// Restore hit-test / paint order after toggling Enabled (UIButton should stay above sibling UIText rows).
		if (enable && slot.page)
			slot.page->MoveChild(slot.button, UIBaseObject::Top);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::finishNpcMessageTypewriter()
{
	if (!m_npcMessageText)
		return;

	m_typewriterPauseRemaining = 0.0f;
	m_typewriterRevealLength = m_typewriterFullText.length();
	m_typewriterCharAccumulator = 0.0f;
	m_typewriterActive = false;
	m_npcMessageText->SetLocalText(m_typewriterFullText);
	updateResponseAvailabilityForTypewriter();
	maybeStartReactionPostLineBuffer();
	tryApplyDeferredNpcSubtitle();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::maybeStartReactionPostLineBuffer()
{
	if (!m_playerReactionBeatPending)
		return;
	if (m_reactionPostLineBufferRemaining >= 0.f)
		return;
	if (!m_deferIncomingNpcSubtitle)
		return;
	if (isNpcLinePrintingLocked())
		return;
	m_reactionPostLineBufferRemaining = REACTION_POST_TYPEWRITER_BUFFER_SEC;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::flushPlayerReactionBeat()
{
	if (!m_playerReactionBeatPending)
		return;

	m_playerReactionBeatPending = false;
	m_playerReactionHoldActive = false;
	m_reactionPostLineBufferRemaining = -1.f;

	m_deferIncomingNpcSubtitle = false;

	Unicode::String npcLine = m_deferredNpcSubtitle;
	m_deferredNpcSubtitle.clear();

	if (npcLine.empty())
		npcLine = CuiConversationManager::getLastMessage();

	if (!npcLine.empty())
		setNpcMessage(npcLine);

	if (m_haveDeferredBranchResponses && !m_deferredBranchResponses.empty())
		setResponses(m_deferredBranchResponses);
	else
	{
		CuiConversationManager::StringVector responses;
		CuiConversationManager::getResponses(responses);
		setResponses(responses);
	}
	m_haveDeferredBranchResponses = false;
	m_deferredBranchResponses.clear();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::tryApplyDeferredNpcSubtitle()
{
	if (m_playerReactionBeatPending)
		return;
	if (!m_deferIncomingNpcSubtitle)
		return;
	if (isNpcLinePrintingLocked())
		return;
	if (m_deferredNpcSubtitle.empty())
	{
		m_deferIncomingNpcSubtitle = false;
		return;
	}

	Unicode::String const next = m_deferredNpcSubtitle;
	m_deferredNpcSubtitle.clear();
	m_deferIncomingNpcSubtitle = false;
	setNpcMessage(next);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::applyTypewriterPauseAfterReveal(size_t newRevealLen)
{
	Unicode::String const & t = m_typewriterFullText;
	static Unicode::String const sixDots = Unicode::narrowToWide("......");
	static Unicode::String const threeDots = Unicode::narrowToWide("...");
	static Unicode::String const commaSpace = Unicode::narrowToWide(", ");

	if (newRevealLen >= 6 && t.compare(newRevealLen - 6, 6, sixDots) == 0)
	{
		m_typewriterPauseRemaining += TYPEWRITER_PAUSE_AFTER_SIX_DOTS;
		return;
	}
	if (newRevealLen >= 2 && t.compare(newRevealLen - 2, 2, commaSpace) == 0)
	{
		m_typewriterPauseRemaining += TYPEWRITER_PAUSE_AFTER_COMMA_SPACE;
		return;
	}
	if (newRevealLen >= 3 && t.compare(newRevealLen - 3, 3, threeDots) == 0)
	{
		if (newRevealLen < t.length() && t[newRevealLen] == static_cast<Unicode::unicode_char_t>('.'))
			return;
		m_typewriterPauseRemaining += TYPEWRITER_PAUSE_AFTER_ELLIPSIS;
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::beginNpcMessageTypewriter(Unicode::String const & message)
{
	if (message.empty())
	{
		m_typewriterFullText.clear();
		m_typewriterRevealLength = 0;
		m_typewriterCharAccumulator = 0.0f;
		m_typewriterPauseRemaining = 0.0f;
		m_typewriterActive = false;
		if (m_npcMessageText)
			m_npcMessageText->SetLocalText(Unicode::emptyString);
		updateResponseAvailabilityForTypewriter();
		return;
	}

	m_typewriterFullText = message;
	m_typewriterRevealLength = 0;
	m_typewriterCharAccumulator = 0.0f;
	m_typewriterPauseRemaining = 0.0f;
	m_typewriterActive = true;

	float const len = static_cast<float>(message.length());
	m_typewriterCharsPerSecond =
		std::max(TYPEWRITER_CHARS_PER_SECOND_MIN,
			std::min(TYPEWRITER_CHARS_PER_SECOND_MAX, TYPEWRITER_CHARS_PER_SECOND_BASE + len * 0.018f));

	if (m_npcMessageText)
		m_npcMessageText->SetLocalText(Unicode::emptyString);

	updateResponseAvailabilityForTypewriter();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateNpcMessageTypewriter(float deltaTimeSecs)
{
	if (!m_npcMessageText)
		return;

	if (m_typewriterPauseRemaining > 0.0f)
	{
		m_typewriterPauseRemaining -= deltaTimeSecs;
		if (m_typewriterPauseRemaining < 0.0f)
			m_typewriterPauseRemaining = 0.0f;

		if (m_typewriterPauseRemaining <= 0.0f && m_typewriterRevealLength >= m_typewriterFullText.length())
		{
			m_typewriterActive = false;
			m_npcMessageText->SetLocalText(m_typewriterFullText);
		}
		updateResponseAvailabilityForTypewriter();
		if (m_typewriterPauseRemaining > 0.0f)
			return;
	}

	size_t const totalLen = m_typewriterFullText.length();

	if (!m_typewriterActive && m_typewriterRevealLength >= totalLen)
	{
		maybeStartReactionPostLineBuffer();
		tryApplyDeferredNpcSubtitle();
		return;
	}

	if (!m_typewriterActive)
		return;

	if (m_typewriterRevealLength >= totalLen)
	{
		m_typewriterActive = false;
		m_npcMessageText->SetLocalText(m_typewriterFullText);
		updateResponseAvailabilityForTypewriter();
		maybeStartReactionPostLineBuffer();
		tryApplyDeferredNpcSubtitle();
		return;
	}

	m_typewriterCharAccumulator += deltaTimeSecs * m_typewriterCharsPerSecond;
	while (m_typewriterCharAccumulator >= 1.0f && m_typewriterRevealLength < totalLen && m_typewriterPauseRemaining <= 0.0f)
	{
		m_typewriterCharAccumulator -= 1.0f;
		++m_typewriterRevealLength;
		m_npcMessageText->SetLocalText(m_typewriterFullText.substr(0, m_typewriterRevealLength));
		applyTypewriterPauseAfterReveal(m_typewriterRevealLength);
		if (m_typewriterPauseRemaining > 0.0f)
			break;
	}

	if (m_typewriterRevealLength >= totalLen && m_typewriterPauseRemaining <= 0.0f)
	{
		m_typewriterActive = false;
		m_npcMessageText->SetLocalText(m_typewriterFullText);
	}
	updateResponseAvailabilityForTypewriter();
	maybeStartReactionPostLineBuffer();
	tryApplyDeferredNpcSubtitle();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateCameraFocus(float deltaTime)
{
	if (!m_cameraControlActive)
		return;

	GroundScene * const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
	if (!groundScene)
		return;

	// Update camera transition
	if (m_cameraTransitioning)
	{
		m_cameraTransitionTime += deltaTime;
		float t = std::min(m_cameraTransitionTime / m_cameraTransitionDuration, 1.0f);
		t = easeInOutCubic(t);

		// Interpolate camera position and look-at
		m_currentCameraPos = m_startCameraPos + (m_targetCameraPos - m_startCameraPos) * t;
		m_currentCameraLookAt = m_startCameraLookAt + (m_targetCameraLookAt - m_startCameraLookAt) * t;

		if (m_cameraTransitionTime >= m_cameraTransitionDuration)
		{
			m_cameraTransitioning = false;
			m_currentCameraPos = m_targetCameraPos;
			m_currentCameraLookAt = m_targetCameraLookAt;
		}
	}

	// Apply framing through god FreeCamera - FreeChaseCamera would overwrite raw GameCamera transforms.
	FreeCamera * const freeCamera = groundScene->getGodClientCamera();
	if (freeCamera)
	{
		// Scripted look-at (vendor shelf): allow FreeCamera pivot orbit/zoom — do not overwrite each frame.
		if (m_scriptedLookAtFramingActive && !m_cameraTransitioning)
			syncScriptedOrbitCameraFromFreeCamera(freeCamera);
		else
			applyShotToFreeCamera(freeCamera, m_targetNpcId, m_currentCameraPos, m_currentCameraLookAt);
	}

	// Player reaction: stay on CST_Reaction until typewriter + REACTION_POST_TYPEWRITER_BUFFER_SEC elapses,
	// then flushPlayerReactionBeat() applies NPC subtitle + new branch (see selectResponse / onResponsesChanged).
	if (m_playerReactionHoldActive && m_playerReactionBeatPending && m_reactionPostLineBufferRemaining >= 0.0f)
	{
		m_reactionPostLineBufferRemaining -= deltaTime;
		if (m_reactionPostLineBufferRemaining <= 0.0f && !m_cameraTransitioning)
		{
			m_reactionPostLineBufferRemaining = 0.0f;
			flushPlayerReactionBeat();
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::syncScriptedOrbitCameraFromFreeCamera(FreeCamera * const freeCamera)
{
	if (!freeCamera)
		return;

	Vector const pivot = freeCamera->getPivotPoint();
	Vector cam = freeCamera->getPosition_w();
	applyShotToFreeCamera(freeCamera, m_targetNpcId, cam, pivot);

	m_currentCameraPos = freeCamera->getPosition_w();
	m_currentCameraLookAt = freeCamera->getPivotPoint();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::handleCameraCommand(MessageQueueNpcConversationCameraCommand const * cmd)
{
	if (!cmd || !ms_enabled)
		return;

	// Do not rely on CuiMediatorFactory::getInWorkspace - the cinematic mediator may not appear in
	// the workspace enumerator while still active; the handler is only registered from performActivate.
	if (ms_cameraCommandTarget)
		ms_cameraCommandTarget->applyCameraCommand(cmd);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::applyCameraCommand(MessageQueueNpcConversationCameraCommand const * cmd)
{
	if (!cmd)
		return;

	if (!m_cameraControlActive)
		initializeCameraControl();

	if (!m_cameraControlActive)
		return;

	// Scripted beats override automatic dialogue camera sequencing.
	m_playerReactionHoldActive = false;
	m_playerReactionBeatPending = false;
	m_reactionPostLineBufferRemaining = -1.f;

	float const duration = cmd->getTransitionDuration();
	if (duration > 0.0f)
	{
		m_cameraTransitionDuration = duration;
	}

	float const holdTime = cmd->getHoldTime();
	if (holdTime > 0.0f)
	{
		m_shotHoldTime = holdTime;
		m_timeSinceLastShotChange = 0.0f;
	}
	else if (holdTime == 0.0f)
	{
		// Zero means disable auto-shot-change for this shot
		m_shotHoldTime = 0.0f;
	}

	switch (cmd->getCommandType())
	{
	case MessageQueueNpcConversationCameraCommand::CT_LookAtTarget:
		{
			NetworkId const & targetId = cmd->getTargetId();
			if (targetId.isValid())
			{
				Object * const targetObj = NetworkIdManager::getObjectById(targetId);
				if (targetObj)
				{
					Vector lookAt = computeScriptedLookAtPoint(*targetObj);
					float objR = 0.65f;
					if (CollisionProperty const * const cp = targetObj->getCollisionProperty())
						objR = std::max(objR, cp->getBoundingSphere_w().getRadius());
					lookAt.y += std::min(0.42f, std::max(0.06f, objR * 0.14f));

					Vector targetPos = m_currentCameraPos;

					Vector speakerFlat = Vector::zero;
					if (m_targetNpcId.isValid())
					{
						Object * const npcObj = NetworkIdManager::getObjectById(m_targetNpcId);
						if (npcObj)
						{
							Vector sp = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*npcObj);
							sp.y += HEAD_HEIGHT_OFFSET;
							Vector spw = npcObj->rotateTranslate_o2w(sp);
							spw.y += 0.05f;
							speakerFlat = spw;
							speakerFlat.y = 0.f;
						}
					}
					Vector objFlat = lookAt;
					objFlat.y = 0.f;

					Vector toObj = objFlat - speakerFlat;
					float const horiz = toObj.magnitude();
					if (horiz > 0.25f)
						toObj *= 1.0f / horiz;
					else
					{
						Vector fallback = m_currentCameraPos - lookAt;
						fallback.y = 0.f;
						float const hf = fallback.magnitude();
						if (hf > 0.15f)
							toObj = fallback * (1.0f / hf);
						else
							toObj = Vector::unitZ;
					}

					Vector camFlat = -toObj;
					Vector lateral(-camFlat.z, 0.f, camFlat.x);
					if (lateral.magnitude() > 0.05f)
						lateral.normalize();
					else
						lateral = Vector::unitX;

					float const dist = std::max(2.2f, std::min(13.5f, 1.35f * objR + 3.15f));
					Vector camDir = camFlat * 0.82f + lateral * 0.28f + Vector::unitY * 0.14f;
					float const cd = camDir.magnitude();
					if (cd > 0.001f)
						camDir *= 1.0f / cd;

					targetPos = lookAt + camDir * dist;

					Vector blendFrom = m_currentCameraPos - lookAt;
					float const blendMag = blendFrom.magnitude();
					if (blendMag > 0.35f)
					{
						blendFrom *= 1.0f / blendMag;
						Vector ortho = lateral * 0.35f + Vector::unitY * 0.08f;
						Vector blended = blendFrom * 0.34f + camDir * 0.66f + ortho;
						float bm = blended.magnitude();
						if (bm > 0.001f)
							blended *= 1.0f / bm;
						targetPos = lookAt + blended * dist;
					}

					resolveDesiredCameraAgainstCollision(m_targetNpcId, lookAt, targetPos);

					float transitionSecs = duration;
					if (transitionSecs <= 0.001f)
						transitionSecs = SCRIPTED_LOOKAT_DEFAULT_DURATION;

					transitionCamera(targetPos, lookAt, transitionSecs);
					m_scriptedLookAtFramingActive = true;
				}
			}
		}
		break;

	case MessageQueueNpcConversationCameraCommand::CT_LookAtPosition:
		{
			Vector lookAt(cmd->getPositionX(), cmd->getPositionY(), cmd->getPositionZ());
			Vector targetPos = m_currentCameraPos;
			transitionCamera(targetPos, lookAt, duration);
		}
		break;

	case MessageQueueNpcConversationCameraCommand::CT_ReturnToSpeaker:
		m_scriptedLookAtFramingActive = false;
		setCameraShot(CST_CloseUp, duration > 0.0f ? duration : DEFAULT_CAMERA_TRANSITION_DURATION);
		break;

	case MessageQueueNpcConversationCameraCommand::CT_None:
	default:
		break;
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateSpeakerTitleText()
{
	if (!m_npcNameText)
		return;

	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	if (!targetObj || !m_targetNpcId.isValid())
	{
		m_npcNameText->SetLocalText(Unicode::emptyString);
		return;
	}

	ClientObject * const clientObj = targetObj->asClientObject();
	CreatureObject * const creature = clientObj ? clientObj->asCreatureObject() : nullptr;

	Unicode::String name;
	if (creature)
		name = creature->getLocalizedName();
	else if (clientObj)
		name = clientObj->getLocalizedName();
	else if (targetObj->getObjectTemplateName())
		name = Unicode::narrowToWide(targetObj->getObjectTemplateName());

	if (name.empty())
		name = Unicode::narrowToWide("Unknown");

	Unicode::String label = Unicode::narrowToWide("Conversing with ");
	label += name;
	m_npcNameText->SetLocalText(label);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setupNpcViewer()
{
	updateSpeakerTitleText();

	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	ClientObject * const clientObj = targetObj ? targetObj->asClientObject() : nullptr;
	CreatureObject * const creature = clientObj ? clientObj->asCreatureObject() : nullptr;

	if (!m_npcViewer)
		return;

	m_npcViewer->clearObjects();

	if (creature)
	{
		m_npcViewer->addObject(*creature);
		m_npcViewer->setViewDirty(true);
		m_npcViewer->setCameraLookAtCenter(true);
		m_npcViewer->recomputeZoom();
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setNpcMessage(Unicode::String const & message)
{
	if (m_cameraControlActive && m_deferIncomingNpcSubtitle && isNpcLinePrintingLocked())
	{
		m_deferredNpcSubtitle = message;
		return;
	}

	// Duplicate refreshes (e.g. clearing responses) keep the same text - do not restart typewriter / camera beat.
	if (m_cameraControlActive && message == m_lastNpcMessageForCamera)
		return;

	m_deferIncomingNpcSubtitle = false;
	m_deferredNpcSubtitle.clear();

	if (m_npcMessageText)
	{
		if (m_cameraControlActive)
			beginNpcMessageTypewriter(message);
		else
		{
			m_npcMessageText->SetLocalText(message);
			updateResponseAvailabilityForTypewriter();
		}
	}

	if (!m_cameraControlActive)
		return;

	m_lastNpcMessageForCamera = message;
	m_playerReactionHoldActive = false;
	m_timeSinceLastShotChange = 0.0f;
	m_shotHoldTime = 0.0f;

	// Scripted look-at (vendor shelf prop, etc.): keep framing until CT_ReturnToSpeaker.
	if (m_scriptedLookAtFramingActive)
		return;

	// New NPC line: axis shot toward NPC (snap); holds while subtitle prints (typewriter).
	setCameraShot(CST_CloseUp, FACE_DIALOGUE_TRANSITION_SEC);
}

//----------------------------------------------------------------------

SwgCuiCinematicConversation::ResponseData SwgCuiCinematicConversation::parseResponse(
	Unicode::String const & response, int index)
{
	ResponseData data;
	data.prefix = RP_None;
	data.responseIndex = index;
	data.responseText = response;

	// Convert to narrow string for prefix checking
	std::string narrowResponse = Unicode::wideToNarrow(response);

	// Check for prefix tags
	for (int i = 0; s_prefixMappings[i].tag != nullptr; ++i)
	{
		char const * tag = s_prefixMappings[i].tag;
		size_t tagLen = strlen(tag);

		if (narrowResponse.length() >= tagLen &&
			narrowResponse.compare(0, tagLen, tag) == 0)
		{
			data.prefix = s_prefixMappings[i].prefix;
			data.prefixText = Unicode::narrowToWide(tag);

			// Remove prefix from response text, trim leading space
			std::string remaining = narrowResponse.substr(tagLen);
			while (!remaining.empty() && remaining[0] == ' ')
			{
				remaining = remaining.substr(1);
			}
			data.responseText = Unicode::narrowToWide(remaining);
			break;
		}
	}

	// Check for custom prefix [Custom:text]
	if (data.prefix == RP_None && narrowResponse.length() > 2 && narrowResponse[0] == '[')
	{
		size_t closePos = narrowResponse.find(']');
		if (closePos != std::string::npos)
		{
			data.prefix = RP_Custom;
			data.prefixText = Unicode::narrowToWide(narrowResponse.substr(0, closePos + 1));

			std::string remaining = narrowResponse.substr(closePos + 1);
			while (!remaining.empty() && remaining[0] == ' ')
			{
				remaining = remaining.substr(1);
			}
			data.responseText = Unicode::narrowToWide(remaining);
		}
	}

	return data;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::clampResponsePageStart()
{
	size_t const total = m_currentResponses.size();
	if (total == 0u)
	{
		m_responsePageStart = 0u;
		return;
	}
	size_t const pageSize = static_cast<size_t>(RESPONSES_PER_PAGE);
	size_t const maxStart = ((total - 1u) / pageSize) * pageSize;
	if (m_responsePageStart > maxStart)
		m_responsePageStart = maxStart;
	m_responsePageStart = (m_responsePageStart / pageSize) * pageSize;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateResponsePageControls()
{
	clampResponsePageStart();
	size_t const total = m_currentResponses.size();
	size_t const totalPages =
		(total == 0u)
			? 1u
			: (total + static_cast<size_t>(RESPONSES_PER_PAGE) - 1u) / static_cast<size_t>(RESPONSES_PER_PAGE);
	size_t const curPage =
		(total == 0u) ? 0u : m_responsePageStart / static_cast<size_t>(RESPONSES_PER_PAGE);
	bool const showNav = totalPages > 1u;

	if (m_responsePagePrevButton)
	{
		m_responsePagePrevButton->SetVisible(showNav);
		m_responsePagePrevButton->SetEnabled(curPage > 0u);
	}
	if (m_responsePageNextButton)
	{
		m_responsePageNextButton->SetVisible(showNav);
		m_responsePageNextButton->SetEnabled(curPage + 1u < totalPages);
	}
	if (m_responsePageLabel)
	{
		if (total > 0u)
		{
			m_responsePageLabel->SetVisible(true);
			char buf[64];
			snprintf(buf, sizeof(buf), "Page %u / %u",
				static_cast<unsigned>(curPage + 1u),
				static_cast<unsigned>(totalPages));
			m_responsePageLabel->SetLocalText(Unicode::narrowToWide(buf));
		}
		else
			m_responsePageLabel->SetVisible(false);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::changeResponsePage(int deltaPages)
{
	if (m_currentResponses.empty() || deltaPages == 0)
		return;

	size_t const totalPages =
		(m_currentResponses.size() + static_cast<size_t>(RESPONSES_PER_PAGE) - 1u)
		/ static_cast<size_t>(RESPONSES_PER_PAGE);
	size_t curPage = m_responsePageStart / static_cast<size_t>(RESPONSES_PER_PAGE);
	int np = static_cast<int>(curPage) + deltaPages;
	if (np < 0)
		np = 0;
	if (static_cast<size_t>(np) >= totalPages)
		np = static_cast<int>(totalPages > 0u ? totalPages - 1u : 0u);
	m_responsePageStart = static_cast<size_t>(np) * static_cast<size_t>(RESPONSES_PER_PAGE);
	refreshVisibleResponseRows();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::refreshVisibleResponseRows()
{
	clampResponsePageStart();
	size_t const total = m_currentResponses.size();

	size_t const pageEnd = std::min(total, m_responsePageStart + static_cast<size_t>(RESPONSES_PER_PAGE));
	size_t const visibleCount = (total == 0u) ? 0u : (pageEnd - m_responsePageStart);

	applyDialogueLayoutForResponseCount(visibleCount);

	for (size_t i = 0; i < m_responseSlots.size(); ++i)
	{
		if (!m_responseSlots[i].page)
			continue;

		if (i >= static_cast<size_t>(RESPONSES_PER_PAGE))
		{
			m_responseSlots[i].page->SetVisible(false);
			continue;
		}

		size_t const globalIdx = m_responsePageStart + i;
		if (globalIdx < total)
		{
			ResponseData const & data = m_currentResponses[globalIdx];

			m_responseSlots[i].page->SetVisible(true);

			if (m_responseSlots[i].text)
				m_responseSlots[i].text->SetVisible(false);

			bool const hasStyledPrefix = data.prefix != RP_None && !data.prefixText.empty();
			if (m_responseSlots[i].prefixText)
			{
				if (hasStyledPrefix)
				{
					m_responseSlots[i].prefixText->SetLocalText(data.prefixText);
					m_responseSlots[i].prefixText->SetVisible(true);
				}
				else
					m_responseSlots[i].prefixText->SetVisible(false);
			}

			if (m_responseSlots[i].button)
			{
				char slotBuf[16];
				snprintf(slotBuf, sizeof(slotBuf), "%u. ", static_cast<unsigned>(i + 1));
				Unicode::String labeled = Unicode::narrowToWide(slotBuf);
				labeled += data.responseText;
				m_responseSlots[i].button->SetText(labeled);
				m_responseSlots[i].page->MoveChild(m_responseSlots[i].button, UIBaseObject::Top);
			}
		}
		else
		{
			m_responseSlots[i].page->SetVisible(false);
		}
	}

	updateResponsePageControls();
	updateResponseAreaVisibility(!m_currentResponses.empty());
	updateResponseAvailabilityForTypewriter();
}

//----------------------------------------------------------------------

bool SwgCuiCinematicConversation::trySelectResponseByNumberKey(unsigned short keystroke)
{
	if (isNpcLinePrintingLocked())
		return false;

	size_t const total = m_currentResponses.size();
	if (total == 0u)
		return false;

	size_t const pageEnd = std::min(total, m_responsePageStart + static_cast<size_t>(RESPONSES_PER_PAGE));
	size_t const visible = pageEnd - m_responsePageStart;
	if (visible == 0u)
		return false;

	int slot = -1;
	if (keystroke >= static_cast<unsigned short>('1') && keystroke <= static_cast<unsigned short>('9'))
		slot = static_cast<int>(keystroke - static_cast<unsigned short>('1'));
	else if (keystroke == static_cast<unsigned short>('0'))
		slot = 9;
	else
		return false;

	if (slot < 0 || static_cast<size_t>(slot) >= visible)
		return false;

	size_t const globalIdx = m_responsePageStart + static_cast<size_t>(slot);
	if (globalIdx >= total)
		return false;

	selectResponse(m_currentResponses[globalIdx].responseIndex);
	return true;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setResponses(std::vector<Unicode::String> const & responses)
{
	m_currentResponses.clear();
	m_responsePageStart = 0;

	int index = 0;
	for (std::vector<Unicode::String>::const_iterator it = responses.begin(); it != responses.end(); ++it, ++index)
	{
		ResponseData data = parseResponse(*it, index);
		m_currentResponses.push_back(data);
	}

	ensureResponseSlotCount(static_cast<size_t>(RESPONSES_PER_PAGE));

	refreshVisibleResponseRows();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::clearResponses()
{
	m_currentResponses.clear();
	m_responsePageStart = 0;

	for (size_t i = 0; i < m_responseSlots.size(); ++i)
	{
		if (m_responseSlots[i].page)
			m_responseSlots[i].page->SetVisible(false);
	}

	applyDialogueLayoutForResponseCount(0);
	updateResponsePageControls();
	updateResponseAreaVisibility(false);
	updateResponseAvailabilityForTypewriter();
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::OnButtonPressed(UIWidget * context)
{
	// Check end conversation button - match Escape / IoWin: always tear down cinematic UI and restore HUD.
	if (context == m_endConversationButton)
	{
		CuiConversationManager::closeCinematicConversationFromInput();
		return;
	}

	if (context == m_responsePagePrevButton)
	{
		changeResponsePage(-1);
		return;
	}
	if (context == m_responsePageNextButton)
	{
		changeResponsePage(1);
		return;
	}

	if (isNpcLinePrintingLocked())
		return;

	for (size_t i = 0; i < m_responseSlots.size() && i < static_cast<size_t>(RESPONSES_PER_PAGE); ++i)
	{
		if (context == m_responseSlots[i].button)
		{
			size_t const globalIdx = m_responsePageStart + i;
			if (globalIdx < m_currentResponses.size())
				selectResponse(m_currentResponses[globalIdx].responseIndex);
			return;
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::selectResponse(int responseIndex)
{
	if (isNpcLinePrintingLocked())
		return;

	updateResponseAreaVisibility(false);

	Unicode::String reactionCaption;
	for (size_t i = 0; i < m_currentResponses.size(); ++i)
	{
		ResponseData const & d = m_currentResponses[i];
		if (d.responseIndex != responseIndex)
			continue;
		if (d.prefix != RP_None && !d.prefixText.empty())
			reactionCaption = d.prefixText + Unicode::narrowToWide(" ") + d.responseText;
		else
			reactionCaption = d.responseText;
		break;
	}

	if (m_cameraControlActive && !m_cameraTransitioning)
	{
		m_playerReactionHoldActive = true;
		m_playerReactionBeatPending = true;
		m_haveDeferredBranchResponses = false;
		m_deferredBranchResponses.clear();
		setCameraShot(CST_Reaction, FACE_DIALOGUE_TRANSITION_SEC);
		m_shotHoldTime = 0.0f;
		m_timeSinceLastShotChange = 0.0f;
		// Buffer countdown starts after player line finishes (maybeStartReactionPostLineBuffer), unless no caption.
		m_reactionPostLineBufferRemaining = -1.f;
		if (reactionCaption.empty())
			m_reactionPostLineBufferRemaining = REACTION_POST_TYPEWRITER_BUFFER_SEC;
	}

	if (!reactionCaption.empty() && m_npcMessageText && m_cameraControlActive)
	{
		m_deferIncomingNpcSubtitle = true;
		m_lastNpcMessageForCamera.clear();
		beginNpcMessageTypewriter(reactionCaption);
	}

	CuiConversationManager::respond(CuiConversationManager::getTarget(), responseIndex);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onTargetChanged(bool const &)
{
	CachedNetworkId const & targetId = CuiConversationManager::getTarget();

	if (!targetId.isValid())
	{
		// Conversation ended — never close synchronously from this emit (stop()/setTarget still unwinding).
		m_targetNpcId = NetworkId::cms_invalid;
		if (m_npcNameText)
			m_npcNameText->SetLocalText(Unicode::emptyString);
		if (isActive())
			m_deferCloseUntilUpdate = true;
		return;
	}

	if (targetId != m_targetNpcId)
	{
		m_targetNpcId = targetId;
		m_scriptedLookAtFramingActive = false;
		m_lastNpcMessageForCamera.clear();
		setupNpcViewer();

		// Re-initialize camera for new target (head close-up)
		if (m_cameraControlActive)
		{
			setCameraShot(CST_CloseUp, FACE_DIALOGUE_TRANSITION_SEC);
			m_shotHoldTime = 0.0f;
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onResponsesChanged(bool const &)
{
	CuiConversationManager::StringVector responses;
	CuiConversationManager::getResponses(responses);

	if (m_playerReactionBeatPending)
	{
		// respond() clears ms_responses and emits before the server replies; do not stash that empty snapshot
		// as the post-reaction branch — flushPlayerReactionBeat() would apply a blank menu / wrong indices.
		if (!responses.empty())
		{
			m_deferredBranchResponses = responses;
			m_haveDeferredBranchResponses = true;
			m_deferredNpcSubtitle = CuiConversationManager::getLastMessage();
		}
		return;
	}

	setResponses(responses);
	setNpcMessage(CuiConversationManager::getLastMessage());
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onConversationEnded(bool const &)
{
	// Do not close here — TargetChanged already queued deferred closeThroughWorkspace via m_deferCloseUntilUpdate.
}

//======================================================================

