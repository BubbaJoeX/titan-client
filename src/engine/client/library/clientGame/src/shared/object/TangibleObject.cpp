// ======================================================================
//
// TangibleObject.cpp
// Copyright 2001, 2002 Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/TangibleObject.h"

#include "clientGame/ClientCollisionProperty.h"
#include "clientGame/ClientCommandQueue.h"
#include "clientGame/ClientDataFile.h"
#include "clientGame/ClientSynchronizedUi.h"
#include "clientGame/ClientTangibleDynamics.h"
#include "clientGame/ClientTangibleObjectTemplate.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/ContainerInterface.h"
#include "clientGame/Game.h"
#include "clientGame/ManufactureSchematicObject.h"
#include "clientGame/PlayerObject.h"
#include "clientGame/RtCameraManager.h"
#include "clientGame/SlotRuleManager.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "clientParticle/ParticleEffectAppearance.h"
#include "clientParticle/ParticleEffectAppearanceTemplate.h"
#include "clientSkeletalAnimation/SkeletalAppearance2.h"
#include "clientUserInterface/CuiObjectTextManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiStringVariablesManager.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ConstCharCrcLowerString.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/DebugInfoManager.h"
#include "sharedFoundation/Production.h"
#include "sharedGame/GameObjectTypes.h"
#include "sharedGame/PvpData.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedGame/SharedStringIds.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMath/Transform.h"
#include "sharedMathArchive/VectorArchive.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "sharedNetworkMessages/MessageQueueSitOnObject.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Container.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/CustomizationDataProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/RotationDynamics.h"
#include "sharedObject/SlottedContainer.h"
#include "sharedFoundation/Tag.h"
#include "sharedUtility/Callback.h"

#include <map>
#include <vector>
#include <cctype>
#include <cfloat>
#include <cstdlib>
#include <string.h>

#include <windows.h>
#include <winhttp.h>
#include <objidl.h>
#include <propidl.h>
#include <wincodec.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

// libVLC types and function pointer typedefs for dynamic loading
typedef struct libvlc_instance_t libvlc_instance_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_player_t libvlc_media_player_t;
typedef __int64 libvlc_time_t;
typedef void (*libvlc_video_lock_cb)(void *opaque, void **planes);
typedef void (*libvlc_video_unlock_cb)(void *opaque, void *picture, void *const *planes);
typedef void (*libvlc_video_display_cb)(void *opaque, void *picture);
typedef void (*libvlc_audio_play_cb)(void *data, const void *samples, unsigned count, __int64 pts);
typedef void (*libvlc_audio_pause_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_resume_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_flush_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_drain_cb)(void *data);

typedef libvlc_instance_t *     (*pfn_libvlc_new)(int argc, const char *const *argv);
typedef void                    (*pfn_libvlc_release)(libvlc_instance_t *p_instance);
typedef libvlc_media_t *        (*pfn_libvlc_media_new_location)(libvlc_instance_t *p_instance, const char *psz_mrl);
typedef void                    (*pfn_libvlc_media_release)(libvlc_media_t *p_md);
typedef libvlc_media_player_t * (*pfn_libvlc_media_player_new)(libvlc_instance_t *p_instance);
typedef void                    (*pfn_libvlc_media_player_release)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_set_media)(libvlc_media_player_t *p_mi, libvlc_media_t *p_md);
typedef int                     (*pfn_libvlc_media_player_play)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_stop)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_set_time)(libvlc_media_player_t *p_mi, libvlc_time_t i_time);
typedef void                    (*pfn_libvlc_video_set_callbacks)(libvlc_media_player_t *mp, libvlc_video_lock_cb lock, libvlc_video_unlock_cb unlock, libvlc_video_display_cb display, void *opaque);
typedef void                    (*pfn_libvlc_video_set_format)(libvlc_media_player_t *mp, const char *chroma, unsigned width, unsigned height, unsigned pitch);
typedef void                    (*pfn_libvlc_media_add_option)(libvlc_media_t *p_md, const char *psz_options);
typedef int                     (*pfn_libvlc_audio_set_volume)(libvlc_media_player_t *p_mi, int i_volume);
typedef void                    (*pfn_libvlc_audio_set_callbacks)(libvlc_media_player_t *mp, libvlc_audio_play_cb play, libvlc_audio_pause_cb pause, libvlc_audio_resume_cb resume, libvlc_audio_flush_cb flush, libvlc_audio_drain_cb drain, void *opaque);
typedef void                    (*pfn_libvlc_audio_set_format)(libvlc_media_player_t *mp, const char *format, unsigned rate, unsigned channels);
typedef libvlc_time_t           (*pfn_libvlc_media_player_get_time)(libvlc_media_player_t *p_mi);
typedef libvlc_time_t           (*pfn_libvlc_media_player_get_length)(libvlc_media_player_t *p_mi);

// ======================================================================

//lint -e1734 // difficulty compiling safe_cast template

// ======================================================================

class TangibleObject::CraftingToolSyncUi : public ClientSynchronizedUi
{
public:
	CraftingToolSyncUi(TangibleObject & owner) :
	  ClientSynchronizedUi(owner)
	  {
		  addToUiPackage(m_prototype);
		  addToUiPackage(m_manfSchematic);
	  }

	  const CachedNetworkId & getPrototype(void) const
	  {
		  return m_prototype.get();
	  }

	  const CachedNetworkId & getManfSchematic(void) const
	  {
		  return m_manfSchematic.get();
	  }

private:

	CraftingToolSyncUi(const CraftingToolSyncUi & src);
	CraftingToolSyncUi & operator =(const CraftingToolSyncUi & src);

private:
	Archive::AutoDeltaVariable<CachedNetworkId> m_prototype;
	Archive::AutoDeltaVariable<CachedNetworkId> m_manfSchematic;
};

// ======================================================================

namespace TangibleObjectNamespace
{
	enum RemoteImageFetchState
	{
		RIFS_idle = 0,
		RIFS_fetching = 1,
		RIFS_ready = 2,
		RIFS_failed = 3
	};

	enum MagicPaintingDisplayMode
	{
		MPDM_cube = 0,
		MPDM_flat = 1,
		MPDM_doubleSided = 2
	};

	struct RemoteImageRuntimeData
	{
		RemoteImageRuntimeData() :
			owner(0),
			appliedUrl(),
			requestedUrl(),
			texture(0),
			overlayObject(0),
			overlayBackObject(0),
			fetchThread(0),
			fetchState(RIFS_idle),
			fetchedBytes(0),
			nonAdminPictureOnlyActive(false),
			nameOverridden(false),
			appliedDisplayMode(MPDM_cube),
			appliedDisplayModeStr(),
			scrollH(0.0f),
			scrollV(0.0f),
			appliedScrollH(0.0f),
			appliedScrollV(0.0f),
			isPaintingTemplate(false),
			isPaintingTemplateResolved(false),
			appliedPictureOnly(false),
			dirty(true),
			settled(false),
			gifFrames(),
			gifFrameDelays(),
			gifCurrentFrame(0),
			gifFrameTimer(0.0f),
			isGif(false)
		{
		}

		TangibleObject const * owner;
		std::string appliedUrl;
		std::string requestedUrl;
		Texture const * texture;
		Object * overlayObject;
		Object * overlayBackObject;
		HANDLE fetchThread;
		LONG fetchState;
		std::vector<unsigned char> * fetchedBytes;
		bool nonAdminPictureOnlyActive;
		bool nameOverridden;
		Unicode::String originalObjectName;
		MagicPaintingDisplayMode appliedDisplayMode;
		std::string appliedDisplayModeStr;
		float scrollH;
		float scrollV;
		float appliedScrollH;
		float appliedScrollV;
		bool isPaintingTemplate;
		bool isPaintingTemplateResolved;
		bool appliedPictureOnly;
		bool dirty;
		bool settled;
		std::vector<Texture const *> gifFrames;
		std::vector<float> gifFrameDelays;
		int gifCurrentFrame;
		float gifFrameTimer;
		bool isGif;
	};

	struct RemoteImageFetchThreadData
	{
		RemoteImageFetchThreadData(RemoteImageRuntimeData * runtimeData, std::string const & sourceUrl) :
			runtime(runtimeData),
			url(sourceUrl)
		{
		}

		RemoteImageRuntimeData * runtime;
		std::string url;
	};

	typedef std::map<TangibleObject const *, RemoteImageRuntimeData> RemoteImageRuntimeDataMap;
	RemoteImageRuntimeDataMap ms_remoteImageRuntimeDataMap;

	// Media distance tracking - only render closest media objects to conserve GPU memory
	float const cs_mediaMaxRenderDistance = 100.0f;  // Maximum distance to render any media
	float const cs_mediaMaxRenderDistanceSquared = cs_mediaMaxRenderDistance * cs_mediaMaxRenderDistance;
	int const cs_maxActivePaintings = 1;  // Only render 1 painting at a time
	int const cs_maxActiveVideoPlayers = 1;  // Only render 1 video player at a time
	int const cs_maxActiveRtScreens = 1;  // Only render 1 RT screen at a time

	// Track the currently active media object (closest one)
	TangibleObject const * ms_closestPainting = nullptr;
	TangibleObject const * ms_closestVideoPlayer = nullptr;
	TangibleObject const * ms_closestRtScreen = nullptr;
	float ms_closestPaintingDistSq = FLT_MAX;
	float ms_closestVideoPlayerDistSq = FLT_MAX;
	float ms_closestRtScreenDistSq = FLT_MAX;

	// Frame tracking for resetting distance calculations
	unsigned long ms_lastMediaFrameTime = 0;

	// Reset tracking each frame (called automatically when frame time changes)
	void checkAndResetMediaDistanceTracking()
	{
		unsigned long const currentTime = GetTickCount();
		// Reset if more than 10ms since last check (new frame)
		if (currentTime - ms_lastMediaFrameTime > 10)
		{
			ms_closestPainting = nullptr;
			ms_closestVideoPlayer = nullptr;
			ms_closestRtScreen = nullptr;
			ms_closestPaintingDistSq = FLT_MAX;
			ms_closestVideoPlayerDistSq = FLT_MAX;
			ms_closestRtScreenDistSq = FLT_MAX;
			ms_lastMediaFrameTime = currentTime;
		}
	}

	// Check if this object should be the active media renderer based on distance
	bool shouldRenderMedia(TangibleObject const * obj, TangibleObject const *& closestTracker, float & closestDistSq)
	{
		if (!obj || !obj->isInWorld())
			return false;

		// Reset tracking if this is a new frame
		checkAndResetMediaDistanceTracking();

		Camera const * playerCamera = Game::getCamera();
		if (!playerCamera)
			return true;  // No camera, allow rendering

		Vector const objPos = obj->getPosition_w();
		Vector const camPos = playerCamera->getPosition_w();
		float const distSq = objPos.magnitudeBetweenSquared(camPos);

		// Too far away
		if (distSq > cs_mediaMaxRenderDistanceSquared)
			return false;

		// Check if we're the closest
		if (distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closestTracker = obj;
		}

		// Only render if we're the closest
		return (closestTracker == obj);
	}

	Tag const TAG_MAIN = TAG(M,A,I,N);
	float const cs_magicPaintingOverlayHeight = 2.0f;
	float const cs_magicPaintingOverlayBaseScale = 2.0f;
	float const cs_magicPaintingOverlayBackYaw = 3.14159265358979323846f;

	bool isHttpUrl(std::string const & url)
	{
		std::string lower(url);
		for (std::string::iterator i = lower.begin(); i != lower.end(); ++i)
			*i = static_cast<char>(std::tolower(static_cast<unsigned char>(*i)));

		return (lower.find("http://") == 0) || (lower.find("https://") == 0);
	}

	MagicPaintingDisplayMode parseDisplayMode(std::string const & mode)
	{
		if (mode.empty())
			return MPDM_cube;

		std::string upper(mode);
		for (std::string::iterator i = upper.begin(); i != upper.end(); ++i)
			*i = static_cast<char>(std::toupper(static_cast<unsigned char>(*i)));

		if (upper == "FLAT")
			return MPDM_flat;
		if (upper == "DOUBLE_SIDED")
			return MPDM_doubleSided;

		return MPDM_cube;
	}

	bool isTailorPngRemoteMode(std::string const & mode)
	{
		if (mode.empty())
			return false;
		return _stricmp(mode.c_str(), "TAILOR_PNG") == 0;
	}

	bool isGifUrl(std::string const & url)
	{
		if (url.size() < 4)
			return false;
		std::string ext = url.substr(url.size() - 4);
		for (std::string::iterator i = ext.begin(); i != ext.end(); ++i)
			*i = static_cast<char>(std::tolower(static_cast<unsigned char>(*i)));
		return ext == ".gif";
	}

	bool templateNameContainsPainting(char const * templateName)
	{
		if (!templateName)
			return false;
		std::string lower(templateName);
		for (std::string::iterator i = lower.begin(); i != lower.end(); ++i)
			*i = static_cast<char>(std::tolower(static_cast<unsigned char>(*i)));
		return lower.find("painting") != std::string::npos;
	}

	Appearance * createMagicPaintingOverlayAppearance()
	{
		Appearance * appearance = AppearanceTemplateList::createAppearance("appearance/defaultappearance.apt");
		if (!appearance)
			appearance = AppearanceTemplateList::createAppearance("appearance/info_disk.apt");
		return appearance;
	}

	void removeOverlayObject(TangibleObject & owner, Object *& objectToRemove)
	{
		if (!objectToRemove)
			return;

		if (objectToRemove->isInWorld())
			objectToRemove->removeFromWorld();

		if (objectToRemove->getAttachedTo() == &owner)
			owner.removeChildObject(objectToRemove, Object::DF_none);

		if (objectToRemove->isInWorld())
			objectToRemove->removeFromWorld();

		delete objectToRemove;
		objectToRemove = 0;
	}

	void updateOverlayObjectTransform(Object & overlayObject, bool backFace, MagicPaintingDisplayMode displayMode, bool isPaintingTemplate)
	{
		Transform overlayTransform = Transform::identity;

		float const yOffset = isPaintingTemplate ? 0.0f : cs_magicPaintingOverlayHeight;
		overlayTransform.setPosition_p(Vector(0.0f, yOffset, 0.0f));

		if (displayMode == MPDM_flat || displayMode == MPDM_doubleSided)
		{
			overlayTransform.pitch_l(1.5707963267948966f);
			if (backFace)
				overlayTransform.yaw_l(cs_magicPaintingOverlayBackYaw);
		}
		else
		{
			if (backFace)
				overlayTransform.yaw_l(cs_magicPaintingOverlayBackYaw);
		}

		overlayObject.setTransform_o2p(overlayTransform);
	}

	void applyCachedRuntimeTextureToSurfaces(RemoteImageRuntimeData & runtimeData, Appearance * ownerAppearance)
	{
		if (!runtimeData.texture)
			return;

		if (runtimeData.isPaintingTemplate)
		{
			if (ownerAppearance)
				ownerAppearance->setTexture(TAG_MAIN, *runtimeData.texture);
			return;
		}

		Appearance * const overlayAppearance = runtimeData.overlayObject ? runtimeData.overlayObject->getAppearance() : 0;
		if (overlayAppearance)
			overlayAppearance->setTexture(TAG_MAIN, *runtimeData.texture);

		Appearance * const overlayBackAppearance = runtimeData.overlayBackObject ? runtimeData.overlayBackObject->getAppearance() : 0;
		if (overlayBackAppearance)
			overlayBackAppearance->setTexture(TAG_MAIN, *runtimeData.texture);

		if (ownerAppearance)
			ownerAppearance->setTexture(TAG_MAIN, *runtimeData.texture);
	}

	void createOverlayObject(TangibleObject & owner, RemoteImageRuntimeData & runtimeData, Object *& target, bool backFace, MagicPaintingDisplayMode displayMode)
	{
		if (target)
			return;

		if (!owner.isInWorld())
			return;

		Appearance * const appearance = createMagicPaintingOverlayAppearance();
		if (!appearance)
			return;

		target = new Object();
		target->setAppearance(appearance);
		target->addNotification(ClientWorld::getIntangibleNotification());
		RenderWorld::addObjectNotifications(*target);

		updateOverlayObjectTransform(*target, backFace, displayMode, runtimeData.isPaintingTemplate);
		target->setScale(Vector(cs_magicPaintingOverlayBaseScale, cs_magicPaintingOverlayBaseScale, cs_magicPaintingOverlayBaseScale));

		bool const portalDisabled = (owner.getCellProperty() != 0);
		if (portalDisabled)
		{
			CellProperty::setPortalTransitionsEnabled(false);
			target->setParentCell(owner.getCellProperty());
		}

		target->attachToObject_w(&owner, true);
		target->addToWorld();

		if (runtimeData.texture)
			appearance->setTexture(TAG_MAIN, *runtimeData.texture);

		if (portalDisabled)
			CellProperty::setPortalTransitionsEnabled(true);
	}

	void ensureRemoteImageOverlayObjects(TangibleObject & owner, RemoteImageRuntimeData & runtimeData, MagicPaintingDisplayMode displayMode, bool forceRecreate)
	{
		if (runtimeData.isPaintingTemplate)
		{
			removeOverlayObject(owner, runtimeData.overlayBackObject);
			removeOverlayObject(owner, runtimeData.overlayObject);
			return;
		}

		if (forceRecreate)
		{
			removeOverlayObject(owner, runtimeData.overlayBackObject);
			removeOverlayObject(owner, runtimeData.overlayObject);
		}

		createOverlayObject(owner, runtimeData, runtimeData.overlayObject, false, displayMode);
		if (!runtimeData.overlayObject)
			return;

		if (displayMode == MPDM_doubleSided)
		{
			createOverlayObject(owner, runtimeData, runtimeData.overlayBackObject, true, displayMode);
		}
		else
		{
			removeOverlayObject(owner, runtimeData.overlayBackObject);
		}

		if (runtimeData.overlayObject)
			updateOverlayObjectTransform(*runtimeData.overlayObject, false, displayMode, runtimeData.isPaintingTemplate);
		if (runtimeData.overlayBackObject)
			updateOverlayObjectTransform(*runtimeData.overlayBackObject, true, displayMode, runtimeData.isPaintingTemplate);
	}

	void clearRemoteImageOverlayObjects(TangibleObject & owner, RemoteImageRuntimeData & runtimeData)
	{
		removeOverlayObject(owner, runtimeData.overlayBackObject);
		removeOverlayObject(owner, runtimeData.overlayObject);
	}

	bool wideFromUtf8OrAnsi(std::string const & input, std::wstring & output)
	{
		output.clear();
		if (input.empty())
			return false;

		int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), static_cast<int>(input.size()), 0, 0);
		if (wideLength <= 0)
			wideLength = MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()), 0, 0);
		if (wideLength <= 0)
			return false;

		output.resize(static_cast<size_t>(wideLength));
		if (MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), &output[0], wideLength) <= 0)
		{
			if (MultiByteToWideChar(CP_ACP, 0, input.c_str(), static_cast<int>(input.size()), &output[0], wideLength) <= 0)
				return false;
		}

		return true;
	}

	bool fetchUrlBytes(std::string const & url, std::vector<unsigned char> & bytes)
	{
		std::wstring urlWide;
		if (!wideFromUtf8OrAnsi(url, urlWide))
			return false;

		URL_COMPONENTS components;
		memset(&components, 0, sizeof(components));
		components.dwStructSize = sizeof(components);
		components.dwSchemeLength = static_cast<DWORD>(-1);
		components.dwHostNameLength = static_cast<DWORD>(-1);
		components.dwUrlPathLength = static_cast<DWORD>(-1);
		components.dwExtraInfoLength = static_cast<DWORD>(-1);
		if (!WinHttpCrackUrl(urlWide.c_str(), static_cast<DWORD>(urlWide.size()), 0, &components))
			return false;

		std::wstring hostName(components.lpszHostName, components.dwHostNameLength);
		std::wstring objectPath;
		if (components.dwUrlPathLength > 0)
			objectPath.assign(components.lpszUrlPath, components.dwUrlPathLength);
		if (components.dwExtraInfoLength > 0)
			objectPath.append(components.lpszExtraInfo, components.dwExtraInfoLength);
		if (objectPath.empty())
			objectPath = L"/";

		HINTERNET const hSession = WinHttpOpen(L"SWGTitan/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
			return false;

		HINTERNET const hConnect = WinHttpConnect(hSession, hostName.c_str(), components.nPort, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD requestFlags = 0;
		if (components.nScheme == INTERNET_SCHEME_HTTPS)
			requestFlags |= WINHTTP_FLAG_SECURE;

		HINTERNET const hRequest = WinHttpOpenRequest(hConnect, L"GET", objectPath.c_str(), 0, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		bool success = false;
		if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(hRequest, 0))
		{
			DWORD bytesRead = 0;
			unsigned char buffer[8192];
			do
			{
				bytesRead = 0;
				if (!WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead))
					break;
				if (bytesRead > 0)
					bytes.insert(bytes.end(), buffer, buffer + bytesRead);
			}
			while (bytesRead > 0);

			success = !bytes.empty();
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return success;
	}

	DWORD WINAPI remoteImageFetchThreadProc(LPVOID context)
	{
		RemoteImageFetchThreadData * const threadData = reinterpret_cast<RemoteImageFetchThreadData *>(context);
		if (!threadData || !threadData->runtime)
			return 1;

		RemoteImageRuntimeData * const runtime = threadData->runtime;
		std::vector<unsigned char> * bytes = new std::vector<unsigned char>();
		if (fetchUrlBytes(threadData->url, *bytes))
		{
			std::vector<unsigned char> * const oldBytes = reinterpret_cast<std::vector<unsigned char> *>(InterlockedExchangePointer(reinterpret_cast<PVOID *>(&runtime->fetchedBytes), bytes));
			if (oldBytes)
				delete oldBytes;

			InterlockedExchange(&runtime->fetchState, RIFS_ready);
		}
		else
		{
			delete bytes;
			InterlockedExchange(&runtime->fetchState, RIFS_failed);
		}

		delete threadData;
		return 0;
	}

	void ensureComInitialized()
	{
		static bool s_comInitialized = false;
		if (!s_comInitialized)
		{
			CoInitializeEx(0, COINIT_MULTITHREADED);
			s_comInitialized = true;
		}
	}

	bool decodeImageBytesToTexture(std::vector<unsigned char> const & bytes, Texture const *& texture)
	{
		texture = 0;
		if (bytes.empty())
			return false;

		ensureComInitialized();

		IWICImagingFactory * factory = 0;
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void **>(&factory));
		if (FAILED(hr) || !factory)
			return false;

		IWICStream * wicStream = 0;
		hr = factory->CreateStream(&wicStream);
		if (FAILED(hr) || !wicStream)
		{
			factory->Release();
			return false;
		}

		hr = wicStream->InitializeFromMemory(const_cast<unsigned char *>(&bytes[0]), static_cast<DWORD>(bytes.size()));
		if (FAILED(hr))
		{
			wicStream->Release();
			factory->Release();
			return false;
		}

		IWICBitmapDecoder * decoder = 0;
		hr = factory->CreateDecoderFromStream(wicStream, 0, WICDecodeMetadataCacheOnDemand, &decoder);
		if (FAILED(hr) || !decoder)
		{
			wicStream->Release();
			factory->Release();
			return false;
		}

		IWICBitmapFrameDecode * frame = 0;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr) || !frame)
		{
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		IWICFormatConverter * converter = 0;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr) || !converter)
		{
			frame->Release();
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{
			converter->Release();
			frame->Release();
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		UINT width = 0;
		UINT height = 0;
		converter->GetSize(&width, &height);
		if (width == 0 || height == 0)
		{
			converter->Release();
			frame->Release();
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		UINT const stride = width * 4u;
		std::vector<unsigned char> pixels(static_cast<size_t>(stride) * static_cast<size_t>(height));
		hr = converter->CopyPixels(0, stride, static_cast<UINT>(pixels.size()), &pixels[0]);

		bool result = false;
		if (SUCCEEDED(hr))
		{
			TextureFormat const runtimeFormats[] = { TF_ARGB_8888 };
			texture = TextureList::fetch(&pixels[0], TF_ARGB_8888, static_cast<int>(width), static_cast<int>(height), runtimeFormats, 1);
			result = (texture != 0);
		}

		converter->Release();
		frame->Release();
		decoder->Release();
		wicStream->Release();
		factory->Release();
		return result;
	}

	bool decodeGifFrames(std::vector<unsigned char> const & bytes, std::vector<Texture const *> & frames, std::vector<float> & delays)
	{
		frames.clear();
		delays.clear();
		if (bytes.empty())
			return false;

		ensureComInitialized();

		IWICImagingFactory * factory = 0;
		HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void **>(&factory));
		if (FAILED(hr) || !factory)
			return false;

		IWICStream * wicStream = 0;
		hr = factory->CreateStream(&wicStream);
		if (FAILED(hr) || !wicStream)
		{
			factory->Release();
			return false;
		}

		hr = wicStream->InitializeFromMemory(const_cast<unsigned char *>(&bytes[0]), static_cast<DWORD>(bytes.size()));
		if (FAILED(hr))
		{
			wicStream->Release();
			factory->Release();
			return false;
		}

		IWICBitmapDecoder * decoder = 0;
		hr = factory->CreateDecoderFromStream(wicStream, 0, WICDecodeMetadataCacheOnDemand, &decoder);
		if (FAILED(hr) || !decoder)
		{
			wicStream->Release();
			factory->Release();
			return false;
		}

		UINT frameCount = 0;
		decoder->GetFrameCount(&frameCount);
		if (frameCount == 0)
		{
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		UINT gifWidth = 0;
		UINT gifHeight = 0;
		{
			IWICBitmapFrameDecode * firstFrame = 0;
			if (SUCCEEDED(decoder->GetFrame(0, &firstFrame)) && firstFrame)
			{
				firstFrame->GetSize(&gifWidth, &gifHeight);
				firstFrame->Release();
			}
		}
		if (gifWidth == 0 || gifHeight == 0)
		{
			decoder->Release();
			wicStream->Release();
			factory->Release();
			return false;
		}

		UINT const compositeStride = gifWidth * 4u;
		std::vector<unsigned char> compositeBuffer(static_cast<size_t>(compositeStride) * static_cast<size_t>(gifHeight), 0);

		for (UINT fi = 0; fi < frameCount; ++fi)
		{
			IWICBitmapFrameDecode * frame = 0;
			hr = decoder->GetFrame(fi, &frame);
			if (FAILED(hr) || !frame)
				continue;

			float frameDelay = 0.1f;
			IWICMetadataQueryReader * metaReader = 0;
			if (SUCCEEDED(frame->GetMetadataQueryReader(&metaReader)) && metaReader)
			{
				PROPVARIANT delayProp;
				PropVariantInit(&delayProp);
				if (SUCCEEDED(metaReader->GetMetadataByName(L"/grctlext/Delay", &delayProp)))
				{
					if (delayProp.vt == VT_UI2)
						frameDelay = static_cast<float>(delayProp.uiVal) * 0.01f;
					else if (delayProp.vt == VT_UI4)
						frameDelay = static_cast<float>(delayProp.ulVal) * 0.01f;
				}
				PropVariantClear(&delayProp);
				metaReader->Release();
			}
			if (frameDelay < 0.02f)
				frameDelay = 0.1f;

			UINT frameLeft = 0, frameTop = 0;
			IWICMetadataQueryReader * metaReader2 = 0;
			if (SUCCEEDED(frame->GetMetadataQueryReader(&metaReader2)) && metaReader2)
			{
				PROPVARIANT leftProp, topProp;
				PropVariantInit(&leftProp);
				PropVariantInit(&topProp);
				if (SUCCEEDED(metaReader2->GetMetadataByName(L"/imgdesc/Left", &leftProp)) && leftProp.vt == VT_UI2)
					frameLeft = leftProp.uiVal;
				if (SUCCEEDED(metaReader2->GetMetadataByName(L"/imgdesc/Top", &topProp)) && topProp.vt == VT_UI2)
					frameTop = topProp.uiVal;
				PropVariantClear(&leftProp);
				PropVariantClear(&topProp);
				metaReader2->Release();
			}

			IWICFormatConverter * converter = 0;
			hr = factory->CreateFormatConverter(&converter);
			if (FAILED(hr) || !converter)
			{
				frame->Release();
				continue;
			}

			hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, 0, 0.0, WICBitmapPaletteTypeCustom);
			if (FAILED(hr))
			{
				converter->Release();
				frame->Release();
				continue;
			}

			UINT fw = 0, fh = 0;
			converter->GetSize(&fw, &fh);
			if (fw > 0 && fh > 0)
			{
				UINT const frameStride = fw * 4u;
				std::vector<unsigned char> framePixels(static_cast<size_t>(frameStride) * static_cast<size_t>(fh));
				hr = converter->CopyPixels(0, frameStride, static_cast<UINT>(framePixels.size()), &framePixels[0]);
				if (SUCCEEDED(hr))
				{
					for (UINT row = 0; row < fh; ++row)
					{
						UINT const destRow = frameTop + row;
						if (destRow >= gifHeight)
							break;
						for (UINT col = 0; col < fw; ++col)
						{
							UINT const destCol = frameLeft + col;
							if (destCol >= gifWidth)
								break;
							size_t const srcIdx = (static_cast<size_t>(row) * frameStride) + (static_cast<size_t>(col) * 4u);
							size_t const dstIdx = (static_cast<size_t>(destRow) * compositeStride) + (static_cast<size_t>(destCol) * 4u);
							unsigned char const alpha = framePixels[srcIdx + 3];
							if (alpha > 0)
							{
								compositeBuffer[dstIdx + 0] = framePixels[srcIdx + 0];
								compositeBuffer[dstIdx + 1] = framePixels[srcIdx + 1];
								compositeBuffer[dstIdx + 2] = framePixels[srcIdx + 2];
								compositeBuffer[dstIdx + 3] = alpha;
							}
						}
					}

					TextureFormat const runtimeFormats[] = { TF_ARGB_8888 };
					Texture const * tex = TextureList::fetch(&compositeBuffer[0], TF_ARGB_8888, static_cast<int>(gifWidth), static_cast<int>(gifHeight), runtimeFormats, 1);
					if (tex)
					{
						frames.push_back(tex);
						delays.push_back(frameDelay);
					}
				}
			}

			converter->Release();
			frame->Release();
		}

		decoder->Release();
		wicStream->Release();
		factory->Release();
		return !frames.empty();
	}

	RemoteImageRuntimeData & getRemoteImageRuntimeData(TangibleObject const * tangibleObject)
	{
		RemoteImageRuntimeData & data = ms_remoteImageRuntimeDataMap[tangibleObject];
		if (!data.owner)
			data.owner = tangibleObject;
		return data;
	}

	void closeFinishedFetchThread(RemoteImageRuntimeData & runtimeData)
	{
		if (runtimeData.fetchThread && (InterlockedCompareExchange(&runtimeData.fetchState, RIFS_idle, RIFS_idle) != RIFS_fetching))
		{
			WaitForSingleObject(runtimeData.fetchThread, 0);
			CloseHandle(runtimeData.fetchThread);
			runtimeData.fetchThread = 0;
		}
	}

	void clearFetchedBytes(RemoteImageRuntimeData & runtimeData)
	{
		std::vector<unsigned char> * const oldBytes = reinterpret_cast<std::vector<unsigned char> *>(InterlockedExchangePointer(reinterpret_cast<PVOID *>(&runtimeData.fetchedBytes), 0));
		if (oldBytes)
			delete oldBytes;
	}

	void clearGifFrames(RemoteImageRuntimeData & runtimeData)
	{
		for (size_t i = 0; i < runtimeData.gifFrames.size(); ++i)
		{
			if (runtimeData.gifFrames[i])
				runtimeData.gifFrames[i]->release();
		}
		runtimeData.gifFrames.clear();
		runtimeData.gifFrameDelays.clear();
		runtimeData.gifCurrentFrame = 0;
		runtimeData.gifFrameTimer = 0.0f;
		runtimeData.isGif = false;
	}

	void applyTextureScrollToAppearance(Appearance * appearance, float scrollH, float scrollV)
	{
		if (!appearance)
			return;
		appearance->setTextureScroll(TAG_MAIN, scrollH, scrollV);
	}

	void applyTextureToSurface(Texture const * texture, Appearance * appearance)
	{
		if (texture && appearance)
			appearance->setTexture(TAG_MAIN, *texture);
	}

	void applyPictureOnlyPresentation(TangibleObject & object, RemoteImageRuntimeData & runtimeData, bool pictureOnly)
	{
		bool const applyNonAdminPictureOnly = pictureOnly && !PlayerObject::isAdmin();
		if (runtimeData.nonAdminPictureOnlyActive == applyNonAdminPictureOnly)
			return;

		if (applyNonAdminPictureOnly)
		{
			if (!runtimeData.nameOverridden)
			{
				runtimeData.originalObjectName = object.getObjectName();
				runtimeData.nameOverridden = true;
			}
			object.setObjectName(Unicode::emptyString);

			object.removeNotification(ClientWorld::getTangibleNotification(), true);
			object.addNotification(ClientWorld::getTangibleNotTargetableNotification(), true);

			Appearance * const appearance = object.getAppearance();
			if (appearance)
				appearance->setAlpha(false, 1.0f, true, 0.0f);
		}
		else
		{
			if (runtimeData.nameOverridden)
			{
				object.setObjectName(runtimeData.originalObjectName);
				runtimeData.originalObjectName = Unicode::emptyString;
				runtimeData.nameOverridden = false;
			}

			object.removeNotification(ClientWorld::getTangibleNotTargetableNotification(), true);
			object.addNotification(ClientWorld::getTangibleNotification(), true);

			Appearance * const appearance = object.getAppearance();
			if (appearance)
				appearance->setAlpha(true, 1.0f, true, 1.0f);
		}

		runtimeData.nonAdminPictureOnlyActive = applyNonAdminPictureOnly;
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	typedef std::vector<Object*>  ObjectVector;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	namespace Transceivers
	{
		MessageDispatch::Transceiver<const TangibleObject::Messages::DamageTaken::Payload &,  TangibleObject::Messages::DamageTaken>  damageTaken;
	}

	const ConstCharCrcLowerString  ms_meshVariableSetName("owner_mesh");
	const ConstCharCrcLowerString  ms_textureVariableSetName("owner_texture");

	const ConstCharCrcLowerString cs_chairHardpointName("foot");

	bool ms_useTestDamageLevel;
	bool ms_logChangedConditions;

	void remove ();

	const uint32 hash_combatTarget   = ConstCharCrcLowerString ("combattarget").getCrc ();
	const uint32 hash_combatUntarget = ConstCharCrcLowerString ("combatuntarget").getCrc ();

	std::string const & ms_debugInfoSectionName = "TangibleObject";
}

using namespace TangibleObjectNamespace;

// ======================================================================
// libVLC dynamic loading and video stream support
// ======================================================================

namespace VideoStreamNamespace
{
	struct VlcApi
	{
		HMODULE                          hLibVlc;
		pfn_libvlc_new                   pNew;
		pfn_libvlc_release               pRelease;
		pfn_libvlc_media_new_location    pMediaNewLocation;
		pfn_libvlc_media_release         pMediaRelease;
		pfn_libvlc_media_player_new      pMediaPlayerNew;
		pfn_libvlc_media_player_release  pMediaPlayerRelease;
		pfn_libvlc_media_player_set_media pMediaPlayerSetMedia;
		pfn_libvlc_media_player_play     pMediaPlayerPlay;
		pfn_libvlc_media_player_stop     pMediaPlayerStop;
		pfn_libvlc_media_player_set_time pMediaPlayerSetTime;
		pfn_libvlc_video_set_callbacks   pVideoSetCallbacks;
		pfn_libvlc_video_set_format      pVideoSetFormat;
		pfn_libvlc_media_add_option      pMediaAddOption;
		pfn_libvlc_audio_set_volume      pAudioSetVolume;
		pfn_libvlc_audio_set_callbacks   pAudioSetCallbacks;
		pfn_libvlc_audio_set_format      pAudioSetFormat;
		pfn_libvlc_media_player_get_time   pMediaPlayerGetTime;
		pfn_libvlc_media_player_get_length pMediaPlayerGetLength;
		libvlc_instance_t *              vlcInstance;
		bool                             loaded;
		bool                             loadAttempted;
	};

	VlcApi ms_vlcApi = {};

	bool loadVlcApi()
	{
		if (ms_vlcApi.loadAttempted)
			return ms_vlcApi.loaded;

		ms_vlcApi.loadAttempted = true;
		ms_vlcApi.loaded = false;

		ms_vlcApi.hLibVlc = LoadLibraryA("libvlc.dll");
		if (!ms_vlcApi.hLibVlc)
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to load libvlc.dll\n"));
			return false;
		}

		#define LOAD_VLC_FUNC(name, type) \
			ms_vlcApi.p##type = (pfn_libvlc_##name)GetProcAddress(ms_vlcApi.hLibVlc, "libvlc_" #name); \
			if (!ms_vlcApi.p##type) { DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to load libvlc_%s\n", #name)); return false; }

		LOAD_VLC_FUNC(new, New);
		LOAD_VLC_FUNC(release, Release);
		LOAD_VLC_FUNC(media_new_location, MediaNewLocation);
		LOAD_VLC_FUNC(media_release, MediaRelease);
		LOAD_VLC_FUNC(media_player_new, MediaPlayerNew);
		LOAD_VLC_FUNC(media_player_release, MediaPlayerRelease);
		LOAD_VLC_FUNC(media_player_set_media, MediaPlayerSetMedia);
		LOAD_VLC_FUNC(media_player_play, MediaPlayerPlay);
		LOAD_VLC_FUNC(media_player_stop, MediaPlayerStop);
		LOAD_VLC_FUNC(media_player_set_time, MediaPlayerSetTime);
		LOAD_VLC_FUNC(video_set_callbacks, VideoSetCallbacks);
		LOAD_VLC_FUNC(video_set_format, VideoSetFormat);
		LOAD_VLC_FUNC(media_add_option, MediaAddOption);
		LOAD_VLC_FUNC(audio_set_volume, AudioSetVolume);
		LOAD_VLC_FUNC(audio_set_callbacks, AudioSetCallbacks);
		LOAD_VLC_FUNC(audio_set_format, AudioSetFormat);
		LOAD_VLC_FUNC(media_player_get_time, MediaPlayerGetTime);
		LOAD_VLC_FUNC(media_player_get_length, MediaPlayerGetLength);

		#undef LOAD_VLC_FUNC

		const char * const vlcArgs[] = {
			"--no-xlib",
			"--no-video-title-show"
		};
		ms_vlcApi.vlcInstance = ms_vlcApi.pNew(2, vlcArgs);
		if (!ms_vlcApi.vlcInstance)
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to create VLC instance\n"));
			return false;
		}

		ms_vlcApi.loaded = true;
		DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: libVLC loaded successfully\n"));
		return true;
	}

	// Video player resolution - capped at 720p maximum to conserve GPU memory
	// Using 16:9 aspect ratio at 360p for optimal performance
	unsigned int const VIDEO_MAX_WIDTH = 1280;   // 720p max width
	unsigned int const VIDEO_MAX_HEIGHT = 720;   // 720p max height
	unsigned int const VIDEO_WIDTH = 640;        // Default 360p width
	unsigned int const VIDEO_HEIGHT = 360;       // Default 360p height
	unsigned int const VIDEO_PITCH = VIDEO_WIDTH * 4;

	enum ResolveState
	{
		RS_none,
		RS_pending,
		RS_done,
		RS_failed
	};

	struct VideoStreamRuntimeData
	{
		VideoStreamRuntimeData() :
			owner(0),
			appliedUrl(),
			requestedUrl(),
			resolvedUrl(),
			resolveState(RS_none),
			resolveThread(0),
			mediaPlayer(0),
			videoBuffer(0),
			videoBufferSize(0),
			texture(0),
			overlayObject(0),
			overlayBackObject(0),
			isPaintingTemplate(false),
			isPaintingTemplateResolved(false),
			dirty(true),
			settled(false),
			frameReady(0),
			appliedTimestamp(0),
			requestedTimestamp(0),
			looping(false)
		{
		}

		TangibleObject const * owner;
		std::string appliedUrl;
		std::string requestedUrl;
		std::string resolvedUrl;
		volatile ResolveState resolveState;
		HANDLE resolveThread;
		libvlc_media_player_t * mediaPlayer;
		unsigned char * videoBuffer;
		unsigned int videoBufferSize;
		Texture const * texture;
		Object * overlayObject;
		Object * overlayBackObject;
		bool isPaintingTemplate;
		bool isPaintingTemplateResolved;
		bool dirty;
		bool settled;
		LONG frameReady;
		int appliedTimestamp;
		int requestedTimestamp;
		CRITICAL_SECTION bufferLock;
		bool looping;
	};

	typedef std::map<TangibleObject const *, VideoStreamRuntimeData> VideoStreamRuntimeDataMap;
	VideoStreamRuntimeDataMap ms_videoStreamRuntimeDataMap;

	struct EmitterRuntimeData
	{
		EmitterRuntimeData() :
			owner(0),
			parentNetworkIdStr(),
			active(false)
		{
		}

		TangibleObject const * owner;
		std::string parentNetworkIdStr;
		bool active;
	};

	typedef std::map<TangibleObject const *, EmitterRuntimeData> EmitterRuntimeDataMap;
	EmitterRuntimeDataMap ms_emitterRuntimeDataMap;

	float const AUDIO_MAX_DISTANCE = 32.0f;
	float const AUDIO_FULL_VOLUME_DISTANCE = 5.0f;
	float const AUDIO_OBSTRUCTION_FACTOR = 0.4f;
	float const AUDIO_OCCLUSION_FACTOR = 0.05f;

	bool urlNeedsResolution(std::string const & url)
	{
		if (url.find("youtube.com/") != std::string::npos)
			return true;
		if (url.find("youtu.be/") != std::string::npos)
			return true;
		if (url.find("vimeo.com/") != std::string::npos)
			return true;
		if (url.find("twitch.tv/") != std::string::npos)
			return true;
		if (url.find("dailymotion.com/") != std::string::npos)
			return true;
		if (url.find("streamable.com/") != std::string::npos)
			return true;

		std::string::size_type const lastDot = url.rfind('.');
		if (lastDot != std::string::npos)
		{
			std::string ext = url.substr(lastDot);
			for (std::string::iterator c = ext.begin(); c != ext.end(); ++c)
				*c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));

			if (ext == ".mp4" || ext == ".webm" || ext == ".mkv" || ext == ".avi"
				|| ext == ".flv" || ext == ".ogg" || ext == ".m3u8" || ext == ".ts"
				|| ext == ".mp3" || ext == ".wav" || ext == ".m4a")
			{
				return false;
			}
		}

		if (url.find("googlevideo.com/") != std::string::npos)
			return false;
		if (url.find("akamaized.net/") != std::string::npos)
			return false;

		return false;
	}

	struct ResolveUrlThreadData
	{
		std::string inputUrl;
		VideoStreamRuntimeData * runtimeData;
	};

	DWORD WINAPI resolveUrlThreadProc(LPVOID param)
	{
		ResolveUrlThreadData * threadData = reinterpret_cast<ResolveUrlThreadData *>(param);
		if (!threadData)
			return 1;

		std::string const & url = threadData->inputUrl;
		VideoStreamRuntimeData * runtimeData = threadData->runtimeData;

		std::string cmdLine = "yt-dlp.exe --get-url -f \"best[height<=720]\" --no-playlist \"";
		cmdLine += url;
		cmdLine += "\"";

		DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Resolving URL via yt-dlp: %s\n", url.c_str()));

		SECURITY_ATTRIBUTES sa;
		memset(&sa, 0, sizeof(sa));
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;

		HANDLE hReadPipe = 0;
		HANDLE hWritePipe = 0;
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: yt-dlp CreatePipe failed\n"));
			runtimeData->resolveState = RS_failed;
			delete threadData;
			return 1;
		}
		SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

		HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0);

		STARTUPINFOA si;
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdOutput = hWritePipe;
		si.hStdError = hNul;
		si.hStdInput = 0;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi;
		memset(&pi, 0, sizeof(pi));

		char cmdBuf[2048];
		strncpy(cmdBuf, cmdLine.c_str(), sizeof(cmdBuf) - 1);
		cmdBuf[sizeof(cmdBuf) - 1] = '\0';

		BOOL created = CreateProcessA(0, cmdBuf, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi);
		CloseHandle(hWritePipe);
		if (hNul)
			CloseHandle(hNul);

		if (!created)
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: yt-dlp CreateProcess failed (error %lu). Is yt-dlp.exe in PATH or exe directory?\n", GetLastError()));
			CloseHandle(hReadPipe);
			runtimeData->resolveState = RS_failed;
			delete threadData;
			return 1;
		}

		std::string output;
		char readBuf[4096];
		DWORD bytesRead = 0;
		while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, 0) && bytesRead > 0)
		{
			readBuf[bytesRead] = '\0';
			output += readBuf;
		}
		CloseHandle(hReadPipe);

		WaitForSingleObject(pi.hProcess, 15000);

		DWORD exitCode = 1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		while (!output.empty() && (output[output.size() - 1] == '\n' || output[output.size() - 1] == '\r'))
			output.erase(output.size() - 1);

		std::string::size_type firstNewline = output.find('\n');
		if (firstNewline != std::string::npos)
			output = output.substr(0, firstNewline);
		while (!output.empty() && (output[output.size() - 1] == '\r'))
			output.erase(output.size() - 1);

		if (exitCode == 0 && !output.empty() && output.find("http") == 0)
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: yt-dlp resolved to: %s\n", output.c_str()));
			runtimeData->resolvedUrl = output;
			runtimeData->resolveState = RS_done;
		}
		else
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: yt-dlp failed (exit=%lu) output: %s\n", exitCode, output.c_str()));
			runtimeData->resolveState = RS_failed;
		}

		delete threadData;
		return 0;
	}

	void * videoLockCallback(void * opaque, void ** planes)
	{
		VideoStreamRuntimeData * data = reinterpret_cast<VideoStreamRuntimeData *>(opaque);
		EnterCriticalSection(&data->bufferLock);
		*planes = data->videoBuffer;
		return 0;
	}

	void videoUnlockCallback(void * opaque, void * picture, void * const * planes)
	{
		UNREF(picture);
		UNREF(planes);
		VideoStreamRuntimeData * data = reinterpret_cast<VideoStreamRuntimeData *>(opaque);
		InterlockedExchange(&data->frameReady, 1);
		LeaveCriticalSection(&data->bufferLock);
	}

	void videoDisplayCallback(void * opaque, void * picture)
	{
		UNREF(opaque);
		UNREF(picture);
	}

	VideoStreamRuntimeData & getVideoStreamRuntimeData(TangibleObject const * owner)
	{
		VideoStreamRuntimeDataMap::iterator it = ms_videoStreamRuntimeDataMap.find(owner);
		if (it == ms_videoStreamRuntimeDataMap.end())
		{
			VideoStreamRuntimeData newData;
			newData.owner = owner;
			newData.videoBufferSize = VIDEO_WIDTH * VIDEO_HEIGHT * 4;
			newData.videoBuffer = new unsigned char[newData.videoBufferSize];
			memset(newData.videoBuffer, 0, newData.videoBufferSize);
			InitializeCriticalSection(&newData.bufferLock);
			it = ms_videoStreamRuntimeDataMap.insert(std::make_pair(owner, newData)).first;
		}
		return it->second;
	}

	void stopAndReleaseMediaPlayer(VideoStreamRuntimeData & data)
	{
		if (data.mediaPlayer && ms_vlcApi.loaded)
		{
			ms_vlcApi.pAudioSetVolume(data.mediaPlayer, 0);
			ms_vlcApi.pMediaPlayerStop(data.mediaPlayer);
			ms_vlcApi.pMediaPlayerRelease(data.mediaPlayer);
			data.mediaPlayer = 0;
		}
	}

	void removeVideoOverlayObject(TangibleObject & owner, Object *& objectToRemove)
	{
		if (!objectToRemove)
			return;

		if (objectToRemove->isInWorld())
			objectToRemove->removeFromWorld();

		if (objectToRemove->getAttachedTo() == &owner)
			owner.removeChildObject(objectToRemove, Object::DF_none);

		if (objectToRemove->isInWorld())
			objectToRemove->removeFromWorld();

		delete objectToRemove;
		objectToRemove = 0;
	}

	void createVideoOverlayObject(TangibleObject & owner, VideoStreamRuntimeData & runtimeData, Object *& target, bool backFace)
	{
		if (target)
			return;

		if (!owner.isInWorld())
			return;

		Appearance * const appearance = createMagicPaintingOverlayAppearance();
		if (!appearance)
			return;

		target = new Object();
		target->setAppearance(appearance);
		target->addNotification(ClientWorld::getIntangibleNotification());
		RenderWorld::addObjectNotifications(*target);

		Transform overlayTransform = Transform::identity;
		float const yOffset = runtimeData.isPaintingTemplate ? 0.0f : cs_magicPaintingOverlayHeight;
		overlayTransform.setPosition_p(Vector(0.0f, yOffset, 0.0f));
		overlayTransform.pitch_l(1.5707963267948966f);
		if (backFace)
			overlayTransform.yaw_l(cs_magicPaintingOverlayBackYaw);
		target->setTransform_o2p(overlayTransform);

		target->setScale(Vector(cs_magicPaintingOverlayBaseScale, cs_magicPaintingOverlayBaseScale, cs_magicPaintingOverlayBaseScale));

		bool const portalDisabled = (owner.getCellProperty() != 0);
		if (portalDisabled)
		{
			CellProperty::setPortalTransitionsEnabled(false);
			target->setParentCell(owner.getCellProperty());
		}

		target->attachToObject_w(&owner, true);
		target->addToWorld();

		if (runtimeData.texture)
			appearance->setTexture(TAG_MAIN, *runtimeData.texture);

		if (portalDisabled)
			CellProperty::setPortalTransitionsEnabled(true);
	}

	void ensureVideoOverlayObjects(TangibleObject & owner, VideoStreamRuntimeData & runtimeData)
	{
		if (runtimeData.isPaintingTemplate)
		{
			removeVideoOverlayObject(owner, runtimeData.overlayBackObject);
			removeVideoOverlayObject(owner, runtimeData.overlayObject);
			return;
		}

		createVideoOverlayObject(owner, runtimeData, runtimeData.overlayObject, false);
		createVideoOverlayObject(owner, runtimeData, runtimeData.overlayBackObject, true);
	}

	void applyVideoTextureToSurfaces(VideoStreamRuntimeData & runtimeData, Appearance * ownerAppearance)
	{
		if (!runtimeData.texture)
			return;

		if (runtimeData.isPaintingTemplate)
		{
			if (ownerAppearance)
				ownerAppearance->setTexture(TAG_MAIN, *runtimeData.texture);
			return;
		}

		Appearance * const overlayAppearance = runtimeData.overlayObject ? runtimeData.overlayObject->getAppearance() : 0;
		if (overlayAppearance)
			overlayAppearance->setTexture(TAG_MAIN, *runtimeData.texture);

		Appearance * const overlayBackAppearance = runtimeData.overlayBackObject ? runtimeData.overlayBackObject->getAppearance() : 0;
		if (overlayBackAppearance)
			overlayBackAppearance->setTexture(TAG_MAIN, *runtimeData.texture);

		if (ownerAppearance)
			ownerAppearance->setTexture(TAG_MAIN, *runtimeData.texture);
	}

	void applyVideoAspectScale(TangibleObject & owner)
	{
		std::string const & aspect = owner.getRemoteStreamAspect();
		Vector scale(Vector::xyz111);

		if (aspect == "16:9")
			scale.y = 0.75f;

		owner.setScale(scale);
	}

} // namespace VideoStreamNamespace

using namespace VideoStreamNamespace;

bool TangibleObject::getVideoPlaybackInfo(TangibleObject const * obj, __int64 & outTimeMs, __int64 & outLengthMs)
{
	outTimeMs = 0;
	outLengthMs = 0;

	if (!obj)
		return false;

	if (!ms_vlcApi.loaded || !ms_vlcApi.pMediaPlayerGetTime || !ms_vlcApi.pMediaPlayerGetLength)
		return false;

	VideoStreamRuntimeDataMap::const_iterator it = ms_videoStreamRuntimeDataMap.find(obj);
	if (it == ms_videoStreamRuntimeDataMap.end())
		return false;

	VideoStreamRuntimeData const & rd = it->second;
	if (!rd.mediaPlayer)
		return false;

	outTimeMs = ms_vlcApi.pMediaPlayerGetTime(rd.mediaPlayer);
	outLengthMs = ms_vlcApi.pMediaPlayerGetLength(rd.mediaPlayer);
	return (outTimeMs >= 0 && outLengthMs > 0);
}

//----------------------------------------------------------------------

bool TangibleObject::seekVideoPlayback(TangibleObject const * obj, __int64 timeMs)
{
	if (!obj)
		return false;

	if (!ms_vlcApi.loaded || !ms_vlcApi.pMediaPlayerSetTime)
		return false;

	VideoStreamRuntimeDataMap::const_iterator it = ms_videoStreamRuntimeDataMap.find(obj);
	if (it == ms_videoStreamRuntimeDataMap.end())
		return false;

	VideoStreamRuntimeData const & rd = it->second;
	if (!rd.mediaPlayer)
		return false;

	ms_vlcApi.pMediaPlayerSetTime(rd.mediaPlayer, static_cast<libvlc_time_t>(timeMs));
	return true;
}

// ======================================================================
// class TangibleObject: public static member functions
// ======================================================================

void TangibleObject::install()
{
	InstallTimer const installTimer("TangibleObject::install");

	DebugFlags::registerFlag (ms_useTestDamageLevel, "ClientGame/TangibleObject", "useTestDamageLevel");
	DebugFlags::registerFlag(ms_logChangedConditions, "ClientGame/TangibleObject", "logChangedConditions");
	ExitChain::add (TangibleObjectNamespace::remove, "TangibleObjectNamespace::remove");
}

//----------------------------------------------------------------------

void TangibleObjectNamespace::remove ()
{
	DebugFlags::unregisterFlag (ms_useTestDamageLevel);
	DebugFlags::unregisterFlag(ms_logChangedConditions);
}

// ----------------------------------------------------------------------

static void TestObjectAndChildrenForChair(Vector const searchPosition_p, Object &object, TangibleObject *&closestChair, float &closestDistanceSquared)
{
	//-- Test this object.
	ClientObject *clientObject = object.asClientObject();
	if (clientObject)
	{
		TangibleObject *const tangibleObject = clientObject->asTangibleObject();
		if (tangibleObject && tangibleObject->canSitOn())
		{
			float const distanceSquared = searchPosition_p.magnitudeBetweenSquared(tangibleObject->getPosition_p());
			if ((distanceSquared < closestDistanceSquared) && (distanceSquared < MessageQueueSitOnObject::cs_maximumChairRangeSquared))
			{
				closestDistanceSquared = distanceSquared;
				closestChair           = tangibleObject;
			}
		}
	}

	//-- Test children of this object.
	int const childCount = object.getNumberOfChildObjects();
	for (int i = 0; i < childCount; ++i)
	{
		Object *const childObject = object.getChildObject(i);
		if (childObject)
			TestObjectAndChildrenForChair(searchPosition_p, *childObject, closestChair, closestDistanceSquared);
	}
}

// ----------------------------------------------------------------------

TangibleObject *TangibleObject::findClosestInRangeChair(NetworkId const &searchCellId, Vector const &searchPosition_p)
{
	bool const isInWorldCell = (searchCellId == NetworkId::cms_invalid);

	if (isInWorldCell)
	{
		ObjectVector objectVector;

		// This creature is in the world cell. Use the ClientWorld
		// object-finding routines to find all the objects close to us.
		ClientWorld::findObjectsInRange(searchPosition_p, MessageQueueSitOnObject::cs_maximumChairRange, objectVector);

		TangibleObject *closestChair           = NULL;
		float           closestDistanceSquared = std::numeric_limits<float>::max();

		// NULL out anything that is not a chair.
		ObjectVector::iterator endIt = objectVector.end();
		for (ObjectVector::iterator it = objectVector.begin(); it != endIt; ++it)
		{
			if (*it)
				TestObjectAndChildrenForChair(searchPosition_p, *(*it), closestChair, closestDistanceSquared);
		}

		return closestChair;
	}
	else
	{
		// This creature is in another cell.  Go through all the objects in the
		// cell, finding the closest chair.
		Object *cellObject = NetworkIdManager::getObjectById(searchCellId);
		if (!cellObject)
		{
			DEBUG_WARNING(true, ("findClosestChairWithinRange(): specified chair parent cell id=[%s] could not be found on client.", searchCellId.getValueString().c_str()));
			return NULL;
		}

		// Get the cell property from the cell owner.  This really shouldn't be NULL.
		CellProperty *const cellProperty = cellObject->getCellProperty();
		if (!cellProperty)
		{
			DEBUG_WARNING(true, ("findClosestInRangeChair(): cellProperty for cell object was NULL, id=[%s].", searchCellId.getValueString().c_str()));
			return NULL;
		}

		TangibleObject *closestChair           = NULL;
		float           closestDistanceSquared = std::numeric_limits<float>::max();

		{
			ContainerIterator const endIt = cellProperty->end();
			for (ContainerIterator it = cellProperty->begin(); it != endIt; ++it)
			{
				if ((*it).getObject())
					TestObjectAndChildrenForChair(searchPosition_p, *((*it).getObject()), closestChair, closestDistanceSquared);
			}
		}

		//-- Also try the owner object's container property --- try to track down where the interior contents are located.
		Container *const cellObjectContainer = ContainerInterface::getContainer(*cellObject);
		if (cellObjectContainer)
		{
			ContainerIterator const endIt = cellObjectContainer->end();
			for (ContainerIterator it = cellObjectContainer->begin(); it != endIt; ++it)
			{
				if ((*it).getObject())
					TestObjectAndChildrenForChair(searchPosition_p, *((*it).getObject()), closestChair, closestDistanceSquared);
			}
		}

		//-- Also try the owner object.
		TestObjectAndChildrenForChair(searchPosition_p, *cellObject, closestChair, closestDistanceSquared);

		return closestChair;
	}
}

// ======================================================================
// class TangibleObject: public member functions
// ======================================================================

TangibleObject::TangibleObject(const SharedTangibleObjectTemplate* newTemplate) :
ClientObject             (newTemplate),
m_appearanceData         (),
m_remoteTextureUrl       (),
m_remoteTextureMode      (),
m_remoteTextureDisplayMode(),
m_remoteTextureScrollH   (),
m_remoteTextureScrollV   (),
m_remoteStreamUrl        (),
m_remoteStreamTimestamp  (),
m_remoteStreamLoop       (),
m_remoteStreamAspect     (),
m_remoteStreamStartTime  (),
m_remoteEmitterParentId  (),
m_remoteEmitterVolume    (),
m_rtScreenLinkedCamera   (),
m_rtCameraFov            (),
m_rtCameraResolution     (),
m_rtCameraActive         (),
m_damageTaken            (),
m_maxHitPoints           (),
m_components             (),
m_visible                (true),
m_inCombat				 (false),
m_count                  (0),
m_condition              (0),
m_accumulatedDamageTaken (0),
m_pvpFlags               (0),
m_pvpType                (0),
m_pvpFactionId           (0),
m_lastDamageLevel        (0.f),
m_testDamageLevel        (0.f),
m_lastOnOffStatus        (false),
m_isReceivingCallbacks   (false),
m_interestingAttachedObject(0),
m_untargettableOverride(false),
m_clientOnlyInteriorLayoutObjectList(0),
m_visabilityFlag(newTemplate->getClientVisabilityFlag()),
m_objectEffects(),
m_passiveRevealPlayerCharacter(),
m_mapColorOverride(0),
m_accessList(),
m_guildAccessList(),
m_effectsMap()
{
	NOT_NULL(newTemplate);
	m_appearanceData.setSourceObject (this);
	m_remoteTextureUrl.setSourceObject(this);
	m_remoteTextureMode.setSourceObject(this);
	m_remoteTextureDisplayMode.setSourceObject(this);
	m_remoteTextureScrollH.setSourceObject(this);
	m_remoteTextureScrollV.setSourceObject(this);
	m_remoteStreamUrl.setSourceObject(this);
	m_remoteStreamTimestamp.setSourceObject(this);
	m_remoteStreamLoop.setSourceObject(this);
	m_remoteStreamAspect.setSourceObject(this);
	m_remoteStreamStartTime.setSourceObject(this);
	m_remoteEmitterParentId.setSourceObject(this);
	m_remoteEmitterVolume.setSourceObject(this);
	m_rtScreenLinkedCamera.setSourceObject(this);
	m_rtCameraFov.setSourceObject(this);
	m_rtCameraResolution.setSourceObject(this);
	m_rtCameraActive.setSourceObject(this);
	m_damageTaken.setSourceObject    (this);
	m_condition.setSourceObject      (this);
	m_maxHitPoints.setSourceObject   (this);

	addSharedVariable(m_pvpFactionId);
	addSharedVariable(m_pvpType);
	addSharedVariable(m_appearanceData);
	addSharedVariable(m_components);
	addSharedVariable(m_condition);
	addSharedVariable(m_count);
	addSharedVariable(m_damageTaken);
	addSharedVariable(m_maxHitPoints);
	addSharedVariable(m_visible);
	addSharedVariable_np(m_inCombat);
	addSharedVariable_np(m_passiveRevealPlayerCharacter);
	addSharedVariable_np(m_mapColorOverride);
	addSharedVariable_np(m_accessList);
	addSharedVariable_np(m_guildAccessList);
	addSharedVariable_np(m_effectsMap);
	addSharedVariable_np(m_remoteTextureUrl);
	addSharedVariable_np(m_remoteTextureMode);
	addSharedVariable_np(m_remoteTextureDisplayMode);
	addSharedVariable_np(m_remoteTextureScrollH);
	addSharedVariable_np(m_remoteTextureScrollV);
	addSharedVariable_np(m_remoteStreamUrl);
	addSharedVariable_np(m_remoteStreamTimestamp);
	addSharedVariable_np(m_remoteStreamLoop);
	addSharedVariable_np(m_remoteStreamAspect);
	addSharedVariable_np(m_remoteStreamStartTime);
	addSharedVariable_np(m_remoteEmitterParentId);
	addSharedVariable_np(m_remoteEmitterVolume);
	addSharedVariable_np(m_rtScreenLinkedCamera);
	addSharedVariable_np(m_rtCameraFov);
	addSharedVariable_np(m_rtCameraResolution);
	addSharedVariable_np(m_rtCameraActive);

	m_effectsMap.setOnErase(this, &TangibleObject::OnObjectEffectErased);
	m_effectsMap.setOnInsert(this, &TangibleObject::OnObjectEffectInsert);
	m_effectsMap.setOnSet(this, &TangibleObject::OnObjectEffectModified);

	//-- set the default appearance.
	changeAppearance(*newTemplate);

	addProperty(*(new ClientCollisionProperty(*this)));

	if (Game::getSinglePlayer ())
		m_maxHitPoints = 1000;
}

//-----------------------------------------------------------------------

TangibleObject::~TangibleObject()
{
	//-- This must be the first line in the destructor to invalidate any watchers watching this object
	nullWatchers();

	if (isInWorld())
		removeFromWorld();

	if ( m_isReceivingCallbacks )
	{
		// We need to make sure the preferences are installed since we get destroyed
		// after the preferences get cleaned up when the game shuts down
		if ( CuiPreferences::isInstalled() )
		{
			// Stop observing client preference changes for displaying the interesting attached object
			CuiPreferences::getShowInterestingAppearanceCallback().detachReceiver(*this);
		}

		m_isReceivingCallbacks = false;
	}

	// -- Clean up any Object Effects
	{
		std::vector<std::string> toRemove;
		std::map<std::string, Object *>::const_iterator iter = m_objectEffects.begin();
		for(; iter != m_objectEffects.end(); ++iter)
			toRemove.push_back(iter->first);

		for(unsigned int i = 0; i < toRemove.size(); ++i)
			RemoveObjectEffect(toRemove[i]);
	}

	clearRemoteImageTexture();
	clearRemoteVideoStream();
	clearVideoEmitterAudio();
	clearRtCameraFeed();
}

// ----------------------------------------------------------------------

TangibleObject * TangibleObject::asTangibleObject()
{
	return this;
}

// ----------------------------------------------------------------------

TangibleObject const * TangibleObject::asTangibleObject() const
{
	return this;
}

// ----------------------------------------------------------------------

float TangibleObject::alter(const float elapsedTime)
{
	const float alterResult = ClientObject::alter(elapsedTime);
	if (alterResult != AlterResult::cms_kill)
	{
		//-- update damage
		{
			if (getClientData ())
			{
				const float currentDamageLevel = ms_useTestDamageLevel ? m_testDamageLevel : getDamageLevel ();
				if (currentDamageLevel != m_lastDamageLevel)
				{
					getClientData ()->applyDamage (this, hasCondition (C_onOff), m_lastDamageLevel, currentDamageLevel);
					m_lastDamageLevel = currentDamageLevel;
				}
			}

			if (m_interestingAttachedObject)
			{
				const Appearance * const app = getAppearance ();
				if (app)
				{
					if (app->getRenderedThisFrame ())
						m_interestingAttachedObject->setPosition_p (CuiObjectTextManager::getCurrentObjectHeadPoint_o (*this) + Vector::unitY * 0.25f);
				}
			}

			if(isInWorld())
			{
				VerifyObjectEffects();

				// Magic painting: closest-only. Tailor PNG (TAILOR_PNG mode): always decode for this object.
				if (usesTailorRemoteTextureMode())
				{
					updateRemoteImageTexture();
					updateGifAnimation(elapsedTime);
				}
				else if (hasCondition(C_magicPaintingUrl))
				{
					if (shouldRenderMedia(this, ms_closestPainting, ms_closestPaintingDistSq))
					{
						updateRemoteImageTexture();
						updateGifAnimation(elapsedTime);
					}
				}

				// Video player - only render if we're the closest one
				if (hasCondition(C_magicVideoPlayer))
				{
					if (shouldRenderMedia(this, ms_closestVideoPlayer, ms_closestVideoPlayerDistSq))
					{
						updateRemoteVideoStream();
					}
				}

				if (!m_remoteEmitterParentId.get().empty())
				{
					updateVideoEmitterAudio();
				}

				// Update RT Screen texture from camera feed - only render if we're the closest one
				if (!m_rtScreenLinkedCamera.get().empty())
				{
					if (shouldRenderMedia(this, ms_closestRtScreen, ms_closestRtScreenDistSq))
					{
						updateRtScreenTexture();
					}
				}

				// Restart any object effects that are finished playing.
				std::map<std::string, Object *>::const_iterator iter = m_objectEffects.begin();
				for(; iter != m_objectEffects.end(); ++iter)
					if(iter->second)
					{
						ParticleEffectAppearance * particle = ParticleEffectAppearance::asParticleEffectAppearance(iter->second->getAppearance());
						if(particle && particle->isDeletable())
							particle->restart();
					}
			}
			else
			{
				std::map<std::string, Object *>::const_iterator iter = m_objectEffects.begin();
				for(; iter != m_objectEffects.end(); ++iter)
					RemoveObjectEffect(iter->first);

				if (usesRemoteImageTextureConsumer())
					clearRemoteImageTexture();
				if (hasCondition(C_magicVideoPlayer))
					clearRemoteVideoStream();
				if (!m_remoteEmitterParentId.get().empty())
					clearVideoEmitterAudio();
			}

		}
	}

	if (m_visabilityFlag == SharedTangibleObjectTemplate::CVF_gm_only)
	{
		PlayerObject const * const playerObject = Game::getPlayerObject();
		bool const isGod = (playerObject != 0) ? playerObject->isAdmin() : false;
		if (!isGod)
		{
			RenderWorld::recursivelyDisableDpvsObjectsForThisRender(this);
		}
	}

	return alterResult;
}

// ----------------------------------------------------------------------
/**
* Default implementation for any code that needs to be processed after
* this container object has changed.
*
* This is called on the container object after its contents have been
* changed.
*
* Implementer note: it is best to chain up to the base class implementation
* for this function in derived class implementations.
*/

void TangibleObject::doPostContainerChangeProcessing ()
{
}

// ----------------------------------------------------------------------
/**
* Called once the object has all its initialization data.
*
* The server can stream down data after the object is created.
* That is why we don't do this activity in the constructor.
*/

void TangibleObject::endBaselines()
{
	//-- chain down to parent class first to perform any necessary processing
	ClientObject::endBaselines();

	// Unnecessary and problematic --- results in a double call
	// to conditionModified(). endBaselines messages already call
	// AutoDelta* modified callbacks.
	// conditionModified (0, getCondition ());

	// We many not want to call the virtual method conditionModified() but
	// TangibleObject still has some logic that needs to be performed
	handleConditionModified (0, getCondition ());

	m_lastOnOffStatus = (getCondition() & C_onOff) != 0;
	setChildWingsOpened((getCondition() & C_wingsOpened) != 0);
}

// ----------------------------------------------------------------------

void TangibleObject::addToWorld()
{
	ClientObject::addToWorld();

	// Update the interesting attached object based upon user preferences
	updateInterestingAttachedObject(getCondition ());
	if (usesRemoteImageTextureConsumer())
		updateRemoteImageTexture();
	if (hasCondition(C_magicVideoPlayer))
		updateRemoteVideoStream();
	if (!m_remoteEmitterParentId.get().empty())
		updateVideoEmitterAudio();
	if (!m_rtScreenLinkedCamera.get().empty())
		updateRtCameraFeed();
}

// ----------------------------------------------------------------------

void TangibleObject::removeFromWorld()
{
	if (m_clientOnlyInteriorLayoutObjectList)
	{
		for (size_t i = 0; i < m_clientOnlyInteriorLayoutObjectList->size(); ++i)
		{
			Object * const object = (*m_clientOnlyInteriorLayoutObjectList)[i];
			delete object;
		}

		delete m_clientOnlyInteriorLayoutObjectList;
		m_clientOnlyInteriorLayoutObjectList = 0;
	}

	if (m_interestingAttachedObject)
	{
		delete m_interestingAttachedObject;
		m_interestingAttachedObject = 0;
	}

	if (usesRemoteImageTextureConsumer())
		clearRemoteImageTexture();
	if (hasCondition(C_magicVideoPlayer))
		clearRemoteVideoStream();
	if (!m_remoteEmitterParentId.get().empty())
		clearVideoEmitterAudio();
	if (!m_rtScreenLinkedCamera.get().empty())
		clearRtCameraFeed();

	ClientObject::removeFromWorld();
}

// ----------------------------------------------------------------------

void TangibleObject::setUntargettableOverride(bool const untargettable) const
{
	m_untargettableOverride = untargettable;
}

// ----------------------------------------------------------------------

bool TangibleObject::isTargettable() const
{
	bool const targettable = !m_untargettableOverride && safe_cast<const SharedTangibleObjectTemplate*>(getObjectTemplate())->getTargetable();

	if (m_visabilityFlag == SharedTangibleObjectTemplate::CVF_gm_only)
	{
		PlayerObject const * const playerObject = Game::getPlayerObject();
		bool const isGod = (playerObject != 0) ? playerObject->isAdmin() : false;
		return targettable && isGod;
	}

	return targettable;
}

// ----------------------------------------------------------------------
/**
* Override the object's filenames for the slot descriptor and arrangement descriptor.
*
* In single player, these come from the client ObjectTemplate.  In multi-player,
* the server sends down the proper filenames.
*/

void TangibleObject::setSlotInfo(const std::string &slotDescriptorName, const std::string &arrangementDescriptorName) const
{
	UNREF(slotDescriptorName);
	UNREF(arrangementDescriptorName);
#if 0 //@todo remove when tested
	if (!m_slotDescriptorName)
		m_slotDescriptorName = new std::string(slotDescriptorName);
	else
		*m_slotDescriptorName = slotDescriptorName;

	if (!m_arrangementDescriptorName)
		m_arrangementDescriptorName = new std::string(arrangementDescriptorName);
	else
		*m_arrangementDescriptorName = arrangementDescriptorName;
#endif
}

// ======================================================================

/**
* Changes this object's appearance from a shared template.
*
* @param objectTemplateName		filename of a shared object template
*/
void TangibleObject::changeAppearance(const char * objectTemplateName)
{
	if (objectTemplateName == NULL || *objectTemplateName == '\0')
		return;

	const SharedTangibleObjectTemplate * objectTemplate = dynamic_cast<
		const SharedTangibleObjectTemplate *>(ObjectTemplateList::fetch(
		objectTemplateName));
	if (objectTemplate == NULL)
		return;

	changeAppearance(*objectTemplate);

	objectTemplate->releaseReference();
}	// TangibleObject::changeAppearance

/**
* Changes this object's appearance from a shared template.
*
* @param objectTemplate		shared object template to get the appearance from
*/
void TangibleObject::changeAppearance(const SharedTangibleObjectTemplate & objectTemplate)
{
	if (Game::isClient () && objectTemplate.getOnlyVisibleInTools ())
		return;

	//-- create appearance (but don't set it into the Object yet).
	Appearance *appearance = 0;

	const std::string &appearanceString = objectTemplate.getAppearanceFilename();
	if (!appearanceString.empty())
		appearance = AppearanceTemplateList::createAppearance(appearanceString.c_str());

	//-- Delete customization data if we already have it.
	{
		CustomizationDataProperty *const cdProperty = safe_cast<CustomizationDataProperty*>(getProperty(CustomizationDataProperty::getClassPropertyId()));
		if (cdProperty)
		{
			removeProperty(CustomizationDataProperty::getClassPropertyId());
			delete cdProperty;
		}
	}

	//-- create CustomizationDataProperty.
	//   Force it to be created (even without customizations owned by the Object)
	//   if the object is a SkeletalAppearance.  This is because most wearables are
	//   worn on objects that get deformed, and the wearable needs to receive the
	//   wearer's customization data.  If the wearable didn't have a CustomizationData
	//   instance, it would never mount the owner's CustomizationData, and the owner
	//   body stature would have no effect.
	objectTemplate.createCustomizationDataPropertyAsNeeded(*this, dynamic_cast<SkeletalAppearance2*>(appearance) != 0);

	//-- Add procedural customization variables before customization data gets associated with appearance.  Otherwise
	//   we have to rebake any skeletal appearances as soon as we add a new variable.
	SlottedContainer *const slottedContainer = ContainerInterface::getSlottedContainer(*this);
	if (slottedContainer)
	{
		int const ruleCount = SlotRuleManager::getRuleCount();

		//-- Determine if any rules apply.
		bool match = false;

		{
			for (int i = 0; i < ruleCount; ++i)
			{
				//-- If this tangible's container has the slot required for this rule, a customization variable will be added.
				match = slottedContainer->hasSlot(SlotRuleManager::getSlotRequiredForRule(i));
				if (match)
					break;
			}
		}

		if (match)
		{
			//-- Get or create customization data.
			CustomizationDataProperty *cdProperty = safe_cast<CustomizationDataProperty*>(getProperty(CustomizationDataProperty::getClassPropertyId()));
			if (!cdProperty)
			{
				cdProperty = new CustomizationDataProperty(*this);
				addProperty(*cdProperty);
			}

			NOT_NULL(cdProperty);
			CustomizationData *const customizationData = cdProperty->fetchCustomizationData();
			NOT_NULL(customizationData);

			//-- Add any rules that apply.
			for (int i = 0; i < ruleCount; ++i)
			{
				//-- If this tangible's container has the slot required for this rule, add the customization variable.
				if (slottedContainer->hasSlot(SlotRuleManager::getSlotRequiredForRule(i)))
					SlotRuleManager::addCustomizationVariableForRule(i, *customizationData);
			}

			//-- Release local references.
			customizationData->release();
		}
	}

	//-- set the appearance.
	if (appearance)
		setAppearance(appearance);
}	// TangibleObject::changeAppearance(const SharedObjectTemplate &)

// ======================================================================
/**
* This function is called after the TangibleObject has received a
* new appearance data customization string.
*
* @arg value  the value of the new appearance customization string
*/

void TangibleObject::appearanceDataModified(const std::string& value)
{
	// we don't value to use the value arg since we already have the
	// data stored in m_appearanceData, but just in case the interface
	// changes, we'll use it here.  Technically we don't need to keep
	// the m_appearanceData string around on the client.  We can build
	// it at will from the appearance.

	//-- skip no-content customization data
	if (value.empty())
		return;

	// check if the property exists
	CustomizationDataProperty *cdProperty = safe_cast<CustomizationDataProperty*>(getProperty( CustomizationDataProperty::getClassPropertyId()));

	if (!cdProperty)
	{
		DEBUG_WARNING(true, ("non-zero-length appearance string sent to object that doesn't declare any customization variables in its template."));
		return;
	}

	// retrieve the CustomizationData instance associated with the property
	CustomizationData *const customizationData = cdProperty->fetchCustomizationData();
	NOT_NULL(customizationData);

	//-- initialize CustomizationData variable values from the string
	customizationData->loadLocalDataFromString(value);

	//-- release local reference
	customizationData->release();

	//-- Make sure the damn appearance gets an alter --- static meshes will not alter unless instructed.
	scheduleForAlter();
}

//----------------------------------------------------------------------

bool TangibleObject::usesTailorRemoteTextureMode() const
{
	return isTailorPngRemoteMode(m_remoteTextureMode.get());
}

//----------------------------------------------------------------------

bool TangibleObject::usesRemoteImageTextureConsumer() const
{
	return hasCondition(C_magicPaintingUrl) || usesTailorRemoteTextureMode();
}

//----------------------------------------------------------------------

void TangibleObject::remoteTextureUrlModified(const std::string & value)
{
	UNREF(value);
	if (!isInWorld())
		return;
	RemoteImageRuntimeDataMap::iterator it = ms_remoteImageRuntimeDataMap.find(this);
	if (it != ms_remoteImageRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteTextureModeModified(const std::string & value)
{
	UNREF(value);
	if (!isInWorld())
		return;
	RemoteImageRuntimeDataMap::iterator it = ms_remoteImageRuntimeDataMap.find(this);
	if (it != ms_remoteImageRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
		it->second.isPaintingTemplateResolved = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteTextureDisplayModeModified(const std::string & value)
{
	UNREF(value);
	if (!isInWorld())
		return;
	RemoteImageRuntimeDataMap::iterator it = ms_remoteImageRuntimeDataMap.find(this);
	if (it != ms_remoteImageRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteTextureScrollHModified(const std::string & value)
{
	UNREF(value);
	if (!isInWorld())
		return;
	RemoteImageRuntimeDataMap::iterator it = ms_remoteImageRuntimeDataMap.find(this);
	if (it != ms_remoteImageRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteTextureScrollVModified(const std::string & value)
{
	UNREF(value);
	if (!isInWorld())
		return;
	RemoteImageRuntimeDataMap::iterator it = ms_remoteImageRuntimeDataMap.find(this);
	if (it != ms_remoteImageRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::updateRemoteImageTexture()
{
	if (!usesRemoteImageTextureConsumer() || !isInWorld())
	{
		RemoteImageRuntimeDataMap::iterator runtimeIt = ms_remoteImageRuntimeDataMap.find(this);
		if (runtimeIt != ms_remoteImageRuntimeDataMap.end())
			clearRemoteImageTexture();
		return;
	}

	// Distance check - don't create image resources for distant objects (magic painting only)
	if (!usesTailorRemoteTextureMode())
	{
		Camera const * playerCamera = Game::getCamera();
		if (playerCamera)
		{
			Vector const objPos = getPosition_w();
			Vector const camPos = playerCamera->getPosition_w();
			float const distSq = objPos.magnitudeBetweenSquared(camPos);
			if (distSq > cs_mediaMaxRenderDistanceSquared)
			{
				RemoteImageRuntimeDataMap::iterator runtimeIt = ms_remoteImageRuntimeDataMap.find(this);
				if (runtimeIt != ms_remoteImageRuntimeDataMap.end())
					clearRemoteImageTexture();
				return;
			}
		}
	}

	RemoteImageRuntimeData & runtimeData = getRemoteImageRuntimeData(this);

	LONG const fetchState = InterlockedCompareExchange(&runtimeData.fetchState, RIFS_idle, RIFS_idle);
	bool const fetchInProgress = (fetchState == RIFS_fetching);
	bool const fetchReady = (fetchState == RIFS_ready);

	if (runtimeData.settled && !runtimeData.dirty && !fetchInProgress && !fetchReady)
		return;

	std::string const & remoteTextureUrl = m_remoteTextureUrl.get();
	if (remoteTextureUrl.empty() || !isHttpUrl(remoteTextureUrl))
	{
		if (runtimeData.texture || runtimeData.overlayObject || runtimeData.overlayBackObject)
			clearRemoteImageTexture();
		return;
	}

	closeFinishedFetchThread(runtimeData);

	if (!runtimeData.isPaintingTemplateResolved)
	{
		runtimeData.isPaintingTemplate = usesTailorRemoteTextureMode() || templateNameContainsPainting(getObjectTemplateName());
		runtimeData.isPaintingTemplateResolved = true;
	}

	bool const urlChanged = (runtimeData.appliedUrl != remoteTextureUrl);

	if (runtimeData.dirty)
	{
		std::string const & remoteTextureMode = m_remoteTextureMode.get();
		std::string const & remoteDisplayModeStr = m_remoteTextureDisplayMode.get();
		std::string const & remoteScrollHStr = m_remoteTextureScrollH.get();
		std::string const & remoteScrollVStr = m_remoteTextureScrollV.get();

		bool const pictureOnlyMode = (!remoteTextureMode.empty() && (_stricmp(remoteTextureMode.c_str(), "DEFAULT") != 0) && !isTailorPngRemoteMode(remoteTextureMode));
		MagicPaintingDisplayMode const displayMode = parseDisplayMode(remoteDisplayModeStr);
		float const scrollH = remoteScrollHStr.empty() ? 0.0f : static_cast<float>(atof(remoteScrollHStr.c_str()));
		float const scrollV = remoteScrollVStr.empty() ? 0.0f : static_cast<float>(atof(remoteScrollVStr.c_str()));

		bool const displayModeChanged = (runtimeData.appliedDisplayMode != displayMode) || (runtimeData.appliedDisplayModeStr != remoteDisplayModeStr);
		bool const pictureOnlyChanged = (runtimeData.appliedPictureOnly != pictureOnlyMode);
		bool const scrollChanged = (runtimeData.appliedScrollH != scrollH) || (runtimeData.appliedScrollV != scrollV);

		if (pictureOnlyChanged)
		{
			applyPictureOnlyPresentation(*this, runtimeData, pictureOnlyMode);
			runtimeData.appliedPictureOnly = pictureOnlyMode;
		}

		if (displayModeChanged)
		{
			ensureRemoteImageOverlayObjects(*this, runtimeData, displayMode, true);
			runtimeData.appliedDisplayMode = displayMode;
			runtimeData.appliedDisplayModeStr = remoteDisplayModeStr;

			if (runtimeData.texture)
			{
				Appearance * const appearance = getAppearance();
				applyCachedRuntimeTextureToSurfaces(runtimeData, appearance);
			}
		}
		else if (!runtimeData.overlayObject && !runtimeData.isPaintingTemplate)
		{
			ensureRemoteImageOverlayObjects(*this, runtimeData, displayMode, false);
		}

		if (scrollChanged && runtimeData.texture)
		{
			runtimeData.appliedScrollH = scrollH;
			runtimeData.appliedScrollV = scrollV;
			runtimeData.scrollH = scrollH;
			runtimeData.scrollV = scrollV;
			if (runtimeData.isPaintingTemplate)
			{
				applyTextureScrollToAppearance(getAppearance(), scrollH, scrollV);
			}
			else
			{
				if (runtimeData.overlayObject && runtimeData.overlayObject->getAppearance())
					applyTextureScrollToAppearance(runtimeData.overlayObject->getAppearance(), scrollH, scrollV);
				if (runtimeData.overlayBackObject && runtimeData.overlayBackObject->getAppearance())
					applyTextureScrollToAppearance(runtimeData.overlayBackObject->getAppearance(), scrollH, scrollV);
			}
		}

		runtimeData.dirty = false;

		if (runtimeData.texture && !urlChanged && !fetchInProgress && !fetchReady)
		{
			runtimeData.settled = true;
			if (runtimeData.isGif)
				scheduleForAlter();
			return;
		}
	}

	if (!runtimeData.isPaintingTemplate && !runtimeData.overlayObject)
		return;

	if ((fetchState == RIFS_idle || fetchState == RIFS_failed) && urlChanged)
	{
		clearFetchedBytes(runtimeData);
		clearGifFrames(runtimeData);
		runtimeData.requestedUrl = remoteTextureUrl;
		runtimeData.appliedUrl.clear();
		runtimeData.settled = false;
		InterlockedExchange(&runtimeData.fetchState, RIFS_fetching);

		RemoteImageFetchThreadData * const threadData = new RemoteImageFetchThreadData(&runtimeData, remoteTextureUrl);
		runtimeData.fetchThread = CreateThread(0, 0, remoteImageFetchThreadProc, threadData, 0, 0);
		if (!runtimeData.fetchThread)
		{
			delete threadData;
			InterlockedExchange(&runtimeData.fetchState, RIFS_failed);
		}

		scheduleForAlter();
		return;
	}

	if (!fetchReady)
	{
		if (fetchInProgress)
			scheduleForAlter();
		else if (runtimeData.texture && !urlChanged)
		{
			runtimeData.settled = true;
			if (runtimeData.isGif)
				scheduleForAlter();
		}
		return;
	}

	std::vector<unsigned char> * const fetchedBytes = reinterpret_cast<std::vector<unsigned char> *>(InterlockedExchangePointer(reinterpret_cast<PVOID *>(&runtimeData.fetchedBytes), 0));
	if (!fetchedBytes)
	{
		InterlockedExchange(&runtimeData.fetchState, RIFS_failed);
		return;
	}

	bool const urlIsGif = isGifUrl(remoteTextureUrl);

	if (urlIsGif)
	{
		std::vector<Texture const *> gifFrames;
		std::vector<float> gifDelays;
		bool const gifDecoded = decodeGifFrames(*fetchedBytes, gifFrames, gifDelays);
		delete fetchedBytes;

		if (!gifDecoded || gifFrames.empty())
		{
			InterlockedExchange(&runtimeData.fetchState, RIFS_failed);
			return;
		}

		if (runtimeData.isGif)
			runtimeData.texture = 0;
		else if (runtimeData.texture)
		{
			runtimeData.texture->release();
			runtimeData.texture = 0;
		}
		clearGifFrames(runtimeData);

		runtimeData.gifFrames = gifFrames;
		runtimeData.gifFrameDelays = gifDelays;
		runtimeData.gifCurrentFrame = 0;
		runtimeData.gifFrameTimer = 0.0f;
		runtimeData.isGif = true;
		runtimeData.texture = gifFrames[0];
		runtimeData.texture->fetch();
	}
	else
	{
		Texture const * decodedTexture = 0;
		bool const decoded = decodeImageBytesToTexture(*fetchedBytes, decodedTexture);
		delete fetchedBytes;

		if (!decoded || !decodedTexture)
		{
			InterlockedExchange(&runtimeData.fetchState, RIFS_failed);
			return;
		}

		if (runtimeData.isGif)
			runtimeData.texture = 0;
		else if (runtimeData.texture)
			runtimeData.texture->release();
		clearGifFrames(runtimeData);

		runtimeData.texture = decodedTexture;
		runtimeData.isGif = false;
	}

	Appearance * const appearance = getAppearance();
	applyCachedRuntimeTextureToSurfaces(runtimeData, appearance);

	if (runtimeData.scrollH != 0.0f || runtimeData.scrollV != 0.0f)
	{
		if (runtimeData.isPaintingTemplate)
		{
			applyTextureScrollToAppearance(appearance, runtimeData.scrollH, runtimeData.scrollV);
		}
		else
		{
			if (runtimeData.overlayObject && runtimeData.overlayObject->getAppearance())
				applyTextureScrollToAppearance(runtimeData.overlayObject->getAppearance(), runtimeData.scrollH, runtimeData.scrollV);
			if (runtimeData.overlayBackObject && runtimeData.overlayBackObject->getAppearance())
				applyTextureScrollToAppearance(runtimeData.overlayBackObject->getAppearance(), runtimeData.scrollH, runtimeData.scrollV);
		}
	}
	runtimeData.appliedScrollH = runtimeData.scrollH;
	runtimeData.appliedScrollV = runtimeData.scrollV;

	runtimeData.appliedUrl = remoteTextureUrl;
	InterlockedExchange(&runtimeData.fetchState, RIFS_idle);

	runtimeData.settled = !runtimeData.isGif;
	if (runtimeData.isGif)
		scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::clearRemoteImageTexture()
{
	RemoteImageRuntimeDataMap::iterator runtimeIt = ms_remoteImageRuntimeDataMap.find(this);
	if (runtimeIt == ms_remoteImageRuntimeDataMap.end())
		return;

	RemoteImageRuntimeData & runtimeData = runtimeIt->second;
	applyPictureOnlyPresentation(*this, runtimeData, false);
	clearRemoteImageOverlayObjects(*this, runtimeData);

	if (runtimeData.isGif)
	{
		runtimeData.texture = 0;
		clearGifFrames(runtimeData);
	}
	else
	{
		if (runtimeData.texture)
		{
			runtimeData.texture->release();
			runtimeData.texture = 0;
		}
	}

	clearFetchedBytes(runtimeData);
	closeFinishedFetchThread(runtimeData);
	if (InterlockedCompareExchange(&runtimeData.fetchState, RIFS_idle, RIFS_idle) != RIFS_fetching)
		InterlockedExchange(&runtimeData.fetchState, RIFS_idle);
	runtimeData.requestedUrl.clear();
	runtimeData.appliedUrl.clear();
	runtimeData.appliedDisplayMode = MPDM_cube;
	runtimeData.appliedDisplayModeStr.clear();
	runtimeData.settled = false;
	runtimeData.dirty = true;
	ms_remoteImageRuntimeDataMap.erase(runtimeIt);
}

//----------------------------------------------------------------------

void TangibleObject::updateGifAnimation(float elapsedTime)
{
	RemoteImageRuntimeDataMap::iterator runtimeIt = ms_remoteImageRuntimeDataMap.find(this);
	if (runtimeIt == ms_remoteImageRuntimeDataMap.end())
		return;

	RemoteImageRuntimeData & runtimeData = runtimeIt->second;
	if (!runtimeData.isGif || runtimeData.gifFrames.empty())
		return;

	int const frameCount = static_cast<int>(runtimeData.gifFrames.size());
	if (runtimeData.gifCurrentFrame < 0 || runtimeData.gifCurrentFrame >= frameCount)
		runtimeData.gifCurrentFrame = 0;

	runtimeData.gifFrameTimer += elapsedTime;
	float const currentDelay = (runtimeData.gifCurrentFrame < static_cast<int>(runtimeData.gifFrameDelays.size()))
		? runtimeData.gifFrameDelays[runtimeData.gifCurrentFrame]
		: 0.1f;

	if (runtimeData.gifFrameTimer < currentDelay)
	{
		scheduleForAlter();
		return;
	}

	runtimeData.gifFrameTimer -= currentDelay;
	runtimeData.gifCurrentFrame = (runtimeData.gifCurrentFrame + 1) % frameCount;

	Texture const * const frameTexture = runtimeData.gifFrames[runtimeData.gifCurrentFrame];
	if (!frameTexture)
	{
		scheduleForAlter();
		return;
	}

	if (runtimeData.texture && runtimeData.texture != frameTexture)
		runtimeData.texture->release();
	runtimeData.texture = frameTexture;
	runtimeData.texture->fetch();

	if (runtimeData.isPaintingTemplate)
	{
		Appearance * const ownerApp = getAppearance();
		if (ownerApp)
			ownerApp->setTexture(TAG_MAIN, *frameTexture);
	}
	else
	{
		if (runtimeData.overlayObject)
		{
			Appearance * const app = runtimeData.overlayObject->getAppearance();
			if (app)
				app->setTexture(TAG_MAIN, *frameTexture);
		}
		if (runtimeData.overlayBackObject)
		{
			Appearance * const app = runtimeData.overlayBackObject->getAppearance();
			if (app)
				app->setTexture(TAG_MAIN, *frameTexture);
		}
	}

	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteStreamUrlModified(const std::string & value)
{
	UNREF(value);
	VideoStreamRuntimeDataMap::iterator it = ms_videoStreamRuntimeDataMap.find(this);
	if (it != ms_videoStreamRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteStreamTimestampModified(const std::string & value)
{
	UNREF(value);
	VideoStreamRuntimeDataMap::iterator it = ms_videoStreamRuntimeDataMap.find(this);
	if (it != ms_videoStreamRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::updateRemoteVideoStream()
{
	if (!hasCondition(C_magicVideoPlayer) || !isInWorld())
	{
		VideoStreamRuntimeDataMap::iterator runtimeIt = ms_videoStreamRuntimeDataMap.find(this);
		if (runtimeIt != ms_videoStreamRuntimeDataMap.end())
			clearRemoteVideoStream();
		return;
	}

	// Distance check - don't create video resources for distant objects
	Camera const * playerCamera = Game::getCamera();
	if (playerCamera)
	{
		Vector const objPos = getPosition_w();
		Vector const camPos = playerCamera->getPosition_w();
		float const distSq = objPos.magnitudeBetweenSquared(camPos);
		if (distSq > cs_mediaMaxRenderDistanceSquared)
		{
			// Too far away - clean up any existing resources
			VideoStreamRuntimeDataMap::iterator runtimeIt = ms_videoStreamRuntimeDataMap.find(this);
			if (runtimeIt != ms_videoStreamRuntimeDataMap.end())
				clearRemoteVideoStream();
			return;
		}
	}

	if (!loadVlcApi())
		return;

	VideoStreamRuntimeData & runtimeData = getVideoStreamRuntimeData(this);

	if (runtimeData.settled && !runtimeData.dirty)
	{
		if (InterlockedCompareExchange(&runtimeData.frameReady, 0, 1) == 1)
		{
			EnterCriticalSection(&runtimeData.bufferLock);

			if (runtimeData.texture)
			{
				runtimeData.texture->release();
				runtimeData.texture = 0;
			}

			TextureFormat const runtimeFormats[] = { TF_ARGB_8888 };
			Texture const * newTexture = TextureList::fetch(runtimeData.videoBuffer, TF_ARGB_8888, static_cast<int>(VIDEO_WIDTH), static_cast<int>(VIDEO_HEIGHT), runtimeFormats, 1);
			if (newTexture)
			{
				runtimeData.texture = newTexture;
				applyVideoTextureToSurfaces(runtimeData, getAppearance());
			}

			LeaveCriticalSection(&runtimeData.bufferLock);
		}
		scheduleForAlter();
		return;
	}

	std::string const & streamUrl = m_remoteStreamUrl.get();
	if (streamUrl.empty())
	{
		if (runtimeData.mediaPlayer || runtimeData.texture || runtimeData.overlayObject)
			clearRemoteVideoStream();
		return;
	}

	if (!runtimeData.isPaintingTemplateResolved)
	{
		runtimeData.isPaintingTemplate = templateNameContainsPainting(getObjectTemplateName());
		runtimeData.isPaintingTemplateResolved = true;
	}

	bool const urlChanged = (runtimeData.appliedUrl != streamUrl);

	std::string const & timestampStr = m_remoteStreamTimestamp.get();
	int const requestedTimestamp = timestampStr.empty() ? 0 : atoi(timestampStr.c_str());

	if (runtimeData.dirty)
	{
		ensureVideoOverlayObjects(*this, runtimeData);
		runtimeData.dirty = false;

		if (!urlChanged && runtimeData.mediaPlayer)
		{
			if (requestedTimestamp != runtimeData.appliedTimestamp && requestedTimestamp > 0)
			{
				ms_vlcApi.pMediaPlayerSetTime(runtimeData.mediaPlayer, static_cast<libvlc_time_t>(requestedTimestamp) * 1000);
				runtimeData.appliedTimestamp = requestedTimestamp;
			}
			runtimeData.settled = true;
			scheduleForAlter();
			return;
		}
	}

	if (urlChanged)
	{
		stopAndReleaseMediaPlayer(runtimeData);

		if (runtimeData.resolveThread)
		{
			DWORD waitResult = WaitForSingleObject(runtimeData.resolveThread, 0);
			if (waitResult != WAIT_OBJECT_0)
			{
				scheduleForAlter();
				return;
			}
			CloseHandle(runtimeData.resolveThread);
			runtimeData.resolveThread = 0;
		}

		runtimeData.appliedUrl = streamUrl;
		runtimeData.requestedUrl = streamUrl;
		runtimeData.resolvedUrl.clear();
		runtimeData.resolveState = RS_none;

		if (urlNeedsResolution(streamUrl))
		{
			ResolveUrlThreadData * threadData = new ResolveUrlThreadData;
			threadData->inputUrl = streamUrl;
			threadData->runtimeData = &runtimeData;

			runtimeData.resolveState = RS_pending;
			runtimeData.resolveThread = CreateThread(0, 0, resolveUrlThreadProc, threadData, 0, 0);
			if (!runtimeData.resolveThread)
			{
				DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to create resolve thread\n"));
				runtimeData.resolveState = RS_failed;
				runtimeData.settled = true;
				delete threadData;
				return;
			}

			scheduleForAlter();
			return;
		}
	}

	if (runtimeData.resolveState == RS_pending)
	{
		scheduleForAlter();
		return;
	}

	if (runtimeData.resolveState == RS_failed)
	{
		DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: URL resolution failed for: %s\n", streamUrl.c_str()));
		runtimeData.settled = true;
		return;
	}

	std::string const & playUrl = runtimeData.resolvedUrl.empty() ? streamUrl : runtimeData.resolvedUrl;

	if (!runtimeData.mediaPlayer)
	{
		libvlc_media_t * media = ms_vlcApi.pMediaNewLocation(ms_vlcApi.vlcInstance, playUrl.c_str());
		if (!media)
		{
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to create media for URL: %s\n", playUrl.c_str()));
			runtimeData.settled = true;
			return;
		}

		ms_vlcApi.pMediaAddOption(media, ":network-caching=1000");

		std::string const & loopStr = m_remoteStreamLoop.get();
		runtimeData.looping = (!loopStr.empty() && loopStr != "0");
		if (runtimeData.looping)
			ms_vlcApi.pMediaAddOption(media, ":input-repeat=65535");

		runtimeData.mediaPlayer = ms_vlcApi.pMediaPlayerNew(ms_vlcApi.vlcInstance);
		if (!runtimeData.mediaPlayer)
		{
			ms_vlcApi.pMediaRelease(media);
			DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Failed to create media player\n"));
			runtimeData.settled = true;
			return;
		}

		ms_vlcApi.pMediaPlayerSetMedia(runtimeData.mediaPlayer, media);
		ms_vlcApi.pMediaRelease(media);

		ms_vlcApi.pVideoSetFormat(runtimeData.mediaPlayer, "BGRA", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_PITCH);
		ms_vlcApi.pVideoSetCallbacks(runtimeData.mediaPlayer,
			reinterpret_cast<libvlc_video_lock_cb>(videoLockCallback),
			reinterpret_cast<libvlc_video_unlock_cb>(videoUnlockCallback),
			reinterpret_cast<libvlc_video_display_cb>(videoDisplayCallback),
			&runtimeData);

		ms_vlcApi.pAudioSetVolume(runtimeData.mediaPlayer, 0);
		ms_vlcApi.pMediaPlayerPlay(runtimeData.mediaPlayer);

		int seekSeconds = requestedTimestamp;
		std::string const & startTimeStr = m_remoteStreamStartTime.get();
		if (!startTimeStr.empty())
		{
			int const startEpoch = atoi(startTimeStr.c_str());
			if (startEpoch > 0)
			{
				int const nowEpoch = static_cast<int>(time(0));
				int const elapsed = nowEpoch - startEpoch;
				if (elapsed > 0)
					seekSeconds = elapsed;
			}
		}

		if (seekSeconds > 0)
		{
			ms_vlcApi.pMediaPlayerSetTime(runtimeData.mediaPlayer, static_cast<libvlc_time_t>(seekSeconds) * 1000);
		}
		runtimeData.appliedTimestamp = seekSeconds;

		applyVideoAspectScale(*this);

		DEBUG_REPORT_LOG(true, ("[Titan] VideoStream: Playing %s (seek=%ds)\n", playUrl.c_str(), seekSeconds));
	}

	runtimeData.settled = true;
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::clearRemoteVideoStream()
{
	VideoStreamRuntimeDataMap::iterator runtimeIt = ms_videoStreamRuntimeDataMap.find(this);
	if (runtimeIt == ms_videoStreamRuntimeDataMap.end())
		return;

	VideoStreamRuntimeData & runtimeData = runtimeIt->second;

	if (runtimeData.resolveThread)
	{
		WaitForSingleObject(runtimeData.resolveThread, 5000);
		CloseHandle(runtimeData.resolveThread);
		runtimeData.resolveThread = 0;
	}

	stopAndReleaseMediaPlayer(runtimeData);

	TangibleObject & mutableSelf = *const_cast<TangibleObject *>(runtimeData.owner);
	removeVideoOverlayObject(mutableSelf, runtimeData.overlayObject);
	removeVideoOverlayObject(mutableSelf, runtimeData.overlayBackObject);

	if (runtimeData.texture)
	{
		runtimeData.texture->release();
		runtimeData.texture = 0;
	}

	if (runtimeData.videoBuffer)
	{
		delete[] runtimeData.videoBuffer;
		runtimeData.videoBuffer = 0;
	}

	DeleteCriticalSection(&runtimeData.bufferLock);

	runtimeData.appliedUrl.clear();
	runtimeData.requestedUrl.clear();
	runtimeData.resolvedUrl.clear();
	runtimeData.resolveState = RS_none;
	runtimeData.settled = false;
	runtimeData.dirty = true;

	NetworkId const myNetId = getNetworkId();
	std::string const myNetIdStr = myNetId.getValueString();

	for (EmitterRuntimeDataMap::iterator emIt = ms_emitterRuntimeDataMap.begin(); emIt != ms_emitterRuntimeDataMap.end(); ++emIt)
	{
		if (emIt->second.parentNetworkIdStr == myNetIdStr)
			emIt->second.active = false;
	}

	ms_videoStreamRuntimeDataMap.erase(runtimeIt);
}

//----------------------------------------------------------------------

void TangibleObject::remoteStreamLoopModified(const std::string & value)
{
	UNREF(value);
	VideoStreamRuntimeDataMap::iterator it = ms_videoStreamRuntimeDataMap.find(this);
	if (it != ms_videoStreamRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteStreamAspectModified(const std::string & value)
{
	UNREF(value);
	applyVideoAspectScale(*this);
}

//----------------------------------------------------------------------

void TangibleObject::remoteStreamStartTimeModified(const std::string & value)
{
	UNREF(value);
	VideoStreamRuntimeDataMap::iterator it = ms_videoStreamRuntimeDataMap.find(this);
	if (it != ms_videoStreamRuntimeDataMap.end())
	{
		it->second.dirty = true;
		it->second.settled = false;
	}
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteEmitterParentIdModified(const std::string & value)
{
	UNREF(value);
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::remoteEmitterVolumeModified(const std::string & value)
{
	UNREF(value);
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::updateVideoEmitterAudio()
{
	std::string const & parentIdStr = m_remoteEmitterParentId.get();
	if (parentIdStr.empty() || !isInWorld())
	{
		clearVideoEmitterAudio();
		return;
	}

	EmitterRuntimeData & emitterData = ms_emitterRuntimeDataMap[this];
	emitterData.owner = this;
	emitterData.parentNetworkIdStr = parentIdStr;
	emitterData.active = true;

	NetworkId const parentNetId(parentIdStr);
	Object * const parentObj = NetworkIdManager::getObjectById(parentNetId);
	TangibleObject * const parentTangible = TangibleObject::asTangibleObject(parentObj);

	if (!parentTangible)
	{
		scheduleForAlter();
		return;
	}

	VideoStreamRuntimeDataMap::iterator parentIt = ms_videoStreamRuntimeDataMap.find(parentTangible);
	if (parentIt == ms_videoStreamRuntimeDataMap.end() || !parentIt->second.mediaPlayer)
	{
		scheduleForAlter();
		return;
	}

	VideoStreamRuntimeData & parentVideoData = parentIt->second;

	Object const * const player = Game::getPlayer();
	if (!player)
		return;

	Vector const emitterPos = getPosition_w();
	Vector const playerPos = player->getPosition_w();
	float const distance = emitterPos.magnitudeBetween(playerPos);

	if (distance > AUDIO_MAX_DISTANCE)
	{
		ms_vlcApi.pAudioSetVolume(parentVideoData.mediaPlayer, 0);
		scheduleForAlter();
		return;
	}

	float volume = 1.0f;
	if (distance > AUDIO_FULL_VOLUME_DISTANCE)
	{
		float const fadeRange = AUDIO_MAX_DISTANCE - AUDIO_FULL_VOLUME_DISTANCE;
		volume = 1.0f - ((distance - AUDIO_FULL_VOLUME_DISTANCE) / fadeRange);
		if (volume < 0.0f)
			volume = 0.0f;
	}

	CellProperty const * const emitterCell = getParentCell();
	CellProperty const * const playerCell = player->getParentCell();

	if (emitterCell != playerCell)
	{
		if (emitterCell && playerCell)
		{
			if (emitterCell->getPortalProperty() == playerCell->getPortalProperty())
				volume *= AUDIO_OBSTRUCTION_FACTOR;
			else
				volume *= AUDIO_OCCLUSION_FACTOR;
		}
		else
		{
			volume *= AUDIO_OCCLUSION_FACTOR;
		}
	}

	std::string const & emitterVolumeStr = m_remoteEmitterVolume.get();
	if (!emitterVolumeStr.empty())
	{
		int emitterVolume = atoi(emitterVolumeStr.c_str());
		if (emitterVolume < 0) emitterVolume = 0;
		if (emitterVolume > 100) emitterVolume = 100;
		volume *= (emitterVolume / 100.0f);
	}

	int vlcVolume = static_cast<int>(volume * 100.0f);
	if (vlcVolume < 0) vlcVolume = 0;
	if (vlcVolume > 100) vlcVolume = 100;

	ms_vlcApi.pAudioSetVolume(parentVideoData.mediaPlayer, vlcVolume);
	scheduleForAlter();
}

//----------------------------------------------------------------------

void TangibleObject::clearVideoEmitterAudio()
{
	EmitterRuntimeDataMap::iterator it = ms_emitterRuntimeDataMap.find(this);
	if (it == ms_emitterRuntimeDataMap.end())
		return;

	if (!it->second.parentNetworkIdStr.empty())
	{
		NetworkId const parentNetId(it->second.parentNetworkIdStr);
		Object * const parentObj = NetworkIdManager::getObjectById(parentNetId);
		TangibleObject * const parentTangible = TangibleObject::asTangibleObject(parentObj);
		if (parentTangible)
		{
			VideoStreamRuntimeDataMap::iterator parentIt = ms_videoStreamRuntimeDataMap.find(parentTangible);
			if (parentIt != ms_videoStreamRuntimeDataMap.end() && parentIt->second.mediaPlayer && ms_vlcApi.loaded)
				ms_vlcApi.pAudioSetVolume(parentIt->second.mediaPlayer, 0);
		}
	}

	ms_emitterRuntimeDataMap.erase(it);
}

//----------------------------------------------------------------------

void TangibleObject::rtScreenLinkedCameraModified(const std::string & value)
{
	UNREF(value);
	updateRtCameraFeed();
}

//----------------------------------------------------------------------

void TangibleObject::rtCameraFovModified(const std::string & value)
{
	// If this is a camera with a valid FOV, update the RtCameraManager directly
	if (!value.empty())
	{
		float fov = static_cast<float>(atof(value.c_str()));
		if (fov >= 30.0f && fov <= 120.0f)
		{
			NetworkId const cameraId = getNetworkId();
			RtCameraManager::updateCameraFov(cameraId, fov);
		}
	}
}

//----------------------------------------------------------------------

void TangibleObject::rtCameraResolutionModified(const std::string & value)
{
	UNREF(value);
	updateRtCameraFeed();
}

//----------------------------------------------------------------------

void TangibleObject::rtCameraActiveModified(const std::string & value)
{
	// Camera state changed
	NetworkId const cameraId = getNetworkId();

	if (value.empty() || value == "0")
	{
		// Camera is being deactivated - unregister the feed
		RtCameraManager::unregisterFeed(cameraId);
	}
	else
	{
		// Camera is being activated - schedule for alter to set up the feed
		scheduleForAlter();
	}
}

//----------------------------------------------------------------------

void TangibleObject::updateRtCameraFeed()
{
	// This object is an RT Screen - check if we have a linked camera
	std::string const & linkedCameraStr = m_rtScreenLinkedCamera.get();
	if (linkedCameraStr.empty())
	{
		clearRtCameraFeed();
		return;
	}

	if (!isInWorld())
	{
		clearRtCameraFeed();
		return;
	}

	// Distance check - don't create RT resources for distant screens
	Camera const * playerCamera = Game::getCamera();
	if (playerCamera)
	{
		Vector const objPos = getPosition_w();
		Vector const camPos = playerCamera->getPosition_w();
		float const distSq = objPos.magnitudeBetweenSquared(camPos);
		if (distSq > cs_mediaMaxRenderDistanceSquared)
		{
			// Too far away - clean up any existing resources
			clearRtCameraFeed();
			return;
		}
	}

	// Get the linked camera object
	NetworkId const cameraNetId(linkedCameraStr);
	if (!cameraNetId.isValid())
	{
		clearRtCameraFeed();
		return;
	}

	Object * const cameraObj = NetworkIdManager::getObjectById(cameraNetId);
	TangibleObject * const cameraTangible = TangibleObject::asTangibleObject(cameraObj);
	if (!cameraTangible)
	{
		// Camera not loaded yet, try again later
		scheduleForAlter();
		return;
	}

	// Check if camera is active
	std::string const & cameraActiveStr = cameraTangible->m_rtCameraActive.get();
	if (cameraActiveStr.empty() || cameraActiveStr == "0")
	{
		clearRtCameraFeed();
		return;
	}

	// Get resolution and FOV
	int resolution = RtCameraManager::DEFAULT_RESOLUTION;
	std::string const & resStr = m_rtCameraResolution.get();
	if (!resStr.empty())
	{
		resolution = atoi(resStr.c_str());
		if (resolution < 128) resolution = 128;
		if (resolution > 512) resolution = 512;
	}

	float fov = 60.0f;
	std::string const & fovStr = cameraTangible->m_rtCameraFov.get();
	if (!fovStr.empty())
	{
		fov = static_cast<float>(atof(fovStr.c_str()));
		if (fov < 30.0f) fov = 30.0f;
		if (fov > 120.0f) fov = 120.0f;
	}

	// Register the feed with RtCameraManager
	NetworkId const screenId = getNetworkId();

	// Check if already registered
	if (RtCameraManager::getFeedByScreen(screenId) != nullptr)
	{
		// Already registered, just update
		RtCameraManager::updateCameraTransform(cameraNetId, cameraTangible->getTransform_o2w());
		RtCameraManager::updateCameraFov(cameraNetId, fov);
		return;
	}

	// Register new feed
	RtCameraManager::registerFeed(cameraNetId, screenId, resolution, fov);
}

//----------------------------------------------------------------------

void TangibleObject::clearRtCameraFeed()
{
	NetworkId const myId = getNetworkId();

	// If this is a screen, unregister by screen ID
	RtCameraManager::unregisterFeedByScreen(myId);

	// If this is a camera, unregister by camera ID
	RtCameraManager::unregisterFeed(myId);
}

//----------------------------------------------------------------------

void TangibleObject::updateRtScreenTexture()
{
	// This object is an RT Screen - apply the camera's render texture to our appearance
	std::string const & linkedCameraStr = m_rtScreenLinkedCamera.get();
	if (linkedCameraStr.empty())
		return;

	NetworkId const screenId = getNetworkId();

	// Get the render texture from RtCameraManager
	Texture const * const cameraTexture = RtCameraManager::getScreenTexture(screenId);
	if (!cameraTexture)
	{
		// No texture yet, make sure we're registered for the feed
		updateRtCameraFeed();
		return;
	}

	// Apply texture to this screen's appearance
	Appearance * const appearance = getAppearance();
	if (appearance)
	{
		// Use TAG_MAIN to replace the primary texture
		appearance->setTexture(TAG_MAIN, *cameraTexture);
	}
}

//----------------------------------------------------------------------

CustomizationData *TangibleObject::fetchCustomizationData ()
{
	CustomizationDataProperty * const cdprop = safe_cast<CustomizationDataProperty *>(getProperty (CustomizationDataProperty::getClassPropertyId()));
	return cdprop ? cdprop->fetchCustomizationData () : 0;
}

//----------------------------------------------------------------------

const CustomizationData *TangibleObject::fetchCustomizationData () const
{
	const CustomizationDataProperty * const cdprop = safe_cast<const CustomizationDataProperty *>(getProperty (CustomizationDataProperty::getClassPropertyId()));
	return cdprop ? cdprop->fetchCustomizationData () : 0;
}

//----------------------------------------------------------------------

int TangibleObject::getDamageTaken () const
{
	// m_accumulatedDamageTaken is the damage amount that we don't want to show on the client
	//  until the combat action has been resolved
	return m_damageTaken.get ();
}

//----------------------------------------------------------------------

int TangibleObject::getMaxHitPoints () const
{
	return m_maxHitPoints.get ();
}

//----------------------------------------------------------------------

const ManufactureSchematicObject * TangibleObject::getCraftingManfSchematic(void) const
{
	const SharedObjectTemplate * const myTemplate = safe_cast<const SharedObjectTemplate *>(getObjectTemplate());
	if (myTemplate->getGameObjectType() != SharedObjectTemplate::GOT_misc_crafting_station)
		return NULL;

	const CraftingToolSyncUi * const sync = safe_cast<const CraftingToolSyncUi *>(getClientSynchronizedUi());
	if (sync == NULL)
		return NULL;

	return dynamic_cast<const ManufactureSchematicObject *>(sync->getManfSchematic().getObject());
}

//----------------------------------------------------------------------

void TangibleObject::startCrafting(void)
{
	static const uint32 listen = Crc::normalizeAndCalculate("synchronizedUiListen");

	const SharedObjectTemplate * myTemplate = safe_cast<const SharedObjectTemplate *>(getObjectTemplate());
	if (myTemplate->getGameObjectType() == SharedObjectTemplate::GOT_tool_crafting)
	{
		// create our synced UI
		if (getClientSynchronizedUi() == NULL)
			setSynchronizedUi (new CraftingToolSyncUi(*this) );

		// tell the server we want it's data
		ClientCommandQueue::enqueueCommand(listen, getNetworkId(), Unicode::String());
	}
}

//----------------------------------------------------------------------

void TangibleObject::stopCrafting(void)
{
	static const uint32 stopListen = Crc::normalizeAndCalculate("synchronizedUiStopListening");

	const SharedObjectTemplate * myTemplate = safe_cast<const SharedObjectTemplate *>(getObjectTemplate());
	if (myTemplate->getGameObjectType() == SharedObjectTemplate::GOT_tool_crafting)
	{
		// tell the server we don't want it's data
		ClientCommandQueue::enqueueCommand(stopListen, getNetworkId(), Unicode::String());
	}
}

//----------------------------------------------------------------------

void TangibleObject::clientSetDamageTaken     (int dam)
{
	m_damageTaken = dam;
}

//----------------------------------------------------------------------

void TangibleObject::clientSetMaxHitPoints    (int dam)
{
	m_maxHitPoints = dam;
}

//----------------------------------------------------------------------

void TangibleObject::Callbacks::DamageTaken::modified(TangibleObject & target, const int& oldValue, const int& value, bool) const
{
	if (target.isInitialized())
	{
		if (value > oldValue)
			Transceivers::damageTaken.emitMessage (Messages::DamageTaken::Payload (&target, value - oldValue));
	}
}

//----------------------------------------------------------------------

// Explicit template specializations for RT Camera callbacks
template<>
void TangibleObject::Callbacks::DefaultCallback<TangibleObject::Messages::RtScreenLinkedCamera, std::string>::modified(TangibleObject & target, const std::string& oldValue, const std::string& value, bool) const
{
	UNREF(oldValue);
	target.rtScreenLinkedCameraModified(value);
}

template<>
void TangibleObject::Callbacks::DefaultCallback<TangibleObject::Messages::RtCameraFov, std::string>::modified(TangibleObject & target, const std::string& oldValue, const std::string& value, bool) const
{
	UNREF(oldValue);
	target.rtCameraFovModified(value);
}

template<>
void TangibleObject::Callbacks::DefaultCallback<TangibleObject::Messages::RtCameraResolution, std::string>::modified(TangibleObject & target, const std::string& oldValue, const std::string& value, bool) const
{
	UNREF(oldValue);
	target.rtCameraResolutionModified(value);
}

template<>
void TangibleObject::Callbacks::DefaultCallback<TangibleObject::Messages::RtCameraActive, std::string>::modified(TangibleObject & target, const std::string& oldValue, const std::string& value, bool) const
{
	UNREF(oldValue);
	target.rtCameraActiveModified(value);
}

//----------------------------------------------------------------------

bool TangibleObject::isPlayer () const
{
	return (m_pvpFlags & PvpStatusFlags::IsPlayer) != 0 || static_cast<const Object *>(Game::getPlayer ()) == this;
}

//----------------------------------------------------------------------

bool TangibleObject::isAttackable () const
{
	return Game::getSinglePlayer () || (!isInvulnerable () && (getPvpFlags() & PvpStatusFlags::YouCanAttack) != 0);
}

//----------------------------------------------------------------------

bool TangibleObject::canAttackPlayer  () const
{
	return (getPvpFlags() & PvpStatusFlags::CanAttackYou) != 0;
}

//----------------------------------------------------------------------

bool TangibleObject::isEnemy() const
{
	return (getPvpFlags() & PvpStatusFlags::IsEnemy) != 0;
}

//----------------------------------------------------------------------

bool TangibleObject::isSameFaction            (const TangibleObject & other) const
{
	return getPvpFaction() && getPvpFaction() == other.getPvpFaction();
}

//----------------------------------------------------------------------

bool TangibleObject::canHelp() const
{
	return (getPvpFlags() & PvpStatusFlags::YouCanHelp) != 0;
}

//----------------------------------------------------------------------

const stdset<int>::fwd& TangibleObject::getComponents () const
{
	return m_components.get ();
}

//----------------------------------------------------------------------

bool TangibleObject::canSitOn () const
{
	Appearance const * const appearance = getAppearance ();
	if (appearance)
	{
		Transform  transform(Transform::IF_none);

		const bool hasChairHardpoint = appearance->findHardpoint (cs_chairHardpointName, transform);
		return hasChairHardpoint;
	}

	return false;
}

// ----------------------------------------------------------------------
/**
* This function builds the parameter string
*
* This function only makes sense to call if the caller is generating a
* command queue entry for sitting in chairs.
*/

Unicode::String TangibleObject::buildChairParameterString() const
{
	DEBUG_WARNING(!canSitOn(), ("buildChairParameterString(): it makes no sense to call this function since canSitOn() returns false."));

	char buffer[256];

	CellProperty const *const chairCell       = getParentCell();
	Vector       const        chairPosition_p = getPosition_p();

	snprintf(buffer, sizeof(buffer) - 1, "%g,%g,%g,%s", chairPosition_p.x, chairPosition_p.y, chairPosition_p.z, (chairCell != CellProperty::getWorldCellProperty()) ? chairCell->getOwner().getNetworkId().getValueString().c_str() : NetworkId::cms_invalid.getValueString().c_str());
	buffer[sizeof(buffer) - 1] = '\0';

	return Unicode::narrowToWide(buffer);
}

// ----------------------------------------------------------------------

Footprint *TangibleObject::getFootprint ()
{
	CollisionProperty *const property = getCollisionProperty ();
	if (property)
		return property->getFootprint ();
	else
		return 0;
}

// ----------------------------------------------------------------------

float TangibleObject::getDamageLevel () const
{
	const int maxHitPoints = getMaxHitPoints ();
	const int damageTaken = getDamageTaken ();

	return maxHitPoints == 0 ? 0.f : static_cast<float> (damageTaken) / maxHitPoints;
}

//----------------------------------------------------------------------

void TangibleObject::setTestDamageLevel (const float testDamageLevel)
{
	m_testDamageLevel = testDamageLevel;
	scheduleForAlter();
}

//----------------------------------------------------------------------

uint32 TangibleObject::getPvpFlags() const
{
	return m_pvpFlags;
}

//----------------------------------------------------------------------

int TangibleObject::getPvpType() const
{
	return m_pvpType.get();
}

//----------------------------------------------------------------------

uint32 TangibleObject::getPvpFaction() const
{
	return m_pvpFactionId.get();
}

//----------------------------------------------------------------------

void TangibleObject::maxHitPointsModified (int oldMaxHitPoints, int newMaxHitPoints)
{
	if (!isInitialized ())
		return;

	//-- destroyed state has changed
	if ((oldMaxHitPoints <= 0 && newMaxHitPoints > 0) || (newMaxHitPoints <= 0 && oldMaxHitPoints > 0))
	{
		setLocalizedNameDirty ();
	}
}

//----------------------------------------------------------------------

void TangibleObject::conditionModified (int oldCondition, int newCondition)
{
	if (!isInitialized ())
		return;

	// We need to check our conditions once initialization is complete,
	// so all the logic exists in handleConditionModified()
	handleConditionModified(oldCondition, newCondition);
}

//----------------------------------------------------------------------

void TangibleObject::setCondition(int const condition)
{
	DEBUG_WARNING(ms_logChangedConditions && !isFakeNetworkId(getNetworkId()), ("TangibleObject::setCondition: called on object without a fake network id. condition=%i, object=%s", condition, getDebugInformation(true).c_str()));
	int const oldCondition = m_condition.get();
	m_condition = oldCondition | condition;

#if PRODUCTION == 0
	if (Game::getSinglePlayer())
		conditionModified(oldCondition, m_condition.get());
#endif
}

//----------------------------------------------------------------------

void TangibleObject::setCount(int count)
{
	if (Game::getSinglePlayer ())
	{
		m_count = count;
	}
}

//----------------------------------------------------------------------

void TangibleObject::clearCondition(int const condition)
{
	DEBUG_WARNING(ms_logChangedConditions && !isFakeNetworkId(getNetworkId()), ("TangibleObject::clearCondition: called on object without a fake network id. condition=%i, object=%s", condition, getDebugInformation(true).c_str()));
	int const oldCondition = m_condition.get();
	m_condition = oldCondition & (~condition);

#if PRODUCTION == 0
	if (Game::getSinglePlayer())
		conditionModified(oldCondition, m_condition.get());
#endif
}

//----------------------------------------------------------------------

void TangibleObject::setAppearanceData(const std::string &newAppearanceData)
{
	m_appearanceData = newAppearanceData;
}

//----------------------------------------------------------------------

void TangibleObject::filterLocalizedName (Unicode::String & localizedName) const
{
	ClientObject::filterLocalizedName (localizedName);

	if (GameObjectTypes::isTypeOf (getGameObjectType (), SharedObjectTemplate::GOT_vehicle))
	{
		if (getMaxHitPoints () == 0 && !isInvulnerable ())
		{
			Unicode::String result;
			CuiStringVariablesManager::process (SharedStringIds::vehicle_destroyed_name_prose, localizedName, Unicode::emptyString, Unicode::emptyString, result);
			localizedName = result;
		}
		else if (hasCondition (TangibleObject::C_disabled) && getMaxHitPoints () > 0 && !isInvulnerable ())
		{
			Unicode::String result;
			CuiStringVariablesManager::process (SharedStringIds::vehicle_disabled_name_prose, localizedName, Unicode::emptyString, Unicode::emptyString, result);
			localizedName = result;
		}
	}
}

// ----------------------------------------------------------------------

void TangibleObject::getRequiredCertifications(std::vector<std::string> & results) const
{
	const SharedTangibleObjectTemplate * const sharedObjectTemplate = safe_cast<const SharedTangibleObjectTemplate*>(getObjectTemplate());
	if (sharedObjectTemplate)
	{
		const int numRequired = sharedObjectTemplate->getCertificationsRequiredCount();
		for(int i=0; i<numRequired; ++i)
		{
			results.push_back(sharedObjectTemplate->getCertificationsRequired(i));
		}
	}
}

// ----------------------------------------------------------------------

InteriorLayoutReaderWriter const * TangibleObject::getInteriorLayout() const
{
	return 0;
}

// ----------------------------------------------------------------------

void TangibleObject::addClientOnlyInteriorLayoutObject(Object * const object)
{
	if (!m_clientOnlyInteriorLayoutObjectList)
		m_clientOnlyInteriorLayoutObjectList = new ClientOnlyInteriorLayoutObjectList;

	m_clientOnlyInteriorLayoutObjectList->push_back(Watcher<Object>(object));
}

//----------------------------------------------------------------------

void TangibleObject::handleConditionModified (int oldCondition, int newCondition)
{
	//-- disabled state has changed
	if ((oldCondition & TangibleObject::C_disabled) != (newCondition & TangibleObject::C_disabled))
	{
		setLocalizedNameDirty ();
	}

	if (getClientData ())
	{
		const bool on = (newCondition & C_onOff) != 0;
		if (on != m_lastOnOffStatus)
		{
			getClientData ()->applyOnOff (this, on);
			m_lastOnOffStatus = on;
		}
	}

	// We will not attach an interesting object unless this object is in the world
	if (isInWorld())
	{
		updateInterestingAttachedObject( newCondition );
	}

	// If the object is interesting we need to keep track of whether they want to see the interesting object
	bool const interesting = (newCondition & C_interesting) != 0;
	bool const spaceInteresting = (newCondition & C_spaceInteresting) != 0;

	if (interesting || spaceInteresting)
	{
		// Start observing client preference changes for displaying the interesting attached object
		if (!m_isReceivingCallbacks)
		{
			CuiPreferences::getShowInterestingAppearanceCallback().attachReceiver(*this);
			m_isReceivingCallbacks = true;
		}
	}
	else
	{
		// Stop observing client preference changes for displaying the interesting attached object
		if (m_isReceivingCallbacks)
		{
			CuiPreferences::getShowInterestingAppearanceCallback().detachReceiver(*this);
			m_isReceivingCallbacks = false;
		}
	}

	//-- Pass wingsOpened to any child objects
	bool const oldWingsOpened = oldCondition & C_wingsOpened;
	bool const newWingsOpened = newCondition & C_wingsOpened;
	if (newWingsOpened != oldWingsOpened)
		setChildWingsOpened(newWingsOpened);

	//-- TangibleDynamics: create or destroy ClientTangibleDynamics based on condition
	{
		bool const oldDynamic = (oldCondition & C_magicTangibleDynamic) != 0;
		bool const newDynamic = (newCondition & C_magicTangibleDynamic) != 0;
		if (newDynamic && !oldDynamic)
		{
			// Condition just set — attach ClientTangibleDynamics if we don't already have one
			if (!getDynamics() || !dynamic_cast<ClientTangibleDynamics*>(getDynamics()))
			{
				setDynamics(new ClientTangibleDynamics(this));
			}
		}
		else if (!newDynamic && oldDynamic)
		{
			// Condition just cleared — remove dynamics
			ClientTangibleDynamics* const ctd = dynamic_cast<ClientTangibleDynamics*>(getDynamics());
			if (ctd)
			{
				ctd->clearAllForces();
				setDynamics(NULL);
			}
		}
	}

	//-- Make sure we get an alter
	if (isInWorld())
		scheduleForAlter();
}

// ----------------------------------------------------------------------

void TangibleObject::setChildWingsOpened(bool const wingsOpened)
{
	for (int i = 0; i < getNumberOfChildObjects(); ++i)
	{
		Object * const childObject = getChildObject(i);
		if (childObject && childObject->isChildObject() && childObject->asClientObject() && childObject->asClientObject()->asTangibleObject())
		{
			if (wingsOpened)
				childObject->asClientObject()->asTangibleObject()->setCondition(C_wingsOpened);
			else
				childObject->asClientObject()->asTangibleObject()->clearCondition(C_wingsOpened);
		}
	}
}

// ======================================================================

void TangibleObject::updateInterestingAttachedObject(int objectCondition)
{
	// Remove the old interesting attached object since we may need to create a different one
	if (m_interestingAttachedObject)
	{
		delete m_interestingAttachedObject;
		m_interestingAttachedObject = 0;
	}

	// Check whether the user wants to see interesting attached objects
	if (CuiPreferences::getShowInterestingAppearance ())
	{
		// Determine in what way the object is interesting
		bool const interesting = (objectCondition & C_interesting) != 0;
		bool const spaceInteresting = (objectCondition & C_spaceInteresting) != 0;
		bool const holidayInteresting = (objectCondition & C_holidayInteresting) != 0;

		if (interesting || spaceInteresting || holidayInteresting)
		{
			m_interestingAttachedObject = new Object ();

			char const * const appearanceTemplateName =  holidayInteresting ? "appearance/item_present_partcile_icon.apt" : (spaceInteresting ? "appearance/space_info_disk.apt" : "appearance/info_disk.apt");
			m_interestingAttachedObject->setAppearance(AppearanceTemplateList::createAppearance(appearanceTemplateName));
			m_interestingAttachedObject->addNotification(ClientWorld::getIntangibleNotification());
			RenderWorld::addObjectNotifications (*m_interestingAttachedObject);
			m_interestingAttachedObject->attachToObject_p (this, false);
			m_interestingAttachedObject->addToWorld();

			RotationDynamics * const rotationDynamics = new RotationDynamics (m_interestingAttachedObject, Vector (0.5f, 0.f, 0.f));
			rotationDynamics->setState (true);
			m_interestingAttachedObject->setDynamics (rotationDynamics);
		}
	}
}

//----------------------------------------------------------------------

void TangibleObject::performCallback()
{
	// We will not attach an interesting object unless this object is in the world
	if (isInWorld())
	{
		// Update the interesting attached object based upon user preferences
		updateInterestingAttachedObject(getCondition ());
	}
}

// ----------------------------------------------------------------------

TangibleObject * TangibleObject::getTangibleObject(NetworkId const & networkId)
{
	return asTangibleObject(NetworkIdManager::getObjectById(networkId));
}

// ----------------------------------------------------------------------

TangibleObject const * TangibleObject::asTangibleObject(Object const * object)
{
	ClientObject const * const clientObject = (object != NULL) ? object->asClientObject() : NULL;

	return (clientObject != NULL) ? clientObject->asTangibleObject() : NULL;
}

// ----------------------------------------------------------------------

TangibleObject * TangibleObject::asTangibleObject(Object * object)
{
	ClientObject * clientObject = (object != NULL) ? object->asClientObject() : NULL;

	return (clientObject != NULL) ? clientObject->asTangibleObject() : NULL;
}

// ----------------------------------------------------------------------
bool TangibleObject::isInCombat() const
{
	return m_inCombat.get();
}

// ----------------------------------------------------------------------

void TangibleObject::setIsInCombat( bool inCombat)
{
	m_inCombat.set(inCombat);
}

// ----------------------------------------------------------------------

void TangibleObject::AddObjectEffect(std::string const & label, std::string const & effectFile, std::string const & hardpoint, Vector const & offset, float const scale)
{
	UNREF(offset);
	UNREF(hardpoint);

	if(m_objectEffects.find(label) != m_objectEffects.end())
		return;

	if(!isInWorld())
		return;

	ParticleEffectAppearance * newObjectEffect = NULL;
	ParticleEffectAppearanceTemplate const * particleTemplate = dynamic_cast<ParticleEffectAppearanceTemplate const *>(AppearanceTemplateList::fetch(effectFile.c_str()));
	if(!particleTemplate)
	{
		DEBUG_WARNING(true, ("AddObjectEffect: Could not fetch the effect using template name[%s]", effectFile.c_str()));
		m_objectEffects[label] = NULL;
		return;
	}

	Object *newObject = new Object();
	NOT_NULL(newObject);

	RenderWorld::addObjectNotifications(*newObject);

	newObjectEffect = ParticleEffectAppearance::asParticleEffectAppearance(particleTemplate->createAppearance());
	NOT_NULL(newObjectEffect);

	AppearanceTemplateList::release(particleTemplate);

	newObjectEffect->setAutoDelete(false);
	newObjectEffect->setOwner(newObject);
	newObjectEffect->setPlayBackRate(1.0f);
	newObjectEffect->setScale(Vector(scale, scale, scale));

	newObject->setAppearance(newObjectEffect);

	if(getCellProperty())
	{
		//this MUST MUST MUST MUST MUST MUST be reset to true before returning!!!
		CellProperty::setPortalTransitionsEnabled(false);
		newObject->setParentCell(getCellProperty());
	}

	newObject->addNotification(ClientWorld::getIntangibleNotification());

	newObject->attachToObject_w(this, true);

	if(!hardpoint.empty())
	{
		Appearance const * const thisApp = this->getAppearance();
		Transform hardpointToParent = Transform::identity;
		if(thisApp)
		{
			if(thisApp->findHardpoint(CrcLowerString(hardpoint.c_str()), hardpointToParent))
			{
				Vector finalOffset = hardpointToParent.getPosition_p() + offset;
				hardpointToParent.setPosition_p(finalOffset);

				newObject->setTransform_o2p(hardpointToParent);
			}
		}
	}

	m_objectEffects[label] = newObject;

	if(getCellProperty())
	{
		CellProperty::setPortalTransitionsEnabled(true);
	}

	return;
}

// ----------------------------------------------------------------------

void TangibleObject::RemoveObjectEffect(std::string const & label)
{
	std::map<std::string, Object *>::iterator iter = m_objectEffects.find(label);
	if(iter == m_objectEffects.end())
		return;

	if(iter->second)
	{

		this->removeChildObject(iter->second, DF_none);

		iter->second->removeFromWorld();

		delete iter->second;
	}

	m_objectEffects.erase(iter);
}

void TangibleObject::VerifyObjectEffects()
{
	if(m_objectEffects.size() != m_effectsMap.size())
	{
		std::vector<std::string> toRemove;
		std::map<std::string, Object *>::const_iterator iter = m_objectEffects.begin();
		for(; iter != m_objectEffects.end(); ++iter)
			toRemove.push_back(iter->first);

		for(unsigned int i = 0; i < toRemove.size(); ++i)
			RemoveObjectEffect(toRemove[i]);
		// <std::string, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> >
		Archive::AutoDeltaMap< std::string, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > >::const_iterator addIter = m_effectsMap.begin();
		for(; addIter != m_effectsMap.end(); ++addIter)
			AddObjectEffect(addIter->first, addIter->second.first, addIter->second.second.first, addIter->second.second.second.first, addIter->second.second.second.second );
	}
}

// ----------------------------------------------------------------------

void TangibleObject::getObjectInfo(std::map<std::string, std::map<std::string, Unicode::String> > & propertyMap) const
{
	/**
	When adding a variable to this class, please add it here.  Variable that aren't easily displayable are still listed, for tracking purposes.
	*/

	/**
	Don't compile in production build because this maps human-readable values to data members and makes hacking easier
	*/
#if PRODUCTION == 0
	//	Archive::AutoDeltaVariableCallback<std::string, Callbacks::AppearanceData, TangibleObject> m_appearanceData;
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "DamageTaken", m_damageTaken.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "MaxHitPoints", m_maxHitPoints.get());
	//	Archive::AutoDeltaSet<int>                m_components;      // component table ids of visible components attached to this object
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "Visible", m_visible.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "Count", m_count.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "Condition", m_condition.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "AccumulatedDamageTaken", m_accumulatedDamageTaken);
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "PVPFlags", m_pvpFlags);
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "PVPType", m_pvpType.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "PVPFactionId", m_pvpFactionId.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "LastDamageLevel", m_lastDamageLevel);
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "TestDamageLevel", m_testDamageLevel);
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "LastOnOffStatus", m_lastOnOffStatus);
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "IsReceivingCallbacks", m_isReceivingCallbacks);
	//	Watcher<Object>                           m_interestingAttachedObject;
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "UntargettableOverride", m_untargettableOverride);
	//ClientOnlyInteriorLayoutObjectList * m_clientOnlyInteriorLayoutObjectList;

	std::string passiveRevealPlayerCharacter;
	for (Archive::AutoDeltaSet<NetworkId>::const_iterator i = m_passiveRevealPlayerCharacter.begin(); i != m_passiveRevealPlayerCharacter.end(); ++i)
	{
		passiveRevealPlayerCharacter += i->getValueString();
		passiveRevealPlayerCharacter += ", ";
	}
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "PassiveRevealPlayerCharacter", passiveRevealPlayerCharacter);

	ClientObject::getObjectInfo(propertyMap);

#else
	UNREF(propertyMap);
#endif
}


void TangibleObject::OnObjectEffectInsert(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & value)
{
	AddObjectEffect(key, value.first, value.second.first, value.second.second.first, value.second.second.second);
}

void TangibleObject::OnObjectEffectErased(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & value)
{
	UNREF(value);

	RemoveObjectEffect(key);
}

void TangibleObject::OnObjectEffectModified(std::string const & key, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & oldValue, std::pair<std::string, std::pair<std::string, std::pair<Vector, float> > > const & newValue)
{
	UNREF(oldValue);

	RemoveObjectEffect(key);
	AddObjectEffect(key, newValue.first, newValue.second.first, newValue.second.second.first, newValue.second.second.second);
}

// ======================================================================
