// ======================================================================
//
// SwgCameraMapCapture.h
//
// Orthographic tile capture + GOT draw filter + environment suppression
// for SwgCameraClient / headless map pipelines.
//
// ======================================================================

#ifndef INCLUDED_SwgCameraMapCapture_H
#define INCLUDED_SwgCameraMapCapture_H

// ======================================================================

class GroundScene;

// ======================================================================

class SwgCameraMapCapture
{
public:

	static void install();
	static void remove();

	static void update(float elapsedTime, GroundScene *groundScene);

	static void prepareDraw(GroundScene *groundScene);
	static void finishDraw(GroundScene *groundScene);

	/// True while an ortho tile batch is active (used to suppress FreeCamera input).
	static bool isBatchActive();

private:

	static void beginBatch(GroundScene *groundScene);
	static void endBatch(GroundScene *groundScene);
	static void advanceTileIndex(void);

	SwgCameraMapCapture();
	SwgCameraMapCapture(SwgCameraMapCapture const &);
	SwgCameraMapCapture &operator=(SwgCameraMapCapture const &);
};

// ======================================================================

#endif
