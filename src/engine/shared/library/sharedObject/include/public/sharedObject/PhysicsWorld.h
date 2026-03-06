// ======================================================================
//
// PhysicsWorld.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Centralized physics management with fixed timestep simulation
// and transform interpolation for smooth rendering.
//
// ======================================================================

#ifndef INCLUDED_PhysicsWorld_H
#define INCLUDED_PhysicsWorld_H

// ======================================================================

#include "sharedMath/Vector.h"
#include "sharedMath/Transform.h"

#include <vector>

// ======================================================================

class Dynamics;
class Object;

// ======================================================================

/**
 * PhysicsWorld manages all physics simulation with a fixed timestep.
 *
 * Key features:
 * - Fixed 60Hz physics update rate for deterministic simulation
 * - Accumulator-based timing for frame-rate independence
 * - Transform interpolation for smooth rendering at any frame rate
 * - Velocity Verlet integration for improved accuracy
 * - Sleep states for static/resting objects
 * - Spatial partitioning for broad-phase collision
 *
 * Usage:
 *   PhysicsWorld::update(frameTime);  // Call each frame
 *   PhysicsWorld::getInterpolatedTransform(object, alpha);  // For rendering
 */
class PhysicsWorld
{
public:
	// Physics body types
	enum BodyType
	{
		BT_static,        // Never moves (buildings, terrain)
		BT_kinematic,     // Moves but not affected by forces (animated platforms)
		BT_dynamic        // Full physics simulation
	};

	// Sleep states
	enum SleepState
	{
		SS_awake,         // Active simulation
		SS_sleeping,      // At rest, skip simulation
		SS_frozen         // Permanently disabled
	};

	// Physics material
	struct PhysicsMaterial
	{
		float friction;        // 0 = frictionless, 1 = maximum
		float restitution;     // Bounciness: 0 = no bounce, 1 = perfect bounce
		float linearDamping;   // Air resistance for linear motion
		float angularDamping;  // Air resistance for rotation

		PhysicsMaterial()
			: friction(0.5f)
			, restitution(0.3f)
			, linearDamping(0.01f)
			, angularDamping(0.05f)
		{}
	};

	// Physics body state (stored per-object)
	struct BodyState
	{
		Vector position;
		Vector previousPosition;
		Vector velocity;
		Vector acceleration;
		Vector angularVelocity;
		float  mass;
		float  inverseMass;
		BodyType bodyType;
		SleepState sleepState;
		PhysicsMaterial material;
		float sleepTimer;         // Time at low velocity before sleeping
		bool  gravityEnabled;

		BodyState()
			: mass(1.0f)
			, inverseMass(1.0f)
			, bodyType(BT_dynamic)
			, sleepState(SS_awake)
			, sleepTimer(0.0f)
			, gravityEnabled(true)
		{}
	};

	// Collision pair for narrow-phase
	struct CollisionPair
	{
		Object * objectA;
		Object * objectB;
		Vector contactPoint;
		Vector contactNormal;
		float penetrationDepth;
	};

public:
	static void install();
	static void remove();

	// Main update - call once per frame with frame delta time
	static void update(float frameTime);

	// Interpolation for rendering
	static float getInterpolationAlpha();
	static Transform getInterpolatedTransform(Object const * object);
	static Vector getInterpolatedPosition(Object const * object);

	// Body management
	static void registerBody(Object * object, BodyType type = BT_dynamic);
	static void unregisterBody(Object * object);
	static bool hasBody(Object const * object);

	// Body state access
	static BodyState * getBodyState(Object * object);
	static BodyState const * getBodyState(Object const * object);

	// Force application
	static void applyForce(Object * object, Vector const & force);
	static void applyImpulse(Object * object, Vector const & impulse);
	static void applyTorque(Object * object, Vector const & torque);
	static void applyForceAtPoint(Object * object, Vector const & force, Vector const & point);

	// Velocity control
	static void setVelocity(Object * object, Vector const & velocity);
	static Vector getVelocity(Object const * object);
	static void setAngularVelocity(Object * object, Vector const & angularVelocity);

	// Sleep control
	static void wake(Object * object);
	static void sleep(Object * object);
	static void freeze(Object * object);
	static bool isSleeping(Object const * object);

	// Gravity
	static void setGravity(Vector const & gravity);
	static Vector const & getGravity();
	static void setObjectGravityEnabled(Object * object, bool enabled);

	// Configuration
	static void setFixedTimestep(float timestep);
	static float getFixedTimestep();
	static void setMaxSubsteps(int maxSubsteps);
	static void setSleepThresholds(float linearThreshold, float angularThreshold, float timeThreshold);

	// Statistics
	static int  getBodyCount();
	static int  getAwakeBodyCount();
	static int  getCollisionPairCount();
	static float getLastSimulationTime();
	static void resetStatistics();

private:
	PhysicsWorld();
	~PhysicsWorld();
	PhysicsWorld(PhysicsWorld const &);
	PhysicsWorld & operator=(PhysicsWorld const &);

	// Internal simulation steps
	static void stepSimulation(float fixedDeltaTime);
	static void integrateVelocityVerlet(float dt);
	static void broadPhaseCollision();
	static void narrowPhaseCollision();
	static void resolveCollisions();
	static void updateSleepStates(float dt);
	static void syncTransforms();
};

// ======================================================================

#endif

