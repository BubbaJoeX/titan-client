// ======================================================================
//
// ClientMain.cpp
// Forked from SwgHeadlessClient — lean client for satellite / map capture.
//
// ======================================================================

#include "FirstSwgCameraClient.h"
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
#include "sharedFoundation/ApplicationVersion.h"
#include "sharedFoundation/Binary.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/CrashReportInformation.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/Production.h"
#include "sharedFoundation/SetupSharedFoundation.h"
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
#include "swgClientUserInterface/SwgCuiChatWindow.h"
#include "swgClientUserInterface/SwgCuiManager.h"
#include "swgSharedNetworkMessages/SetupSwgSharedNetworkMessages.h"

#include "Resource.h"

#include "sharedGame/PlatformFeatureBits.h"

#include <cctype>
#include <cstring>
#include <dinput.h>
#include <string>
#include <ctime>
#include <cstdio>

namespace ClientMainNamespace
{
	void installConfigFileOverride ()
	{
		AbstractFile * const abstractFile = TreeFile::open ("misc/override.cfg", AbstractFile::PriorityData, true);
		if (abstractFile)
		{
			int const length = abstractFile->length ();
			byte * const data = abstractFile->readEntireFileAndClose ();
			IGNORE_RETURN (ConfigFile::loadFromBuffer (reinterpret_cast<char const *> (data), length));
			delete [] data;
			delete abstractFile;
		}
	}

	// Optional: -loginUser <id> -loginPassword <pass> → ClientGame loginClientID / loginClientPassword
	// (same keys as ConfigClientGame / SwgCuiLoginScreen). Values with spaces must be quoted.
	static bool extractNextArg(const char *& p, std::string &out)
	{
		out.clear();
		if (!p)
			return false;
		while (*p && static_cast<bool>(std::isspace(static_cast<unsigned char>(*p))))
			++p;
		if (!*p)
			return false;
		if (*p == '"')
		{
			++p;
			while (*p && *p != '"')
				out += *p++;
			if (*p == '"')
				++p;
			return true;
		}
		while (*p && !static_cast<bool>(std::isspace(static_cast<unsigned char>(*p))))
			out += *p++;
		return true;
	}

	static const char *findArgValue(const char *cmdLine, const char *flag)
	{
		if (!cmdLine || !flag)
			return 0;
		const size_t flen = strlen(flag);
		const char *at = cmdLine;
		while ((at = strstr(at, flag)) != 0)
		{
			if (at > cmdLine && !static_cast<bool>(std::isspace(static_cast<unsigned char>(at[-1]))))
			{
				at += flen;
				continue;
			}
			at += flen;
			while (*at && static_cast<bool>(std::isspace(static_cast<unsigned char>(*at))))
				++at;
			return at;
		}
		return 0;
	}

	static void applyLoginOverridesFromCommandLine(const char *cmdLine)
	{
		const char *uStart = findArgValue(cmdLine, "-loginUser");
		const char *pStart = findArgValue(cmdLine, "-loginPassword");
		std::string user;
		std::string pass;
		if (uStart)
		{
			const char *q = uStart;
			IGNORE_RETURN(extractNextArg(q, user));
		}
		if (pStart)
		{
			const char *q = pStart;
			IGNORE_RETURN(extractNextArg(q, pass));
		}
		if (user.empty() && pass.empty())
			return;

		ConfigFile::Section * sec = ConfigFile::getSection("ClientGame");
		if (!sec)
			sec = ConfigFile::createSection("ClientGame");
		if (!sec)
			return;
		if (!user.empty())
		{
			if (sec->getKeyExists("loginClientID"))
				sec->removeKey("loginClientID");
			sec->addKey("loginClientID", user.c_str(), true);
		}
		if (!pass.empty())
		{
			if (sec->getKeyExists("loginClientPassword"))
				sec->removeKey("loginClientPassword");
			sec->addKey("loginClientPassword", pass.c_str(), true);
		}
	}
}

using namespace ClientMainNamespace;

// ======================================================================

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

	const char * configFileName = "client.cfg";
	{
		// get the name of the executable
		char programName[Os::MAX_PATH_LENGTH];
		DWORD result = GetModuleFileName(NULL, programName, sizeof(programName));
		FATAL(result == 0, ("GetModuleFileName failed"));

		// lower case it all
		for (char *s = programName; *s; ++s)
			*s = static_cast<char>(tolower(*s));

		if (strstr(programName, "testswgclient") != 0)
			configFileName = "test.cfg";
	}

	//-- foundation
	SetupSharedFoundation::Data data(SetupSharedFoundation::Data::D_game);
	data.windowName                = "SwgCameraClient";
	data.windowNormalIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	data.windowSmallIcon           = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON2));
	data.hInstance                 = hInstance;
	data.commandLine               = lpCmdLine;
	data.configFile                = configFileName;
	data.clockUsesSleep            = true;
	data.frameRateLimit            = 30.f;
#if PRODUCTION
	data.demoMode                  = true;
#endif
	if (ApplicationVersion::isPublishBuild() || ApplicationVersion::isBootlegBuild())
		data.writeMiniDumps          = true;
	SetupSharedFoundation::install (data);

	REPORT_LOG(true, ("SwgCameraClient: Command Line = \"%s\"\n", lpCmdLine ? lpCmdLine : ""));
	REPORT_LOG (true, ("SwgCameraClient: Memory size = %i MB\n", MemoryManager::getLimit()));

	// check for any config file entries
	if (ConfigFile::isEmpty())
		FATAL(true, ("Config file not specified"));

	InstallTimer::checkConfigFile();

	SetLastError(0);
	HANDLE semaphore = CreateSemaphore(NULL, 0, 1, "SwgCameraClientInstanceRunning");
	if (GetLastError() == ERROR_ALREADY_EXISTS && !ConfigFile::getKeyBool("SwgClient", "allowMultipleInstances", PRODUCTION ? false : true))
	{
		MessageBox(NULL, "Another instance of SwgCameraClient is already running. Application will now close.", NULL, MB_OK | MB_ICONSTOP);
	}
	else
	{
		{
			uint32 gameFeatures = ConfigFile::getKeyInt("Station",  "gameFeatures", 0) & ~ConfigFile::getKeyInt("ClientGame", "gameBitsToClear", 0);
			// hack to set retail if beta or preorder
			if (ConfigFile::getKeyBool("ClientGame",  "setJtlRetailIfBetaIsSet", 0))
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
			Game::setSubscriptionFeatureBits(ConfigFile::getKeyInt("Station",  "subscriptionFeatures", 0));
		}
		
		{
			SetupSharedCompression::Data data;
			data.numberOfThreadsAccessingZlib = 3;
			SetupSharedCompression::install(data);
		}

		//-- Regular expression support.
		SetupSharedRegex::install();

		//-- file
		{
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

		installConfigFileOverride();
		applyLoginOverridesFromCommandLine(lpCmdLine);

		//-- math
		SetupSharedMath::install();

		//-- utility
		SetupSharedUtility::Data setupUtilityData;
		SetupSharedUtility::setupGameData (setupUtilityData);
		setupUtilityData.m_allowFileCaching = true;
		SetupSharedUtility::install (setupUtilityData);

		//-- random
		SetupSharedRandom::install(static_cast<uint32>(time(NULL)));

		SetupSharedLog::install("SwgCameraClient");

		//-- image
		SetupSharedImage::Data setupImageData;
		SetupSharedImage::setupDefaultData (setupImageData);
		SetupSharedImage::install (setupImageData);

		//-- network
		SetupSharedNetwork::SetupData  networkSetupData;
		SetupSharedNetwork::getDefaultClientSetupData(networkSetupData);
		SetupSharedNetwork::install(networkSetupData);

		SetupSharedNetworkMessages::install();
		SetupSwgSharedNetworkMessages::install();

		//-- object
		SetupSharedObject::Data setupObjectData;
		SetupSharedObject::setupDefaultGameData (setupObjectData);
		setupObjectData.useTimedAppearanceTemplates = true;
		// we want the SlotIdManager initialized, and we need the associated hardpoint names loaded.
		SetupSharedObject::addSlotIdManagerData(setupObjectData, true);
		// we want CustomizationData support on the client.
		SetupSharedObject::addCustomizationSupportData(setupObjectData);
		SetupSharedObject::install (setupObjectData);

		//-- game
		SetupSharedGame::Data setupSharedGameData;

		setupSharedGameData.setUseGameScheduler (true);
		setupSharedGameData.setUseMountValidScaleRangeTable (true);
		setupSharedGameData.m_debugBadStringsFunc = CuiManager::debugBadStringIdsFunc;
		SetupSharedGame::install (setupSharedGameData);

		//-- terrain
		SetupSharedTerrain::Data setupSharedTerrainData;
		SetupSharedTerrain::setupGameData (setupSharedTerrainData);
		SetupSharedTerrain::install (setupSharedTerrainData);

		//-- SharedXml
		SetupSharedXml::install();

		//-- pathfinding
		SetupSharedPathfinding::install();
		
		//-- setup client

		//-- audio
		SetupClientAudio::install ();

		//-- graphics
		SetupClientGraphics::Data setupGraphicsData;
		setupGraphicsData.screenWidth  = 1024;
		setupGraphicsData.screenHeight = 768;
		setupGraphicsData.alphaBufferBitDepth = 0;
		SetupClientGraphics::setupDefaultGameData (setupGraphicsData);

		// Same effect as ConfigClientGraphics::setHeadless() before install(), without requiring
		// a rebuild of clientGraphics.lib when that helper was added after the static lib was produced.
		{
			ConfigFile::Section *clientGraphicsSection = ConfigFile::getSection("ClientGraphics");
			if (!clientGraphicsSection)
				clientGraphicsSection = ConfigFile::createSection("ClientGraphics");
			if (clientGraphicsSection)
				clientGraphicsSection->addKey("headless", "1", true);
		}

		if (SetupClientGraphics::install (setupGraphicsData))
		{

			//-- directinput
			SetupClientDirectInput::install(hInstance, Os::getWindow(), DIK_LCONTROL, Graphics::isWindowed);
			DirectInput::setScreenShotFunction(ScreenShotHelper::screenShot);
			DirectInput::setToggleWindowedModeFunction(Graphics::toggleWindowedMode);
			DirectInput::setRequestDebugMenuFunction(Os::requestPopupDebugMenu);

			Os::setLostFocusHookFunction(DirectInput::unacquireAllDevices);

			//-- object
			SetupClientObject::Data setupClientObjectData;
			SetupClientObject::setupGameData (setupClientObjectData);
			SetupClientObject::install (setupClientObjectData);

			//-- animation and skeletal animation
			SetupClientAnimation::install ();

			SetupClientSkeletalAnimation::Data  saData;
			SetupClientSkeletalAnimation::setupGameData(saData);
			SetupClientSkeletalAnimation::install (saData);

			//-- texture renderer
			SetupClientTextureRenderer::install ();

			//-- terrain
			SetupClientTerrain::install ();

			//-- particle system
			SetupClientParticle::install ();

			//-- game
			SetupClientGame::Data data;
			SetupClientGame::setupGameData (data);
			SetupClientGame::install (data);

			CuiManager::setImplementationInstallFunctions (SwgCuiManager::install, SwgCuiManager::remove, SwgCuiManager::update);
			CuiManager::setImplementationTestFunction     (SwgCuiManager::test);

			SetupClientBugReporting::install();

			//-- iowin
			SetupSharedIoWin::install();

			//-- SWG UI (toolbar, ConfigSwgClientUserInterface, etc.) — same as SwgClient; omitting
			// this can leave login / character flow inconsistent with full client.
			SetupSwgClientUserInterface::install();

			//-- run game
			rootInstallTimer.manualExit();
			SetupSharedFoundation::callbackWithExceptionHandling(Game::run);

			//-- save options
			// @todo: write a flexible options load/save system, both of ours suck
			CuiWorkspace * workspace = CuiWorkspace::getGameWorkspace();
			if (workspace != NULL)
			{
				workspace->saveAllSettings();
				SwgCuiChatWindow * chatWindow = safe_cast<SwgCuiChatWindow *>(workspace->findMediatorByType(typeid(SwgCuiChatWindow)));
				if (chatWindow != NULL)
					chatWindow->saveSettings();
			}
			CuiSettings::save();
			CuiChatHistory::save();
			CurrentUserOptionManager::save ();
			LocalMachineOptionManager::save ();
		}
	}

	Audio::setEnabled( false );

	SetupSharedFoundation::remove ();
	SetupSharedThread::remove ();

	if (semaphore)
		CloseHandle(semaphore);

	return 0;
}

// ======================================================================
