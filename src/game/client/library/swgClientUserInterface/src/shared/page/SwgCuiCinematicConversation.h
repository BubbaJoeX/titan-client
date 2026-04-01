//======================================================================
//
// SwgCuiCinematicConversation.h
// KOTOR-style cinematic dialogue system for ground conversations
//
//======================================================================

#ifndef INCLUDED_SwgCuiCinematicConversation_H
#define INCLUDED_SwgCuiCinematicConversation_H

//======================================================================

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"
#include "UIMessage.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkId.h"
#include "sharedMath/Vector.h"
#include "sharedMath/Transform.h"
#include "Unicode.h"
#include <vector>

//----------------------------------------------------------------------

class UIButton;
class UIPage;
class UIText;
class CuiWidget3dObjectListViewer;
class GroundScene;
class Object;

namespace MessageDispatch
{
	class Callback;
}

//----------------------------------------------------------------------

class SwgCuiCinematicConversation : public CuiMediator, public UIEventCallback
{
public:
	// Response prefix types for KOTOR-style formatting
	enum ResponsePrefix
	{
		RP_None,
		RP_Agree,
		RP_Decline,
		RP_Persuade,
		RP_Intimidate,
		RP_Lie,
		RP_Question,
		RP_Info,
		RP_Attack,
		RP_Custom
	};

	struct ResponseData
	{
		ResponsePrefix prefix;
		Unicode::String prefixText;
		Unicode::String responseText;
		int responseIndex;
	};

	// Camera shot types for cinematic variety
	enum CameraShotType
	{
		CST_CloseUp,        // Close up on NPC face
		CST_MediumShot,     // Medium shot showing upper body
		CST_OverShoulder,   // Over player's shoulder looking at NPC
		CST_TwoShot,        // Shows both player and NPC
		CST_Reaction        // Quick cut to player for reaction
	};

public:
	static bool isEnabled();
	static void setEnabled(bool enabled);
	static bool isActive();

	// Handle server-driven camera commands (called from PlayerCreatureController)
	static void handleCameraCommand(class MessageQueueNpcConversationCameraCommand const * cmd);

public:
	SwgCuiCinematicConversation(UIPage & page);
	virtual ~SwgCuiCinematicConversation();

	virtual void OnButtonPressed(UIWidget * context);
	virtual bool OnMessage(UIWidget * context, UIMessage const & msg);
	virtual void update(float deltaTimeSecs);

	// Set the NPC message text
	void setNpcMessage(Unicode::String const & message);

	// Set response options - parses [Prefix] tags
	void setResponses(std::vector<Unicode::String> const & responses);

	// Clear all responses
	void clearResponses();

protected:
	virtual void performActivate();
	virtual void performDeactivate();
	virtual bool canActivateWhenWorkspaceDisabled () const;
	virtual bool shouldSurviveDisabledWorkspace () const;

private:
	// Disabled
	SwgCuiCinematicConversation();
	SwgCuiCinematicConversation(SwgCuiCinematicConversation const & rhs);
	SwgCuiCinematicConversation & operator=(SwgCuiCinematicConversation const & rhs);

	// Parse response text for [Prefix] tags
	ResponseData parseResponse(Unicode::String const & response, int index);

	// Camera control methods
	void initializeCameraControl();
	void restoreCameraControl();
	void updateCameraFocus(float deltaTime);
	void setCameraShot(CameraShotType shotType);
	void setCameraShot(CameraShotType shotType, float transitionDuration);
	void transitionCamera(Vector const & targetPos, Vector const & targetLookAt, float duration);
	void applyCameraCommand(class MessageQueueNpcConversationCameraCommand const * cmd);
	Vector computeNpcHeadPosition() const;
	Vector computePlayerPosition() const;

	// Calculate ideal camera positions for different shot types
	void calculateCloseUpShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculateMediumShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculateOverShoulderShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculateTwoShot(Vector & outCameraPos, Vector & outLookAt) const;

	// Animate letterbox bars
	void updateLetterbox(float deltaTime);

	// Setup viewer with NPC appearance
	void setupNpcViewer();

	// Callbacks for conversation events
	void onTargetChanged(bool const & value);
	void onResponsesChanged(bool const & value);
	void onConversationEnded(bool const & value);

	// Send response to server
	void selectResponse(int responseIndex);

	// Easing functions
	static float easeInOutCubic(float t);
	static float easeOutQuad(float t);

private:
	MessageDispatch::Callback * m_callback;

	// UI Elements
	UIPage * m_letterboxTop;
	UIPage * m_letterboxBottom;
	UIPage * m_dialoguePanel;
	UIText * m_npcNameText;
	UIText * m_npcMessageText;
	UIPage * m_responsePanel;
	UIPage * m_npcViewerPage;
	CuiWidget3dObjectListViewer * m_npcViewer;

	// Response buttons (max 6 response options)
	static int const MAX_RESPONSES = 6;
	UIButton * m_responseButtons[MAX_RESPONSES];
	UIText * m_responsePrefixTexts[MAX_RESPONSES];
	UIText * m_responseTexts[MAX_RESPONSES];
	UIPage * m_responsePages[MAX_RESPONSES];

	// End conversation button
	UIButton * m_endConversationButton;

	// State
	NetworkId m_targetNpcId;
	std::vector<ResponseData> m_currentResponses;

	// Animation state
	float m_letterboxAnimationTime;
	float m_letterboxTargetHeight;
	float m_currentLetterboxHeight;
	bool m_letterboxAnimating;

	// Camera state
	bool m_cameraControlActive;
	int m_savedCameraView;
	Transform m_savedCameraTransform;
	Vector m_currentCameraPos;
	Vector m_currentCameraLookAt;
	Vector m_targetCameraPos;
	Vector m_targetCameraLookAt;
	Vector m_startCameraPos;
	Vector m_startCameraLookAt;
	float m_cameraTransitionTime;
	float m_cameraTransitionDuration;
	bool m_cameraTransitioning;
	CameraShotType m_currentShotType;
	float m_shotHoldTime;
	float m_timeSinceLastShotChange;

	bool m_savedHudEnabled;

	// Static settings
	static bool ms_enabled;
	static bool ms_active;

	// Camera configuration constants
	static float const CLOSE_UP_DISTANCE;
	static float const MEDIUM_SHOT_DISTANCE;
	static float const OVER_SHOULDER_DISTANCE;
	static float const TWO_SHOT_DISTANCE;
	static float const HEAD_HEIGHT_OFFSET;
	static float const CAMERA_TRANSITION_SPEED;
};

//======================================================================

#endif
