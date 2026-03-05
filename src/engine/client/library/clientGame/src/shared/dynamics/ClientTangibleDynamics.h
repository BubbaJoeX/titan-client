// ======================================================================
//
// ClientTangibleDynamics.h
// Copyright 2025, 2026 Titan SWG, All Rights Reserved
//
// Client-side dynamics for tangible objects: push, spin, breathing,
// bounce, wobble, orbit, drag, and easing.
//
// ======================================================================

#ifndef INCLUDED_ClientTangibleDynamics_H
#define INCLUDED_ClientTangibleDynamics_H

// ======================================================================

#include "sharedObject/Dynamics.h"
#include "sharedMath/Vector.h"
#include "sharedFoundation/Timer.h"

class Object;

// ======================================================================

class ClientTangibleDynamics : public Dynamics
{
public:

	enum ForceMode
	{
		FM_none         = 0,
		FM_push         = (1 << 0),
		FM_spin         = (1 << 1),
		FM_breathing    = (1 << 2),
		FM_bounce       = (1 << 3),
		FM_wobble       = (1 << 4),
		FM_orbit        = (1 << 5),
		FM_hover        = (1 << 6),
		FM_followTarget = (1 << 7),
		FM_sway         = (1 << 8),
		FM_shake        = (1 << 9),
		FM_float        = (1 << 10),
		FM_conveyor     = (1 << 11)
	};

	enum MovementSpace
	{
		MS_world,
		MS_parent,
		MS_object
	};

	enum EaseType
	{
		ET_none,
		ET_easeIn,
		ET_easeOut,
		ET_easeInOut
	};

	explicit ClientTangibleDynamics(Object* owner);
	virtual ~ClientTangibleDynamics();

	// Push
	void   setPushForce(const Vector& velocity, float duration = -1.0f, MovementSpace space = MS_world);
	void   setPushForceWithDrag(const Vector& velocity, float drag, float duration = -1.0f, MovementSpace space = MS_world);
	void   clearPushForce();
	Vector getPushForce() const;
	float  getPushForceDuration() const;
	void   setPushDrag(float drag);
	float  getPushDrag() const;

	// Spin
	void   setSpinForce(const Vector& rotationRadiansPerSecond, float duration = -1.0f);
	void   clearSpinForce();
	Vector getSpinForce() const;
	float  getSpinForceDuration() const;
	bool   getSpinAroundAppearanceCenter() const;
	void   setSpinAroundAppearanceCenter(bool spinAroundAppearanceCenter);

	// Breathing (sinusoidal)
	void  setBreathingEffect(float minimumScale, float maximumScale, float speed, float duration = -1.0f);
	void  clearBreathingEffect();
	float getBreathingMinScale() const;
	float getBreathingMaxScale() const;
	float getBreathingSpeed() const;
	float getBreathingDuration() const;

	// Bounce
	void  setBounceEffect(float gravity, float elasticity, float initialUpVelocity, float duration = -1.0f);
	void  clearBounceEffect();
	float getBounceGravity() const;
	float getBounceElasticity() const;

	// Wobble
	void   setWobbleEffect(const Vector& amplitude, const Vector& frequency, float duration = -1.0f);
	void   clearWobbleEffect();
	Vector getWobbleAmplitude() const;
	Vector getWobbleFrequency() const;

	// Orbit
	void   setOrbitEffect(const Vector& center, float radius, float radiansPerSecond, float duration = -1.0f);
	void   clearOrbitEffect();
	Vector getOrbitCenter() const;
	float  getOrbitRadius() const;

	// Hover (terrain-following with slight bob)
	void  setHoverEffect(float hoverHeight, float bobAmplitude = 0.1f, float bobSpeed = 1.0f, float duration = -1.0f);
	void  clearHoverEffect();
	float getHoverHeight() const;
	float getHoverBobAmplitude() const;
	float getHoverBobSpeed() const;

	// Follow Target (hover + follow another object, matching rotation)
	void   setFollowTargetEffect(uint64 targetNetworkId, float followDistance, float followSpeed,
	                             float hoverHeight = 1.0f, float bobAmplitude = 0.05f, float duration = -1.0f);
	void   clearFollowTargetEffect();
	uint64 getFollowTargetId() const;
	float  getFollowDistance() const;
	float  getFollowSpeed() const;
	float  getFollowHoverHeight() const;
	float  getFollowBobAmplitude() const;

	// Sway/Pendulum (swinging back and forth)
	void   setSwayEffect(float swingAngle, float swingSpeed, float damping = 0.0f, float duration = -1.0f);
	void   clearSwayEffect();
	float  getSwayAngle() const;
	float  getSwaySpeed() const;

	// Shake/Vibrate (rapid small position offsets)
	void   setShakeEffect(float intensity, float frequency, float duration = -1.0f);
	void   clearShakeEffect();
	float  getShakeIntensity() const;
	float  getShakeFrequency() const;

	// Float/Levitate (slow drift up and down)
	void   setFloatEffect(float floatHeight, float driftSpeed, float randomStrength = 0.1f, float duration = -1.0f);
	void   clearFloatEffect();
	float  getFloatHeight() const;
	float  getFloatDriftSpeed() const;

	// Conveyor (continuous linear movement with optional wrap)
	void   setConveyorEffect(const Vector& direction, float speed, float wrapDistance = 0.0f, float duration = -1.0f);
	void   clearConveyorEffect();
	Vector getConveyorDirection() const;
	float  getConveyorSpeed() const;
	float  getConveyorWrapDistance() const;

	// Easing
	void setEasing(EaseType easeType, float easeDuration);

	// Combined
	void setCombinedForces(const Vector& pushVelocity, const Vector& spinAngular,
		float minScale, float maxScale, float breatheSpeed, float duration = -1.0f);
	void clearAllForces();

	// Query
	int  getActiveForceMask() const;
	bool isActive() const;
	bool isForceActive(ForceMode mode) const;

	// Alter
	virtual float alter(float elapsedTime);

protected:

	virtual void updatePushForce(float elapsedTime);
	virtual void updateSpinForce(float elapsedTime);
	virtual void updateBreathingEffect(float elapsedTime);
	virtual void updateBounceEffect(float elapsedTime);
	virtual void updateWobbleEffect(float elapsedTime);
	virtual void updateOrbitEffect(float elapsedTime);
	virtual void updateHoverEffect(float elapsedTime);
	virtual void updateFollowTargetEffect(float elapsedTime);

private:

	ClientTangibleDynamics();
	ClientTangibleDynamics(const ClientTangibleDynamics&);
	ClientTangibleDynamics& operator=(const ClientTangibleDynamics&);

	static float computeEaseFactor(EaseType easeType, float elapsed, float duration, float easeDuration);

	// Push
	Vector        m_pushVelocity;
	float         m_pushDuration;
	float         m_pushElapsed;
	float         m_pushDrag;
	MovementSpace m_pushSpace;
	bool          m_pushForceActive;

	// Spin
	Vector m_spinAngular;
	float  m_spinDuration;
	float  m_spinElapsed;
	bool   m_spinForceActive;
	bool   m_spinAroundAppearanceCenter;

	// Breathing
	Vector m_baseScale;
	float  m_breathingMin;
	float  m_breathingMax;
	float  m_breathingSpeed;
	float  m_breathingDuration;
	float  m_breathingElapsed;
	float  m_breathingPhase;
	bool   m_breathingEffectActive;

	// Bounce
	float  m_bounceGravity;
	float  m_bounceElasticity;
	float  m_bounceVerticalVelocity;
	float  m_bounceFloorY;
	float  m_bounceDuration;
	float  m_bounceElapsed;
	bool   m_bounceEffectActive;

	// Wobble
	Vector m_wobbleAmplitude;
	Vector m_wobbleFrequency;
	float  m_wobbleDuration;
	float  m_wobbleElapsed;
	float  m_wobblePhase;
	Vector m_wobbleOrigin;
	bool   m_wobbleEffectActive;

	// Orbit
	Vector m_orbitCenter;
	float  m_orbitRadius;
	float  m_orbitSpeed;
	float  m_orbitAngle;
	float  m_orbitDuration;
	float  m_orbitElapsed;
	bool   m_orbitEffectActive;

	// Hover
	float  m_hoverHeight;
	float  m_hoverBobAmplitude;
	float  m_hoverBobSpeed;
	float  m_hoverBobPhase;
	float  m_hoverDuration;
	float  m_hoverElapsed;
	bool   m_hoverEffectActive;

	// Follow Target
	uint64 m_followTargetId;
	float  m_followDistance;
	float  m_followSpeed;
	float  m_followHoverHeight;
	float  m_followBobAmplitude;
	float  m_followBobPhase;
	float  m_followDuration;
	float  m_followElapsed;
	bool   m_followTargetEffectActive;

	// Sway
	float  m_swayAngle;
	float  m_swaySpeed;
	float  m_swayDamping;
	float  m_swayPhase;
	float  m_swayDuration;
	float  m_swayElapsed;
	bool   m_swayEffectActive;

	// Shake
	float  m_shakeIntensity;
	float  m_shakeFrequency;
	Vector m_shakeOrigin;
	float  m_shakePhase;
	float  m_shakeDuration;
	float  m_shakeElapsed;
	bool   m_shakeEffectActive;

	// Float
	float  m_floatHeight;
	float  m_floatDriftSpeed;
	float  m_floatRandomStrength;
	Vector m_floatOrigin;
	float  m_floatPhase;
	float  m_floatDuration;
	float  m_floatElapsed;
	bool   m_floatEffectActive;

	// Conveyor
	Vector m_conveyorDirection;
	float  m_conveyorSpeed;
	float  m_conveyorWrapDistance;
	Vector m_conveyorOrigin;
	float  m_conveyorTravelDistance;
	float  m_conveyorDuration;
	float  m_conveyorElapsed;
	bool   m_conveyorEffectActive;

	// Easing
	EaseType m_easeType;
	float    m_easeDuration;

	// Overall
	int m_activeForceMask;

	void updateSwayEffect(float elapsedTime);
	void updateShakeEffect(float elapsedTime);
	void updateFloatEffect(float elapsedTime);
	void updateConveyorEffect(float elapsedTime);
	void recalculateMode();
};

// ======================================================================

#endif
