// ======================================================================
//
// PhysicsWorld.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Centralized physics management with fixed timestep simulation
//
// ======================================================================

#include "sharedObject/FirstSharedObject.h"
#include "sharedObject/PhysicsWorld.h"

#include "sharedObject/Object.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/Dynamics.h"
#include "sharedCollision/CollisionWorld.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedMath/AxialBox.h"
#include "sharedMath/Sphere.h"

#include <algorithm>
#include <map>

// ======================================================================

namespace PhysicsWorldNamespace
{
	// Configuration constants
	float const cs_defaultFixedTimestep = 1.0f / 60.0f;  // 60 Hz physics
	int   const cs_defaultMaxSubsteps = 4;                // Max physics steps per frame
	float const cs_defaultLinearSleepThreshold = 0.01f;   // m/s
	float const cs_defaultAngularSleepThreshold = 0.01f;  // rad/s
	float const cs_defaultSleepTimeThreshold = 0.5f;      // seconds

	// State
	bool ms_installed = false;

	// Timing
	float ms_fixedTimestep = cs_defaultFixedTimestep;
	float ms_accumulator = 0.0f;
	float ms_interpolationAlpha = 0.0f;
	int   ms_maxSubsteps = cs_defaultMaxSubsteps;

	// Gravity
	Vector ms_gravity(0.0f, -9.81f, 0.0f);

	// Sleep thresholds
	float ms_linearSleepThreshold = cs_defaultLinearSleepThreshold;
	float ms_angularSleepThreshold = cs_defaultAngularSleepThreshold;
	float ms_sleepTimeThreshold = cs_defaultSleepTimeThreshold;

	// Body storage
	typedef std::map<Object const *, PhysicsWorld::BodyState> BodyMap;
	BodyMap ms_bodies;

	// Collision pairs (populated during broad phase)
	std::vector<PhysicsWorld::CollisionPair> ms_collisionPairs;

	// Previous frame transforms for interpolation
	typedef std::map<Object const *, Transform> TransformMap;
	TransformMap ms_previousTransforms;
	TransformMap ms_currentTransforms;

	// Statistics
	int   ms_awakeBodyCount = 0;
	float ms_lastSimulationTime = 0.0f;

	// Debug
#ifdef _DEBUG
	bool ms_debugDrawEnabled = false;
	bool ms_showSleepingBodies = false;
#endif
}

using namespace PhysicsWorldNamespace;

// ======================================================================

void PhysicsWorld::install()
{
	DEBUG_FATAL(ms_installed, ("PhysicsWorld already installed"));

	ms_bodies.clear();
	ms_collisionPairs.clear();
	ms_previousTransforms.clear();
	ms_currentTransforms.clear();

	ms_accumulator = 0.0f;
	ms_interpolationAlpha = 0.0f;

	ms_installed = true;
	ExitChain::add(remove, "PhysicsWorld::remove");

	DEBUG_REPORT_LOG(true, ("PhysicsWorld installed: timestep=%.4f, maxSubsteps=%d\n",
		ms_fixedTimestep, ms_maxSubsteps));
}

// ----------------------------------------------------------------------

void PhysicsWorld::remove()
{
	DEBUG_FATAL(!ms_installed, ("PhysicsWorld not installed"));

	ms_bodies.clear();
	ms_collisionPairs.clear();
	ms_previousTransforms.clear();
	ms_currentTransforms.clear();

	ms_installed = false;
}

// ----------------------------------------------------------------------

void PhysicsWorld::update(float frameTime)
{
	if (!ms_installed)
		return;

	PerformanceTimer timer;
	timer.start();

	// Clamp frame time to prevent spiral of death
	float const maxFrameTime = ms_fixedTimestep * ms_maxSubsteps;
	float const clampedFrameTime = std::min(frameTime, maxFrameTime);

	ms_accumulator += clampedFrameTime;

	// Store previous transforms for interpolation
	for (BodyMap::const_iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		Object const * const object = it->first;
		if (object)
		{
			ms_previousTransforms[object] = object->getTransform_o2w();
		}
	}

	// Fixed timestep simulation
	int substeps = 0;
	while (ms_accumulator >= ms_fixedTimestep && substeps < ms_maxSubsteps)
	{
		stepSimulation(ms_fixedTimestep);
		ms_accumulator -= ms_fixedTimestep;
		++substeps;
	}

	// Store current transforms for interpolation
	for (BodyMap::const_iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		Object const * const object = it->first;
		if (object)
		{
			ms_currentTransforms[object] = object->getTransform_o2w();
		}
	}

	// Calculate interpolation alpha for rendering
	ms_interpolationAlpha = ms_accumulator / ms_fixedTimestep;

	timer.stop();
	ms_lastSimulationTime = timer.getElapsedTime();
}

// ----------------------------------------------------------------------

float PhysicsWorld::getInterpolationAlpha()
{
	return ms_interpolationAlpha;
}

// ----------------------------------------------------------------------

Transform PhysicsWorld::getInterpolatedTransform(Object const * object)
{
	if (!object)
		return Transform::identity;

	TransformMap::const_iterator prevIt = ms_previousTransforms.find(object);
	TransformMap::const_iterator currIt = ms_currentTransforms.find(object);

	if (prevIt == ms_previousTransforms.end() || currIt == ms_currentTransforms.end())
		return object->getTransform_o2w();

	Transform const & prev = prevIt->second;
	Transform const & curr = currIt->second;

	// Interpolate position
	Vector const position = prev.getPosition_p() +
		(curr.getPosition_p() - prev.getPosition_p()) * ms_interpolationAlpha;

	// For rotation, we use the current rotation (simple approach)
	// A more accurate method would use quaternion slerp
	Transform result = curr;
	result.setPosition_p(position);

	return result;
}

// ----------------------------------------------------------------------

Vector PhysicsWorld::getInterpolatedPosition(Object const * object)
{
	return getInterpolatedTransform(object).getPosition_p();
}

// ----------------------------------------------------------------------

void PhysicsWorld::registerBody(Object * object, BodyType type)
{
	if (!object)
		return;

	BodyState state;
	state.position = object->getPosition_w();
	state.previousPosition = state.position;
	state.bodyType = type;
	state.mass = 1.0f;
	state.inverseMass = (type == BT_static) ? 0.0f : 1.0f;

	ms_bodies[object] = state;
}

// ----------------------------------------------------------------------

void PhysicsWorld::unregisterBody(Object * object)
{
	if (!object)
		return;

	ms_bodies.erase(object);
	ms_previousTransforms.erase(object);
	ms_currentTransforms.erase(object);
}

// ----------------------------------------------------------------------

bool PhysicsWorld::hasBody(Object const * object)
{
	return ms_bodies.find(object) != ms_bodies.end();
}

// ----------------------------------------------------------------------

PhysicsWorld::BodyState * PhysicsWorld::getBodyState(Object * object)
{
	BodyMap::iterator it = ms_bodies.find(object);
	return (it != ms_bodies.end()) ? &it->second : nullptr;
}

// ----------------------------------------------------------------------

PhysicsWorld::BodyState const * PhysicsWorld::getBodyState(Object const * object)
{
	BodyMap::const_iterator it = ms_bodies.find(object);
	return (it != ms_bodies.end()) ? &it->second : nullptr;
}

// ----------------------------------------------------------------------

void PhysicsWorld::applyForce(Object * object, Vector const & force)
{
	BodyState * const state = getBodyState(object);
	if (!state || state->bodyType != BT_dynamic || state->sleepState == SS_frozen)
		return;

	// F = ma, so a = F/m = F * inverseMass
	state->acceleration += force * state->inverseMass;

	// Wake up sleeping body
	if (state->sleepState == SS_sleeping)
		state->sleepState = SS_awake;
}

// ----------------------------------------------------------------------

void PhysicsWorld::applyImpulse(Object * object, Vector const & impulse)
{
	BodyState * const state = getBodyState(object);
	if (!state || state->bodyType != BT_dynamic || state->sleepState == SS_frozen)
		return;

	// Impulse directly changes velocity: dv = J/m = J * inverseMass
	state->velocity += impulse * state->inverseMass;

	// Wake up sleeping body
	if (state->sleepState == SS_sleeping)
		state->sleepState = SS_awake;
}

// ----------------------------------------------------------------------

void PhysicsWorld::applyTorque(Object * object, Vector const & torque)
{
	BodyState * const state = getBodyState(object);
	if (!state || state->bodyType != BT_dynamic || state->sleepState == SS_frozen)
		return;

	// Simplified: directly apply to angular velocity (should use inertia tensor)
	state->angularVelocity += torque * state->inverseMass;

	if (state->sleepState == SS_sleeping)
		state->sleepState = SS_awake;
}

// ----------------------------------------------------------------------

void PhysicsWorld::applyForceAtPoint(Object * object, Vector const & force, Vector const & point)
{
	BodyState * const state = getBodyState(object);
	if (!state || state->bodyType != BT_dynamic || state->sleepState == SS_frozen)
		return;

	// Apply linear force
	applyForce(object, force);

	// Apply torque from offset
	Vector const offset = point - state->position;
	Vector const torque = offset.cross(force);
	applyTorque(object, torque);
}

// ----------------------------------------------------------------------

void PhysicsWorld::setVelocity(Object * object, Vector const & velocity)
{
	BodyState * const state = getBodyState(object);
	if (!state)
		return;

	state->velocity = velocity;

	if (state->sleepState == SS_sleeping && velocity.magnitudeSquared() > 0.0001f)
		state->sleepState = SS_awake;
}

// ----------------------------------------------------------------------

Vector PhysicsWorld::getVelocity(Object const * object)
{
	BodyState const * const state = getBodyState(object);
	return state ? state->velocity : Vector::zero;
}

// ----------------------------------------------------------------------

void PhysicsWorld::setAngularVelocity(Object * object, Vector const & angularVelocity)
{
	BodyState * const state = getBodyState(object);
	if (state)
		state->angularVelocity = angularVelocity;
}

// ----------------------------------------------------------------------

void PhysicsWorld::wake(Object * object)
{
	BodyState * const state = getBodyState(object);
	if (state && state->sleepState != SS_frozen)
	{
		state->sleepState = SS_awake;
		state->sleepTimer = 0.0f;
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::sleep(Object * object)
{
	BodyState * const state = getBodyState(object);
	if (state && state->sleepState != SS_frozen)
		state->sleepState = SS_sleeping;
}

// ----------------------------------------------------------------------

void PhysicsWorld::freeze(Object * object)
{
	BodyState * const state = getBodyState(object);
	if (state)
	{
		state->sleepState = SS_frozen;
		state->velocity = Vector::zero;
		state->angularVelocity = Vector::zero;
	}
}

// ----------------------------------------------------------------------

bool PhysicsWorld::isSleeping(Object const * object)
{
	BodyState const * const state = getBodyState(object);
	return state && state->sleepState != SS_awake;
}

// ----------------------------------------------------------------------

void PhysicsWorld::setGravity(Vector const & gravity)
{
	ms_gravity = gravity;
}

// ----------------------------------------------------------------------

Vector const & PhysicsWorld::getGravity()
{
	return ms_gravity;
}

// ----------------------------------------------------------------------

void PhysicsWorld::setObjectGravityEnabled(Object * object, bool enabled)
{
	BodyState * const state = getBodyState(object);
	if (state)
		state->gravityEnabled = enabled;
}

// ----------------------------------------------------------------------

void PhysicsWorld::setFixedTimestep(float timestep)
{
	ms_fixedTimestep = std::max(0.001f, timestep);
}

// ----------------------------------------------------------------------

float PhysicsWorld::getFixedTimestep()
{
	return ms_fixedTimestep;
}

// ----------------------------------------------------------------------

void PhysicsWorld::setMaxSubsteps(int maxSubsteps)
{
	ms_maxSubsteps = std::max(1, maxSubsteps);
}

// ----------------------------------------------------------------------

void PhysicsWorld::setSleepThresholds(float linearThreshold, float angularThreshold, float timeThreshold)
{
	ms_linearSleepThreshold = linearThreshold;
	ms_angularSleepThreshold = angularThreshold;
	ms_sleepTimeThreshold = timeThreshold;
}

// ----------------------------------------------------------------------

int PhysicsWorld::getBodyCount()
{
	return static_cast<int>(ms_bodies.size());
}

// ----------------------------------------------------------------------

int PhysicsWorld::getAwakeBodyCount()
{
	return ms_awakeBodyCount;
}

// ----------------------------------------------------------------------

int PhysicsWorld::getCollisionPairCount()
{
	return static_cast<int>(ms_collisionPairs.size());
}

// ----------------------------------------------------------------------

float PhysicsWorld::getLastSimulationTime()
{
	return ms_lastSimulationTime;
}

// ----------------------------------------------------------------------

void PhysicsWorld::resetStatistics()
{
	ms_awakeBodyCount = 0;
	ms_lastSimulationTime = 0.0f;
}

// ----------------------------------------------------------------------

void PhysicsWorld::stepSimulation(float dt)
{
	// 1. Apply gravity and external forces
	for (BodyMap::iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		BodyState & state = it->second;

		if (state.bodyType != BT_dynamic || state.sleepState != SS_awake)
			continue;

		// Apply gravity
		if (state.gravityEnabled)
			state.acceleration += ms_gravity;
	}

	// 2. Integrate using Velocity Verlet
	integrateVelocityVerlet(dt);

	// 3. Broad-phase collision detection
	broadPhaseCollision();

	// 4. Narrow-phase collision detection
	narrowPhaseCollision();

	// 5. Resolve collisions
	resolveCollisions();

	// 6. Update sleep states
	updateSleepStates(dt);

	// 7. Sync transforms to objects
	syncTransforms();

	// Clear acceleration for next frame
	for (BodyMap::iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		it->second.acceleration = Vector::zero;
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::integrateVelocityVerlet(float dt)
{
	/**
	 * Velocity Verlet Integration:
	 *
	 * 1. x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt^2
	 * 2. Calculate a(t+dt) based on new position
	 * 3. v(t+dt) = v(t) + 0.5*(a(t) + a(t+dt))*dt
	 *
	 * This provides O(dt^4) accuracy vs O(dt^2) for Euler.
	 * For games, we approximate by using current acceleration for both.
	 */

	float const halfDtSquared = 0.5f * dt * dt;

	ms_awakeBodyCount = 0;

	for (BodyMap::iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		Object const * const object = it->first;
		BodyState & state = it->second;

		if (state.bodyType != BT_dynamic || state.sleepState != SS_awake)
			continue;

		++ms_awakeBodyCount;

		// Store previous position for collision
		state.previousPosition = state.position;

		// Position update: x += v*dt + 0.5*a*dt^2
		state.position += state.velocity * dt + state.acceleration * halfDtSquared;

		// Apply damping
		float const linearDampingFactor = 1.0f - state.material.linearDamping * dt;
		float const angularDampingFactor = 1.0f - state.material.angularDamping * dt;

		// Velocity update: v += a*dt
		state.velocity += state.acceleration * dt;
		state.velocity *= std::max(0.0f, linearDampingFactor);

		// Angular velocity damping
		state.angularVelocity *= std::max(0.0f, angularDampingFactor);
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::broadPhaseCollision()
{
	ms_collisionPairs.clear();

	// Simple O(n^2) broad phase - should use spatial partitioning for large scenes
	// TODO: Integrate with existing SpatialDatabase/SphereTree

	std::vector<Object const *> dynamicObjects;
	for (BodyMap::const_iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		if (it->second.bodyType == BT_dynamic && it->second.sleepState == SS_awake)
			dynamicObjects.push_back(it->first);
	}

	// Check dynamic vs all bodies
	for (size_t i = 0; i < dynamicObjects.size(); ++i)
	{
		Object const * const objA = dynamicObjects[i];
		BodyState const * const stateA = getBodyState(objA);
		if (!stateA)
			continue;

		for (BodyMap::const_iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
		{
			Object const * const objB = it->first;
			if (objA == objB)
				continue;

			BodyState const & stateB = it->second;

			// Get collision radius from appearance bounding sphere
			float radiusA = 1.0f;
			float radiusB = 1.0f;

			Appearance const * const appearanceA = objA->getAppearance();
			if (appearanceA)
			{
				float const r = appearanceA->getSphere().getRadius();
				if (r > 0.0f)
					radiusA = r;
			}

			Appearance const * const appearanceB = objB->getAppearance();
			if (appearanceB)
			{
				float const r = appearanceB->getSphere().getRadius();
				if (r > 0.0f)
					radiusB = r;
			}

			float const combinedRadius = radiusA + radiusB;

			Vector const delta = stateB.position - stateA->position;
			float const distanceSquared = delta.magnitudeSquared();

			if (distanceSquared < combinedRadius * combinedRadius)
			{
				CollisionPair pair;
				pair.objectA = const_cast<Object *>(objA);
				pair.objectB = const_cast<Object *>(objB);

				// Compute contact normal - normalize delta or use up vector if overlapping exactly
				if (distanceSquared > 0.0001f)
				{
					Vector normalizedDelta = delta;
					normalizedDelta.normalize();
					pair.contactNormal = normalizedDelta;
				}
				else
				{
					pair.contactNormal = Vector(0.0f, 1.0f, 0.0f);
				}

				pair.penetrationDepth = combinedRadius - sqrt(distanceSquared);
				pair.contactPoint = stateA->position + delta * 0.5f;

				ms_collisionPairs.push_back(pair);
			}
		}
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::narrowPhaseCollision()
{
	// Refine broad-phase pairs by removing false positives
	// The broad phase uses bounding spheres which are conservative.
	// Remove pairs where the actual separation exceeds a tolerance.
	for (size_t i = ms_collisionPairs.size(); i > 0; --i)
	{
		CollisionPair const & pair = ms_collisionPairs[i - 1];

		// Remove pair if penetration depth is negligible
		if (pair.penetrationDepth < 0.001f)
		{
			ms_collisionPairs.erase(ms_collisionPairs.begin() + static_cast<int>(i - 1));
		}
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::resolveCollisions()
{
	for (size_t i = 0; i < ms_collisionPairs.size(); ++i)
	{
		CollisionPair const & pair = ms_collisionPairs[i];

		BodyState * const stateA = getBodyState(pair.objectA);
		BodyState * const stateB = getBodyState(pair.objectB);

		if (!stateA || !stateB)
			continue;

		// Impulse-based collision response
		Vector const & normal = pair.contactNormal;

		// Relative velocity
		Vector const relativeVelocity = stateA->velocity - stateB->velocity;
		float const velocityAlongNormal = relativeVelocity.dot(normal);

		// Don't resolve if velocities are separating
		if (velocityAlongNormal > 0.0f)
			continue;

		// Calculate restitution (use minimum)
		float const restitution = std::min(stateA->material.restitution, stateB->material.restitution);

		// Calculate impulse scalar
		float const impulseScalar = -(1.0f + restitution) * velocityAlongNormal /
			(stateA->inverseMass + stateB->inverseMass);

		// Apply impulse
		Vector const impulse = normal * impulseScalar;

		if (stateA->bodyType == BT_dynamic)
			stateA->velocity += impulse * stateA->inverseMass;

		if (stateB->bodyType == BT_dynamic)
			stateB->velocity -= impulse * stateB->inverseMass;

		// Position correction (prevent sinking)
		float const percent = 0.8f;  // Penetration correction percent
		float const slop = 0.01f;    // Penetration allowance
		float const correctionMagnitude = std::max(pair.penetrationDepth - slop, 0.0f) * percent /
			(stateA->inverseMass + stateB->inverseMass);

		Vector const correction = normal * correctionMagnitude;

		if (stateA->bodyType == BT_dynamic)
			stateA->position -= correction * stateA->inverseMass;

		if (stateB->bodyType == BT_dynamic)
			stateB->position += correction * stateB->inverseMass;
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::updateSleepStates(float dt)
{
	for (BodyMap::iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		BodyState & state = it->second;

		if (state.bodyType != BT_dynamic || state.sleepState == SS_frozen)
			continue;

		float const linearSpeed = state.velocity.magnitude();
		float const angularSpeed = state.angularVelocity.magnitude();

		if (linearSpeed < ms_linearSleepThreshold && angularSpeed < ms_angularSleepThreshold)
		{
			state.sleepTimer += dt;

			if (state.sleepTimer >= ms_sleepTimeThreshold)
			{
				state.sleepState = SS_sleeping;
				state.velocity = Vector::zero;
				state.angularVelocity = Vector::zero;
			}
		}
		else
		{
			state.sleepTimer = 0.0f;
			state.sleepState = SS_awake;
		}
	}
}

// ----------------------------------------------------------------------

void PhysicsWorld::syncTransforms()
{
	for (BodyMap::iterator it = ms_bodies.begin(); it != ms_bodies.end(); ++it)
	{
		Object const * const constObject = it->first;
		Object * const object = const_cast<Object *>(constObject);
		BodyState const & state = it->second;

		if (!object || state.bodyType == BT_static || state.sleepState != SS_awake)
			continue;

		// Update object position
		object->setPosition_w(state.position);

		// Apply angular velocity to rotation
		// Angular velocity is stored as (yaw rate, pitch rate, roll rate) in radians/sec
		float const angularSpeed = state.angularVelocity.magnitudeSquared();
		if (angularSpeed > 0.0001f)
		{
			// Apply yaw rotation (around Y axis) as the primary rotation
			// This gives objects a spin effect from physics forces
			object->yaw_o(state.angularVelocity.y * ms_fixedTimestep);
		}
	}
}

// ======================================================================

