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
class UIScrollbar;
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

	/// Registered with CuiConversationManager while active — workspace enumeration may not find this mediator.
	static CuiMediator * provideActiveInstanceForConversationManager();

	/// Force-dismiss cinematic UI (Escape / End Conversation); uses deactivate(), not workspace close.
	static void executeCloseFromConversationManager();

	/// Registered with CuiConversationManager — GroundScene / IoWin use for scripted-look-at orbit only.
	static bool scriptedLookAtOrbitPredicateThunk ();

	/// IoWin / CuiConversationManager route digit and paging keys here (UIManager focus is unreliable).
	static void dispatchConversationKeystrokeFromManager (unsigned short keystroke);

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
	void syncScriptedOrbitCameraFromFreeCamera(class FreeCamera * freeCamera);
	void setCameraShot(CameraShotType shotType);
	void setCameraShot(CameraShotType shotType, float transitionDuration);
	void transitionCamera(Vector const & targetPos, Vector const & targetLookAt, float duration);
	void applyCameraCommand(class MessageQueueNpcConversationCameraCommand const * cmd);
	Vector computeScriptedLookAtPoint(Object & targetObj) const;
	Vector computeNpcHeadPosition() const;
	/// Dialogue camera aim point: humanoids use head; large creatures use chest / torso blend so the full figure fits.
	Vector computeNpcDialogueFramingPosition() const;
	Vector computePlayerPosition() const;

	// Calculate ideal camera positions for different shot types
	void calculateCloseUpShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculatePlayerFaceCloseUpShot(Vector & outCameraPos, Vector & outLookAt) const;
	bool useWideOpenFaceEstablishingShot() const;
	void calculateWideOpenFaceShot(Vector & outCameraPos, Vector & outLookAt) const;
	/// KOTOR/TOR-style: camera sits between participant heads on alternating sides of the P–N axis by speaker.
	void calculateAxisDialogueShot(bool npcSpeaking, Vector & outCameraPos, Vector & outLookAt) const;
	void calculateMediumShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculateOverShoulderShot(Vector & outCameraPos, Vector & outLookAt) const;
	void calculateTwoShot(Vector & outCameraPos, Vector & outLookAt) const;

	// Animate letterbox bars
	void updateLetterbox(float deltaTime);
	void updateScriptedLookAtCameraHint();
	void updateGmCinematicBadge();

	// NPC subtitle typewriter (timed with dialogue beat; cinematic mode only)
	void beginNpcMessageTypewriter(Unicode::String const & message);
	void updateNpcMessageTypewriter(float deltaTimeSecs);
	void finishNpcMessageTypewriter();
	void tryApplyDeferredNpcSubtitle();
	void maybeStartReactionPostLineBuffer();
	void flushPlayerReactionBeat();
	void applyTypewriterPauseAfterReveal(size_t newRevealLen);
	bool isNpcLinePrintingLocked() const;
	void updateResponseAvailabilityForTypewriter();

	void cacheDialogueUILayout();
	void applyDialogueLayoutForResponseCount(size_t responseCount);
	void restoreDialogueUILayout();
	void updateResponseAreaVisibility(bool visible);
	void ensureResponseSlotCount(size_t needed);
	void releaseDynamicResponseSlots();
	void updateResponseScrollbarVisibility();
	void refreshVisibleResponseRows();
	void changeResponsePage(int deltaPages);
	void updateResponsePageControls();
	void clampResponsePageStart();
	bool trySelectResponseByNumberKey(unsigned short keystroke);

	// Setup viewer with NPC appearance
	void setupNpcViewer();
	/// Sets npcName from current conversation target (localized name or template fallback).
	void updateSpeakerTitleText();

	// Callbacks for conversation events
	void onTargetChanged(bool const & value);
	void onResponsesChanged(bool const & value);
	void onConversationEnded(bool const & value);

	// Send response to server
	void selectResponse(int responseIndex);

	// Easing functions
	static float easeInOutCubic(float t);

private:
	MessageDispatch::Callback * m_callback;

	// UI Elements
	UIPage * m_letterboxTop;
	UIPage * m_letterboxBottom;
	UIPage * m_dialoguePanel;
	UIText * m_npcNameText;
	UIText * m_npcMessageText;
	UIPage * m_responsePanel;
	UIScrollbar * m_responseScrollbar;
	UIPage * m_npcViewerPage;
	CuiWidget3dObjectListViewer * m_npcViewer;

	struct ResponseSlot
	{
		UIPage * page;
		UIButton * button;
		UIText * prefixText;
		UIText * text;
		bool ownedDuplicate;
	};

	// First six rows come from ui_cinematic_conversation.inc; pagination uses up to four visible rows.
	static int const BASE_RESPONSE_SLOTS = 6;
	static int const MAX_RESPONSE_SLOTS = 64;
	static int const RESPONSES_PER_PAGE = 4;

	std::vector<ResponseSlot> m_responseSlots;
	UIPage * m_responseClonePrototype;

	// End conversation button
	UIButton * m_endConversationButton;
	UIButton * m_responsePagePrevButton;
	UIButton * m_responsePageNextButton;
	UIText * m_responsePageLabel;
	/// Shown only during server scripted look-at (Shift+mouse orbit).
	UIText * m_scriptedLookAtCameraHint;
	/// Compact admin marker when world "GAME MASTER" banner is suppressed by cinematic UI.
	UIText * m_gmCinematicBadge;
	bool m_cachedUILayoutValid;
	UISize m_cachedDialoguePanelSize;
	UIPoint m_cachedDialoguePanelLocation;
	UISize m_cachedResponsePanelSize;
	UIPoint m_cachedResponsePanelLocation;
	UIPoint m_cachedEndButtonLocation;

	// State
	NetworkId m_targetNpcId;
	std::vector<ResponseData> m_currentResponses;
	/// First visible response index in `m_currentResponses` (multiples of RESPONSES_PER_PAGE when paginated).
	size_t m_responsePageStart;
	/// Target cleared during TargetChanged — defer deactivateInWorkspace to update(); closing synchronously in the emit stack crashed / conflicted with workspace iterators.
	bool m_deferCloseUntilUpdate;

	// Animation state
	float m_letterboxAnimationTime;
	float m_letterboxTargetHeight;
	float m_currentLetterboxHeight;
	float m_letterboxAnimationStartHeight;
	bool m_letterboxAnimating;
	/// After deactivate, re-show page for letterbox closing animation; then hide and clear updating.
	bool m_hidePageAfterLetterboxOut;

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
	/// Ground HUD was left disabled until letterbox-out finishes so HUD and conversation chrome never overlap.
	bool m_deferHudRestoreUntilLetterboxOut;

	void applyDeferredHudRestoreIfNeeded();

	// Dialogue-driven camera: NPC lines refresh framing; player picks a reaction shot before returning to NPC.
	bool                                      m_playerReactionHoldActive;
	/// After selectResponse: wait for player typewriter + buffer before applying server NPC line + response branch.
	bool                                      m_playerReactionBeatPending;
	/// < 0: not counting yet; >= 0: seconds until flushPlayerReactionBeat()
	float                                     m_reactionPostLineBufferRemaining;
	bool                                      m_haveDeferredBranchResponses;
	std::vector<Unicode::String>              m_deferredBranchResponses;
	Unicode::String                           m_lastNpcMessageForCamera;

	// Player reaction uses the same subtitle typewriter; defer incoming NPC lines until it finishes.
	bool                                      m_deferIncomingNpcSubtitle;
	Unicode::String                           m_deferredNpcSubtitle;

	// Server npcConversationCameraLookAtTarget — do not let setNpcMessage() snap back to NPC medium/close-up.
	bool                                      m_scriptedLookAtFramingActive;

	// Typewriter state (full text stored; widget shows progressive prefix)
	bool                                      m_typewriterActive;
	Unicode::String                           m_typewriterFullText;
	size_t                                    m_typewriterRevealLength;
	float                                     m_typewriterCharAccumulator;
	float                                     m_typewriterCharsPerSecond;
	float                                     m_typewriterPauseRemaining;

	// Static settings
	static bool ms_enabled;
	static bool ms_active;
	// Instance that registered the camera command handler (getInWorkspace lookup can fail while UI is active)
	static SwgCuiCinematicConversation * ms_cameraCommandTarget;

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
