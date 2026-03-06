// ======================================================================
//
// LODManager.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Enhanced Level-of-Detail management with screen-space coverage,
// hysteresis, and transition support.
//
// ======================================================================

#ifndef INCLUDED_LODManager_H
#define INCLUDED_LODManager_H

// ======================================================================

#include "sharedMath/Vector.h"

// ======================================================================

class Object;
class Camera;
class Appearance;

// ======================================================================

/**
 * LODManager provides enhanced Level-of-Detail selection based on:
 *
 * - Screen-space coverage (projected size / screen size)
 * - Distance from camera
 * - Object importance/priority
 * - Hysteresis to prevent LOD thrashing
 * - Smooth transitions between LOD levels
 *
 * Features:
 * - Automatic LOD selection based on visual impact
 * - Configurable LOD bias for quality vs performance
 * - Frame budget system to limit LOD switches per frame
 * - Statistics and profiling support
 */
class LODManager
{
public:
	// LOD selection mode
	enum SelectionMode
	{
		SM_distance,           // Traditional distance-based
		SM_screenCoverage,     // Screen-space coverage based
		SM_hybrid,             // Combination of distance and coverage
		SM_importance          // Based on object importance/priority
	};

	// LOD transition mode
	enum TransitionMode
	{
		TM_immediate,          // Instant switch
		TM_fade,               // Alpha fade between LODs
		TM_morph,              // Vertex morphing (requires shader support)
		TM_dither              // Dithered transition
	};

	// LOD level info
	struct LODLevel
	{
		int   level;              // LOD level (0 = highest detail)
		float minCoverage;        // Minimum screen coverage to use this LOD
		float maxCoverage;        // Maximum screen coverage
		float minDistance;        // Minimum distance
		float maxDistance;        // Maximum distance

		LODLevel()
			: level(0)
			, minCoverage(0.0f)
			, maxCoverage(1.0f)
			, minDistance(0.0f)
			, maxDistance(1e10f)
		{}
	};

	// Per-object LOD state
	struct ObjectLODState
	{
		int   currentLOD;         // Current LOD level
		int   targetLOD;          // Target LOD level (during transition)
		float transitionProgress; // 0.0 to 1.0 during transition
		float lastCoverage;       // Last calculated screen coverage
		float lastDistance;       // Last calculated distance
		float hysteresisTimer;    // Time at current LOD (for hysteresis)
		int   frameLastUpdated;   // Frame number of last update
		bool  inTransition;       // Currently transitioning between LODs

		ObjectLODState()
			: currentLOD(0)
			, targetLOD(0)
			, transitionProgress(1.0f)
			, lastCoverage(0.0f)
			, lastDistance(0.0f)
			, hysteresisTimer(0.0f)
			, frameLastUpdated(-1)
			, inTransition(false)
		{}
	};

	// Statistics
	struct Statistics
	{
		int objectsProcessed;      // Objects evaluated this frame
		int lodSwitches;           // LOD changes this frame
		int transitionsActive;     // Active LOD transitions
		int objectsCulled;         // Objects below minimum coverage
		float averageLOD;          // Average LOD level
		float totalProcessingTime; // Time spent on LOD calculations

		void reset();
	};

public:
	static void install();
	static void remove();

	// Frame management
	static void beginFrame(Camera const * camera);
	static void endFrame();
	static void update(float deltaTime);

	// LOD calculation
	static int  selectLOD(Object const * object);
	static int  selectLOD(Object const * object, Appearance const * appearance);
	static int  selectLODForPosition(Vector const & position, float boundingRadius, int maxLOD);

	// Screen coverage calculation
	static float calculateScreenCoverage(Vector const & position, float boundingRadius);
	static float calculateScreenCoverage(Object const * object);

	// LOD state access
	static ObjectLODState * getObjectState(Object const * object);
	static void clearObjectState(Object const * object);

	// Configuration
	static void setSelectionMode(SelectionMode mode);
	static SelectionMode getSelectionMode();

	static void setTransitionMode(TransitionMode mode);
	static TransitionMode getTransitionMode();

	static void setLODBias(float bias);  // -1.0 = lower quality, +1.0 = higher quality
	static float getLODBias();

	static void setHysteresisTime(float seconds);
	static float getHysteresisTime();

	static void setHysteresisDistance(float percent);  // e.g., 0.1 = 10% distance band
	static float getHysteresisDistance();

	static void setTransitionDuration(float seconds);
	static float getTransitionDuration();

	static void setMinimumCoverage(float coverage);  // Below this = culled
	static float getMinimumCoverage();

	static void setMaxLODSwitchesPerFrame(int max);
	static int getMaxLODSwitchesPerFrame();

	// Distance thresholds (for distance-based mode)
	static void setLODDistances(float const * distances, int count);
	static void setLODDistance(int level, float distance);
	static float getLODDistance(int level);

	// Coverage thresholds (for coverage-based mode)
	static void setLODCoverages(float const * coverages, int count);
	static void setLODCoverage(int level, float coverage);
	static float getLODCoverage(int level);

	// Statistics
	static Statistics const & getStatistics();
	static void resetStatistics();

private:
	LODManager();
	~LODManager();
	LODManager(LODManager const &);
	LODManager & operator=(LODManager const &);

	static int  selectLODByDistance(float distance, int maxLOD);
	static int  selectLODByCoverage(float coverage, int maxLOD);
	static int  selectLODHybrid(float distance, float coverage, int maxLOD);
	static bool shouldSwitchLOD(ObjectLODState const & state, int newLOD);
	static void updateTransitions(float deltaTime);
};

// ======================================================================

#endif

