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
	static void dismiss();
	
	static bool isActive();

private:
	SplashScreen();
	SplashScreen(const SplashScreen &);
	SplashScreen & operator=(const SplashScreen &);
};

// ======================================================================

#endif
