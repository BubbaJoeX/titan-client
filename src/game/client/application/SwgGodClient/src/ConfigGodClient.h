//
// ConfigGodClient.h
//
// copyright 2000, verant interactive
//

//-------------------------------------------------------------------

#ifndef CONFIGGodClient_H
#define CONFIGGodClient_H

//-------------------------------------------------------------------

/**
* The Configuration
*/

class ConfigGodClient
{
public:

	struct Data
	{
		/** depot-relative path for script classes */
		const char *    scriptClassPath;
		/** depot-relative path for script sources */
		const char *    scriptSourcePath;
		/** executable name, relative, or absolute path to script editor app */
		const char *    scriptEditor;
		const char *    templateClientIffPath;
		const char *    templateClientSourcePath;
		const char *    templateServerIffPath;
		const char *    templateServerSourcePath;
		const char *    templateEditor;

//		const char *    localScriptClassPath;
//		const char *    localTemplateClientIffPath;
//		const char *    localTemplateServerIffPath;
		const char *    localClientSrcPath;
		const char *    localClientDataPath;
		const char *    localServerSrcPath;
		const char *    localServerDataPath;
		const char *    localServerCrcStringTable;
		/** When set, buildout save/load uses this root instead of dsrc/data (local rebuild) */
		const char *    buildoutsRoot;



		int             frameRateLimit;
		/** If both > 0, God Client game view uses these instead of titan.cfg / SharedFoundation screen size. */
		int             gameScreenWidth;
		int             gameScreenHeight;
	};

private:
	static Data *data;

public:
	static void        install(void);
	static void        remove(void);

	static const Data & getData ();

	static const char* getServerObjectTemplateTheaterDirectory ();
	static const char* getServerDataTableTheaterDirectory ();
	static const char* getSharedObjectTemplateTheaterDirectory ();
	static const char* getClientDataFileTheaterDirectory ();
	static const char* getServerObjectTemplateTheaterPath ();
	static const char* getServerDataTableTheaterPath ();
	static const char* getSharedObjectTemplateTheaterPath ();
	static const char* getClientDataFileTheaterPath ();
	static const char* getSharedTerrainDirectory ();

	static bool getConnectToPerforce ();
	static bool getLoadServerObjects ();

	static bool getUseGameScreenSize (int &outWidth, int &outHeight);
};

//-------------------------------------------------------------------

inline const ConfigGodClient::Data & ConfigGodClient::getData ()
{
	NOT_NULL (data);
	return *data;
}


//-------------------------------------------------------------------

#endif
