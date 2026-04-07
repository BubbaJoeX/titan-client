// ======================================================================
//
// SplashScreen.h
// copyright 2024 SWG Titan
//
// Displays a splash screen image during client startup
//
// ======================================================================

#ifndef INCLUDED_SplashScreen_H
#define INCLUDED_SplashScreen_H

// ======================================================================

class Shader;
class Texture;

// ======================================================================

class SplashScreen
{
public:
	static void install();
	static void remove();
	
	static void render();
	/** Process the Windows message queue and redraw the splash (call between heavy startup steps). */
	static void pump();
	/**
	 * Optionally fetch textures listed in cfg [ClientGame] splashPreload@i so the GPU/driver cache warms
	 * while the splash is visible. Each path calls pump() afterward.
	 */
	static void preloadConfiguredAssets();
	static void dismiss();
	
	static bool isActive();

private:
	SplashScreen();
	SplashScreen(const SplashScreen &);
	SplashScreen & operator=(const SplashScreen &);
};

// ======================================================================

#endif
