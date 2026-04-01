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
#include "clientGame/FreeChaseCamera.h"
#include "clientObject/GameCamera.h"
#include "clientUserInterface/CuiConversationManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "swgClientUserInterface/SwgCuiHud.h"
#include "swgClientUserInterface/SwgCuiHudFactory.h"
#include "clientUserInterface/CuiObjectTextManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedNetworkMessages/MessageQueueNpcConversationCameraCommand.h"
#include "sharedCollision/CollisionProperty.h"
#include "sharedFoundation/Clock.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/NetworkIdManager.h"

#include "swgClientUserInterface/SwgCuiMediatorTypes.h"
#include "UIBaseObject.h"
#include "UIButton.h"
#include "UIData.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"

#include <cmath>

//======================================================================

namespace SwgCuiCinematicConversationNamespace
{
	// Animation constants
	float const LETTERBOX_HEIGHT = 80.0f;
	float const LETTERBOX_ANIMATION_DURATION = 0.5f;
	float const DEFAULT_CAMERA_TRANSITION_DURATION = 1.0f;
	float const SHOT_HOLD_TIME_MIN = 4.0f;
	float const SHOT_HOLD_TIME_MAX = 8.0f;

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

float SwgCuiCinematicConversation::easeInOutCubic(float t)
{
	return t < 0.5f
		? 4.0f * t * t * t
		: 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

//----------------------------------------------------------------------

float SwgCuiCinematicConversation::easeOutQuad(float t)
{
	return 1.0f - (1.0f - t) * (1.0f - t);
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
m_npcViewerPage(nullptr),
m_npcViewer(nullptr),
m_endConversationButton(nullptr),
m_targetNpcId(),
m_currentResponses(),
m_letterboxAnimationTime(0.0f),
m_letterboxTargetHeight(0.0f),
m_currentLetterboxHeight(0.0f),
m_letterboxAnimating(false),
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
m_currentShotType(CST_MediumShot),
m_shotHoldTime(SHOT_HOLD_TIME_MIN),
m_timeSinceLastShotChange(0.0f),
m_savedHudEnabled(true)
{
	// Get UI elements
	getCodeDataObject(TUIPage, m_letterboxTop, "letterboxTop");
	getCodeDataObject(TUIPage, m_letterboxBottom, "letterboxBottom");
	getCodeDataObject(TUIPage, m_dialoguePanel, "dialoguePanel");
	getCodeDataObject(TUIText, m_npcNameText, "npcName");
	getCodeDataObject(TUIText, m_npcMessageText, "npcMessage");
	getCodeDataObject(TUIPage, m_responsePanel, "responsePanel");
	getCodeDataObject(TUIButton, m_endConversationButton, "endConversation");

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

	// Get response buttons
	for (int i = 0; i < MAX_RESPONSES; ++i)
	{
		char buffer[32];

		snprintf(buffer, sizeof(buffer), "response%d", i + 1);
		if (!getCodeDataObject(TUIPage, m_responsePages[i], buffer, false))
		{
			m_responsePages[i] = nullptr;
			m_responseButtons[i] = nullptr;
			m_responsePrefixTexts[i] = nullptr;
			m_responseTexts[i] = nullptr;
			continue;
		}

		m_responseButtons[i] = dynamic_cast<UIButton *>(m_responsePages[i]->GetChild("button"));
		m_responsePrefixTexts[i] = dynamic_cast<UIText *>(m_responsePages[i]->GetChild("prefix"));
		m_responseTexts[i] = dynamic_cast<UIText *>(m_responsePages[i]->GetChild("text"));

		if (m_responseButtons[i])
		{
			registerMediatorObject(*m_responseButtons[i], true);
		}
		if (m_responsePrefixTexts[i])
		{
			m_responsePrefixTexts[i]->SetPreLocalized(true);
		}
		if (m_responseTexts[i])
		{
			m_responseTexts[i]->SetPreLocalized(true);
		}
	}

	if (m_endConversationButton)
	{
		registerMediatorObject(*m_endConversationButton, true);
	}

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

	// Initialize letterbox to hidden
	if (m_letterboxTop)
	{
		UISize size = m_letterboxTop->GetSize();
		size.y = 0;
		m_letterboxTop->SetSize(size);
	}
	if (m_letterboxBottom)
	{
		UISize size = m_letterboxBottom->GetSize();
		size.y = 0;
		m_letterboxBottom->SetSize(size);
	}
}

//----------------------------------------------------------------------

SwgCuiCinematicConversation::~SwgCuiCinematicConversation()
{
	delete m_callback;
	m_callback = nullptr;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::performActivate()
{
	setStickyVisible(true);
	CuiConversationManager::setCinematicConversationUiActive(true);

	CuiMediator::performActivate();

	ms_active = true;

	m_savedHudEnabled = true;
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

	CuiConversationManager::setCameraCommandHandler(&SwgCuiCinematicConversation::handleCameraCommand);

	// Start letterbox animation
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
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::performDeactivate()
{
	ms_active = false;

	CuiConversationManager::setCinematicConversationUiActive(false);

	if (Game::getHudSceneType() == Game::ST_ground)
	{
		SwgCuiHud * const hud = SwgCuiHudFactory::findMediatorForCurrentHud();
		if (hud)
			hud->setHudEnabled(m_savedHudEnabled);
	}

	setStickyVisible(false);

	// Restore camera control
	restoreCameraControl();

	// Disconnect signals
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onTargetChanged,
		static_cast<CuiConversationManager::Messages::TargetChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onResponsesChanged,
		static_cast<CuiConversationManager::Messages::ResponsesChanged *>(0));
	m_callback->disconnect(*this, &SwgCuiCinematicConversation::onConversationEnded,
		static_cast<CuiConversationManager::Messages::ConversationEnded *>(0));

	CuiConversationManager::setCameraCommandHandler(nullptr);

	// Release pointer
	CuiManager::requestPointer(false);

	// Animate letterbox out
	m_letterboxTargetHeight = 0.0f;
	m_letterboxAnimating = true;
	m_letterboxAnimationTime = 0.0f;

	// Clear viewer
	if (m_npcViewer)
	{
		m_npcViewer->clearObjects();
	}

	CuiMediator::performDeactivate();
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

	GameCamera * const currentCamera = groundScene->getCurrentCamera();
	if (currentCamera)
	{
		m_savedCameraTransform = currentCamera->getTransform_o2w();
		m_currentCameraPos = currentCamera->getPosition_w();

		// Calculate initial look-at based on camera forward direction
		Vector forward = currentCamera->getObjectFrameK_w();
		m_currentCameraLookAt = m_currentCameraPos + forward * 5.0f;
	}

	m_cameraControlActive = true;
	m_timeSinceLastShotChange = 0.0f;

	// Start with a medium shot only if we have a valid target in the world
	if (NetworkIdManager::getObjectById(m_targetNpcId))
	{
		setCameraShot(CST_MediumShot);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::restoreCameraControl()
{
	if (!m_cameraControlActive)
		return;

	GroundScene * const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
	if (!groundScene)
		return;

	// Restore the original camera view
	groundScene->setView(m_savedCameraView);

	// The FreeChaseCamera will automatically snap back to following the player

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

	outLookAt = computeNpcHeadPosition();

	// Position camera in front of NPC, slightly to the side for a more dynamic angle
	Vector npcForward = targetObj->getObjectFrameK_w();
	Vector npcRight = targetObj->getObjectFrameI_w();

	// Camera positioned in front and slightly right
	outCameraPos = outLookAt - npcForward * CLOSE_UP_DISTANCE + npcRight * 0.3f;
	outCameraPos.y = outLookAt.y + 0.1f; // Slightly above eye level
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

	Vector npcHeadPos = computeNpcHeadPosition();

	// Look at upper chest/neck area for medium shot
	outLookAt = npcHeadPos;
	outLookAt.y -= 0.3f;

	// Position camera further back
	Vector npcForward = targetObj->getObjectFrameK_w();
	Vector npcRight = targetObj->getObjectFrameI_w();

	outCameraPos = outLookAt - npcForward * MEDIUM_SHOT_DISTANCE + npcRight * 0.5f;
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
	outLookAt = computeNpcHeadPosition();

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
	Vector npcPos = computeNpcHeadPosition();

	// Look at the midpoint between player and NPC
	outLookAt = (playerPos + npcPos) * 0.5f;

	// Calculate perpendicular direction to the player-NPC line
	Vector playerToNpc = npcPos - playerPos;
	playerToNpc.y = 0.0f;
	float distance = playerToNpc.normalize();

	Vector perpendicular(-playerToNpc.z, 0.0f, playerToNpc.x);

	// Position camera to the side to show both characters
	outCameraPos = outLookAt + perpendicular * (distance * 0.8f + TWO_SHOT_DISTANCE);
	outCameraPos.y = outLookAt.y + 0.3f;
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
		// For reaction shot, look at the player instead
		{
			CreatureObject * const player = Game::getPlayerCreature();
			if (player)
			{
				targetLookAt = computePlayerPosition();
				Vector playerForward = player->getObjectFrameK_w();
				targetPos = targetLookAt - playerForward * CLOSE_UP_DISTANCE;
				targetPos.y = targetLookAt.y + 0.1f;
			}
			else
			{
				targetPos = m_currentCameraPos;
				targetLookAt = m_currentCameraLookAt;
			}
		}
		break;
	}

	transitionCamera(targetPos, targetLookAt, transitionDuration);

	// Reset shot hold timer
	m_timeSinceLastShotChange = 0.0f;
	m_shotHoldTime = SHOT_HOLD_TIME_MIN + (static_cast<float>(rand()) / RAND_MAX) * (SHOT_HOLD_TIME_MAX - SHOT_HOLD_TIME_MIN);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::transitionCamera(Vector const & targetPos, Vector const & targetLookAt, float duration)
{
	m_startCameraPos = m_currentCameraPos;
	m_startCameraLookAt = m_currentCameraLookAt;
	m_targetCameraPos = targetPos;
	m_targetCameraLookAt = targetLookAt;
	m_cameraTransitionTime = 0.0f;
	m_cameraTransitionDuration = duration;
	m_cameraTransitioning = true;
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	updateLetterbox(deltaTimeSecs);
	updateCameraFocus(deltaTimeSecs);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::updateLetterbox(float deltaTime)
{
	if (!m_letterboxAnimating)
		return;

	m_letterboxAnimationTime += deltaTime;
	float t = std::min(m_letterboxAnimationTime / LETTERBOX_ANIMATION_DURATION, 1.0f);
	t = easeInOutCubic(t);

	float startHeight = m_currentLetterboxHeight;
	float targetHeight = m_letterboxTargetHeight;
	float newHeight = startHeight + (targetHeight - startHeight) * t;

	// Only update if there's a meaningful change
	if (std::fabs(newHeight - m_currentLetterboxHeight) > 0.01f || m_letterboxAnimationTime >= LETTERBOX_ANIMATION_DURATION)
	{
		m_currentLetterboxHeight = newHeight;

		if (m_letterboxTop)
		{
			UISize size = m_letterboxTop->GetSize();
			size.y = static_cast<long>(newHeight);
			m_letterboxTop->SetSize(size);
		}
		if (m_letterboxBottom)
		{
			UISize size = m_letterboxBottom->GetSize();
			size.y = static_cast<long>(newHeight);
			m_letterboxBottom->SetSize(size);
		}
	}

	if (m_letterboxAnimationTime >= LETTERBOX_ANIMATION_DURATION)
	{
		m_letterboxAnimating = false;
		m_currentLetterboxHeight = targetHeight;
	}
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
		t = easeOutQuad(t);

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

	// Apply camera position and orientation
	GameCamera * const camera = groundScene->getCurrentCamera();
	if (camera)
	{
		// Set camera position
		camera->setPosition_w(m_currentCameraPos);

		// Calculate camera orientation to look at target
		Vector forward = m_currentCameraLookAt - m_currentCameraPos;
		forward.normalize();

		// Calculate right vector (cross product with up)
		Vector right = Vector::unitY.cross(forward);
		if (right.magnitudeSquared() < 0.001f)
		{
			// Handle case where forward is parallel to up
			right = Vector::unitX;
		}
		right.normalize();

		// Calculate true up vector
		Vector up = forward.cross(right);
		up.normalize();

		// Set camera orientation
		camera->setTransformIJK_o2p(right, up, forward);
	}

	// Update shot hold timer and potentially change shots (only if no server override)
	if (m_shotHoldTime > 0.0f)
	{
		m_timeSinceLastShotChange += deltaTime;

		// Auto-change shots for variety (optional - can be disabled)
		if (m_timeSinceLastShotChange >= m_shotHoldTime && !m_cameraTransitioning)
		{
			// Cycle through shot types
			int nextShot = (static_cast<int>(m_currentShotType) + 1) % 4; // Cycle through first 4 shot types
			setCameraShot(static_cast<CameraShotType>(nextShot));
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::handleCameraCommand(MessageQueueNpcConversationCameraCommand const * cmd)
{
	if (!cmd || !ms_enabled)
		return;

	CuiMediator * const mediator = CuiMediatorFactory::getInWorkspace(CuiMediatorTypes::WS_CinematicConversation, false, false);
	SwgCuiCinematicConversation * const cinematic = dynamic_cast<SwgCuiCinematicConversation *>(mediator);
	if (cinematic && cinematic->isActive())
	{
		cinematic->applyCameraCommand(cmd);
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::applyCameraCommand(MessageQueueNpcConversationCameraCommand const * cmd)
{
	if (!cmd || !m_cameraControlActive)
		return;

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
					Vector lookAt = CuiObjectTextManager::getCurrentObjectHeadPoint_o(*targetObj);
					lookAt.y += HEAD_HEIGHT_OFFSET;
					lookAt = targetObj->rotateTranslate_o2w(lookAt);

					// Keep camera roughly where it is, rotate to look at target
					Vector targetPos = m_currentCameraPos;
					transitionCamera(targetPos, lookAt, duration);
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
		setCameraShot(CST_MediumShot, duration > 0.0f ? duration : DEFAULT_CAMERA_TRANSITION_DURATION);
		break;

	case MessageQueueNpcConversationCameraCommand::CT_None:
	default:
		break;
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setupNpcViewer()
{
	if (!m_npcViewer)
		return;

	m_npcViewer->clearObjects();

	Object * const targetObj = NetworkIdManager::getObjectById(m_targetNpcId);
	ClientObject * const clientObj = targetObj ? targetObj->asClientObject() : nullptr;
	CreatureObject * const creature = clientObj ? clientObj->asCreatureObject() : nullptr;

	if (creature)
	{
		// Set NPC name
		if (m_npcNameText)
		{
			m_npcNameText->SetLocalText(creature->getLocalizedName());
		}

		// Add to viewer - viewer will create its own copy
		m_npcViewer->addObject(*creature);
		m_npcViewer->setViewDirty(true);

		// Focus on head
		m_npcViewer->setCameraLookAtCenter(true);
		m_npcViewer->recomputeZoom();
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::setNpcMessage(Unicode::String const & message)
{
	if (m_npcMessageText)
	{
		m_npcMessageText->SetLocalText(message);
	}

	// When a new message arrives, potentially change camera angle for variety
	if (m_cameraControlActive && !m_cameraTransitioning && m_timeSinceLastShotChange > 2.0f)
	{
		// Randomly decide whether to change shot
		if ((rand() % 100) < 40) // 40% chance to change shot on new message
		{
			int nextShot = rand() % 4;
			if (nextShot != static_cast<int>(m_currentShotType))
			{
				setCameraShot(static_cast<CameraShotType>(nextShot));
			}
		}
	}
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

void SwgCuiCinematicConversation::setResponses(std::vector<Unicode::String> const & responses)
{
	m_currentResponses.clear();

	// Parse all responses
	int index = 0;
	for (std::vector<Unicode::String>::const_iterator it = responses.begin();
		it != responses.end() && index < MAX_RESPONSES;
		++it, ++index)
	{
		ResponseData data = parseResponse(*it, index);
		m_currentResponses.push_back(data);
	}

	// Update UI
	for (int i = 0; i < MAX_RESPONSES; ++i)
	{
		if (m_responsePages[i] == nullptr)
			continue;

		if (i < static_cast<int>(m_currentResponses.size()))
		{
			ResponseData const & data = m_currentResponses[i];

			m_responsePages[i]->SetVisible(true);

			// Label is drawn on the UIButton; sibling UIText widgets were listed after the button in .ui and
			// painted on top (often blank), hiding the button caption. Keep overlays off and raise the button.
			if (m_responsePrefixTexts[i])
				m_responsePrefixTexts[i]->SetVisible(false);
			if (m_responseTexts[i])
				m_responseTexts[i]->SetVisible(false);

			if (m_responseButtons[i])
			{
				Unicode::String caption = data.responseText;
				if (data.prefix != RP_None)
					caption = data.prefixText + Unicode::narrowToWide(" ") + data.responseText;
				// UIButton::RenderText bails when mText is empty; SetLocalText only fills mLocalText.
				m_responseButtons[i]->SetText(caption);
				m_responsePages[i]->MoveChild(m_responseButtons[i], UIBaseObject::Top);
			}
		}
		else
		{
			m_responsePages[i]->SetVisible(false);
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::clearResponses()
{
	m_currentResponses.clear();

	for (int i = 0; i < MAX_RESPONSES; ++i)
	{
		if (m_responsePages[i])
		{
			m_responsePages[i]->SetVisible(false);
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::OnButtonPressed(UIWidget * context)
{
	// Check end conversation button — match Escape / IoWin: always tear down cinematic UI and restore HUD.
	if (context == m_endConversationButton)
	{
		CuiConversationManager::closeCinematicConversationFromInput();
		return;
	}

	// Check response buttons
	for (int i = 0; i < MAX_RESPONSES; ++i)
	{
		if (context == m_responseButtons[i])
		{
			if (i < static_cast<int>(m_currentResponses.size()))
			{
				selectResponse(m_currentResponses[i].responseIndex);
			}
			return;
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::selectResponse(int responseIndex)
{
	CuiConversationManager::respond(CuiConversationManager::getTarget(), responseIndex);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onTargetChanged(bool const &)
{
	CachedNetworkId const & targetId = CuiConversationManager::getTarget();

	if (!targetId.isValid())
	{
		// Conversation ended
		closeThroughWorkspace();
		return;
	}

	if (targetId != m_targetNpcId)
	{
		m_targetNpcId = targetId;
		setupNpcViewer();

		// Re-initialize camera for new target
		if (m_cameraControlActive)
		{
			setCameraShot(CST_MediumShot);
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onResponsesChanged(bool const &)
{
	// Update message
	setNpcMessage(CuiConversationManager::getLastMessage());

	// Update responses
	CuiConversationManager::StringVector responses;
	CuiConversationManager::getResponses(responses);
	setResponses(responses);
}

//----------------------------------------------------------------------

void SwgCuiCinematicConversation::onConversationEnded(bool const &)
{
	closeThroughWorkspace();
}

//======================================================================

