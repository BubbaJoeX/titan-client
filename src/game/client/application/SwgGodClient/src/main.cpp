// ======================================================================
//
// main.cpp
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"

#include "GodClientPen.h"
#include "MainFrame.h"
#include "SplashScreen.h"

#include <qlabel.h>

#if defined(_DEBUG)
#include <windows.h>
#include <cstdio>
#endif

// ======================================================================
// Ask NVIDIA Optimus / AMD PowerXpress laptops to run the god client on the discrete GPU; the drivers read these
// exports from the executable, and without them a Direct3D 9 application defaults to the integrated GPU.

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// ======================================================================

int main(int argc, char ** argv)
{
#if defined(_DEBUG)
	// SubSystem is Windows + qtmain: no console is attached, so printf / stdout are invisible.
	if (AllocConsole())
	{
		(void)SetConsoleTitleA("SWG Editor (Debug console)");
		FILE * out = nullptr;
		FILE * err = nullptr;
		(void)freopen_s(&out, "CONOUT$", "w", stdout);
		(void)freopen_s(&err, "CONOUT$", "w", stderr);
		fprintf(stderr, "SwgGodClient_d: entering main() (cwd should be exe dir with cfg/DLL/tre).\n");
		fflush(stderr);
	}
#endif

	GodClientApplication application(argc, argv);

	// pen tablet state comes from the application event filter; settings load through QSettings
	GodClientPen::install();

	//show a splash screen until we get the main window ready
	const QRect screen = QApplication::desktop()->screenGeometry();
	SplashScreen* splashScreen = new SplashScreen(0, "splashscreen");
	splashScreen->move(screen.center() - QPoint(splashScreen->width() / 2, splashScreen->height() / 2));
	splashScreen->raise();
	splashScreen->show();
	QApplication::flush();
 
	//get the main window ready
	MainFrame mainFrame (0, "SWG Editor");
	application.setMainWidget(&mainFrame);
	mainFrame.setCaption ("SWG Editor");
	mainFrame.resize (screen.width () - 64, screen.height () - 64);
	mainFrame.move (0,0);
	mainFrame.showMaximized();

	//the app is ready, chuck the splashscreen
	splashScreen->hide ();
	delete splashScreen;

	// run the event loop — detach main widget before stack destruction (Qt3 main widget pointer)
	const int rc = application.exec();
	application.setMainWidget(nullptr);
	GodClientPen::remove();
	return rc;
}

// ======================================================================
