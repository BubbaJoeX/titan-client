// ======================================================================
// BinkVideo.h — in-game video playback via libVLC (class name kept for
// game/tool compatibility; no RAD Bink / proprietary DLL).
// copyright (c) 2001 Sony Online Entertainment
// ======================================================================

#ifndef INCLUDED_BinkVideo_H
#define INCLUDED_BinkVideo_H

#include "clientGraphics/Video.h"

struct IDirect3DSurface9;
struct VlcVideoContext;

// ====================================================================== 

class BinkVideo : public Video
{
public:

	static BinkVideo *newBinkVideo(const char *name);

	virtual int   getWidth() const;
	virtual int   getHeight() const;
	virtual int   getLoopCount() const;

	virtual bool  canStretchBlt() const;

	virtual bool pause(bool enable);
	void goTo(unsigned frameNumber, unsigned binkGotoFlags);
	bool setVideoOnOff(bool on);
	bool setSoundOnOff(bool on);
	void setVolume(unsigned trackID, int volume) override;
	void setPan(unsigned trackID, int pan);
	unsigned getKeyFrame(unsigned frameNumber, unsigned binkGetKeyFlags);

	bool doFrame();
	void nextFrame();
	bool wait();
	bool shouldSkip();
	virtual void service() override;

	virtual bool performDrawing(int screenX, int screenY, int screenCX, int screenCY) override;
	virtual bool performBlitting(int screenX, int screenY) override;

	bool copyToBuffer(
		void *   destBuffer,
		int      destYStride,
		unsigned destHeight,
		unsigned destX,
		unsigned destY,
		unsigned binkCopyFlags
		);
	bool copyToBufferRect(
		void *   destBuffer,
		int      destYStride,
		unsigned destHeight,
		unsigned destX,
		unsigned destY,
		unsigned srcX,
		unsigned srcY,
		unsigned srcWidth,
		unsigned srcHeight,
		unsigned binkCopyFlags
		);
	int    getRects(unsigned binkGetRectsFlags);
	void   getFrameBuffersInfo(void *set);
	void   registerFrameBuffers(const void *set);
	void   getSummary(void *summary);
	void   getRealtime(void *realTime, unsigned frameWindowLength);
	bool   controlBackgroundIO(unsigned binkBGControlFlags);
	unsigned getTrackID(unsigned trackIndex);
	static unsigned DX9SurfaceType(struct IDirect3DSurface9 *lpDirect3DSurface);

private:

	BinkVideo();
	BinkVideo(const BinkVideo &);
	BinkVideo &operator =(const BinkVideo &);

	BinkVideo(const char *name, VlcVideoContext *ctx);
	virtual ~BinkVideo();

	bool _isFirstFrame() const;
	bool _isFinished() const;
	bool _isPlaying() const;

	void _doFrame();
	void _nextFrame();

	VlcVideoContext    *m_ctx;

	int                 m_loopCount;
	bool                m_didFrame;
	bool                m_nextFrame;
};

// ======================================================================

#endif
