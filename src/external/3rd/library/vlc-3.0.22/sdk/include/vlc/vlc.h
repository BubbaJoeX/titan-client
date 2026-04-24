#ifndef VLC_VLC_H
#define VLC_VLC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libvlc_instance_t libvlc_instance_t;
typedef struct libvlc_media_t libvlc_media_t;
typedef struct libvlc_media_player_t libvlc_media_player_t;

typedef __int64 libvlc_time_t;

typedef void *(*libvlc_video_lock_cb)(void *opaque, void **planes);
typedef void (*libvlc_video_unlock_cb)(void *opaque, void *picture, void *const *planes);
typedef void (*libvlc_video_display_cb)(void *opaque, void *picture);
typedef unsigned (*libvlc_video_format_cb)(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines);
typedef void (*libvlc_video_cleanup_cb)(void *opaque);

typedef void (*libvlc_audio_play_cb)(void *data, const void *samples, unsigned count, __int64 pts);
typedef void (*libvlc_audio_pause_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_resume_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_flush_cb)(void *data, __int64 pts);
typedef void (*libvlc_audio_drain_cb)(void *data);

typedef enum libvlc_state_t
{
    libvlc_NothingSpecial = 0,
    libvlc_Opening,
    libvlc_Buffering,
    libvlc_Playing,
    libvlc_Paused,
    libvlc_Stopped,
    libvlc_Ended,
    libvlc_Error
} libvlc_state_t;

// Function pointer typedefs for dynamic loading
typedef libvlc_instance_t *     (*pfn_libvlc_new)(int argc, const char *const *argv);
typedef void                    (*pfn_libvlc_release)(libvlc_instance_t *p_instance);
typedef const char *            (*pfn_libvlc_errmsg)(void);
typedef libvlc_media_t *        (*pfn_libvlc_media_new_location)(libvlc_instance_t *p_instance, const char *psz_mrl);
typedef libvlc_media_t *        (*pfn_libvlc_media_new_path)(libvlc_instance_t *p_instance, const char *path);
typedef void                    (*pfn_libvlc_media_release)(libvlc_media_t *p_md);
typedef libvlc_media_player_t * (*pfn_libvlc_media_player_new)(libvlc_instance_t *p_instance);
typedef void                    (*pfn_libvlc_media_player_release)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_set_media)(libvlc_media_player_t *p_mi, libvlc_media_t *p_md);
typedef int                     (*pfn_libvlc_media_player_play)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_stop)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_pause)(libvlc_media_player_t *p_mi);
typedef libvlc_time_t           (*pfn_libvlc_media_player_get_time)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_media_player_set_time)(libvlc_media_player_t *p_mi, libvlc_time_t i_time);
typedef libvlc_time_t           (*pfn_libvlc_media_player_get_length)(libvlc_media_player_t *p_mi);
typedef libvlc_state_t          (*pfn_libvlc_media_player_get_state)(libvlc_media_player_t *p_mi);
typedef int                     (*pfn_libvlc_media_player_is_playing)(libvlc_media_player_t *p_mi);
typedef void                    (*pfn_libvlc_video_set_callbacks)(libvlc_media_player_t *mp, libvlc_video_lock_cb lock, libvlc_video_unlock_cb unlock, libvlc_video_display_cb display, void *opaque);
typedef void                    (*pfn_libvlc_video_set_format)(libvlc_media_player_t *mp, const char *chroma, unsigned width, unsigned height, unsigned pitch);
typedef void                    (*pfn_libvlc_video_set_format_callbacks)(libvlc_media_player_t *mp, libvlc_video_format_cb setup, libvlc_video_cleanup_cb cleanup);
typedef void                    (*pfn_libvlc_media_add_option)(libvlc_media_t *p_md, const char *psz_options);
typedef int                     (*pfn_libvlc_audio_set_volume)(libvlc_media_player_t *p_mi, int i_volume);
typedef int                     (*pfn_libvlc_audio_set_mute)(libvlc_media_player_t *p_mi, int mute);
typedef void                    (*pfn_libvlc_audio_set_callbacks)(libvlc_media_player_t *mp, libvlc_audio_play_cb play, libvlc_audio_pause_cb pause, libvlc_audio_resume_cb resume, libvlc_audio_flush_cb flush, libvlc_audio_drain_cb drain, void *opaque);
typedef void                    (*pfn_libvlc_audio_set_format)(libvlc_media_player_t *mp, const char *format, unsigned rate, unsigned channels);

#ifdef __cplusplus
}
#endif

#endif /* VLC_VLC_H */
