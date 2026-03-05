// ======================================================================
//
// ClientTangibleDynamics.cpp
// Copyright 2025, 2026 Titan SWG, All Rights Reserved
//
// Client-side TangibleDynamics: push, spin, breathing,
// bounce, wobble, orbit with drag and easing support.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientTangibleDynamics.h"

#include "sharedMath/Vector.h"
#include "sharedObject/Object.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedMath/Transform.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedCollision/CollisionWorld.h"

#include <cmath>

// ======================================================================

namespace ClientTangibleDynamicsNamespace
{
	static float const s_bounceMinVelocity = 0.05f;
}

using namespace ClientTangibleDynamicsNamespace;

// Use global PI and PI_TIMES_2 from FloatMath.h (included via FirstClientGame.h)

// ======================================================================
// EASING
// ======================================================================

float ClientTangibleDynamics::computeEaseFactor(EaseType easeType, float elapsed, float duration, float easeDuration)
{
	if (easeType == ET_none || easeDuration <= 0.0f)
		return 1.0f;

	float factor = 1.0f;

	switch (easeType)
	{
	case ET_easeIn:
		if (elapsed < easeDuration)
			factor = elapsed / easeDuration;
		break;
	case ET_easeOut:
		if (duration > 0.0f && elapsed > (duration - easeDuration))
		{
			float remaining = duration - elapsed;
			factor = (remaining > 0.0f) ? (remaining / easeDuration) : 0.0f;
		}
		break;
	case ET_easeInOut:
		if (elapsed < easeDuration)
			factor = elapsed / easeDuration;
		else if (duration > 0.0f && elapsed > (duration - easeDuration))
		{
			float remaining = duration - elapsed;
			factor = (remaining > 0.0f) ? (remaining / easeDuration) : 0.0f;
		}
		break;
	default:
		break;
	}

	return factor * factor * (3.0f - 2.0f * factor);
}

// ======================================================================
// CONSTRUCTOR / DESTRUCTOR
// ======================================================================

ClientTangibleDynamics::ClientTangibleDynamics(Object* owner) :
	Dynamics(owner),
	m_pushVelocity(Vector::zero),
	m_pushDuration(-1.0f),
	m_pushElapsed(0.0f),
	m_pushDrag(0.0f),
	m_pushSpace(MS_world),
	m_pushForceActive(false),
	m_spinAngular(Vector::zero),
	m_spinDuration(-1.0f),
	m_spinElapsed(0.0f),
	m_spinForceActive(false),
	m_spinAroundAppearanceCenter(false),
	m_baseScale(owner ? owner->getScale() : Vector::xyz111),
	m_breathingMin(1.0f),
	m_breathingMax(1.0f),
	m_breathingSpeed(1.0f),
	m_breathingDuration(-1.0f),
	m_breathingElapsed(0.0f),
	m_breathingPhase(0.0f),
	m_breathingEffectActive(false),
	m_bounceGravity(9.8f),
	m_bounceElasticity(0.6f),
	m_bounceVerticalVelocity(0.0f),
	m_bounceFloorY(0.0f),
	m_bounceDuration(-1.0f),
	m_bounceElapsed(0.0f),
	m_bounceEffectActive(false),
	m_wobbleAmplitude(Vector::zero),
	m_wobbleFrequency(Vector::zero),
	m_wobbleDuration(-1.0f),
	m_wobbleElapsed(0.0f),
	m_wobblePhase(0.0f),
	m_wobbleOrigin(Vector::zero),
	m_wobbleEffectActive(false),
	m_orbitCenter(Vector::zero),
	m_orbitRadius(1.0f),
	m_orbitSpeed(1.0f),
	m_orbitAngle(0.0f),
	m_orbitDuration(-1.0f),
	m_orbitElapsed(0.0f),
	m_orbitEffectActive(false),
	m_hoverHeight(1.0f),
	m_hoverBobAmplitude(0.1f),
	m_hoverBobSpeed(1.0f),
	m_hoverBobPhase(0.0f),
	m_hoverDuration(-1.0f),
	m_hoverElapsed(0.0f),
	m_hoverEffectActive(false),
	m_followTargetId(0),
	m_followDistance(2.0f),
	m_followSpeed(3.0f),
	m_followHoverHeight(1.0f),
	m_followBobAmplitude(0.05f),
	m_followBobPhase(0.0f),
	m_followDuration(-1.0f),
	m_followElapsed(0.0f),
	m_followTargetEffectActive(false),
	m_swayAngle(0.1f),
	m_swaySpeed(1.0f),
	m_swayDamping(0.0f),
	m_swayPhase(0.0f),
	m_swayDuration(-1.0f),
	m_swayElapsed(0.0f),
	m_swayEffectActive(false),
	m_shakeIntensity(0.1f),
	m_shakeFrequency(10.0f),
	m_shakeOrigin(Vector::zero),
	m_shakePhase(0.0f),
	m_shakeDuration(-1.0f),
	m_shakeElapsed(0.0f),
	m_shakeEffectActive(false),
	m_floatHeight(0.5f),
	m_floatDriftSpeed(0.5f),
	m_floatRandomStrength(0.1f),
	m_floatOrigin(Vector::zero),
	m_floatPhase(0.0f),
	m_floatDuration(-1.0f),
	m_floatElapsed(0.0f),
	m_floatEffectActive(false),
	m_conveyorDirection(Vector::unitZ),
	m_conveyorSpeed(1.0f),
	m_conveyorWrapDistance(0.0f),
	m_conveyorOrigin(Vector::zero),
	m_conveyorTravelDistance(0.0f),
	m_conveyorDuration(-1.0f),
	m_conveyorElapsed(0.0f),
	m_conveyorEffectActive(false),
	m_easeType(ET_none),
	m_easeDuration(0.5f),
	m_activeForceMask(FM_none)
{
}

// ----------------------------------------------------------------------

ClientTangibleDynamics::~ClientTangibleDynamics()
{
	Object* const owner = getOwner();
	if (owner != NULL && m_breathingEffectActive)
	{
		owner->setRecursiveScale(m_baseScale);
	}
}

// ======================================================================
// PUSH
// ======================================================================

void ClientTangibleDynamics::setPushForce(const Vector& velocity, float duration, MovementSpace space)
{
	m_pushVelocity = velocity;
	m_pushDuration = duration;
	m_pushElapsed = 0.0f;
	m_pushDrag = 0.0f;
	m_pushSpace = space;
	m_pushForceActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::setPushForceWithDrag(const Vector& velocity, float drag, float duration, MovementSpace space)
{
	m_pushVelocity = velocity;
	m_pushDuration = duration;
	m_pushElapsed = 0.0f;
	m_pushDrag = drag;
	m_pushSpace = space;
	m_pushForceActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearPushForce()
{
	m_pushVelocity = Vector::zero;
	m_pushDuration = -1.0f;
	m_pushElapsed = 0.0f;
	m_pushDrag = 0.0f;
	m_pushForceActive = false;
	recalculateMode();
}

Vector ClientTangibleDynamics::getPushForce() const              { return m_pushVelocity; }
void   ClientTangibleDynamics::setPushDrag(float drag)           { m_pushDrag = drag; }
float  ClientTangibleDynamics::getPushDrag() const               { return m_pushDrag; }

float ClientTangibleDynamics::getPushForceDuration() const
{
	return (m_pushDuration < 0.0f) ? -1.0f : (m_pushDuration - m_pushElapsed);
}

// ======================================================================
// SPIN
// ======================================================================

void ClientTangibleDynamics::setSpinForce(const Vector& r, float duration)
{
	m_spinAngular = r;
	m_spinDuration = duration;
	m_spinElapsed = 0.0f;
	m_spinForceActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearSpinForce()
{
	m_spinAngular = Vector::zero;
	m_spinDuration = -1.0f;
	m_spinElapsed = 0.0f;
	m_spinForceActive = false;
	recalculateMode();
}

Vector ClientTangibleDynamics::getSpinForce() const                  { return m_spinAngular; }
bool   ClientTangibleDynamics::getSpinAroundAppearanceCenter() const { return m_spinAroundAppearanceCenter; }
void   ClientTangibleDynamics::setSpinAroundAppearanceCenter(bool v) { m_spinAroundAppearanceCenter = v; }

float ClientTangibleDynamics::getSpinForceDuration() const
{
	return (m_spinDuration < 0.0f) ? -1.0f : (m_spinDuration - m_spinElapsed);
}

// ======================================================================
// BREATHING
// ======================================================================

void ClientTangibleDynamics::setBreathingEffect(float min, float max, float speed, float duration)
{
	m_breathingMin = min;
	m_breathingMax = max;
	m_breathingSpeed = speed;
	m_breathingDuration = duration;
	m_breathingElapsed = 0.0f;
	m_breathingPhase = 0.0f;
	m_breathingEffectActive = true;

	Object* const owner = getOwner();
	if (owner != NULL)
		m_baseScale = owner->getScale();

	recalculateMode();
}

void ClientTangibleDynamics::clearBreathingEffect()
{
	Object* const owner = getOwner();
	if (owner != NULL && m_breathingEffectActive)
		owner->setRecursiveScale(m_baseScale);

	m_breathingMin = 1.0f;
	m_breathingMax = 1.0f;
	m_breathingSpeed = 1.0f;
	m_breathingDuration = -1.0f;
	m_breathingElapsed = 0.0f;
	m_breathingPhase = 0.0f;
	m_breathingEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getBreathingMinScale() const  { return m_breathingMin; }
float ClientTangibleDynamics::getBreathingMaxScale() const  { return m_breathingMax; }
float ClientTangibleDynamics::getBreathingSpeed() const     { return m_breathingSpeed; }

float ClientTangibleDynamics::getBreathingDuration() const
{
	return (m_breathingDuration < 0.0f) ? -1.0f : (m_breathingDuration - m_breathingElapsed);
}

// ======================================================================
// BOUNCE
// ======================================================================

void ClientTangibleDynamics::setBounceEffect(float gravity, float elasticity, float initialUpVelocity, float duration)
{
	Object* const owner = getOwner();
	m_bounceGravity = gravity;
	m_bounceElasticity = clamp(0.0f, elasticity, 1.0f);
	m_bounceVerticalVelocity = initialUpVelocity;
	m_bounceFloorY = owner ? owner->getPosition_w().y : 0.0f;
	m_bounceDuration = duration;
	m_bounceElapsed = 0.0f;
	m_bounceEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearBounceEffect()
{
	m_bounceGravity = 9.8f;
	m_bounceElasticity = 0.6f;
	m_bounceVerticalVelocity = 0.0f;
	m_bounceDuration = -1.0f;
	m_bounceElapsed = 0.0f;
	m_bounceEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getBounceGravity() const     { return m_bounceGravity; }
float ClientTangibleDynamics::getBounceElasticity() const  { return m_bounceElasticity; }

// ======================================================================
// WOBBLE
// ======================================================================

void ClientTangibleDynamics::setWobbleEffect(const Vector& amplitude, const Vector& frequency, float duration)
{
	Object* const owner = getOwner();
	m_wobbleAmplitude = amplitude;
	m_wobbleFrequency = frequency;
	m_wobbleDuration = duration;
	m_wobbleElapsed = 0.0f;
	m_wobblePhase = 0.0f;
	m_wobbleOrigin = owner ? owner->getPosition_w() : Vector::zero;
	m_wobbleEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearWobbleEffect()
{
	Object* const owner = getOwner();
	if (owner != NULL && m_wobbleEffectActive)
		owner->setPosition_w(m_wobbleOrigin);

	m_wobbleAmplitude = Vector::zero;
	m_wobbleFrequency = Vector::zero;
	m_wobbleDuration = -1.0f;
	m_wobbleElapsed = 0.0f;
	m_wobblePhase = 0.0f;
	m_wobbleEffectActive = false;
	recalculateMode();
}

Vector ClientTangibleDynamics::getWobbleAmplitude() const  { return m_wobbleAmplitude; }
Vector ClientTangibleDynamics::getWobbleFrequency() const  { return m_wobbleFrequency; }

// ======================================================================
// ORBIT
// ======================================================================

void ClientTangibleDynamics::setOrbitEffect(const Vector& center, float radius, float radiansPerSecond, float duration)
{
	Object* const owner = getOwner();
	m_orbitCenter = center;
	m_orbitRadius = radius;
	m_orbitSpeed = radiansPerSecond;
	m_orbitDuration = duration;
	m_orbitElapsed = 0.0f;
	m_orbitEffectActive = true;

	if (owner != NULL)
	{
		Vector delta = owner->getPosition_w() - center;
		m_orbitAngle = atan2(delta.x, delta.z);
	}
	else
	{
		m_orbitAngle = 0.0f;
	}

	recalculateMode();
}

void ClientTangibleDynamics::clearOrbitEffect()
{
	m_orbitCenter = Vector::zero;
	m_orbitRadius = 1.0f;
	m_orbitSpeed = 1.0f;
	m_orbitAngle = 0.0f;
	m_orbitDuration = -1.0f;
	m_orbitElapsed = 0.0f;
	m_orbitEffectActive = false;
	recalculateMode();
}

Vector ClientTangibleDynamics::getOrbitCenter() const  { return m_orbitCenter; }
float  ClientTangibleDynamics::getOrbitRadius() const  { return m_orbitRadius; }

// ======================================================================
// HOVER
// ======================================================================

void ClientTangibleDynamics::setHoverEffect(float hoverHeight, float bobAmplitude, float bobSpeed, float duration)
{
	m_hoverHeight = hoverHeight;
	m_hoverBobAmplitude = bobAmplitude;
	m_hoverBobSpeed = bobSpeed;
	m_hoverBobPhase = 0.0f;
	m_hoverDuration = duration;
	m_hoverElapsed = 0.0f;
	m_hoverEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearHoverEffect()
{
	m_hoverHeight = 1.0f;
	m_hoverBobAmplitude = 0.1f;
	m_hoverBobSpeed = 1.0f;
	m_hoverBobPhase = 0.0f;
	m_hoverDuration = -1.0f;
	m_hoverElapsed = 0.0f;
	m_hoverEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getHoverHeight() const       { return m_hoverHeight; }
float ClientTangibleDynamics::getHoverBobAmplitude() const { return m_hoverBobAmplitude; }
float ClientTangibleDynamics::getHoverBobSpeed() const     { return m_hoverBobSpeed; }

// ======================================================================
// FOLLOW TARGET
// ======================================================================

void ClientTangibleDynamics::setFollowTargetEffect(uint64 targetNetworkId, float followDistance, float followSpeed,
	float hoverHeight, float bobAmplitude, float duration)
{
	m_followTargetId = targetNetworkId;
	m_followDistance = followDistance;
	m_followSpeed = followSpeed;
	m_followHoverHeight = hoverHeight;
	m_followBobAmplitude = bobAmplitude;
	m_followBobPhase = 0.0f;
	m_followDuration = duration;
	m_followElapsed = 0.0f;
	m_followTargetEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearFollowTargetEffect()
{
	m_followTargetId = 0;
	m_followDistance = 2.0f;
	m_followSpeed = 3.0f;
	m_followHoverHeight = 1.0f;
	m_followBobAmplitude = 0.05f;
	m_followBobPhase = 0.0f;
	m_followDuration = -1.0f;
	m_followElapsed = 0.0f;
	m_followTargetEffectActive = false;
	recalculateMode();
}

uint64 ClientTangibleDynamics::getFollowTargetId() const  { return m_followTargetId; }
float  ClientTangibleDynamics::getFollowDistance() const  { return m_followDistance; }
float  ClientTangibleDynamics::getFollowSpeed() const     { return m_followSpeed; }
float  ClientTangibleDynamics::getFollowHoverHeight() const    { return m_followHoverHeight; }
float  ClientTangibleDynamics::getFollowBobAmplitude() const   { return m_followBobAmplitude; }

// ======================================================================
// SWAY
// ======================================================================

void ClientTangibleDynamics::setSwayEffect(float swingAngle, float swingSpeed, float damping, float duration)
{
	m_swayAngle = swingAngle;
	m_swaySpeed = swingSpeed;
	m_swayDamping = damping;
	m_swayPhase = 0.0f;
	m_swayDuration = duration;
	m_swayElapsed = 0.0f;
	m_swayEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearSwayEffect()
{
	m_swayAngle = 0.1f;
	m_swaySpeed = 1.0f;
	m_swayDamping = 0.0f;
	m_swayPhase = 0.0f;
	m_swayDuration = -1.0f;
	m_swayElapsed = 0.0f;
	m_swayEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getSwayAngle() const { return m_swayAngle; }
float ClientTangibleDynamics::getSwaySpeed() const { return m_swaySpeed; }

// ======================================================================
// SHAKE
// ======================================================================

void ClientTangibleDynamics::setShakeEffect(float intensity, float frequency, float duration)
{
	Object* const owner = getOwner();

	m_shakeIntensity = intensity;
	m_shakeFrequency = frequency;
	m_shakeOrigin = owner ? owner->getPosition_w() : Vector::zero;
	m_shakePhase = 0.0f;
	m_shakeDuration = duration;
	m_shakeElapsed = 0.0f;
	m_shakeEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearShakeEffect()
{
	Object* const owner = getOwner();
	if (owner != NULL && m_shakeEffectActive)
	{
		owner->setPosition_w(m_shakeOrigin);
	}

	m_shakeIntensity = 0.1f;
	m_shakeFrequency = 10.0f;
	m_shakeOrigin = Vector::zero;
	m_shakePhase = 0.0f;
	m_shakeDuration = -1.0f;
	m_shakeElapsed = 0.0f;
	m_shakeEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getShakeIntensity() const { return m_shakeIntensity; }
float ClientTangibleDynamics::getShakeFrequency() const { return m_shakeFrequency; }

// ======================================================================
// FLOAT
// ======================================================================

void ClientTangibleDynamics::setFloatEffect(float floatHeight, float driftSpeed, float randomStrength, float duration)
{
	Object* const owner = getOwner();

	m_floatHeight = floatHeight;
	m_floatDriftSpeed = driftSpeed;
	m_floatRandomStrength = randomStrength;
	m_floatOrigin = owner ? owner->getPosition_w() : Vector::zero;
	m_floatPhase = 0.0f;
	m_floatDuration = duration;
	m_floatElapsed = 0.0f;
	m_floatEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearFloatEffect()
{
	Object* const owner = getOwner();
	if (owner != NULL && m_floatEffectActive)
	{
		owner->setPosition_w(m_floatOrigin);
	}

	m_floatHeight = 0.5f;
	m_floatDriftSpeed = 0.5f;
	m_floatRandomStrength = 0.1f;
	m_floatOrigin = Vector::zero;
	m_floatPhase = 0.0f;
	m_floatDuration = -1.0f;
	m_floatElapsed = 0.0f;
	m_floatEffectActive = false;
	recalculateMode();
}

float ClientTangibleDynamics::getFloatHeight() const { return m_floatHeight; }
float ClientTangibleDynamics::getFloatDriftSpeed() const { return m_floatDriftSpeed; }

// ======================================================================
// CONVEYOR
// ======================================================================

void ClientTangibleDynamics::setConveyorEffect(const Vector& direction, float speed, float wrapDistance, float duration)
{
	Object* const owner = getOwner();

	// Normalize direction
	Vector normalizedDir = direction;
	if (normalizedDir.normalize())
	{
		m_conveyorDirection = normalizedDir;
	}
	else
	{
		m_conveyorDirection = Vector::unitZ;
	}

	m_conveyorSpeed = speed;
	m_conveyorWrapDistance = wrapDistance;
	m_conveyorOrigin = owner ? owner->getPosition_w() : Vector::zero;
	m_conveyorTravelDistance = 0.0f;
	m_conveyorDuration = duration;
	m_conveyorElapsed = 0.0f;
	m_conveyorEffectActive = true;
	recalculateMode();
}

void ClientTangibleDynamics::clearConveyorEffect()
{
	m_conveyorDirection = Vector::unitZ;
	m_conveyorSpeed = 1.0f;
	m_conveyorWrapDistance = 0.0f;
	m_conveyorOrigin = Vector::zero;
	m_conveyorTravelDistance = 0.0f;
	m_conveyorDuration = -1.0f;
	m_conveyorElapsed = 0.0f;
	m_conveyorEffectActive = false;
	recalculateMode();
}

Vector ClientTangibleDynamics::getConveyorDirection() const    { return m_conveyorDirection; }
float  ClientTangibleDynamics::getConveyorSpeed() const        { return m_conveyorSpeed; }
float  ClientTangibleDynamics::getConveyorWrapDistance() const { return m_conveyorWrapDistance; }

// ======================================================================
// EASING / COMBINED / QUERY
// ======================================================================

void ClientTangibleDynamics::setEasing(EaseType easeType, float easeDuration)
{
	m_easeType = easeType;
	m_easeDuration = easeDuration;
}

void ClientTangibleDynamics::setCombinedForces(const Vector& pushVelocity, const Vector& spinAngular,
	float minScale, float maxScale, float breatheSpeed, float duration)
{
	setPushForce(pushVelocity, duration, MS_world);
	setSpinForce(spinAngular, duration);
	setBreathingEffect(minScale, maxScale, breatheSpeed, duration);
}

void ClientTangibleDynamics::clearAllForces()
{
	clearPushForce();
	clearSpinForce();
	clearBreathingEffect();
	clearBounceEffect();
	clearWobbleEffect();
	clearOrbitEffect();
	clearHoverEffect();
	clearFollowTargetEffect();
	clearSwayEffect();
	clearShakeEffect();
	clearFloatEffect();
	clearConveyorEffect();
}

int  ClientTangibleDynamics::getActiveForceMask() const          { return m_activeForceMask; }
bool ClientTangibleDynamics::isActive() const                    { return m_activeForceMask != FM_none; }
bool ClientTangibleDynamics::isForceActive(ForceMode mode) const { return (m_activeForceMask & static_cast<int>(mode)) != 0; }

// ======================================================================
// ALTER
// ======================================================================

float ClientTangibleDynamics::alter(float elapsedTime)
{
	float const baseAlterResult = Dynamics::alter(elapsedTime);

	if (m_activeForceMask != FM_none)
	{
		if (m_pushForceActive)         updatePushForce(elapsedTime);
		if (m_spinForceActive)         updateSpinForce(elapsedTime);
		if (m_breathingEffectActive)   updateBreathingEffect(elapsedTime);
		if (m_bounceEffectActive)      updateBounceEffect(elapsedTime);
		if (m_wobbleEffectActive)      updateWobbleEffect(elapsedTime);
		if (m_orbitEffectActive)       updateOrbitEffect(elapsedTime);
		if (m_hoverEffectActive)       updateHoverEffect(elapsedTime);
		if (m_followTargetEffectActive) updateFollowTargetEffect(elapsedTime);
		if (m_swayEffectActive)        updateSwayEffect(elapsedTime);
		if (m_shakeEffectActive)       updateShakeEffect(elapsedTime);
		if (m_floatEffectActive)       updateFloatEffect(elapsedTime);
		if (m_conveyorEffectActive)    updateConveyorEffect(elapsedTime);
	}

	// Return cms_alterNextFrame (0.0f) to ensure we get called every frame when forces are active
	return (m_activeForceMask != FM_none) ? AlterResult::cms_alterNextFrame : baseAlterResult;
}

// ======================================================================
// UPDATE HELPERS
// ======================================================================

void ClientTangibleDynamics::updatePushForce(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearPushForce(); return; }

	if (m_pushDuration >= 0.0f)
	{
		m_pushElapsed += elapsedTime;
		if (m_pushElapsed >= m_pushDuration) { clearPushForce(); return; }
	}

	float const ease = computeEaseFactor(m_easeType, m_pushElapsed, m_pushDuration, m_easeDuration);

	if (m_pushDrag > 0.0f)
	{
		float const dragFactor = exp(-m_pushDrag * elapsedTime);
		m_pushVelocity *= dragFactor;
		// Soft termination when velocity drops below ~0.1 m/s
		if (m_pushVelocity.magnitudeSquared() < 0.01f) { clearPushForce(); return; }
	}

	Vector const vel = m_pushVelocity * ease;

	// Skip complex physics if inside a building cell
	CellProperty const * const cell = owner->getParentCell();
	bool const insideBuilding = (cell && cell != CellProperty::getWorldCellProperty());

	if (m_pushSpace == MS_world && !insideBuilding)
	{
		// Hockey puck mode: terrain following and collision
		Vector const currentPos = owner->getPosition_w();
		Vector nextPos = currentPos + vel * elapsedTime;

		// ========================================
		// TERRAIN FOLLOWING
		// ========================================
		TerrainObject const * const terrain = TerrainObject::getConstInstance();
		if (terrain)
		{
			float terrainHeight = currentPos.y;
			if (terrain->getHeight(nextPos, terrainHeight))
			{
				float const groundOffset = 0.05f;
				nextPos.y = terrainHeight + groundOffset;
			}
		}

		// ========================================
		// COLLISION DETECTION (RICOCHET)
		// ========================================
		float const objectRadius = 0.5f;
		CanMoveResult const moveResult = CollisionWorld::canMove(
			owner,
			nextPos,
			objectRadius,
			false,    // checkY
			false,    // checkFlora
			false     // checkFauna
		);

		if (moveResult != CMR_MoveOK)
		{
			// Hit something - find obstacle and ricochet
			Object const * hitObject = nullptr;
			bool const foundObstacle = CollisionWorld::findFirstObstacle(
				owner,
				objectRadius,
				currentPos,
				nextPos,
				true,     // testStatics
				false,    // testCreatures
				hitObject
			);

			if (foundObstacle && hitObject)
			{
				// Calculate reflection
				Vector const toObstacle = hitObject->getPosition_w() - currentPos;
				Vector normal(toObstacle.x, 0.0f, toObstacle.z);
				if (normal.normalize())
				{
					normal = -normal;
					float const dot = m_pushVelocity.x * normal.x + m_pushVelocity.z * normal.z;
					m_pushVelocity.x = m_pushVelocity.x - 2.0f * dot * normal.x;
					m_pushVelocity.z = m_pushVelocity.z - 2.0f * dot * normal.z;
					m_pushVelocity *= 0.8f; // Energy loss

					// Push away from wall
					nextPos = currentPos + normal * (objectRadius * 0.5f);

					// Re-apply terrain height
					if (terrain)
					{
						float terrainHeight = nextPos.y;
						if (terrain->getHeight(nextPos, terrainHeight))
						{
							nextPos.y = terrainHeight + 0.05f;
						}
					}
				}
			}
			else
			{
				// Unknown obstacle - reverse and lose energy
				m_pushVelocity = -m_pushVelocity * 0.5f;
				return;
			}
		}

		owner->setPosition_w(nextPos);
	}
	else
	{
		// Standard movement (inside building or non-world space)
		switch (m_pushSpace)
		{
		case MS_world:
			owner->move_o(owner->rotate_w2o(vel * elapsedTime));
			break;
		case MS_parent:
			owner->move_o(owner->rotate_p2o(vel * elapsedTime));
			break;
		case MS_object:
			owner->move_o(vel * elapsedTime);
			break;
		}
	}
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateSpinForce(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearSpinForce(); return; }

	if (m_spinDuration >= 0.0f)
	{
		m_spinElapsed += elapsedTime;
		if (m_spinElapsed >= m_spinDuration) { clearSpinForce(); return; }
	}

	float const ease = computeEaseFactor(m_easeType, m_spinElapsed, m_spinDuration, m_easeDuration);
	Vector const angular = m_spinAngular * ease;

	if (m_spinAroundAppearanceCenter)
	{
		Transform t = owner->getTransform_o2p();
		t.move_l(owner->getAppearanceSphereCenter());
		t.yaw_l(angular.x * elapsedTime);
		t.pitch_l(angular.y * elapsedTime);
		t.roll_l(angular.z * elapsedTime);
		t.move_l(-owner->getAppearanceSphereCenter());
		owner->setTransform_o2p(t);
	}
	else
	{
		owner->yaw_o(angular.x * elapsedTime);
		owner->pitch_o(angular.y * elapsedTime);
		owner->roll_o(angular.z * elapsedTime);
	}
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateBreathingEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearBreathingEffect(); return; }

	if (m_breathingDuration >= 0.0f)
	{
		m_breathingElapsed += elapsedTime;
		if (m_breathingElapsed >= m_breathingDuration) { clearBreathingEffect(); return; }
	}

	float const ease = computeEaseFactor(m_easeType, m_breathingElapsed, m_breathingDuration, m_easeDuration);

	m_breathingPhase += elapsedTime * m_breathingSpeed;
	float const halfRange = (m_breathingMax - m_breathingMin) * 0.5f;
	float const midpoint  = (m_breathingMax + m_breathingMin) * 0.5f;
	float const rawScale  = midpoint + halfRange * cos(m_breathingPhase * PI_TIMES_2);
	float const effectiveScale = 1.0f + (rawScale - 1.0f) * ease;

	owner->setRecursiveScale(m_baseScale * effectiveScale);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateBounceEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearBounceEffect(); return; }

	if (m_bounceDuration >= 0.0f)
	{
		m_bounceElapsed += elapsedTime;
		if (m_bounceElapsed >= m_bounceDuration) { clearBounceEffect(); return; }
	}

	m_bounceVerticalVelocity -= m_bounceGravity * elapsedTime;

	Vector pos = owner->getPosition_w();
	pos.y += m_bounceVerticalVelocity * elapsedTime;

	if (pos.y <= m_bounceFloorY)
	{
		pos.y = m_bounceFloorY;
		m_bounceVerticalVelocity = -m_bounceVerticalVelocity * m_bounceElasticity;
		if (fabs(m_bounceVerticalVelocity) < s_bounceMinVelocity) { clearBounceEffect(); return; }
	}

	owner->setPosition_w(pos);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateWobbleEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearWobbleEffect(); return; }

	if (m_wobbleDuration >= 0.0f)
	{
		m_wobbleElapsed += elapsedTime;
		if (m_wobbleElapsed >= m_wobbleDuration) { clearWobbleEffect(); return; }
	}

	float const ease = computeEaseFactor(m_easeType, m_wobbleElapsed, m_wobbleDuration, m_easeDuration);
	m_wobblePhase += elapsedTime;

	Vector pos = m_wobbleOrigin;
	pos.x += m_wobbleAmplitude.x * sin(m_wobbleFrequency.x * m_wobblePhase * PI_TIMES_2) * ease;
	pos.y += m_wobbleAmplitude.y * sin(m_wobbleFrequency.y * m_wobblePhase * PI_TIMES_2) * ease;
	pos.z += m_wobbleAmplitude.z * sin(m_wobbleFrequency.z * m_wobblePhase * PI_TIMES_2) * ease;

	owner->setPosition_w(pos);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateOrbitEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearOrbitEffect(); return; }

	if (m_orbitDuration >= 0.0f)
	{
		m_orbitElapsed += elapsedTime;
		if (m_orbitElapsed >= m_orbitDuration) { clearOrbitEffect(); return; }
	}

	float const ease = computeEaseFactor(m_easeType, m_orbitElapsed, m_orbitDuration, m_easeDuration);
	m_orbitAngle += m_orbitSpeed * elapsedTime * ease;
	float const effectiveRadius = m_orbitRadius * ease;

	Vector pos;
	pos.x = m_orbitCenter.x + sin(m_orbitAngle) * effectiveRadius;
	pos.y = owner->getPosition_w().y;
	pos.z = m_orbitCenter.z + cos(m_orbitAngle) * effectiveRadius;

	owner->setPosition_w(pos);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateHoverEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearHoverEffect(); return; }

	if (m_hoverDuration >= 0.0f)
	{
		m_hoverElapsed += elapsedTime;
		if (m_hoverElapsed >= m_hoverDuration) { clearHoverEffect(); return; }
	}

	// Update bob phase continuously for smooth animation
	m_hoverBobPhase += elapsedTime * m_hoverBobSpeed;

	// Get current position
	Vector pos = owner->getPosition_w();

	// Get terrain height at current XZ position
	TerrainObject const * const terrain = TerrainObject::getConstInstance();
	if (terrain)
	{
		float terrainHeight = 0.0f;
		if (terrain->getHeight(pos, terrainHeight))
		{
			// Calculate target Y with hover height and smooth bob
			float const bob = m_hoverBobAmplitude * sin(m_hoverBobPhase * PI_TIMES_2);
			float const targetY = terrainHeight + m_hoverHeight + bob;

			// Ultra-smooth interpolation for Y position
			float const smoothFactor = 12.0f;  // Higher for hover since it's just Y axis
			float const lerpFactor = 1.0f - exp(-smoothFactor * elapsedTime);
			pos.y = pos.y + (targetY - pos.y) * lerpFactor;

			owner->setPosition_w(pos);
		}
	}
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateFollowTargetEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearFollowTargetEffect(); return; }

	if (m_followDuration >= 0.0f)
	{
		m_followElapsed += elapsedTime;
		if (m_followElapsed >= m_followDuration) { clearFollowTargetEffect(); return; }
	}

	// Get target object
	if (m_followTargetId == 0) { clearFollowTargetEffect(); return; }

	Object const * const target = NetworkIdManager::getObjectById(NetworkId(static_cast<NetworkId::NetworkIdType>(m_followTargetId)));
	if (!target) { clearFollowTargetEffect(); return; }

	// Update bob phase continuously for smooth animation
	m_followBobPhase += elapsedTime * 1.0f;

	Vector const targetPos = target->getPosition_w();
	Vector const ownerPos = owner->getPosition_w();

	// Get target's facing direction
	Transform const & targetTransform = target->getTransform_o2w();
	Vector const targetFacing = targetTransform.getLocalFrameK_p();

	// Calculate desired position behind the target at follow distance
	Vector desiredPos;
	desiredPos.x = targetPos.x - targetFacing.x * m_followDistance;
	desiredPos.z = targetPos.z - targetFacing.z * m_followDistance;

	// Apply hover height with smooth bob
	TerrainObject const * const terrain = TerrainObject::getConstInstance();
	float terrainHeight = targetPos.y;
	if (terrain)
	{
		terrain->getHeight(desiredPos, terrainHeight);
	}
	float const bob = m_followBobAmplitude * sin(m_followBobPhase * PI_TIMES_2);
	desiredPos.y = terrainHeight + m_followHoverHeight + bob;

	// Ultra-smooth exponential interpolation
	// Lower smoothFactor = smoother but more lag, higher = snappier but can feel jerky
	// 5.0 provides a nice balance of smooth and responsive
	float const positionSmoothFactor = 5.0f;
	float const rotationSmoothFactor = 8.0f;

	float const posLerpFactor = 1.0f - exp(-positionSmoothFactor * elapsedTime);
	float const rotLerpFactor = 1.0f - exp(-rotationSmoothFactor * elapsedTime);

	// Smooth position interpolation
	Vector newPos;
	newPos.x = ownerPos.x + (desiredPos.x - ownerPos.x) * posLerpFactor;
	newPos.y = ownerPos.y + (desiredPos.y - ownerPos.y) * posLerpFactor;
	newPos.z = ownerPos.z + (desiredPos.z - ownerPos.z) * posLerpFactor;

	owner->setPosition_w(newPos);

	// Smooth rotation interpolation toward target's facing
	Transform ownerTransform = owner->getTransform_o2w();
	Vector const currentFacing = ownerTransform.getLocalFrameK_p();

	// Interpolate facing direction
	Vector newFacing;
	newFacing.x = currentFacing.x + (targetFacing.x - currentFacing.x) * rotLerpFactor;
	newFacing.y = 0.0f;
	newFacing.z = currentFacing.z + (targetFacing.z - currentFacing.z) * rotLerpFactor;

	// Normalize and apply rotation
	float const facingMag = sqrt(newFacing.x * newFacing.x + newFacing.z * newFacing.z);
	if (facingMag > 0.001f)
	{
		newFacing.x /= facingMag;
		newFacing.z /= facingMag;
		ownerTransform.setLocalFrameKJ_p(newFacing, Vector::unitY);
		owner->setTransform_o2w(ownerTransform);
	}
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateSwayEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearSwayEffect(); return; }

	if (m_swayDuration >= 0.0f)
	{
		m_swayElapsed += elapsedTime;
		if (m_swayElapsed >= m_swayDuration) { clearSwayEffect(); return; }
	}

	// Update phase
	m_swayPhase += elapsedTime * m_swaySpeed;

	// Calculate current angle with damping
	float currentAngle = m_swayAngle;
	if (m_swayDamping > 0.0f)
	{
		currentAngle *= exp(-m_swayDamping * m_swayElapsed);
	}

	// Apply pendulum swing (roll on Z axis)
	float const swing = currentAngle * sin(m_swayPhase * PI_TIMES_2);
	owner->roll_o(swing * elapsedTime);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateShakeEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearShakeEffect(); return; }

	if (m_shakeDuration >= 0.0f)
	{
		m_shakeElapsed += elapsedTime;
		if (m_shakeElapsed >= m_shakeDuration) { clearShakeEffect(); return; }
	}

	// Update phase
	m_shakePhase += elapsedTime * m_shakeFrequency;

	// Calculate random-ish offset using multiple sine waves for chaos
	float const offsetX = m_shakeIntensity * sin(m_shakePhase * PI_TIMES_2);
	float const offsetY = m_shakeIntensity * sin(m_shakePhase * PI_TIMES_2 * 1.7f + 0.5f) * 0.5f;
	float const offsetZ = m_shakeIntensity * sin(m_shakePhase * PI_TIMES_2 * 2.3f + 1.0f);

	Vector newPos = m_shakeOrigin;
	newPos.x += offsetX;
	newPos.y += offsetY;
	newPos.z += offsetZ;

	owner->setPosition_w(newPos);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateFloatEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearFloatEffect(); return; }

	if (m_floatDuration >= 0.0f)
	{
		m_floatElapsed += elapsedTime;
		if (m_floatElapsed >= m_floatDuration) { clearFloatEffect(); return; }
	}

	// Update phase
	m_floatPhase += elapsedTime * m_floatDriftSpeed;

	// Calculate vertical float with smooth sine wave
	float const yOffset = m_floatHeight * 0.5f * (1.0f + sin(m_floatPhase * PI_TIMES_2));

	// Calculate subtle horizontal drift
	float const xOffset = m_floatRandomStrength * sin(m_floatPhase * PI_TIMES_2 * 0.7f);
	float const zOffset = m_floatRandomStrength * sin(m_floatPhase * PI_TIMES_2 * 0.9f + 0.5f);

	Vector newPos = m_floatOrigin;
	newPos.x += xOffset;
	newPos.y += yOffset;
	newPos.z += zOffset;

	owner->setPosition_w(newPos);
}

// ----------------------------------------------------------------------

void ClientTangibleDynamics::updateConveyorEffect(float elapsedTime)
{
	Object* const owner = getOwner();
	if (owner == NULL) { clearConveyorEffect(); return; }

	if (m_conveyorDuration >= 0.0f)
	{
		m_conveyorElapsed += elapsedTime;
		if (m_conveyorElapsed >= m_conveyorDuration) { clearConveyorEffect(); return; }
	}

	// Update travel distance
	float const distanceDelta = m_conveyorSpeed * elapsedTime;
	m_conveyorTravelDistance += distanceDelta;

	// Handle wrap-around if wrapDistance is set
	if (m_conveyorWrapDistance > 0.0f && m_conveyorTravelDistance >= m_conveyorWrapDistance)
	{
		// Reset travel distance (wraps back to origin)
		m_conveyorTravelDistance = fmod(m_conveyorTravelDistance, m_conveyorWrapDistance);
	}

	// Calculate new position along conveyor path
	Vector newPos;
	newPos.x = m_conveyorOrigin.x + m_conveyorDirection.x * m_conveyorTravelDistance;
	newPos.y = m_conveyorOrigin.y + m_conveyorDirection.y * m_conveyorTravelDistance;
	newPos.z = m_conveyorOrigin.z + m_conveyorDirection.z * m_conveyorTravelDistance;

	owner->setPosition_w(newPos);
}

// ======================================================================
// RECALCULATE
// ======================================================================

void ClientTangibleDynamics::recalculateMode()
{
	m_activeForceMask = FM_none;
	if (m_pushForceActive)         m_activeForceMask |= FM_push;
	if (m_spinForceActive)         m_activeForceMask |= FM_spin;
	if (m_breathingEffectActive)   m_activeForceMask |= FM_breathing;
	if (m_bounceEffectActive)      m_activeForceMask |= FM_bounce;
	if (m_wobbleEffectActive)      m_activeForceMask |= FM_wobble;
	if (m_orbitEffectActive)       m_activeForceMask |= FM_orbit;
	if (m_hoverEffectActive)       m_activeForceMask |= FM_hover;
	if (m_followTargetEffectActive) m_activeForceMask |= FM_followTarget;
	if (m_swayEffectActive)        m_activeForceMask |= FM_sway;
	if (m_shakeEffectActive)       m_activeForceMask |= FM_shake;
	if (m_floatEffectActive)       m_activeForceMask |= FM_float;
	if (m_conveyorEffectActive)    m_activeForceMask |= FM_conveyor;
}

// ======================================================================
