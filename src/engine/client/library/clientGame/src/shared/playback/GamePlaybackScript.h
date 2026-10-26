// ======================================================================
//
// GamePlaybackScript.h
// Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_GamePlaybackScript_H
#define INCLUDED_GamePlaybackScript_H

// ======================================================================

class CrcString;
class Object;

// ======================================================================

class GamePlaybackScript
{
public:

	static void install();

	static void sitCreatureOnChair(Object &creatureObject, Object &chairObject, int positionIndex);
	static void standFromChair(Object &creatureObject);
	static void alterBedRest(Object &creatureObject, float elapsedTime);
	// play a raw .ans on one of a creature's tracks ("loop" / "action"), e.g. from /devtest anim
	static bool playAnimation(Object &object, char const *trackName, char const *path, bool loop);
	// a chair appearance carrying a station hardpoint (bed, meditate, kneel, gong, wipe, pour, terminal, tan, console)
	static bool isStation(Object const &chairObject);

	static void sitOnSaddle(Object &riderObject, Object &mountObject, CrcString const &riderHardpointName);
	static void standFromSaddle(Object &riderObject);

private:

	static void remove();

};

// ======================================================================

#endif
