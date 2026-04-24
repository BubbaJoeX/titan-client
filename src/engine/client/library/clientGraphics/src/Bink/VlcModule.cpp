// VlcModule.cpp — load libVLC at runtime (LGPL dynamic linking).
#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/VlcModule.h"

#include <string.h>
#include <stdio.h>
#include <vector>

VlcApi g_vlc;
HMODULE  g_hLibVlc = 0;

// ----------------------------------------------------------------------

bool vlcGetModuleDirectory(char *out, size_t outSize)
{
	if (!out || outSize < 2)
		return false;
	out[0] = 0;
	if (!GetModuleFileNameA(NULL, out, (DWORD)outSize))
		return false;
	char *slash = strrchr(out, '\\');
	if (!slash) slash = strrchr(out, '/');
	if (slash) *slash = 0;
	return true;
}

// ----------------------------------------------------------------------

static void vlcClear()
{
	memset(&g_vlc, 0, sizeof(g_vlc));
}

static bool loadSymbol(void *&dest, const char *name)
{
	FARPROC p = GetProcAddress(g_hLibVlc, name);
	if (!p)
		return false;
	dest = (void *)p;
	return true;
}

bool vlcLoad(const char *directoryWithLibVlcDllOrNull)
{
	if (g_hLibVlc)
		return true;

	vlcClear();

	char modDir[MAX_PATH * 2];
	if (directoryWithLibVlcDllOrNull && directoryWithLibVlcDllOrNull[0])
	{
		strcpy(modDir, directoryWithLibVlcDllOrNull);
	}
	else if (!vlcGetModuleDirectory(modDir, sizeof(modDir)))
	{
		return false;
	}

	std::vector<char> path;
	path.resize((size_t)strlen(modDir) + 20);
	_snprintf_s(path.data(), path.size(), _TRUNCATE, "%s\\libvlc.dll", modDir);
	g_hLibVlc = LoadLibraryA(path.data());
	if (!g_hLibVlc)
	{
		// If libvlc is already on the path (e.g. cwd), try the plain name
		g_hLibVlc = LoadLibraryA("libvlc.dll");
		if (!g_hLibVlc)
			return false;
	}

	bool ok = true;
	#define Z(fn) if (!loadSymbol(*(void**)&g_vlc.f_##fn, #fn)) ok = false
	Z(libvlc_new);
	Z(libvlc_release);
	Z(libvlc_errmsg);
	Z(libvlc_media_new_path);
	Z(libvlc_media_release);
	Z(libvlc_media_add_option);
	Z(libvlc_media_player_new);
	Z(libvlc_media_player_release);
	Z(libvlc_media_player_set_media);
	Z(libvlc_media_player_play);
	Z(libvlc_media_player_stop);
	Z(libvlc_media_player_pause);
	Z(libvlc_media_player_get_time);
	Z(libvlc_media_player_set_time);
	Z(libvlc_media_player_get_length);
	Z(libvlc_media_player_get_state);
	Z(libvlc_media_player_is_playing);
	Z(libvlc_video_set_callbacks);
	Z(libvlc_video_set_format);
	Z(libvlc_video_set_format_callbacks);
	Z(libvlc_audio_set_volume);
	Z(libvlc_audio_set_mute);
	#undef Z

	if (!ok)
	{
		WARNING(true, ("VLC: one or more libvlc entry points are missing. Install VideoLAN 3.x DLLs and plugins next to the executable.\n"));
		FreeLibrary(g_hLibVlc);
		g_hLibVlc = 0;
		vlcClear();
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

void vlcUnload()
{
	if (g_hLibVlc)
	{
		FreeLibrary(g_hLibVlc);
		g_hLibVlc = 0;
	}
	vlcClear();
}
