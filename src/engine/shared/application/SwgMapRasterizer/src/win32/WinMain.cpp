// ======================================================================
//
// WinMain.cpp
// copyright 2026 
//
// Entry point for SwgMapRasterizer - a tool that renders terrain from
// a top-down orthographic view and captures tile images for use as
// planet map textures (ui_planetmap.inc).
//
// ======================================================================

#include "FirstSwgMapRasterizer.h"
#include "SwgMapRasterizer.h"

#include "sharedFoundation/Production.h"

// ======================================================================

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow
)
{
	UNREF(hPrevInstance);
	UNREF(nCmdShow);

	// Allocate a console for output since this is a tool
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	const int result = SwgMapRasterizer::run(hInstance, lpCmdLine);

	printf("\nPress any key to exit...\n");
	getchar();

	FreeConsole();
	return result;
}

// ======================================================================
