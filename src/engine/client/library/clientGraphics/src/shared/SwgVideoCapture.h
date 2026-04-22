// ======================================================================
//
// SwgVideoCapture.h
// copyright (c) 2009 Sony Online Entertainment
//
// ======================================================================

#ifndef VIDEOCAPTURE_SWGVIDEOCAPTURE_H
#define VIDEOCAPTURE_SWGVIDEOCAPTURE_H

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"

#if PRODUCTION == 0

namespace AudioCapture
{

class IManager;

} // AudioCapture

namespace VideoCapture
{

#if defined(_WIN64)
// Legacy SOE VideoCapture stack is x86-only (vendor libs). Dev-build hooks remain as no-ops on x64.
inline void install()
{
}

namespace SingleUse
{

class ICallback
{
public:
	virtual ~ICallback()
	{
	}
	virtual void OnStart() = 0;
	virtual void OnStop() = 0;
};

inline void config(int, int, int, const char*, AudioCapture::IManager*)
{
}
inline void start(ICallback*, AudioCapture::IManager*)
{
}
inline void stop()
{
}
inline void run()
{
}

} // namespace SingleUse

#else

void install(); // Installs SoeUtilMemoryAdapter

namespace SingleUse
{

class ICallback
{
public:
	virtual ~ICallback()
	{
	}
	virtual void OnStart() = 0;
	virtual void OnStop() = 0;
};

void config(int resolution, int seconds, int quality, const char* filename, AudioCapture::IManager* pAudioCaptureManager);
void start(VideoCapture::SingleUse::ICallback* pVideoCaptureCallback, AudioCapture::IManager* pAudioCaptureManager);
void stop();
void run();

} // namespace SingleUse

#endif // _WIN64

} // namespace VideoCapture

#endif // PRODUCTION

#endif // VIDEOCAPTURE_SWGVIDEOCAPTURE_H
