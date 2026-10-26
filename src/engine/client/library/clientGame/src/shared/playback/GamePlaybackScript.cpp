// ======================================================================
//
// GamePlaybackScript.cpp
// Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/GamePlaybackScript.h"

#include "clientAnimation/PlaybackScript.h"
#include "clientAnimation/PlaybackScriptTemplate.h"
#include "clientAnimation/PlaybackScriptTemplateList.h"
#include "clientAnimation/PriorityPlaybackScriptManager.h"
#include "clientGame/ClientObject.h"
#include "clientGame/CreatureObject.h"
#include "clientSkeletalAnimation/AnimationTrackId.h"
#include "clientSkeletalAnimation/SkeletalAnimation.h"
#include "clientSkeletalAnimation/SkeletalAnimationTemplate.h"
#include "clientSkeletalAnimation/SkeletalAnimationTemplateList.h"
#include "clientSkeletalAnimation/SkeletalAppearance2.h"
#include "clientSkeletalAnimation/StateHierarchyAnimationController.h"
#include "clientSkeletalAnimation/TrackAnimationController.h"
#include "clientSkeletalAnimation/TransformAnimationResolver.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/ConstCharCrcLowerString.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Tag.h"
#include "sharedFoundation/Watcher.h"
#include "sharedMath/Transform.h"
#include "sharedObject/Appearance.h"
#include "swgSharedUtility/Postures.h"

#include <deque>
#include <string>
#include <vector>

// ======================================================================

namespace GamePlaybackScriptNamespace
{
	Tag const TAG_EPS  = TAG3(E,P,S);
	Tag const TAG_HPNT = TAG(H,P,N,T);
	Tag const TAG_TIME = TAG(T,I,M,E);

	const int                             cs_sitScriptPriority = 100;

	bool                                  s_installed;

	const PlaybackScriptTemplate         *s_sitInChairScriptTemplate;
	const PlaybackScriptTemplate         *s_standFromChairScriptTemplate;

	PlaybackScriptTemplate::ObjectVector  s_actors;

	//-- Stations: a chair appearance (it has the "foot" hardpoint) that also carries one of the station hardpoints
	//   below is used instead of sat on - a bed is lain on, a meditation mat sat on cross-legged, a gong struck, a
	//   counter wiped. The sit script aligns the creature to the station hardpoint (the spot, facing its +z). When
	//   the sitting posture arrives, alterBedRest cancels the chair-sit transition (the state machine plays
	//   transitions on the action track; its root motion would slide the creature off the spot), snaps the
	//   creature back onto the hardpoint and plays the station's animations on the loop track in place of the
	//   sitting_chair loop: the way in (if any), then its loop. Standing up cancels the chair-stand transition as
	//   soon as it starts and plays the way out (if any) on the action track over the standing loop.
	struct Station
	{
		char const *hardpoint;
		char const *enter[2];    // played once, in order, before the loop (0 = none)
		char const *loop;
		char const *exit;        // replaces the chair-stand transition (0 = straight back to standing)
	};
#define STATION_ANIM(name) "appearance/animation/" name ".ans"
	Station const cs_stations[] =
	{
		{ "bed",      { STATION_ANIM("all_b_std_standing_to_laying_bed_multi"), 0 }, STATION_ANIM("all_b_idl_sleeping_idle1"),
		              STATION_ANIM("all_b_std_laying_to_standing_bed_multi") },
		{ "meditate", { STATION_ANIM("all_b_force_standing_to_meditation"), 0 }, STATION_ANIM("all_b_force_meditation"),
		              STATION_ANIM("all_b_force_meditation_to_standing") },
		// no way in for kneel: standing_to_crouched / crouched_to_kneeling carry root motion that walks the creature
		// 0.4 m back off the spot; blending straight into the kneeling loop keeps it there
		{ "kneel",    { 0, 0 }, STATION_ANIM("all_b_idl_kneeling_idle1"), STATION_ANIM("all_b_std_kneeling_to_standing") },
		{ "gong",     { 0, 0 }, STATION_ANIM("all_b_dnc_musician_gong_dance"), 0 },
		{ "wipe",     { 0, 0 }, STATION_ANIM("all_b_int_wipe_surface"), 0 },
		{ "pour",     { 0, 0 }, STATION_ANIM("all_b_int_standing_pour_container"), 0 },
		{ "terminal", { 0, 0 }, STATION_ANIM("all_b_int_use_data_terminal"), 0 },
		{ "tan",      { 0, 0 }, STATION_ANIM("all_b_int_skin_creature"), 0 },
		{ "console",  { 0, 0 }, STATION_ANIM("all_b_int_use_console"), 0 }
	};
#undef STATION_ANIM

	enum BedPhase
	{
		BP_aligning,     // the sit script is moving the creature onto the station hardpoint
		BP_lying,        // at the station: on the way in / looping
		BP_gettingUp     // posture left sitting: the way out once the state machine has acted
	};

	//-- Seat activities: a chair whose appearance carries one of these hardpoints is sat on normally, and once
	//   seated the activity's loop replaces the sitting_chair loop (on the loop track, like the bed loop).
	struct SeatActivity
	{
		char const *hardpoint;
		char const *loopAnimation;
	};
	SeatActivity const cs_seatActivities[] =
	{
		{ "use_console", "appearance/animation/all_b_int_sitting_use_console.ans" },
		{ "eat",         "appearance/animation/all_b_int_eat_sitting.ans" }
	};

	struct BedRest
	{
		Watcher<Object> creature;
		Watcher<Object> bed;       // the chair object
		BedPhase        phase;
		float           time;      // seconds in the current phase
		char const     *activity;  // seat activity loop (sat on normally), or 0 for a station
		Station const  *station;   // station, or 0 for a seat activity
	};

	char const *findSeatActivity(Object const &object)
	{
		Appearance const *const appearance = object.getAppearance();
		Transform transform(Transform::IF_none);
		for (size_t i = 0; appearance && i < sizeof(cs_seatActivities) / sizeof(cs_seatActivities[0]); ++i)
			if (appearance->findHardpoint(ConstCharCrcLowerString(cs_seatActivities[i].hardpoint), transform))
				return cs_seatActivities[i].loopAnimation;
		return 0;
	}
	// the state machine starts its transition on the action track, then its loop on the loop track after a blend
	// delay: act once both have happened
	float const cs_bedSettleTime = 0.6f;
	std::vector<BedRest>                  s_bedRests;

	Station const *findStation(Object const &object, Transform &transform)
	{
		Appearance const *const appearance = object.getAppearance();
		for (size_t i = 0; appearance && i < sizeof(cs_stations) / sizeof(cs_stations[0]); ++i)
			if (appearance->findHardpoint(ConstCharCrcLowerString(cs_stations[i].hardpoint), transform))
				return &cs_stations[i];
		return 0;
	}

	TrackAnimationController *getTrackController(Object &object)
	{
		SkeletalAppearance2 *const appearance = dynamic_cast<SkeletalAppearance2 *>(object.getAppearance());
		if (!appearance)
			return 0;
		TransformAnimationController *const base = appearance->getAnimationController();
		TrackAnimationController *controller = dynamic_cast<TrackAnimationController *>(base);
		if (!controller)
		{
			StateHierarchyAnimationController *const shac = dynamic_cast<StateHierarchyAnimationController *>(base);
			if (shac)
				controller = &shac->getTrackAnimationController();
		}
		return controller;
	}

	bool getTrack(char const *name, AnimationTrackId &trackId)
	{
		return TrackAnimationController::getTrackIdByName(ConstCharCrcLowerString(name), trackId);
	}

	//-- Play a raw .ans on one of the creature's tracks ("loop" or "action").
	bool playOnTrack(Object &object, char const *trackName, char const *path, bool loop, bool immediate)
	{
		SkeletalAppearance2 *const appearance = dynamic_cast<SkeletalAppearance2 *>(object.getAppearance());
		TrackAnimationController *const controller = getTrackController(object);
		if (!appearance || !controller || !TreeFile::exists(path))
			return false;
		TransformNameMap const *const transformNameMap = appearance->getAnimationResolver().getPrimarySkeletonTransformNameMap();
		AnimationTrackId trackId(AnimationTrackId::cms_invalid);
		if (!transformNameMap || !getTrack(trackName, trackId))
			return false;
		SkeletalAnimationTemplate const *const animationTemplate = SkeletalAnimationTemplateList::fetch(CrcLowerString(path));
		if (!animationTemplate)
			return false;
		SkeletalAnimation *const animation = animationTemplate->fetchSkeletalAnimation(appearance->getAnimationEnvironment(), *transformNameMap);
		animationTemplate->release();
		if (!animation)
			return false;
		IGNORE_RETURN(controller->playAnimation(trackId, animation, immediate ? TrackAnimationController::PM_immediate : TrackAnimationController::PM_queue,
			loop, TrackAnimationController::BM_linearBlend, 0.25f, NULL));
		animation->release();
		object.scheduleForAlter();
		return true;
	}

	void stopTrack(Object &object, char const *trackName)
	{
		TrackAnimationController *const controller = getTrackController(object);
		AnimationTrackId trackId(AnimationTrackId::cms_invalid);
		if (controller && getTrack(trackName, trackId))
			controller->stopAnimation(trackId);
	}

	std::vector<BedRest>::iterator findBedRest(Object const &creatureObject)
	{
		for (std::vector<BedRest>::iterator it = s_bedRests.begin(); it != s_bedRests.end(); ++it)
			if (it->creature.getPointer() == &creatureObject)
				return it;
		return s_bedRests.end();
	}
}

using namespace GamePlaybackScriptNamespace;

// ======================================================================

void GamePlaybackScript::install()
{
	InstallTimer const installTimer("GamePlaybackScript::install");

	DEBUG_FATAL(s_installed, ("already installed"));

	s_sitInChairScriptTemplate = PlaybackScriptTemplateList::fetch(ConstCharCrcLowerString("playback/change_posture_align.pst"));
	NOT_NULL(s_sitInChairScriptTemplate);

	s_standFromChairScriptTemplate = PlaybackScriptTemplateList::fetch(ConstCharCrcLowerString("playback/restore_scale.pst"));
	NOT_NULL(s_standFromChairScriptTemplate);

	s_installed = true;
	ExitChain::add(remove, "GamePlaybackScript");
}

// ----------------------------------------------------------------------

void GamePlaybackScript::sitCreatureOnChair(Object &creatureObject, Object &chairObject, int positionIndex)
{
	// We bailed on multi-seater chairs.
	UNREF(positionIndex);

	DEBUG_FATAL(!s_installed, ("not installed."));

	//-- Setup script actors.
	s_actors.resize(2);
	s_actors[0] = &creatureObject;
	s_actors[1] = &chairObject;

	//-- Create a playback script using the creature as the move object and the target as the alignment object.
	//   Default values for script variables are fine.
	PlaybackScript *const playbackScript = s_sitInChairScriptTemplate->createPlaybackScript(cs_sitScriptPriority, s_actors);
	NOT_NULL(playbackScript);

	//-- A station (bed, mat, gong...): align to its hardpoint and start it once seated; a seat with an activity:
	//   sit, then run the activity loop (see alterBedRest).
	Transform stationHardpoint(Transform::IF_none);
	Station const *const station = (&chairObject != &creatureObject) ? findStation(chairObject, stationHardpoint) : 0;
	char const *const activity = (&chairObject != &creatureObject) && !station ? findSeatActivity(chairObject) : 0;
	if (station || activity)
	{
		if (station)
			IGNORE_RETURN(playbackScript->setStringVariable(TAG_HPNT, station->hardpoint));
		std::vector<BedRest>::iterator it = findBedRest(creatureObject);
		if (it == s_bedRests.end())
		{
			s_bedRests.push_back(BedRest());
			it = s_bedRests.end() - 1;
			it->creature = &creatureObject;
		}
		it->bed      = &chairObject;
		it->phase    = BP_aligning;
		it->time     = 0.0f;
		it->activity = activity;
		it->station  = station;
	}

	//-- Submit the script for playback.
	IGNORE_RETURN(PriorityPlaybackScriptManager::submitPlaybackScript(playbackScript));
}

// ----------------------------------------------------------------------

void GamePlaybackScript::alterBedRest(Object &creatureObject, float elapsedTime)
{
	if (s_bedRests.empty())
		return;
	std::vector<BedRest>::iterator const it = findBedRest(creatureObject);
	if (it == s_bedRests.end())
		return;

	if (it->phase == BP_gettingUp)
	{
		//-- Replace the chair-stand transition with the way out as soon as the state machine starts it on the
		//   action track (the standing loop runs underneath); give up waiting after a while.
		it->time += elapsedTime;
		TrackAnimationController *const controller = getTrackController(creatureObject);
		AnimationTrackId actionTrack(AnimationTrackId::cms_invalid);
		bool const started = controller && getTrack("action", actionTrack) && !controller->isTrackIdle(actionTrack);
		if (started || (it->time >= 2.0f * cs_bedSettleTime))
		{
			stopTrack(creatureObject, "action");
			if (it->station && it->station->exit)
				IGNORE_RETURN(playOnTrack(creatureObject, "action", it->station->exit, false, true));
			IGNORE_RETURN(s_bedRests.erase(it));
		}
		return;
	}

	ClientObject *const clientObject = creatureObject.asClientObject();
	CreatureObject *const creature = clientObject ? clientObject->asCreatureObject() : 0;
	if (!creature || !creature->isSittingOnObject() || (creature->getVisualPosture() != Postures::Sitting))
		return;

	if (it->phase == BP_aligning)
	{
		it->time += elapsedTime;
		if (it->time < cs_bedSettleTime)
			return;

		if (it->activity)
		{
			//-- Seated like on any chair: swap the sitting loop for the activity loop.
			IGNORE_RETURN(playOnTrack(creatureObject, "loop", it->activity, true, true));
			it->phase = BP_lying;
			return;
		}

		//-- Seated: drop the chair-sit transition and its slide, stand on the station spot, the way in, the loop.
		stopTrack(creatureObject, "action");
		Object *const chair = it->bed.getPointer();
		Transform hardpoint(Transform::IF_none);
		if (chair && (chair->getParentCell() == creatureObject.getParentCell()) && findStation(*chair, hardpoint) == it->station)
		{
			Transform onSpot(Transform::IF_none);
			onSpot.multiply(chair->getTransform_o2p(), hardpoint);
			creatureObject.setTransform_o2p(onSpot);
		}
		bool queued = false;
		for (int k = 0; k < 2; ++k)
			if (it->station->enter[k])
				queued = playOnTrack(creatureObject, "loop", it->station->enter[k], false, !queued) || queued;
		IGNORE_RETURN(playOnTrack(creatureObject, "loop", it->station->loop, true, !queued));
		it->phase = BP_lying;
		return;
	}

	//-- At the station / busy: keep at it if something stopped the loop.
	TrackAnimationController *const controller = getTrackController(creatureObject);
	AnimationTrackId trackId(AnimationTrackId::cms_invalid);
	if (controller && getTrack("loop", trackId) && controller->isTrackIdle(trackId))
		IGNORE_RETURN(playOnTrack(creatureObject, "loop", it->activity ? it->activity : it->station->loop, true, false));
}

// ----------------------------------------------------------------------

bool GamePlaybackScript::playAnimation(Object &object, char const *trackName, char const *path, bool loop)
{
	return playOnTrack(object, trackName, path, loop, true);
}

// ----------------------------------------------------------------------

bool GamePlaybackScript::isStation(Object const &chairObject)
{
	Transform hardpoint(Transform::IF_none);
	return findStation(chairObject, hardpoint) != 0;
}

// ----------------------------------------------------------------------

void GamePlaybackScript::standFromChair(Object &creatureObject)
{
	DEBUG_FATAL(!s_installed, ("not installed."));

	//-- Out of a bed: climb out once the posture change has kicked off (see alterBedRest).
	std::vector<BedRest>::iterator const it = findBedRest(creatureObject);
	if (it != s_bedRests.end())
	{
		if ((it->phase == BP_lying) && it->station)
		{
			it->phase  = BP_gettingUp;
			it->time   = 0.0f;
		}
		else
			IGNORE_RETURN(s_bedRests.erase(it));
	}

	s_actors.resize(1);
	s_actors[0] = &creatureObject;

	PlaybackScript *const playbackScript = s_standFromChairScriptTemplate->createPlaybackScript(cs_sitScriptPriority, s_actors);
	NOT_NULL(playbackScript);
	
	//-- Submit the script for playback.
	IGNORE_RETURN(PriorityPlaybackScriptManager::submitPlaybackScript(playbackScript));
}

// ----------------------------------------------------------------------

void GamePlaybackScript::sitOnSaddle(Object &riderObject, Object &mountObject, CrcString const &riderHardpointName)
{
	DEBUG_FATAL(!s_installed, ("not installed."));

	//-- For now we just snap the player appearance's scale to 1.0f.
	Appearance *const appearance = riderObject.getAppearance();
	if (appearance)
		appearance->setScale(Vector::xyz111);

	UNREF(mountObject);
	UNREF(riderHardpointName);
}

// ----------------------------------------------------------------------

void GamePlaybackScript::standFromSaddle(Object &riderObject)
{
	DEBUG_FATAL(!s_installed, ("not installed."));

	//-- For now we just snap the player appearance's scale back to whatever scale is specified for the CreatureObject.
	ClientObject *const riderAsClientObject = riderObject.asClientObject();
	if (riderAsClientObject)
	{
		CreatureObject *const riderAsCreatureObject = riderAsClientObject->asCreatureObject();
		if (riderAsCreatureObject)
		{
			Appearance *const appearance = riderAsCreatureObject->getAppearance();
			if (appearance)
			{
				float const riderDismountedScale = riderAsCreatureObject->getScaleFactor();
				appearance->setScale(Vector(riderDismountedScale, riderDismountedScale, riderDismountedScale));
			}
		}
	}
}

// ======================================================================

void GamePlaybackScript::remove()
{
	DEBUG_FATAL(!s_installed, ("not installed."));

	s_standFromChairScriptTemplate->release();
	s_standFromChairScriptTemplate = 0;

	s_sitInChairScriptTemplate->release();
	s_sitInChairScriptTemplate = 0;

	s_installed = false;
}

// ======================================================================
