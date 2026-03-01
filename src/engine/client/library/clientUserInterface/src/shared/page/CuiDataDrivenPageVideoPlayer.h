// ======================================================================
//
// CuiDataDrivenPageVideoPlayer.h
//
// ======================================================================

#ifndef INCLUDED_CuiDataDrivenPageVideoPlayer_H
#define INCLUDED_CuiDataDrivenPageVideoPlayer_H

#include "clientUserInterface/CuiDataDrivenPage.h"
#include "sharedObject/CachedNetworkId.h"
#include "UIEventCallback.h"

#include <cstdint>

class UIPage;
class UISliderbar;
class UIText;

// ======================================================================

class CuiDataDrivenPageVideoPlayer :
public CuiDataDrivenPage,
public UIEventCallback
{
public:
	                           CuiDataDrivenPageVideoPlayer(const std::string & name, UIPage & thePage, int clientPageId);
	virtual                    ~CuiDataDrivenPageVideoPlayer();
	virtual void               update                      (float deltaTimeSecs);

	virtual void               OnSliderbarChanged          (UIWidget * context);

protected:
	virtual void               performActivate             ();
	virtual void               performDeactivate           ();
	virtual void               onSetProperty   (std::string const & widgetPath, bool isThisPage, std::string const & propertyName, Unicode::String const & propertyValue);

private:
	CuiDataDrivenPageVideoPlayer            ();
	CuiDataDrivenPageVideoPlayer            (const CuiDataDrivenPageVideoPlayer & rhs);
	CuiDataDrivenPageVideoPlayer & operator=(const CuiDataDrivenPageVideoPlayer & rhs);

	static void formatTime(int64_t totalSeconds, char * buffer, size_t bufferSize);

private:
	UISliderbar * m_slider;
	UIText *      m_lblTime;
	CachedNetworkId m_videoObjectId;
	bool          m_tracking;
	bool          m_userSeeking;
	int64_t       m_lastTimeMs;
};

// ======================================================================

#endif
