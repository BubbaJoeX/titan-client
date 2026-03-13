// ======================================================================
//
// ExportManager.h
// copyright 2001 Sony Online Entertainment
// 
// ======================================================================

#ifndef ExportManager_H
#define ExportManager_H

// ======================================================================

//forward declarations
class Messenger;

// ======================================================================

class ExportManager
{
public:
		static void install(Messenger* newMessenger);
		static void remove();

		static bool validateTextureList(bool showGUI);

		static void LaunchViewer(const std::string& asset);

private:
		static Messenger   *messenger;
};

// ======================================================================

#endif //ExportManager_H
