// ======================================================================
//
// CuiDataDrivenPageVideoPlayer.cpp
//
// ======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiDataDrivenPageVideoPlayer.h"

#include "UIPage.h"
#include "UISliderbar.h"
#include "UIText.h"
#include "clientGame/TangibleObject.h"
#include "sharedObject/NetworkIdManager.h"

#include <cstdio>

// ======================================================================

CuiDataDrivenPageVideoPlayer::CuiDataDrivenPageVideoPlayer(const std::string & name, UIPage & page, int clientPageId) :
CuiDataDrivenPage(name, page, clientPageId),
m_slider(NULL),
m_lblTime(NULL),
m_videoObjectId(),
m_tracking(false),
m_userSeeking(false),
m_lastTimeMs(-1)
{
	getCodeDataObject(TUISliderbar, m_slider,  "slider");
	getCodeDataObject(TUIText,      m_lblTime, "lblTime");

	if (m_slider)
	{
		m_slider->SetLowerLimit(0);
		m_slider->SetUpperLimit(100);
		m_slider->SetValue(0, false);
	}

	if (m_lblTime)
	{
		m_lblTime->SetPreLocalized(true);
		m_lblTime->SetLocalText(Unicode::narrowToWide("00:00:00 / 00:00:00"));
	}
}

//-----------------------------------------------------------------

CuiDataDrivenPageVideoPlayer::~CuiDataDrivenPageVideoPlayer()
{
	if (m_slider)
		m_slider->RemoveCallback(this);

	setIsUpdating(false);
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::performActivate()
{
	CuiDataDrivenPage::performActivate();

	if (m_slider)
		m_slider->AddCallback(this);
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::performDeactivate()
{
	if (m_slider)
		m_slider->RemoveCallback(this);

	CuiDataDrivenPage::performDeactivate();
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::OnSliderbarChanged(UIWidget * context)
{
	if (context == m_slider && m_tracking)
	{
		m_userSeeking = true;

		long const sliderValue = m_slider->GetValue();
		int64_t const seekMs = static_cast<int64_t>(sliderValue) * 1000;

		Object * const obj = NetworkIdManager::getObjectById(m_videoObjectId);
		TangibleObject const * const tangible = TangibleObject::asTangibleObject(obj);

		if (tangible)
			TangibleObject::seekVideoPlayback(tangible, seekMs);
	}
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::formatTime(int64_t totalSeconds, char * buffer, size_t bufferSize)
{
	if (totalSeconds < 0)
		totalSeconds = 0;

	int const hours   = static_cast<int>(totalSeconds / 3600);
	int const minutes = static_cast<int>((totalSeconds % 3600) / 60);
	int const seconds = static_cast<int>(totalSeconds % 60);

	_snprintf(buffer, bufferSize - 1, "%02d:%02d:%02d", hours, minutes, seconds);
	buffer[bufferSize - 1] = '\0';
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::update(float deltaTimeSecs)
{
	if (m_tracking && m_slider && m_lblTime)
	{
		Object * const obj = NetworkIdManager::getObjectById(m_videoObjectId);
		TangibleObject const * const tangible = TangibleObject::asTangibleObject(obj);

		int64_t timeMs = 0;
		int64_t lengthMs = 0;

		if (tangible && TangibleObject::getVideoPlaybackInfo(tangible, timeMs, lengthMs))
		{
			int64_t const timeSec = timeMs / 1000;
			int64_t const lengthSec = lengthMs / 1000;

			long const upperLimit = static_cast<long>(lengthSec > 0 ? lengthSec : 100);
			long const value = static_cast<long>(timeSec);

			if (m_slider->GetUpperLimit() != upperLimit)
				m_slider->SetUpperLimit(upperLimit, false);

			if (m_userSeeking)
			{
				long const sliderVal = m_slider->GetValue();
				int64_t const diff = static_cast<int64_t>(sliderVal) - timeSec;
				if (diff >= -1 && diff <= 1)
					m_userSeeking = false;
			}

			if (!m_userSeeking)
				m_slider->SetValue(value, false);

			char currentBuf[16];
			char totalBuf[16];
			formatTime(timeSec, currentBuf, sizeof(currentBuf));
			formatTime(lengthSec, totalBuf, sizeof(totalBuf));

			char combined[40];
			_snprintf(combined, sizeof(combined) - 1, "%s / %s", currentBuf, totalBuf);
			combined[sizeof(combined) - 1] = '\0';

			m_lblTime->SetLocalText(Unicode::narrowToWide(combined));
			m_lastTimeMs = timeMs;
		}
		else
		{
			if (m_lastTimeMs > 0)
			{
				m_slider->SetValue(0, false);
				m_lblTime->SetLocalText(Unicode::narrowToWide("00:00:00 / 00:00:00"));
				m_lastTimeMs = 0;
			}
		}
	}

	CuiDataDrivenPage::update(deltaTimeSecs);
}

//-----------------------------------------------------------------

void CuiDataDrivenPageVideoPlayer::onSetProperty(std::string const & widgetPath, bool isThisPage, std::string const & propertyName, Unicode::String const & propertyValue)
{
	if (isThisPage && (_stricmp(propertyName.c_str(), "videoPlayerTimeValue") == 0))
	{
		std::string const narrowValue = Unicode::wideToNarrow(propertyValue);
		if (!narrowValue.empty())
		{
			m_videoObjectId = NetworkId(narrowValue);
			m_tracking = true;
			m_lastTimeMs = -1;
			setIsUpdating(true);
		}
	}

	CuiDataDrivenPage::onSetProperty(widgetPath, isThisPage, propertyName, propertyValue);
}

// ======================================================================
