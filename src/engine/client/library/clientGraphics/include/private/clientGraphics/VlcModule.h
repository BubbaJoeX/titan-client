// VlcModule — dynamic load of libVLC (VideoLAN, LGPL 2.1+). Ship libvlc.dll, libvlccore.dll, and plugins/ next to the exe.
#ifndef INCLUDED_VlcModule_H
#define INCLUDED_VlcModule_H

#include "../../../../../../external/3rd/library/vlc-3.0.22/sdk/include/vlc/vlc.h"

#if defined(_WIN32)
#	include <windows.h>
#endif

// Runtime symbols (no static .lib; resolve from libvlc.dll at vlcLoad()).
struct VlcApi
{
	pfn_libvlc_new                          f_libvlc_new;
	pfn_libvlc_release                      f_libvlc_release;
	pfn_libvlc_errmsg                       f_libvlc_errmsg;

	pfn_libvlc_media_new_path               f_libvlc_media_new_path;
	pfn_libvlc_media_release                f_libvlc_media_release;
	pfn_libvlc_media_add_option             f_libvlc_media_add_option;

	pfn_libvlc_media_player_new             f_libvlc_media_player_new;
	pfn_libvlc_media_player_release         f_libvlc_media_player_release;
	pfn_libvlc_media_player_set_media       f_libvlc_media_player_set_media;
	pfn_libvlc_media_player_play            f_libvlc_media_player_play;
	pfn_libvlc_media_player_stop            f_libvlc_media_player_stop;
	pfn_libvlc_media_player_pause            f_libvlc_media_player_pause;
	pfn_libvlc_media_player_get_time         f_libvlc_media_player_get_time;
	pfn_libvlc_media_player_set_time         f_libvlc_media_player_set_time;
	pfn_libvlc_media_player_get_length       f_libvlc_media_player_get_length;
	pfn_libvlc_media_player_get_state        f_libvlc_media_player_get_state;
	pfn_libvlc_media_player_is_playing       f_libvlc_media_player_is_playing;

	pfn_libvlc_video_set_callbacks            f_libvlc_video_set_callbacks;
	pfn_libvlc_video_set_format               f_libvlc_video_set_format;
	pfn_libvlc_video_set_format_callbacks     f_libvlc_video_set_format_callbacks;

	pfn_libvlc_audio_set_volume              f_libvlc_audio_set_volume;
	pfn_libvlc_audio_set_mute                 f_libvlc_audio_set_mute;
};

extern VlcApi g_vlc;
#if defined(_WIN32)
extern HMODULE g_hLibVlc;
#endif

bool vlcGetModuleDirectory(char *out, size_t outSize);
bool vlcLoad (const char *directoryWithLibVlcDllOrNull);
void vlcUnload();

#endif
