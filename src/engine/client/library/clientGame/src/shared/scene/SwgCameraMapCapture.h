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

	struct MapCaptureRequest
	{
		char  outputDir[512];
		float worldMinX;
		float worldMaxX;
		float worldMinZ;
		float worldMaxZ;
		float tileWorldSize;
		float cameraHeight;
		int   settleFrames;
		int   viewportWidth;
		int   viewportHeight;
		bool  filterCreatures;
		bool  filterLairs;
		bool  filterNpcLike;
		bool  suppressFog;
		bool  suppressParticles;
		bool  suppressWeather;
		bool  useFixedWorldBounds;

		MapCaptureRequest();
	};

	static void install();
	static void remove();

	static void update(float elapsedTime, GroundScene *groundScene);

	static void prepareDraw(GroundScene *groundScene);
	static void finishDraw(GroundScene *groundScene);

	/// True while an ortho tile batch is active (used to suppress FreeCamera input).
	static bool isBatchActive();
	static bool isExternalCapturePending();

	/// Programmatic capture (e.g. God Client Satellite Map Generator).
	static void requestBatch(MapCaptureRequest const &request);
	static void cancelBatch();
	static void getBatchProgress(int &currentTile, int &totalTiles);
	/// Returns true once when an externally requested batch has finished.
	static bool consumeBatchCompleted();

	/// Grid dimensions from the last started batch (valid until next batch).
	static void getBatchGrid(int &numCols, int &numRows, int &totalTiles);
	static void getTileFileBasePath(int linearIndex, char *path, size_t pathSize);

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
