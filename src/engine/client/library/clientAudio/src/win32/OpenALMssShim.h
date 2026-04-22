// ======================================================================
// OpenALMssShim.h — Miles AIL_* types and declarations for SWG_USE_OPENAL
// ======================================================================

#ifndef INCLUDED_OpenALMssShim_H
#define INCLUDED_OpenALMssShim_H

#ifndef S8
#define S8 signed char
#endif
#ifndef U8
#define U8 unsigned char
#endif
#ifndef S16
#define S16 signed short
#endif
#ifndef U16
#define U16 unsigned short
#endif
#ifndef S32
#define S32 signed int
#endif
#ifndef U32
#define U32 unsigned int
#endif
#ifndef F32
#define F32 float
#endif
#ifndef C8
#define C8 char
#endif
#ifndef UINTa
#define UINTa unsigned __int64
#endif
#ifndef SINTa
#define SINTa signed __int64
#endif

#ifndef FAR
#define FAR
#endif
#ifndef AILCALL
#define AILCALL __stdcall
#endif
#ifndef AILCALLBACK
#define AILCALLBACK __stdcall
#endif
#ifndef DXDEC
#define DXDEC extern
#endif

#define DIG_MIXER_CHANNELS 1
#define DIG_DS_MIX_FRAGMENT_CNT 42

#define SMP_FREE 0x0001
#define SMP_DONE 0x0002
#define SMP_PLAYING 0x0004
#define SMP_STOPPED 0x0008

#define AIL_NO_ERROR 0
#define AIL_IO_ERROR 1
#define AIL_OUT_OF_MEMORY 2
#define AIL_FILE_NOT_FOUND 3
#define AIL_CANT_WRITE_FILE 4
#define AIL_CANT_READ_FILE 5
#define AIL_DISK_FULL 6

#define AIL_FILE_SEEK_BEGIN 0
#define AIL_FILE_SEEK_CURRENT 1
#define AIL_FILE_SEEK_END 2

#define AILFILETYPE_UNKNOWN 0
#define AILFILETYPE_PCM_WAV 1
#define AILFILETYPE_ADPCM_WAV 2
#define AILFILETYPE_OTHER_WAV 3
#define AILFILETYPE_VOC 4
#define AILFILETYPE_MIDI 5
#define AILFILETYPE_XMIDI 6
#define AILFILETYPE_XMIDI_DLS 7
#define AILFILETYPE_XMIDI_MLS 8
#define AILFILETYPE_DLS 9
#define AILFILETYPE_MLS 10
#define AILFILETYPE_MPEG_L1_AUDIO 11
#define AILFILETYPE_MPEG_L2_AUDIO 12
#define AILFILETYPE_MPEG_L3_AUDIO 13
#define AILFILETYPE_OTHER_ASI_WAV 14

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IMA_ADPCM 0x0011

#define DIG_F_16BITS_MASK 1
#define DIG_F_STEREO_MASK 2
#define DIG_F_ADPCM_MASK 4
#define DIG_F_MONO_8 0
#define DIG_F_MONO_16 (DIG_F_16BITS_MASK)
#define DIG_F_STEREO_8 (DIG_F_STEREO_MASK)
#define DIG_F_STEREO_16 (DIG_F_STEREO_MASK | DIG_F_16BITS_MASK)

enum
{
	ENVIRONMENT_GENERIC,
	ENVIRONMENT_PADDEDCELL,
	ENVIRONMENT_ROOM,
	ENVIRONMENT_BATHROOM,
	ENVIRONMENT_LIVINGROOM,
	ENVIRONMENT_STONEROOM,
	ENVIRONMENT_AUDITORIUM,
	ENVIRONMENT_CONCERTHALL,
	ENVIRONMENT_CAVE,
	ENVIRONMENT_ARENA,
	ENVIRONMENT_HANGAR,
	ENVIRONMENT_CARPETEDHALLWAY,
	ENVIRONMENT_HALLWAY,
	ENVIRONMENT_STONECORRIDOR,
	ENVIRONMENT_ALLEY,
	ENVIRONMENT_FOREST,
	ENVIRONMENT_CITY,
	ENVIRONMENT_MOUNTAINS,
	ENVIRONMENT_QUARRY,
	ENVIRONMENT_PLAIN,
	ENVIRONMENT_PARKINGLOT,
	ENVIRONMENT_SEWERPIPE,
	ENVIRONMENT_UNDERWATER,
	ENVIRONMENT_DRUGGED,
	ENVIRONMENT_DIZZY,
	ENVIRONMENT_PSYCHOTIC,
	ENVIRONMENT_COUNT
};

typedef enum
{
	MSS_MC_INVALID = 0,
	MSS_MC_MONO = 1,
	MSS_MC_STEREO = 2,
	MSS_MC_USE_SYSTEM_CONFIG = 0x10,
	MSS_MC_HEADPHONES = 0x20,
	MSS_MC_DOLBY_SURROUND = 0x30,
	MSS_MC_SRS_CIRCLE_SURROUND = 0x40,
	MSS_MC_40_DTS = 0x48,
	MSS_MC_40_DISCRETE = 0x50,
	MSS_MC_51_DTS = 0x58,
	MSS_MC_51_DISCRETE = 0x60,
	MSS_MC_61_DISCRETE = 0x70,
	MSS_MC_71_DISCRETE = 0x80,
	MSS_MC_81_DISCRETE = 0x90,
	MSS_MC_DIRECTSOUND3D = 0xA0,
	MSS_MC_EAX2 = 0xC0,
	MSS_MC_EAX3 = 0xD0,
	MSS_MC_EAX4 = 0xE0
} MSS_MC_SPEC;

typedef struct MSSVECTOR3D_TAG
{
	F32 x, y, z;
} MSSVECTOR3D;

typedef struct AILSOUNDINFO_TAG
{
	S32 format;
	void const FAR *data_ptr;
	U32 data_len;
	U32 rate;
	S32 bits;
	S32 channels;
	U32 channel_mask;
	U32 samples;
	U32 block_size;
	void const FAR *initial_ptr;
} AILSOUNDINFO;

// OalDig / OalVoice / OalStream: defined in OpenALMssShim.cpp (full definitions), forward-decl only here
struct OalDig;
struct OalVoice;
struct OalStream;

typedef OalDig FAR *HDIGDRIVER;
typedef OalVoice FAR *HSAMPLE;
typedef OalStream FAR *HSTREAM;

typedef U32 HPROVIDER;
typedef SINTa HDRIVERSTATE;

typedef void (AILCALLBACK FAR *AILSAMPLECB)(HSAMPLE sample);
typedef void (AILCALLBACK FAR *AILSTREAMCB)(HSTREAM stream);

typedef U32(AILCALLBACK FAR *AIL_file_open_callback)(C8 const FAR *filename, UINTa FAR *handle);
typedef void(AILCALLBACK FAR *AIL_file_close_callback)(UINTa handle);
typedef S32(AILCALLBACK FAR *AIL_file_seek_callback)(UINTa handle, S32 offset, U32 type);
typedef U32(AILCALLBACK FAR *AIL_file_read_callback)(UINTa handle, void FAR *buffer, U32 bytes);

void OpenAL_Shim_MSS_version(char *str, S32 len);

#ifdef AIL_MSS_version
#undef AIL_MSS_version
#endif
#define AIL_MSS_version(str, len) OpenAL_Shim_MSS_version((str), (S32)(len))

DXDEC S32 AILCALL AIL_startup(void);
DXDEC void AILCALL AIL_shutdown(void);
DXDEC C8 FAR *AILCALL AIL_last_error(void);
DXDEC char FAR *AILCALL AIL_set_redist_directory(char const FAR *dir);
DXDEC void AILCALL AIL_set_file_callbacks(AIL_file_open_callback o, AIL_file_close_callback c, AIL_file_seek_callback s, AIL_file_read_callback r);
DXDEC SINTa AILCALL AIL_get_preference(U32 n);
DXDEC void AILCALL AIL_set_preference(U32 number, SINTa value);
DXDEC HDIGDRIVER AILCALL AIL_open_digital_driver(U32 frequency, S32 bits, S32 channel, U32 flags);
DXDEC void AILCALL AIL_set_3D_rolloff_factor(HDIGDRIVER dig, F32 factor);
DXDEC MSSVECTOR3D FAR *AILCALL AIL_speaker_configuration(HDIGDRIVER dig, S32 FAR *n_phys, S32 FAR *n_log, F32 FAR *falloff, MSS_MC_SPEC FAR *spec);
DXDEC U32 AILCALL AIL_get_timer_highest_delay(void);
DXDEC void AILCALL AIL_serve(void);
DXDEC void AILCALL AIL_lock(void);
DXDEC void AILCALL AIL_unlock(void);
DXDEC S32 AILCALL AIL_file_type(void const FAR *data, U32 size);
DXDEC S32 AILCALL AIL_WAV_info(void const FAR *data, AILSOUNDINFO FAR *info);
DXDEC S32 AILCALL AIL_file_error(void);
DXDEC S32 AILCALL AIL_active_sample_count(HDIGDRIVER dig);
DXDEC HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER dig);
DXDEC void AILCALL AIL_release_sample_handle(HSAMPLE s);
DXDEC S32 AILCALL AIL_set_sample_file(HSAMPLE s, void const FAR *file_image, S32 block);
DXDEC S32 AILCALL AIL_set_named_sample_file(HSAMPLE s, char const FAR *ext, void const FAR *mem, S32 size, S32 block);
DXDEC void AILCALL AIL_set_sample_loop_block(HSAMPLE s, S32 start, S32 end);
DXDEC void AILCALL AIL_set_sample_loop_count(HSAMPLE s, S32 count);
DXDEC AILSAMPLECB AILCALL AIL_register_EOS_callback(HSAMPLE s, AILSAMPLECB cb);
DXDEC void AILCALL AIL_start_sample(HSAMPLE s);
DXDEC void AILCALL AIL_stop_sample(HSAMPLE s);
DXDEC void AILCALL AIL_end_sample(HSAMPLE s);
DXDEC U32 AILCALL AIL_sample_status(HSAMPLE s);
DXDEC void AILCALL AIL_set_sample_volume_levels(HSAMPLE s, F32 l, F32 r);
DXDEC void AILCALL AIL_sample_volume_levels(HSAMPLE s, F32 FAR *l, F32 FAR *r);
DXDEC void AILCALL AIL_set_sample_playback_rate(HSAMPLE s, S32 rate);
DXDEC S32 AILCALL AIL_sample_playback_rate(HSAMPLE s);
DXDEC void AILCALL AIL_set_sample_3D_position(HSAMPLE s, F32 x, F32 y, F32 z);
DXDEC void AILCALL AIL_set_sample_3D_velocity_vector(HSAMPLE s, F32 x, F32 y, F32 z);
DXDEC void AILCALL AIL_set_sample_3D_distances(HSAMPLE s, F32 max_dist, F32 min_dist, S32 auto_rolloff);
DXDEC void AILCALL AIL_set_sample_occlusion(HSAMPLE s, F32 o);
DXDEC void AILCALL AIL_set_sample_obstruction(HSAMPLE s, F32 o);
DXDEC void AILCALL AIL_set_sample_reverb_levels(HSAMPLE s, F32 dry, F32 wet);
DXDEC void AILCALL AIL_sample_reverb_levels(HSAMPLE s, F32 FAR *dry, F32 FAR *wet);
DXDEC void AILCALL AIL_set_sample_ms_position(HSAMPLE s, S32 ms);
DXDEC void AILCALL AIL_sample_ms_position(HSAMPLE s, S32 FAR *total_ms, S32 FAR *cur_ms);
DXDEC U32 AILCALL AIL_sample_position(HSAMPLE s);
DXDEC void AILCALL AIL_set_sample_position(HSAMPLE s, U32 pos);

DXDEC HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, char const FAR *path, S32 mem);
DXDEC void AILCALL AIL_close_stream(HSTREAM stream);
DXDEC HSAMPLE AILCALL AIL_stream_sample_handle(HSTREAM stream);
DXDEC void AILCALL AIL_set_stream_loop_block(HSTREAM st, S32 a, S32 b);
DXDEC void AILCALL AIL_set_stream_loop_count(HSTREAM st, S32 c);
DXDEC AILSTREAMCB AILCALL AIL_register_stream_callback(HSTREAM st, AILSTREAMCB cb);
DXDEC void AILCALL AIL_start_stream(HSTREAM st);
DXDEC S32 AILCALL AIL_stream_status(HSTREAM st);
DXDEC void AILCALL AIL_set_stream_ms_position(HSTREAM st, S32 ms);
DXDEC void AILCALL AIL_stream_ms_position(HSTREAM st, S32 FAR *tot, S32 FAR *cur);

DXDEC void AILCALL AIL_set_listener_3D_position(HDIGDRIVER d, F32 x, F32 y, F32 z);
DXDEC void AILCALL AIL_set_listener_3D_velocity_vector(HDIGDRIVER d, F32 x, F32 y, F32 z);
DXDEC void AILCALL AIL_set_listener_3D_orientation(HDIGDRIVER d, F32 fx, F32 fy, F32 fz, F32 ux, F32 uy, F32 uz);

DXDEC S32 AILCALL AIL_digital_CPU_percent(HDIGDRIVER d);
DXDEC S32 AILCALL AIL_digital_latency(HDIGDRIVER d);
DXDEC void AILCALL AIL_digital_configuration(HDIGDRIVER d, S32 FAR *rate, S32 FAR *format, S32 FAR *channels);

DXDEC void AILCALL AIL_set_room_type(HDIGDRIVER d, S32 env);
DXDEC S32 AILCALL AIL_room_type(HDIGDRIVER d);

#endif // INCLUDED_OpenALMssShim_H
