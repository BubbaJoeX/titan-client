// ======================================================================
//
// LODManager.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Enhanced Level-of-Detail management implementation
//
// ======================================================================

#include "clientObject/FirstClientObject.h"
#include "clientObject/LODManager.h"

#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientObject/DetailAppearance.h"
#include "sharedObject/Object.h"
#include "sharedObject/Appearance.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/ConfigFile.h"

#include <algorithm>
#include <map>
#include <vector>

// ======================================================================

namespace LODManagerNamespace
{
	// Configuration defaults
	int   const cs_maxLODLevels = 8;
	float const cs_defaultHysteresisTime = 0.25f;       // 250ms minimum time at LOD
	float const cs_defaultHysteresisDistance = 0.15f;   // 15% distance band
	float const cs_defaultTransitionDuration = 0.3f;    // 300ms transition
	float const cs_defaultMinimumCoverage = 0.001f;     // 0.1% screen coverage minimum
	int   const cs_defaultMaxSwitchesPerFrame = 20;

	// Default distance thresholds (in meters)
	float const cs_defaultDistances[cs_maxLODLevels] = {
		25.0f, 50.0f, 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f
	};

	// Default coverage thresholds (as fraction of screen)
	float const cs_defaultCoverages[cs_maxLODLevels] = {
		0.25f, 0.1f, 0.04f, 0.015f, 0.005f, 0.002f, 0.001f, 0.0005f
	};

	// State
	bool ms_installed = false;
	bool ms_inFrame = false;
	int  ms_frameNumber = 0;
	int  ms_lastAutoInitFrame = -1;  // Track last auto-init to prevent repeated init

	// Current camera info
	Camera const * ms_camera = nullptr;
	Vector ms_cameraPosition;
	float ms_screenWidth = 1920.0f;
	float ms_screenHeight = 1080.0f;
	float ms_tanHalfFOV = 1.0f;

	// Configuration
	LODManager::SelectionMode ms_selectionMode = LODManager::SM_hybrid;
	LODManager::TransitionMode ms_transitionMode = LODManager::TM_immediate;
	float ms_lodBias = 0.0f;
	float ms_hysteresisTime = cs_defaultHysteresisTime;
	float ms_hysteresisDistance = cs_defaultHysteresisDistance;
	float ms_transitionDuration = cs_defaultTransitionDuration;
	float ms_minimumCoverage = cs_defaultMinimumCoverage;
	int   ms_maxSwitchesPerFrame = cs_defaultMaxSwitchesPerFrame;
	int   ms_switchesThisFrame = 0;

	// LOD thresholds
	std::vector<float> ms_lodDistances;
	std::vector<float> ms_lodCoverages;

	// Per-object state
	typedef std::map<Object const *, LODManager::ObjectLODState> ObjectStateMap;
	ObjectStateMap ms_objectStates;

	// Statistics
	LODManager::Statistics ms_stats;

	// Timer
	PerformanceTimer ms_timer;

	// Helper function to get LOD level count from an appearance
	int getAppearanceLODCount(Appearance const * appearance)
	{
		if (!appearance)
			return 1;

		// Check if this is a DetailAppearance (has multiple LOD levels)
		DetailAppearance const * const detailAppearance = appearance->asDetailAppearance();
		if (detailAppearance)
		{
			int const count = detailAppearance->getDetailLevelCount();
			return (count > 0) ? count : 1;
		}

		// Non-detail appearances have only one LOD level
		return 1;
	}

	// Helper function to get current LOD from appearance
	int getAppearanceCurrentLOD(Appearance const * appearance)
	{
		if (!appearance)
			return 0;

		DetailAppearance const * const detailAppearance = appearance->asDetailAppearance();
		if (detailAppearance)
		{
			return detailAppearance->getCurrentDetailLevel();
		}

		return 0;
	}
}

using namespace LODManagerNamespace;

// ======================================================================

void LODManager::Statistics::reset()
{
	objectsProcessed = 0;
	lodSwitches = 0;
	transitionsActive = 0;
	objectsCulled = 0;
	averageLOD = 0.0f;
	totalProcessingTime = 0.0f;
}

// ======================================================================

void LODManager::install()
{
	DEBUG_FATAL(ms_installed, ("LODManager already installed"));

	// Initialize distance thresholds
	ms_lodDistances.resize(cs_maxLODLevels);
	for (int i = 0; i < cs_maxLODLevels; ++i)
		ms_lodDistances[i] = cs_defaultDistances[i];

	// Initialize coverage thresholds
	ms_lodCoverages.resize(cs_maxLODLevels);
	for (int i = 0; i < cs_maxLODLevels; ++i)
		ms_lodCoverages[i] = cs_defaultCoverages[i];

	ms_objectStates.clear();
	ms_stats.reset();

	ms_installed = true;
	ExitChain::add(remove, "LODManager::remove");

	DEBUG_REPORT_LOG(true, ("LODManager installed: mode=%d, transitions=%d\n",
		static_cast<int>(ms_selectionMode), static_cast<int>(ms_transitionMode)));
}

// ----------------------------------------------------------------------

void LODManager::remove()
{
	DEBUG_FATAL(!ms_installed, ("LODManager not installed"));

	ms_objectStates.clear();
	ms_lodDistances.clear();
	ms_lodCoverages.clear();
	ms_installed = false;
}

// ----------------------------------------------------------------------

void LODManager::beginFrame(Camera const * camera)
{
	DEBUG_FATAL(!ms_installed, ("LODManager not installed"));
	DEBUG_FATAL(ms_inFrame, ("LODManager::beginFrame called while in frame"));

	++ms_frameNumber;
	ms_camera = camera;
	ms_switchesThisFrame = 0;

	// Cache camera info
	if (camera)
	{
		ms_cameraPosition = camera->getPosition_w();
		ms_screenWidth = static_cast<float>(Graphics::getCurrentRenderTargetWidth());
		ms_screenHeight = static_cast<float>(Graphics::getCurrentRenderTargetHeight());

		// Calculate tan(FOV/2) for screen coverage calculation
		float const fov = camera->getFieldOfView();
		ms_tanHalfFOV = tan(fov * 0.5f);
	}

	ms_timer.start();
	ms_stats.reset();
	ms_inFrame = true;
}

// ----------------------------------------------------------------------

void LODManager::endFrame()
{
	DEBUG_FATAL(!ms_inFrame, ("LODManager::endFrame called without beginFrame"));

	ms_timer.stop();
	ms_stats.totalProcessingTime = ms_timer.getElapsedTime() * 1000.0f;
	ms_inFrame = false;
	ms_camera = nullptr;
}

// ----------------------------------------------------------------------

void LODManager::update(float deltaTime)
{
	updateTransitions(deltaTime);
}

// ----------------------------------------------------------------------

int LODManager::selectLOD(Object const * object)
{
	if (!object)
		return -2;  // Invalid object, use fallback

	// Auto-initialize frame state from ShaderPrimitiveSorter if not already in frame
	if (!ms_inFrame)
	{
		// Try to get camera from ShaderPrimitiveSorter (it's available during rendering)
		Camera const * const camera = &ShaderPrimitiveSorter::getCurrentCamera();
		if (camera)
		{
			// Start a new implicit frame
			ms_camera = camera;
			ms_cameraPosition = camera->getPosition_w();
			ms_screenWidth = static_cast<float>(Graphics::getCurrentRenderTargetWidth());
			ms_screenHeight = static_cast<float>(Graphics::getCurrentRenderTargetHeight());

			float const fov = camera->getFieldOfView();
			ms_tanHalfFOV = (fov > 0.0f) ? tan(fov * 0.5f) : 1.0f;

			ms_inFrame = true;
			++ms_frameNumber;
		}
		else
		{
			return -2;  // No camera available, use fallback
		}
	}

	return selectLOD(object, object->getAppearance());
}

// ----------------------------------------------------------------------

int LODManager::selectLOD(Object const * object, Appearance const * appearance)
{
	if (!object)
		return -2;  // Invalid object, use fallback

	// Auto-initialize frame state if needed
	if (!ms_inFrame)
	{
		Camera const * const camera = &ShaderPrimitiveSorter::getCurrentCamera();
		if (camera)
		{
			ms_camera = camera;
			ms_cameraPosition = camera->getPosition_w();
			ms_screenWidth = static_cast<float>(Graphics::getCurrentRenderTargetWidth());
			ms_screenHeight = static_cast<float>(Graphics::getCurrentRenderTargetHeight());

			float const fov = camera->getFieldOfView();
			ms_tanHalfFOV = (fov > 0.0f) ? tan(fov * 0.5f) : 1.0f;

			ms_inFrame = true;
			++ms_frameNumber;
		}
		else
		{
			return -2;  // No camera available, use fallback
		}
	}

	++ms_stats.objectsProcessed;

	// Get object position and bounding radius
	Vector const position = object->getPosition_w();
	float boundingRadius = 1.0f;

	if (appearance)
	{
		Sphere const & sphere = appearance->getSphere();
		boundingRadius = sphere.getRadius();
	}

	// Calculate distance and coverage
	float const distance = (position - ms_cameraPosition).magnitude();
	float const coverage = calculateScreenCoverage(position, boundingRadius);

	// Check minimum coverage threshold
	if (coverage < ms_minimumCoverage)
	{
		++ms_stats.objectsCulled;
		return -1;  // Signal to cull
	}

	// Get or create object state
	ObjectLODState & state = ms_objectStates[object];

	// Get actual LOD level count from appearance
	int const lodCount = getAppearanceLODCount(appearance);
	int const maxLOD = (lodCount > 0) ? (lodCount - 1) : 0;

	// If only one LOD level, just return 0
	if (maxLOD == 0)
	{
		state.currentLOD = 0;
		state.targetLOD = 0;
		state.lastDistance = distance;
		state.lastCoverage = coverage;
		state.frameLastUpdated = ms_frameNumber;
		return 0;
	}

	// Select LOD based on mode
	int selectedLOD;
	switch (ms_selectionMode)
	{
	case SM_distance:
		selectedLOD = selectLODByDistance(distance, maxLOD);
		break;

	case SM_screenCoverage:
		selectedLOD = selectLODByCoverage(coverage, maxLOD);
		break;

	case SM_hybrid:
	default:
		selectedLOD = selectLODHybrid(distance, coverage, maxLOD);
		break;
	}

	// Apply LOD bias
	selectedLOD = std::max(0, std::min(maxLOD, selectedLOD + static_cast<int>(ms_lodBias)));

	// Check hysteresis
	if (!shouldSwitchLOD(state, selectedLOD))
		selectedLOD = state.currentLOD;

	// Apply change if needed (and within frame budget)
	if (selectedLOD != state.currentLOD)
	{
		if (ms_switchesThisFrame < ms_maxSwitchesPerFrame)
		{
			state.targetLOD = selectedLOD;
			state.hysteresisTimer = 0.0f;

			if (ms_transitionMode != TM_immediate)
			{
				state.inTransition = true;
				state.transitionProgress = 0.0f;
				++ms_stats.transitionsActive;
			}
			else
			{
				state.currentLOD = selectedLOD;
			}

			++ms_switchesThisFrame;
			++ms_stats.lodSwitches;
		}
	}

	// Update state
	state.lastDistance = distance;
	state.lastCoverage = coverage;
	state.frameLastUpdated = ms_frameNumber;

	// Update average LOD statistic
	float const totalLOD = ms_stats.averageLOD * (ms_stats.objectsProcessed - 1) + state.currentLOD;
	ms_stats.averageLOD = totalLOD / ms_stats.objectsProcessed;

	return state.currentLOD;
}

// ----------------------------------------------------------------------

int LODManager::selectLODForPosition(Vector const & position, float boundingRadius, int maxLOD)
{
	float const distance = (position - ms_cameraPosition).magnitude();
	float const coverage = calculateScreenCoverage(position, boundingRadius);

	return selectLODHybrid(distance, coverage, maxLOD);
}

// ----------------------------------------------------------------------

float LODManager::calculateScreenCoverage(Vector const & position, float boundingRadius)
{
	if (ms_screenHeight <= 0.0f || ms_tanHalfFOV <= 0.0f)
		return 0.0f;

	// Calculate distance from camera
	float const distance = (position - ms_cameraPosition).magnitude();

	if (distance < 0.001f)
		return 1.0f;  // Object at camera

	// Calculate projected size in screen space
	// projectedSize = (objectSize / distance) * (screenHeight / (2 * tan(fov/2)))
	float const projectedPixels = (boundingRadius / distance) * (ms_screenHeight / (2.0f * ms_tanHalfFOV));

	// Coverage is projected diameter / screen height
	float const coverage = (2.0f * projectedPixels) / ms_screenHeight;

	return std::max(0.0f, std::min(1.0f, coverage));
}

// ----------------------------------------------------------------------

float LODManager::calculateScreenCoverage(Object const * object)
{
	if (!object)
		return 0.0f;

	Vector const position = object->getPosition_w();
	float boundingRadius = 1.0f;

	Appearance const * const appearance = object->getAppearance();
	if (appearance)
	{
		Sphere const & sphere = appearance->getSphere();
		boundingRadius = sphere.getRadius();
	}

	return calculateScreenCoverage(position, boundingRadius);
}

// ----------------------------------------------------------------------

LODManager::ObjectLODState * LODManager::getObjectState(Object const * object)
{
	ObjectStateMap::iterator it = ms_objectStates.find(object);
	return (it != ms_objectStates.end()) ? &it->second : nullptr;
}

// ----------------------------------------------------------------------

void LODManager::clearObjectState(Object const * object)
{
	ms_objectStates.erase(object);
}

// ----------------------------------------------------------------------

void LODManager::setSelectionMode(SelectionMode mode)
{
	ms_selectionMode = mode;
}

// ----------------------------------------------------------------------

LODManager::SelectionMode LODManager::getSelectionMode()
{
	return ms_selectionMode;
}

// ----------------------------------------------------------------------

void LODManager::setTransitionMode(TransitionMode mode)
{
	ms_transitionMode = mode;
}

// ----------------------------------------------------------------------

LODManager::TransitionMode LODManager::getTransitionMode()
{
	return ms_transitionMode;
}

// ----------------------------------------------------------------------

void LODManager::setLODBias(float bias)
{
	ms_lodBias = std::max(-3.0f, std::min(3.0f, bias));
}

// ----------------------------------------------------------------------

float LODManager::getLODBias()
{
	return ms_lodBias;
}

// ----------------------------------------------------------------------

void LODManager::setHysteresisTime(float seconds)
{
	ms_hysteresisTime = std::max(0.0f, seconds);
}

// ----------------------------------------------------------------------

float LODManager::getHysteresisTime()
{
	return ms_hysteresisTime;
}

// ----------------------------------------------------------------------

void LODManager::setHysteresisDistance(float percent)
{
	ms_hysteresisDistance = std::max(0.0f, std::min(0.5f, percent));
}

// ----------------------------------------------------------------------

float LODManager::getHysteresisDistance()
{
	return ms_hysteresisDistance;
}

// ----------------------------------------------------------------------

void LODManager::setTransitionDuration(float seconds)
{
	ms_transitionDuration = std::max(0.0f, seconds);
}

// ----------------------------------------------------------------------

float LODManager::getTransitionDuration()
{
	return ms_transitionDuration;
}

// ----------------------------------------------------------------------

void LODManager::setMinimumCoverage(float coverage)
{
	ms_minimumCoverage = std::max(0.0f, coverage);
}

// ----------------------------------------------------------------------

float LODManager::getMinimumCoverage()
{
	return ms_minimumCoverage;
}

// ----------------------------------------------------------------------

void LODManager::setMaxLODSwitchesPerFrame(int max)
{
	ms_maxSwitchesPerFrame = std::max(1, max);
}

// ----------------------------------------------------------------------

int LODManager::getMaxLODSwitchesPerFrame()
{
	return ms_maxSwitchesPerFrame;
}

// ----------------------------------------------------------------------

void LODManager::setLODDistances(float const * distances, int count)
{
	int const actualCount = std::min(count, cs_maxLODLevels);
	for (int i = 0; i < actualCount; ++i)
		ms_lodDistances[i] = distances[i];
}

// ----------------------------------------------------------------------

void LODManager::setLODDistance(int level, float distance)
{
	if (level >= 0 && level < cs_maxLODLevels)
		ms_lodDistances[level] = distance;
}

// ----------------------------------------------------------------------

float LODManager::getLODDistance(int level)
{
	if (level >= 0 && level < cs_maxLODLevels)
		return ms_lodDistances[level];
	return 0.0f;
}

// ----------------------------------------------------------------------

void LODManager::setLODCoverages(float const * coverages, int count)
{
	int const actualCount = std::min(count, cs_maxLODLevels);
	for (int i = 0; i < actualCount; ++i)
		ms_lodCoverages[i] = coverages[i];
}

// ----------------------------------------------------------------------

void LODManager::setLODCoverage(int level, float coverage)
{
	if (level >= 0 && level < cs_maxLODLevels)
		ms_lodCoverages[level] = coverage;
}

// ----------------------------------------------------------------------

float LODManager::getLODCoverage(int level)
{
	if (level >= 0 && level < cs_maxLODLevels)
		return ms_lodCoverages[level];
	return 0.0f;
}

// ----------------------------------------------------------------------

LODManager::Statistics const & LODManager::getStatistics()
{
	return ms_stats;
}

// ----------------------------------------------------------------------

void LODManager::resetStatistics()
{
	ms_stats.reset();
}

// ----------------------------------------------------------------------

int LODManager::selectLODByDistance(float distance, int maxLOD)
{
	for (int lod = 0; lod < maxLOD && lod < static_cast<int>(ms_lodDistances.size()); ++lod)
	{
		if (distance < ms_lodDistances[lod])
			return lod;
	}
	return maxLOD;
}

// ----------------------------------------------------------------------

int LODManager::selectLODByCoverage(float coverage, int maxLOD)
{
	for (int lod = 0; lod < maxLOD && lod < static_cast<int>(ms_lodCoverages.size()); ++lod)
	{
		if (coverage > ms_lodCoverages[lod])
			return lod;
	}
	return maxLOD;
}

// ----------------------------------------------------------------------

int LODManager::selectLODHybrid(float distance, float coverage, int maxLOD)
{
	// Use whichever metric suggests higher detail (lower LOD number)
	int const distanceLOD = selectLODByDistance(distance, maxLOD);
	int const coverageLOD = selectLODByCoverage(coverage, maxLOD);

	return std::min(distanceLOD, coverageLOD);
}

// ----------------------------------------------------------------------

bool LODManager::shouldSwitchLOD(ObjectLODState const & state, int newLOD)
{
	if (newLOD == state.currentLOD)
		return false;

	// Check time-based hysteresis
	if (state.hysteresisTimer < ms_hysteresisTime)
		return false;

	// Check distance-based hysteresis
	if (ms_hysteresisDistance > 0.0f && !ms_lodDistances.empty())
	{
		int const currentLOD = state.currentLOD;
		if (currentLOD >= 0 && currentLOD < static_cast<int>(ms_lodDistances.size()))
		{
			float const threshold = ms_lodDistances[currentLOD];
			float const band = threshold * ms_hysteresisDistance;

			// If switching up (lower LOD number = higher detail)
			if (newLOD < currentLOD)
			{
				// Only switch if distance decreased significantly
				if (state.lastDistance > threshold - band)
					return false;
			}
			// If switching down (higher LOD number = lower detail)
			else
			{
				// Only switch if distance increased significantly
				if (state.lastDistance < threshold + band)
					return false;
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------------

void LODManager::updateTransitions(float deltaTime)
{
	if (ms_transitionMode == TM_immediate)
		return;

	for (ObjectStateMap::iterator it = ms_objectStates.begin(); it != ms_objectStates.end(); ++it)
	{
		ObjectLODState & state = it->second;

		// Update hysteresis timer
		state.hysteresisTimer += deltaTime;

		// Update transition
		if (state.inTransition && ms_transitionDuration > 0.0f)
		{
			state.transitionProgress += deltaTime / ms_transitionDuration;

			if (state.transitionProgress >= 1.0f)
			{
				state.transitionProgress = 1.0f;
				state.currentLOD = state.targetLOD;
				state.inTransition = false;
			}
		}
	}
}

// ======================================================================

