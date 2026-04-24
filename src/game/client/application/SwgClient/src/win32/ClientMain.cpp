// ======================================================================
//
// ClientMain.cpp
// copyright 1998 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstSwgClient.h"
#include "ClientMain.h"

#include "clientAnimation/SetupClientAnimation.h"
#include "clientAudio/Audio.h"
#include "clientAudio/SetupClientAudio.h"
#include "clientBugReporting/SetupClientBugReporting.h"
#include "clientDirectInput/DirectInput.h"
#include "clientDirectInput/SetupClientDirectInput.h"
#include "clientGame/Game.h"
#include "clientGame/SetupClientGame.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ScreenShotHelper.h"
#include "clientGraphics/ShaderTemplate.h"
#include "clientGraphics/SetupClientGraphics.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/VideoList.h"
#include "clientObject/SetupClientObject.h"
#include "clientParticle/SetupClientParticle.h"
#include "clientSkeletalAnimation/SetupClientSkeletalAnimation.h"
#include "clientTerrain/SetupClientTerrain.h"
#include "clientTextureRenderer/SetupClientTextureRenderer.h"
#include "clientUserInterface/CuiChatHistory.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiSettings.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "clientGraphics/IndexedTriangleListAppearance.h"
#include "sharedCompression/SetupSharedCompression.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedDebug/SetupSharedDebug.h"
#include "sharedFile/SetupSharedFile.h"
#include "sharedFile/TreeFile.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ApplicationVersion.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Branch.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Binary.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigFile.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/CrashReportInformation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ExitChain.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation//Os.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/SetupSharedFoundation.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/ConfigSharedFoundation.h"
#include "sharedGame/CommoditiesAdvancedSearchAttribute.h"
#include "sharedGame/SetupSharedGame.h"
#include "sharedImage/SetupSharedImage.h"
#include "sharedIoWin/SetupSharedIoWin.h"
#include "sharedLog/SetupSharedLog.h"
#include "sharedLog/LogManager.h"
#include "sharedMath/SetupSharedMath.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMemoryManager/MemoryManager.h"
#include "sharedNetwork/SetupSharedNetwork.h"
#include "sharedNetworkMessages/SetupSharedNetworkMessages.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/SetupSharedObject.h"
#include "sharedPathfinding/SetupSharedPathfinding.h"
#include "sharedRandom/SetupSharedRandom.h"
#include "sharedRegex/SetupSharedRegex.h"
#include "sharedTerrain/SetupSharedTerrain.h"
#include "sharedTerrain/TerrainAppearance.h"
#include "sharedThread/SetupSharedThread.h"
#include "sharedUtility/CurrentUserOptionManager.h"
#include "sharedUtility/LocalMachineOptionManager.h"
#include "sharedUtility/SetupSharedUtility.h"
#include "sharedXml/SetupSharedXml.h"
#include "swgClientUserInterface/SetupSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAuctionFilter.h"
#include "swgClientUserInterface/SwgCuiChatWindow.h"
#include "swgClientUserInterface/SwgCuiG15Lcd.h"
#include "swgClientUserInterface/SwgCuiManager.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"

#include "clientGame/SplashScreen.h"

#include "Resource.h"

#include "sharedGame/PlatformFeatureBits.h"

#include <dinput.h>
#include <string>
#include <ctime>
#include <cstdio>
#include <cstring>

extern void externalCommandHandler(const char*);

#if defined(_WIN32)
#include <windows.h>
void TitanAppendBootLog(char const *line);
#endif

namespace ClientMainNamespace
{
	void installConfigFileOverride ()
	{
		AbstractFile * const abstractFile = TreeFile::open ("misc/override.cfg", AbstractFile::PriorityData, true);
		if (!abstractFile)
			return;

		int const length = abstractFile->length ();
		if (length < 0)
		{
			REPORT_LOG(true, ("ClientMain: misc/override.cfg length()=%d (invalid), skipping override\n", length));
			delete abstractFile;
			return;
		}

		byte * const data = abstractFile->readEntireFileAndClose ();
		if (!data)
		{
			REPORT_LOG(true, ("ClientMain: misc/override.cfg readEntireFileAndClose failed, skipping override\n"));
			delete abstractFile;
			return;
		}
		IGNORE_RETURN (ConfigFile::loadFromBuffer (reinterpret_cast<char const *> (data), length));
		delete [] data;
		delete abstractFile;
	}
}

using namespace ClientMainNamespace;

// ======================================================================
// Entry point for the application
//
// Return Value:
//
//   Result code to return to the operating system
//
// Remarks:
//
//   This routine should set up the engine, invoke the main game loop,
//   and then tear down the engine.

int ClientMain(
	HINSTANCE hInstance,      // handle to current instance
	HINSTANCE hPrevInstance,  // handle to previous instance
	LPSTR     lpCmdLine,      // pointer to command line
	int       nCmdShow        // show state of window
)
{
	UNREF(hPrevInstance);
	UNREF(nCmdShow);


	//-- thread
	SetupSharedThread::install();

	//-- debug
	SetupSharedDebug::install(4096);

	InstallTimer rootInstallTimer("root");

	char clientWindowName[128];
	// Format: MMDDYYYY-BRANCH-PROJECT (e.g. 02202025-Development-SwgTitan)
	{
		static const char* const monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
		char monthStr[4] = {0};
		int month = 1, day = 1, year = 2025;
		sscanf(__DATE__, "%3s %d %d", monthStr, &day, &year);
		for (int i = 0; i < 12; ++i)
			if (strcmp(monthStr, monthNames[i]) == 0) { month = i + 1; break; }
		IGNORE_RETURN(snprintf(
			clientWindowName,
			sizeof(clientWindowName),
			"SWG: Titan (%d %s %d - Project: Titan)",
			day,
			monthStr,
			year
		));
		clientWindowName[sizeof(clientWindowName) - 1] = '\0';
	}


	//-- foundation
	SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_game);
	data.windowName = clientWindowName;
	data.windowNormalIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	data.windowSmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	data.hInstance = hInstance;
	data.commandLine = lpCmdLine;
#if PRODUCTION == 0
	data.configFile = "titan_d.cfg";
#else
	data.configFile = "titan.cfg";
#endif
	data.clockUsesSleep = true;
	// D_game defaults to clockUsesRecalibrationThread=true, which spawns a worker during
	// SetupSharedFoundation::install (before the main window). That has been a source of
	// hard-to-debug startup stalls; QPC recalibration is optional for the client.
	data.clockUsesRecalibrationThread = false;
	data.minFrameRate = 1.f;
	data.frameRateLimit = 144.f;
#if PRODUCTION
	data.demoMode = true;
#endif
	data.writeMiniDumps = true; // SWG Source Change - Just always write crash log .txt files, there's no reason not to

	SetupSharedFoundation::install(data);

#if defined(_WIN32)
	{
		char cwd[MAX_PATH];
		char line[MAX_PATH + 128];
		if (GetCurrentDirectoryA(sizeof(cwd), cwd))
			_snprintf_s(line, sizeof(line), _TRUNCATE, "ClientMain: after SetupSharedFoundation::install, cwd=%s", cwd);
		else
			_snprintf_s(line, sizeof(line), _TRUNCATE, "%s", "ClientMain: after SetupSharedFoundation::install, GetCurrentDirectory failed");
		TitanAppendBootLog(line);
	}
#endif

	REPORT_LOG(true, ("ClientMain: Command Line = \"%s\"\n", lpCmdLine));
	REPORT_LOG(true, ("ClientMain: Memory size = %i MB\n", MemoryManager::getLimit()));

	// check for any config file entries
	if (ConfigFile::isEmpty())
		FATAL(true, ("Config file not specified"));

	REPORT_LOG(true, ("ClientMain: config OK (not empty), before InstallTimer::checkConfigFile\n"));
	InstallTimer::checkConfigFile();
	REPORT_LOG(true, ("ClientMain: after InstallTimer::checkConfigFile, before instance semaphore\n"));

	SetLastError(0);
	HANDLE semaphore = CreateSemaphore(NULL, 0, 1, "SwgClientInstanceRunning");
	if (GetLastError() == ERROR_ALREADY_EXISTS && !ConfigFile::getKeyBool("SwgClient", "allowMultipleInstances", PRODUCTION ? false : true))
	{
		MessageBox(NULL, "Another instance of this application is already running.  Application will now close.", NULL, MB_OK | MB_ICONSTOP);
	}
	else
	{
		REPORT_LOG(true, ("ClientMain: instance semaphore OK, starting subsystem install chain (compression through audio)\n"));
		REPORT_LOG(true, ("ClientMain: before Game::setGameFeatureBits / subscription / externalCommandHandler\n"));
		{
			uint32 gameFeatures = ConfigFile::getKeyInt("Station", "gameFeatures", 0) & ~ConfigFile::getKeyInt("ClientGame", "gameBitsToClear", 0);
			// hack to set retail if beta or preorder
			if (ConfigFile::getKeyBool("ClientGame", "setJtlRetailIfBetaIsSet", 0))
			{
				if (gameFeatures & (ClientGameFeature::SpaceExpansionBeta | ClientGameFeature::SpaceExpansionPreOrder))
					gameFeatures |= ClientGameFeature::SpaceExpansionRetail;
			}

			//-- set ep3 retail if beta or preorder
			if (gameFeatures & (ClientGameFeature::Episode3PreorderDownload))
				gameFeatures |= ClientGameFeature::Episode3ExpansionRetail;

			//-- set Obiwan retail if beta or preorder
			if (gameFeatures & ClientGameFeature::TrialsOfObiwanPreorder)
				gameFeatures |= ClientGameFeature::TrialsOfObiwanRetail;
			Game::setGameFeatureBits(gameFeatures);
			Game::setSubscriptionFeatureBits(ConfigFile::getKeyInt("Station", "subscriptionFeatures", 0));
			Game::setExternalCommandHandler(externalCommandHandler);
		}
		REPORT_LOG(true, ("ClientMain: after Game:: feature setters, before SetupSharedCompression::install (Zlib pool)\n"));

		{
			SetupSharedCompression::Data data;
			data.numberOfThreadsAccessingZlib = 3;
			SetupSharedCompression::install(data);
		}
		REPORT_LOG(true, ("ClientMain: after SetupSharedCompression::install\n"));

		//-- Regular expression support.
		SetupSharedRegex::install();
		REPORT_LOG(true, ("ClientMain: after SetupSharedRegex::install\n"));

		//-- file
		{
			REPORT_LOG(true, ("ClientMain: before SetupSharedFile::install (TreeFile / search path; can block on bad or huge data dir)\n"));
			// figure out what skus we need to support in the tree file system
			uint32 skuBits = 0;
			if ((Game::getGameFeatureBits() & ClientGameFeature::Base) != 0)
				skuBits |= BINARY1(0001);
			if ((Game::getGameFeatureBits() & ClientGameFeature::SpaceExpansionRetail) != 0)
				skuBits |= BINARY1(0010);
			if ((Game::getGameFeatureBits() & ClientGameFeature::Episode3ExpansionRetail) != 0)
				skuBits |= BINARY1(0100);
			if ((Game::getGameFeatureBits() & ClientGameFeature::TrialsOfObiwanRetail) != 0)
				skuBits |= BINARY1(1000);

			SetupSharedFile::install(true, skuBits);
		}
		REPORT_LOG(true, ("ClientMain: after SetupSharedFile::install\n"));

		installConfigFileOverride();
		REPORT_LOG(true, ("ClientMain: after installConfigFileOverride\n"));

		//-- math
		SetupSharedMath::install();
		REPORT_LOG(true, ("ClientMain: after SetupSharedMath::install\n"));
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: ODS after SetupSharedMath REPORT, before utility stack\r\n");
#endif

		//-- utility
		SetupSharedUtility::Data setupUtilityData;
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: ODS after SetupSharedUtility::Data, before setupGameData\r\n");
#endif
		SetupSharedUtility::setupGameData(setupUtilityData);
		setupUtilityData.m_allowFileCaching = true;
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: ODS before SetupSharedUtility::install\r\n");
#endif
		SetupSharedUtility::install(setupUtilityData);
		REPORT_LOG(true, ("ClientMain: after SetupSharedUtility::install\n"));

		//-- random
		SetupSharedRandom::install(static_cast<uint32>(time(NULL)));
		REPORT_LOG(true, ("ClientMain: after SetupSharedRandom::install\n"));

		SetupSharedLog::install("SwgClient");
		REPORT_LOG(true, ("ClientMain: after SetupSharedLog::install\n"));

		//-- image
		SetupSharedImage::Data setupImageData;
		SetupSharedImage::setupDefaultData(setupImageData);
		SetupSharedImage::install(setupImageData);
		REPORT_LOG(true, ("ClientMain: after SetupSharedImage::install\n"));

		//-- network
		SetupSharedNetwork::SetupData  networkSetupData;
		SetupSharedNetwork::getDefaultClientSetupData(networkSetupData);
		SetupSharedNetwork::install(networkSetupData);
		REPORT_LOG(true, ("ClientMain: after SetupSharedNetwork::install\n"));

		SetupSharedNetworkMessages::install();
		SetupSwgSharedNetworkMessages::install();
		REPORT_LOG(true, ("ClientMain: after network messages setup\n"));

		//-- object
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: before SetupSharedObject::Data + setup (slot/movement/customization)\r\n");
#endif
		SetupSharedObject::Data setupObjectData;
		SetupSharedObject::setupDefaultGameData(setupObjectData);
		setupObjectData.useTimedAppearanceTemplates = true;
		// we want the SlotIdManager initialized, and we need the associated hardpoint names loaded.
		SetupSharedObject::addSlotIdManagerData(setupObjectData, true);
		// we want CustomizationData support on the client.
		SetupSharedObject::addCustomizationSupportData(setupObjectData);
		SetupSharedObject::addMovementTableData(setupObjectData);
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: before SetupSharedObject::install (loads slot_definitions / movementstates / customization iff)\r\n");
#endif
		SetupSharedObject::install(setupObjectData);
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: after SetupSharedObject::install\r\n");
#endif
		REPORT_LOG(true, ("ClientMain: after SetupSharedObject::install\n"));

		//-- game
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: SSG before default Data() (crash here = static init order / heap)\r\n");
#endif
		SetupSharedGame::Data setupSharedGameData;
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: SSG after Data() ctor, before setters\r\n");
#endif
		setupSharedGameData.setUseGameScheduler(true);
		setupSharedGameData.setUseMountValidScaleRangeTable(true);
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: SSG after setters, before CuiManager::debugBadStringIdsFunc assign\r\n");
#endif
		setupSharedGameData.m_debugBadStringsFunc = CuiManager::debugBadStringIdsFunc;
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: SSG before SetupSharedGame::install() call (see [Titan] SSG: lines inside)\r\n");
#endif
		SetupSharedGame::install(setupSharedGameData);
#if defined(_WIN32)
		::OutputDebugStringA("[Titan] ClientMain: after SetupSharedGame::install\r\n");
#endif
		REPORT_LOG(true, ("ClientMain: after SetupSharedGame::install\n"));

		CommoditiesAdvancedSearchAttribute::install();
		SwgCuiAuctionFilter::buildAttributeFilterDisplayString(); // must be called after CommoditiesAdvancedSearchAttribute::install()

		//-- terrain
		SetupSharedTerrain::Data setupSharedTerrainData;
		SetupSharedTerrain::setupGameData(setupSharedTerrainData);
		SetupSharedTerrain::install(setupSharedTerrainData);
		REPORT_LOG(true, ("ClientMain: after SetupSharedTerrain::install\n"));

		//-- SharedXml
		SetupSharedXml::install();
		REPORT_LOG(true, ("ClientMain: after SetupSharedXml::install\n"));

		//-- pathfinding
		SetupSharedPathfinding::install();
		REPORT_LOG(true, ("ClientMain: after SetupSharedPathfinding::install\n"));

		//-- setup client

		//-- audio
		SetupClientAudio::install();
		REPORT_LOG(true, ("ClientMain: after SetupClientAudio::install, next: SetupClientGraphics (D3D)\n"));

		//-- graphics
		SetupClientGraphics::Data setupGraphicsData;
		setupGraphicsData.screenWidth = 1024;
		setupGraphicsData.screenHeight = 768;
		setupGraphicsData.alphaBufferBitDepth = 0;
		SetupClientGraphics::setupDefaultGameData(setupGraphicsData);

		REPORT_LOG(true, ("ClientMain: calling SetupClientGraphics::install (D3D / gl05_r / raster)\n"));
		if (SetupClientGraphics::install(setupGraphicsData))
		{
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 200 SetupClientGraphics::install true (splash->shutdown saves)\r\n");
#endif
			REPORT_LOG(true, ("ClientMain: SetupClientGraphics::install succeeded; splash / DirectInput / UI setup\n"));
			// Install and render splash screen immediately after graphics initialization
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 210 SplashScreen::install ->\r\n");
#endif
			SplashScreen::install();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 211 SplashScreen::install <-; render+preload ->\r\n");
#endif
			SplashScreen::render();
			SplashScreen::preloadConfiguredAssets();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 212 SplashScreen render+preload done\r\n");
#endif

#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 220 VideoList::install ->\r\n");
#endif
			VideoList::install(Audio::getMilesDigitalDriver());
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 221 VideoList::install <-; pump after VideoList\r\n");
#endif
			SplashScreen::pump();

			//-- directinput
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 230 SetupClientDirectInput::install ->\r\n");
#endif
			SetupClientDirectInput::install(hInstance, Os::getWindow(), DIK_LCONTROL, Graphics::isWindowed);
			DirectInput::setScreenShotFunction(ScreenShotHelper::screenShot);
			DirectInput::setToggleWindowedModeFunction(Graphics::toggleWindowedMode);
			DirectInput::setRequestDebugMenuFunction(Os::requestPopupDebugMenu);
			Os::setLostFocusHookFunction(DirectInput::unacquireAllDevices);
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 231 directinput hooks set; pump\r\n");
#endif
			SplashScreen::pump();

			//-- object
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 240 SetupClientObject::install ->\r\n");
#endif
			SetupClientObject::Data setupClientObjectData;
			SetupClientObject::setupGameData(setupClientObjectData);
			SetupClientObject::install(setupClientObjectData);
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 241 SetupClientObject::install <-; pump\r\n");
#endif
			SplashScreen::pump();

			//-- animation and skeletal animation
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 250 SetupClientAnimation::install ->\r\n");
#endif
			SetupClientAnimation::install();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 251 SetupClientSkeletalAnimation::install ->\r\n");
#endif

			SetupClientSkeletalAnimation::Data  saData;
			SetupClientSkeletalAnimation::setupGameData(saData);
			SetupClientSkeletalAnimation::install(saData);
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 252 animation/skeletal done; pump\r\n");
#endif
			SplashScreen::pump();

			//-- texture renderer
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 260 SetupClientTextureRenderer::install ->\r\n");
#endif
			SetupClientTextureRenderer::install();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 261 texture renderer done; pump\r\n");
#endif
			SplashScreen::pump();

			//-- terrain
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 270 SetupClientTerrain::install ->\r\n");
#endif
			SetupClientTerrain::install();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 271 SetupClientTerrain::install <-\r\n");
#endif

			//-- particle system
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 280 SetupClientParticle::install ->\r\n");
#endif
			SetupClientParticle::install();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 281 particle done; pump\r\n");
#endif
			SplashScreen::pump();

			//-- game
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 290 SetupClientGame::install ->\r\n");
#endif
			SetupClientGame::Data data;
			SetupClientGame::setupGameData(data);
			SetupClientGame::install(data);
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 291 SetupClientGame::install <-; pump\r\n");
#endif
			SplashScreen::pump();

#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 300 CuiManager implementation + bug reporting + IoWin + UI ->\r\n");
#endif
			CuiManager::setImplementationInstallFunctions(SwgCuiManager::install, SwgCuiManager::remove, SwgCuiManager::update);
			CuiManager::setImplementationTestFunction(SwgCuiManager::test);

			SetupClientBugReporting::install();

			//-- iowin
			SetupSharedIoWin::install();

			//-- setup the client user interface.
			SetupSwgClientUserInterface::install();
			REPORT_LOG(true, ("ClientMain: after SetupSwgClientUserInterface::install (UI shell; Game::run -> login/cluster)\n"));
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 301 client UI install done; pump\r\n");
#endif
			SplashScreen::pump();

			//-- G15 LCD
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 310 SwgCuiG15Lcd::initializeLcd ->\r\n");
#endif
			SwgCuiG15Lcd::initializeLcd();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 311 G15 init <-; Game::run next\r\n");
#endif

			//-- run game
			rootInstallTimer.manualExit();
			REPORT_LOG(true, ("ClientMain: entering Game::run (main loop; login appears when UI reaches it)\n"));
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 400 callbackWithExceptionHandling(Game::run) ->\r\n");
#endif
			SetupSharedFoundation::callbackWithExceptionHandling(Game::run);
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 401 Game::run returned; shutdown save sequence\r\n");
#endif
			REPORT_LOG(true, ("ClientMain: Game::run returned\n"));

			//-- save options
			// @todo: write a flexible options load/save system, both of ours suck
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 500 CuiWorkspace saveAllSettings (if any)\r\n");
#endif
			CuiWorkspace * workspace = CuiWorkspace::getGameWorkspace();
			if (workspace != NULL)
			{
				workspace->saveAllSettings();
				SwgCuiChatWindow * chatWindow = safe_cast<SwgCuiChatWindow *>(workspace->findMediatorByType(typeid(SwgCuiChatWindow)));
				if (chatWindow != NULL)
					chatWindow->saveSettings();
			}
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 510 CuiSettings::save + CuiChatHistory::save + CurrentUserOptionManager::save ->\r\n");
#endif
			CuiSettings::save();
			CuiChatHistory::save();
			CurrentUserOptionManager::save();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 520 LocalMachineOptionManager::save (next; see LMO: lines in util)\r\n");
#endif
			LocalMachineOptionManager::save();
#if defined(_WIN32)
			::OutputDebugStringA("[Titan] PostGfx: 599 post-splash path complete (all saves done)\r\n");
#endif
		}
		else
		{
			REPORT_LOG(
				true,
				("ClientMain: SetupClientGraphics::install FAILED - game loop not started. Typical: ms_api->install (D3D) false, or missing gl05_d.dll / Direct3d9_d.dll / d3d9.\n"));
#if defined(_WIN32)
			TitanAppendBootLog("ClientMain: SetupClientGraphics::install returned false");
			MessageBoxA(
				nullptr,
				"Graphics initialization failed (SetupClientGraphics::install returned false).\n\n"
				"The client sets its working directory to the folder containing the exe so titan_d.cfg "
				"and the tres\\ data trees can be found.\n\n"
				"Check that this folder contains titan_d.cfg, a tres\\ subdirectory, and GPU DLLs "
				"(e.g. Direct3d9_*.dll, gl05_*.dll) next to the executable.\n\n"
				"Details are appended to logs\\SwgTitan_boot.log beside the executable.",
				"SWG Titan — Graphics initialization failed",
				MB_OK | MB_ICONERROR);
#endif
		}
	}

	SetupSharedFoundation::remove();
	SetupSharedThread::remove();

	if (semaphore)
		CloseHandle(semaphore);
	return 0;

}
// ======================================================================
