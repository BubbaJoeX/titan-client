// ======================================================================
// OpenALMssShim.cpp — Miles AIL_* implementation on OpenAL Soft (x64)
// ======================================================================

#include "clientAudio/FirstClientAudio.h"

#if defined(SWG_USE_OPENAL)

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include <windows.h>

// Types must match the opaque typedefs in OpenALMssShim.h (not in an anonymous namespace).
struct OalStream;

struct OalVoice
{
	ALuint source = 0;
	ALuint buffer = 0;
	std::vector<S16> pcm;
	int rate = 22050;
	int channels = 2;
	bool is3d = false;
	bool loaded = false;
	U32 status = SMP_FREE;
	AILSAMPLECB eosCb = nullptr;
	AILSTREAMCB streamCb = nullptr;
	OalStream *streamOwner = nullptr;
	S32 loopStartFrame = 0;
	S32 loopEndFrame = -1;
	S32 loopCount = 1;
	S32 loopsRemaining = 0;
	F32 volL = 1.f;
	F32 volR = 1.f;
	F32 dryRv = 1.f;
	F32 wetRv = 0.f;
	F32 occ = 0.f;
	F32 obs = 0.f;
	F32 refDist = 1.f;
	F32 maxDist = 10000.f;
	S32 playbackRate = 22050;
	bool eosFired = false;
};

struct OalStream
{
	OalVoice *voice;
};

struct OalDig
{
	int dummy = 0;
};

namespace
{
CRITICAL_SECTION g_cs;
bool g_csInit = false;

ALCdevice *g_device = nullptr;
ALCcontext *g_context = nullptr;
bool g_started = false;

OalDig *g_theDig = nullptr;
S32 g_roomType = ENVIRONMENT_GENERIC;
F32 g_rolloff = 1.f;
S32 g_mixChannels = 32;
S32 g_digRate = 22050;
S32 g_digFormat = DIG_F_STEREO_16;

AIL_file_open_callback g_fopen = nullptr;
AIL_file_close_callback g_fclose = nullptr;
AIL_file_seek_callback g_fseek = nullptr;
AIL_file_read_callback g_fread = nullptr;

char g_err[256];
char g_redistDir[256] = "openal";

__declspec(thread) S32 g_file_err = AIL_NO_ERROR;

void setErr(char const *msg)
{
	strncpy_s(g_err, msg, _TRUNCATE);
}

std::vector<OalVoice *> g_allVoices;

void lock()
{
	if (g_csInit)
		EnterCriticalSection(&g_cs);
}

void unlock()
{
	if (g_csInit)
		LeaveCriticalSection(&g_cs);
}

void destroyVoice(OalVoice *v)
{
	if (!v)
		return;
	if (v->source)
	{
		alSourceStop(v->source);
		alDeleteSources(1, &v->source);
		v->source = 0;
	}
	if (v->buffer)
	{
		alDeleteBuffers(1, &v->buffer);
		v->buffer = 0;
	}
	v->pcm.clear();
	v->loaded = false;
	v->status = SMP_FREE;
}

bool ensureContext()
{
	if (g_context && alcGetCurrentContext() == g_context)
		return true;
	if (g_context)
		return alcMakeContextCurrent(g_context) == ALC_TRUE;
	return false;
}

void apply3d(OalVoice *v)
{
	if (!v || !v->source || !v->is3d)
		return;
	alSourcei(v->source, AL_SOURCE_RELATIVE, AL_FALSE);
	F32 g = std::max(0.f, 1.f - 0.35f * (v->occ + v->obs));
	alSourcef(v->source, AL_GAIN, g);
}

void applyVolume(OalVoice *v)
{
	if (!v || !v->source)
		return;
	F32 g = std::max(0.f, std::max(v->volL, v->volR));
	if (!v->is3d)
		alSourcef(v->source, AL_GAIN, g);
	else
		apply3d(v);
}

void rebuildAndQueue(OalVoice *v, bool forInfiniteLoop)
{
	if (!v->loaded || v->pcm.empty() || !v->source)
		return;
	ensureContext();

	int totalFrames = static_cast<int>(v->pcm.size() / std::max(1, v->channels));
	int s = 0;
	int e = totalFrames;
	if (v->loopEndFrame > v->loopStartFrame && v->loopEndFrame <= totalFrames)
	{
		s = v->loopStartFrame;
		e = v->loopEndFrame;
	}
	int frames = std::max(1, e - s);
	ALenum fmt = (v->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	alSourceStop(v->source);
	if (v->buffer)
	{
		alSourcei(v->source, AL_BUFFER, 0);
		alDeleteBuffers(1, &v->buffer);
		v->buffer = 0;
	}
	alGenBuffers(1, &v->buffer);
	alBufferData(v->buffer, fmt, &v->pcm[s * v->channels],
	             frames * v->channels * static_cast<ALsizei>(sizeof(S16)), v->rate);
	alSourcei(v->source, AL_BUFFER, static_cast<ALint>(v->buffer));
	alSourcei(v->source, AL_LOOPING, forInfiniteLoop ? AL_TRUE : AL_FALSE);
	alSourcef(v->source, AL_REFERENCE_DISTANCE, v->refDist);
	alSourcef(v->source, AL_MAX_DISTANCE, v->maxDist);
	alSourcef(v->source, AL_ROLLOFF_FACTOR, g_rolloff);
	{
		F32 pr = static_cast<F32>(v->playbackRate) / static_cast<F32>(std::max(1, v->rate));
		if (pr <= 0.f)
			pr = 1.f;
		alSourcef(v->source, AL_PITCH, pr);
	}
	applyVolume(v);
}

bool decodeWavMem(void const *data, U32 size, OalVoice *v, char *errBuf, size_t errLen)
{
	if (!data || errBuf == nullptr || errLen == 0)
		return false;
	U8 const *p = static_cast<U8 const *>(data);
	if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
	{
		strncpy_s(errBuf, errLen, "Not RIFF WAVE", _TRUNCATE);
		return false;
	}
	U32 effSize = size;
	if (size >= 0x7F000000U)
	{
		U32 riffChunk = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
		effSize = 8 + riffChunk;
	}
	if (effSize < 24)
	{
		strncpy_s(errBuf, errLen, "WAV too small", _TRUNCATE);
		return false;
	}
	U32 off = 12;
	U16 audioFormat = 0;
	U16 numChannels = 0;
	U32 sampleRate = 0;
	U16 bits = 0;
	U32 dataSize = 0;
	U8 const *dataChunk = nullptr;
	while (off + 8 <= effSize)
	{
		U32 chunkId = p[off] | (p[off + 1] << 8) | (p[off + 2] << 16) | (p[off + 3] << 24);
		U32 chunkSize = p[off + 4] | (p[off + 5] << 8) | (p[off + 6] << 16) | (p[off + 7] << 24);
		off += 8;
		if (off + chunkSize > effSize)
			break;
		if (chunkId == 0x20746d66)
		{
			if (chunkSize < 16)
				break;
			audioFormat = static_cast<U16>(p[off] | (p[off + 1] << 8));
			numChannels = static_cast<U16>(p[off + 2] | (p[off + 3] << 8));
			sampleRate = p[off + 4] | (p[off + 5] << 8) | (p[off + 6] << 16) | (p[off + 7] << 24);
			bits = static_cast<U16>(p[off + 14] | (p[off + 15] << 8));
		}
		else if (chunkId == 0x61746164)
		{
			dataChunk = p + off;
			dataSize = chunkSize;
		}
		off += chunkSize + (chunkSize & 1);
	}
	if (!dataChunk || dataSize == 0)
	{
		strncpy_s(errBuf, errLen, "No data chunk", _TRUNCATE);
		return false;
	}
	if (audioFormat != WAVE_FORMAT_PCM || (bits != 8 && bits != 16) || (numChannels < 1 || numChannels > 2))
	{
		strncpy_s(errBuf, errLen, "Unsupported WAV (need PCM 8/16 mono/stereo)", _TRUNCATE);
		return false;
	}
	v->rate = static_cast<int>(sampleRate);
	v->channels = numChannels;
	U32 nSamp = dataSize / ((bits / 8) * numChannels);
	v->pcm.resize(nSamp * numChannels);
	if (bits == 16)
	{
		memcpy(v->pcm.data(), dataChunk, nSamp * numChannels * sizeof(S16));
	}
	else
	{
		for (U32 i = 0; i < nSamp * numChannels; ++i)
			v->pcm[static_cast<size_t>(i)] = static_cast<S16>((static_cast<int>(dataChunk[i]) - 128) << 8);
	}
	v->playbackRate = v->rate;
	v->loaded = true;
	return true;
}

OalVoice *voiceFrom(HSAMPLE s)
{
	return reinterpret_cast<OalVoice *>(s);
}

void fireEos(OalVoice *v)
{
	if (!v || v->eosFired)
		return;
	v->eosFired = true;
	v->status = SMP_DONE;
	if (v->streamOwner && v->streamCb)
		v->streamCb(v->streamOwner);
	else if (v->eosCb)
		v->eosCb(reinterpret_cast<HSAMPLE>(v));
}

void pollStopped()
{
	lock();
	ensureContext();
	for (OalVoice *v : g_allVoices)
	{
		if (!v || !v->source || v->status != SMP_PLAYING)
			continue;
		ALint st = AL_STOPPED;
		alGetSourcei(v->source, AL_SOURCE_STATE, &st);
		if (st != AL_STOPPED)
			continue;
		if (v->loopCount == 0)
			continue;
		if (v->loopsRemaining > 1)
		{
			v->loopsRemaining--;
			v->eosFired = false;
			alSourceRewind(v->source);
			alSourcePlay(v->source);
		}
		else
			fireEos(v);
	}
	unlock();
}
} // namespace

void OpenAL_Shim_MSS_version(char *str, S32 len)
{
	if (!str || len <= 0)
		return;
	strncpy_s(str, static_cast<size_t>(len), "OpenAL Soft (SWG shim)", _TRUNCATE);
}

DXDEC S32 AILCALL AIL_startup(void)
{
	if (!g_csInit)
	{
		InitializeCriticalSection(&g_cs);
		g_csInit = true;
	}
	lock();
	if (g_started)
	{
		unlock();
		return 1;
	}
	g_device = alcOpenDevice(nullptr);
	if (!g_device)
	{
		setErr("alcOpenDevice failed");
		unlock();
		return 0;
	}
	ALCint attrs[] = { ALC_FREQUENCY, 44100, 0 };
	g_context = alcCreateContext(g_device, attrs);
	if (!g_context || !alcMakeContextCurrent(g_context))
	{
		setErr("alcCreateContext failed");
		if (g_context)
			alcDestroyContext(g_context);
		g_context = nullptr;
		alcCloseDevice(g_device);
		g_device = nullptr;
		unlock();
		return 0;
	}
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	g_theDig = new OalDig;
	g_started = true;
	g_file_err = AIL_NO_ERROR;
	setErr("");
	unlock();
	return 1;
}

DXDEC void AILCALL AIL_shutdown(void)
{
	lock();
	if (!g_started)
	{
		unlock();
		return;
	}
	ensureContext();
	for (OalVoice *v : g_allVoices)
	{
		destroyVoice(v);
		delete v;
	}
	g_allVoices.clear();
	delete g_theDig;
	g_theDig = nullptr;
	alcMakeContextCurrent(nullptr);
	if (g_context)
	{
		alcDestroyContext(g_context);
		g_context = nullptr;
	}
	if (g_device)
	{
		alcCloseDevice(g_device);
		g_device = nullptr;
	}
	g_started = false;
	unlock();
}

DXDEC C8 FAR *AILCALL AIL_last_error(void)
{
	return g_err;
}

DXDEC char FAR *AILCALL AIL_set_redist_directory(char const FAR *dir)
{
	if (dir)
		strncpy_s(g_redistDir, dir, _TRUNCATE);
	return g_redistDir;
}

DXDEC void AILCALL AIL_set_file_callbacks(AIL_file_open_callback o, AIL_file_close_callback c, AIL_file_seek_callback s, AIL_file_read_callback r)
{
	g_fopen = o;
	g_fclose = c;
	g_fseek = s;
	g_fread = r;
}

DXDEC SINTa AILCALL AIL_get_preference(U32 n)
{
	if (n == DIG_MIXER_CHANNELS)
		return g_mixChannels;
	return 0;
}

DXDEC void AILCALL AIL_set_preference(U32 number, SINTa value)
{
	(void)number;
	(void)value;
}

DXDEC HDIGDRIVER AILCALL AIL_open_digital_driver(U32 frequency, S32 bits, S32 channel, U32 flags)
{
	(void)flags;
	lock();
	if (!g_started)
	{
		unlock();
		return nullptr;
	}
	g_digRate = static_cast<S32>(frequency);
	if (bits == 8)
		g_digFormat = (channel == MSS_MC_STEREO) ? DIG_F_STEREO_8 : DIG_F_MONO_8;
	else
		g_digFormat = (channel == MSS_MC_STEREO) ? DIG_F_STEREO_16 : DIG_F_MONO_16;
	setErr("");
	unlock();
	return reinterpret_cast<HDIGDRIVER>(g_theDig);
}

DXDEC void AILCALL AIL_set_3D_rolloff_factor(HDIGDRIVER dig, F32 factor)
{
	(void)dig;
	g_rolloff = factor;
}

DXDEC MSSVECTOR3D FAR *AILCALL AIL_speaker_configuration(HDIGDRIVER dig, S32 FAR *n_phys, S32 FAR *n_log, F32 FAR *falloff, MSS_MC_SPEC FAR *spec)
{
	(void)dig;
	if (n_phys)
		*n_phys = 2;
	if (n_log)
		*n_log = 2;
	if (falloff)
		*falloff = 1.f;
	if (spec)
		*spec = MSS_MC_STEREO;
	return nullptr;
}

DXDEC U32 AILCALL AIL_get_timer_highest_delay(void)
{
	return 0;
}

DXDEC void AILCALL AIL_serve(void)
{
	pollStopped();
	if (g_context)
		alcProcessContext(g_context);
}

DXDEC void AILCALL AIL_lock(void)
{
	lock();
}

DXDEC void AILCALL AIL_unlock(void)
{
	unlock();
}

static S32 sniffFileType(U8 const *p, U32 size)
{
	if (size >= 12 && memcmp(p, "RIFF", 4) == 0 && memcmp(p + 8, "WAVE", 4) == 0)
		return AILFILETYPE_PCM_WAV;
	return AILFILETYPE_UNKNOWN;
}

DXDEC S32 AILCALL AIL_file_type(void const FAR *data, U32 size)
{
	if (!data || size == 0)
		return AILFILETYPE_UNKNOWN;
	return sniffFileType(static_cast<U8 const *>(data), size);
}

DXDEC S32 AILCALL AIL_WAV_info(void const FAR *data, AILSOUNDINFO FAR *info)
{
	if (!data || !info)
		return 0;
	memset(info, 0, sizeof(*info));
	U8 const *p = static_cast<U8 const *>(data);
	if (memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
		return 0;
	U32 riffChunk = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
	U32 effSize = 8 + riffChunk;
	U32 off = 12;
	while (off + 8 <= effSize)
	{
		U32 chunkId = p[off] | (p[off + 1] << 8) | (p[off + 2] << 16) | (p[off + 3] << 24);
		U32 chunkSize = p[off + 4] | (p[off + 5] << 8) | (p[off + 6] << 16) | (p[off + 7] << 24);
		off += 8;
		if (off + chunkSize > effSize)
			break;
		if (chunkId == 0x20746d66 && chunkSize >= 16)
		{
			U16 af = static_cast<U16>(p[off] | (p[off + 1] << 8));
			U16 ch = static_cast<U16>(p[off + 2] | (p[off + 3] << 8));
			U32 rate = p[off + 4] | (p[off + 5] << 8) | (p[off + 6] << 16) | (p[off + 7] << 24);
			U16 bits = static_cast<U16>(p[off + 14] | (p[off + 15] << 8));
			if (af == WAVE_FORMAT_PCM && bits == 16)
				info->format = (ch > 1) ? DIG_F_STEREO_16 : DIG_F_MONO_16;
			else if (bits == 8)
				info->format = (ch > 1) ? DIG_F_STEREO_8 : DIG_F_MONO_8;
			else
				info->format = DIG_F_MONO_16;
			info->rate = rate;
			info->bits = bits;
			info->channels = ch;
			return 1;
		}
		off += chunkSize + (chunkSize & 1);
	}
	return 0;
}

DXDEC S32 AILCALL AIL_file_error(void)
{
	return g_file_err;
}

DXDEC S32 AILCALL AIL_active_sample_count(HDIGDRIVER dig)
{
	(void)dig;
	lock();
	S32 n = 0;
	ensureContext();
	for (OalVoice *v : g_allVoices)
	{
		if (v && v->source && v->status == SMP_PLAYING)
		{
			ALint st = AL_STOPPED;
			alGetSourcei(v->source, AL_SOURCE_STATE, &st);
			if (st == AL_PLAYING)
				n++;
		}
	}
	unlock();
	return n;
}

DXDEC HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER dig)
{
	(void)dig;
	lock();
	if (!g_started)
	{
		unlock();
		return nullptr;
	}
	ensureContext();
	OalVoice *v = new OalVoice;
	alGenSources(1, &v->source);
	g_allVoices.push_back(v);
	unlock();
	return reinterpret_cast<HSAMPLE>(v);
}

DXDEC void AILCALL AIL_release_sample_handle(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	lock();
	ensureContext();
	destroyVoice(v);
	auto it = std::find(g_allVoices.begin(), g_allVoices.end(), v);
	if (it != g_allVoices.end())
		g_allVoices.erase(it);
	delete v;
	unlock();
}

DXDEC S32 AILCALL AIL_set_sample_file(HSAMPLE s, void const FAR *file_image, S32 block)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !file_image || !v->source)
		return 0;
	lock();
	ensureContext();
	if (v->buffer)
	{
		alSourcei(v->source, AL_BUFFER, 0);
		alDeleteBuffers(1, &v->buffer);
		v->buffer = 0;
	}
	v->pcm.clear();
	v->loaded = false;
	char e[128];
	U32 sz = 0x7FFFFFFFU;
	if (block > 0)
		sz = static_cast<U32>(block);
	bool ok = decodeWavMem(file_image, sz, v, e, sizeof e);
	if (!ok)
	{
		setErr(e);
		g_file_err = AIL_CANT_READ_FILE;
		unlock();
		return 0;
	}
	setErr("");
	unlock();
	return 1;
}

DXDEC S32 AILCALL AIL_set_named_sample_file(HSAMPLE s, char const FAR *ext, void const FAR *mem, S32 size, S32 block)
{
	(void)ext;
	(void)block;
	OalVoice *v = voiceFrom(s);
	if (!v || !mem || size <= 0)
		return 0;
	lock();
	ensureContext();
	char e[128];
	destroyVoice(v);
	v->source = 0;
	alGenSources(1, &v->source);
	bool ok = decodeWavMem(mem, static_cast<U32>(size), v, e, sizeof e);
	if (!ok)
	{
		setErr(e);
		g_file_err = AIL_CANT_READ_FILE;
		unlock();
		return 0;
	}
	setErr("");
	unlock();
	return 1;
}

DXDEC void AILCALL AIL_set_sample_loop_block(HSAMPLE s, S32 start, S32 end)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->loopStartFrame = start;
	v->loopEndFrame = end;
}

DXDEC void AILCALL AIL_set_sample_loop_count(HSAMPLE s, S32 count)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->loopCount = count;
}

DXDEC AILSAMPLECB AILCALL AIL_register_EOS_callback(HSAMPLE s, AILSAMPLECB cb)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return nullptr;
	AILSAMPLECB prev = v->eosCb;
	v->eosCb = cb;
	return prev;
}

DXDEC void AILCALL AIL_start_sample(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->loaded)
		return;
	lock();
	ensureContext();
	v->eosFired = false;
	bool infinite = (v->loopCount == 0);
	v->loopsRemaining = infinite ? 0 : ((v->loopCount > 1) ? v->loopCount : 1);
	rebuildAndQueue(v, infinite);
	alSourcePlay(v->source);
	v->status = SMP_PLAYING;
	unlock();
}

DXDEC void AILCALL AIL_stop_sample(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source)
		return;
	lock();
	ensureContext();
	alSourceStop(v->source);
	v->status = SMP_STOPPED;
	unlock();
}

DXDEC void AILCALL AIL_end_sample(HSAMPLE s)
{
	AIL_stop_sample(s);
	OalVoice *v = voiceFrom(s);
	if (v)
		v->status = SMP_DONE;
}

DXDEC U32 AILCALL AIL_sample_status(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return SMP_DONE;
	lock();
	ensureContext();
	if (v->status == SMP_PLAYING && v->source)
	{
		ALint st = AL_STOPPED;
		alGetSourcei(v->source, AL_SOURCE_STATE, &st);
		if (st != AL_PLAYING)
			v->status = SMP_DONE;
	}
	U32 r = v->status;
	unlock();
	return r;
}

DXDEC void AILCALL AIL_set_sample_volume_levels(HSAMPLE s, F32 l, F32 r)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->volL = l;
	v->volR = r;
	lock();
	ensureContext();
	applyVolume(v);
	unlock();
}

DXDEC void AILCALL AIL_sample_volume_levels(HSAMPLE s, F32 FAR *l, F32 FAR *r)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	if (l)
		*l = v->volL;
	if (r)
		*r = v->volR;
}

DXDEC void AILCALL AIL_set_sample_playback_rate(HSAMPLE s, S32 rate)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->playbackRate = rate;
	lock();
	ensureContext();
	if (v->source)
	{
		F32 pr = static_cast<F32>(v->playbackRate) / static_cast<F32>(std::max(1, v->rate));
		alSourcef(v->source, AL_PITCH, pr);
	}
	unlock();
}

DXDEC S32 AILCALL AIL_sample_playback_rate(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	return v ? v->playbackRate : 0;
}

DXDEC void AILCALL AIL_set_sample_3D_position(HSAMPLE s, F32 x, F32 y, F32 z)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source)
		return;
	v->is3d = true;
	lock();
	ensureContext();
	alSource3f(v->source, AL_POSITION, x, y, z);
	unlock();
}

DXDEC void AILCALL AIL_set_sample_3D_velocity_vector(HSAMPLE s, F32 x, F32 y, F32 z)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source)
		return;
	lock();
	ensureContext();
	alSource3f(v->source, AL_VELOCITY, x, y, z);
	unlock();
}

DXDEC void AILCALL AIL_set_sample_3D_distances(HSAMPLE s, F32 max_dist, F32 min_dist, S32 auto_rolloff)
{
	(void)auto_rolloff;
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->maxDist = max_dist;
	v->refDist = min_dist;
	lock();
	ensureContext();
	if (v->source)
	{
		alSourcef(v->source, AL_REFERENCE_DISTANCE, v->refDist);
		alSourcef(v->source, AL_MAX_DISTANCE, v->maxDist);
	}
	unlock();
}

DXDEC void AILCALL AIL_set_sample_occlusion(HSAMPLE s, F32 o)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->occ = o;
	lock();
	ensureContext();
	apply3d(v);
	unlock();
}

DXDEC void AILCALL AIL_set_sample_obstruction(HSAMPLE s, F32 o)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->obs = o;
	lock();
	ensureContext();
	apply3d(v);
	unlock();
}

DXDEC void AILCALL AIL_set_sample_reverb_levels(HSAMPLE s, F32 dry, F32 wet)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	v->dryRv = dry;
	v->wetRv = wet;
}

DXDEC void AILCALL AIL_sample_reverb_levels(HSAMPLE s, F32 FAR *dry, F32 FAR *wet)
{
	OalVoice *v = voiceFrom(s);
	if (!v)
		return;
	if (dry)
		*dry = v->dryRv;
	if (wet)
		*wet = v->wetRv;
}

DXDEC void AILCALL AIL_set_sample_ms_position(HSAMPLE s, S32 ms)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source || v->rate <= 0)
		return;
	lock();
	ensureContext();
	float sec = static_cast<float>(ms) / 1000.f;
	alSourcef(v->source, AL_SEC_OFFSET, sec);
	unlock();
}

DXDEC void AILCALL AIL_sample_ms_position(HSAMPLE s, S32 FAR *total_ms, S32 FAR *cur_ms)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source)
		return;
	lock();
	ensureContext();
	ALfloat off = 0;
	alGetSourcef(v->source, AL_SEC_OFFSET, &off);
	if (cur_ms)
		*cur_ms = static_cast<S32>(off * 1000.f);
	if (total_ms && v->rate > 0)
	{
		int frames = static_cast<int>(v->pcm.size() / std::max(1, v->channels));
		*total_ms = static_cast<S32>((1000.0 * static_cast<double>(frames)) / static_cast<double>(v->rate));
	}
	unlock();
}

DXDEC U32 AILCALL AIL_sample_position(HSAMPLE s)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source || v->rate <= 0)
		return 0;
	lock();
	ensureContext();
	ALfloat sec = 0;
	alGetSourcef(v->source, AL_SEC_OFFSET, &sec);
	U32 sample = static_cast<U32>(sec * static_cast<float>(v->rate));
	unlock();
	return sample;
}

DXDEC void AILCALL AIL_set_sample_position(HSAMPLE s, U32 pos)
{
	OalVoice *v = voiceFrom(s);
	if (!v || !v->source || v->rate <= 0)
		return;
	lock();
	ensureContext();
	float sec = static_cast<float>(pos) / static_cast<float>(v->rate);
	alSourcef(v->source, AL_SEC_OFFSET, sec);
	unlock();
}

static bool readFileAll(char const *path, std::vector<U8> &out)
{
	g_file_err = AIL_NO_ERROR;
	if (!g_fopen || !g_fclose || !g_fseek || !g_fread)
		return false;
	UINTa h = 0;
	if (!g_fopen(path, &h) || h == 0)
	{
		g_file_err = AIL_FILE_NOT_FOUND;
		return false;
	}
	S32 end = g_fseek(h, 0, AIL_FILE_SEEK_END);
	if (end <= 0)
	{
		g_fclose(h);
		g_file_err = AIL_IO_ERROR;
		return false;
	}
	g_fseek(h, 0, AIL_FILE_SEEK_BEGIN);
	out.resize(static_cast<size_t>(end));
	U32 rd = g_fread(h, out.data(), static_cast<U32>(end));
	g_fclose(h);
	if (rd != static_cast<U32>(end))
	{
		g_file_err = AIL_CANT_READ_FILE;
		return false;
	}
	return true;
}

DXDEC HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, char const FAR *path, S32 mem)
{
	(void)dig;
	(void)mem;
	if (!path || !g_started)
		return nullptr;
	std::vector<U8> raw;
	if (!readFileAll(path, raw))
		return nullptr;
	lock();
	ensureContext();
	OalStream *st = new OalStream{};
	st->voice = new OalVoice;
	alGenSources(1, &st->voice->source);
	st->voice->streamOwner = st;
	g_allVoices.push_back(st->voice);
	char e[128];
	if (!decodeWavMem(raw.data(), static_cast<U32>(raw.size()), st->voice, e, sizeof e))
	{
		destroyVoice(st->voice);
		auto it = std::find(g_allVoices.begin(), g_allVoices.end(), st->voice);
		if (it != g_allVoices.end())
			g_allVoices.erase(it);
		delete st->voice;
		delete st;
		setErr(e);
		unlock();
		return nullptr;
	}
	unlock();
	return reinterpret_cast<HSTREAM>(st);
}

DXDEC void AILCALL AIL_close_stream(HSTREAM stream)
{
	OalStream *st = reinterpret_cast<OalStream *>(stream);
	if (!st)
		return;
	lock();
	if (st->voice)
	{
		destroyVoice(st->voice);
		auto it = std::find(g_allVoices.begin(), g_allVoices.end(), st->voice);
		if (it != g_allVoices.end())
			g_allVoices.erase(it);
		delete st->voice;
	}
	delete st;
	unlock();
}

DXDEC HSAMPLE AILCALL AIL_stream_sample_handle(HSTREAM stream)
{
	OalStream *st = reinterpret_cast<OalStream *>(stream);
	if (!st || !st->voice)
		return nullptr;
	return reinterpret_cast<HSAMPLE>(st->voice);
}

DXDEC void AILCALL AIL_set_stream_loop_block(HSTREAM st, S32 a, S32 b)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (str && str->voice)
		AIL_set_sample_loop_block(reinterpret_cast<HSAMPLE>(str->voice), a, b);
}

DXDEC void AILCALL AIL_set_stream_loop_count(HSTREAM st, S32 c)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (str && str->voice)
		AIL_set_sample_loop_count(reinterpret_cast<HSAMPLE>(str->voice), c);
}

DXDEC AILSTREAMCB AILCALL AIL_register_stream_callback(HSTREAM st, AILSTREAMCB cb)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (!str || !str->voice)
		return nullptr;
	AILSTREAMCB p = str->voice->streamCb;
	str->voice->streamCb = cb;
	return p;
}

DXDEC void AILCALL AIL_start_stream(HSTREAM st)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (str && str->voice)
		AIL_start_sample(reinterpret_cast<HSAMPLE>(str->voice));
}

DXDEC S32 AILCALL AIL_stream_status(HSTREAM st)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (!str || !str->voice)
		return SMP_DONE;
	return static_cast<S32>(AIL_sample_status(reinterpret_cast<HSAMPLE>(str->voice)));
}

DXDEC void AILCALL AIL_set_stream_ms_position(HSTREAM st, S32 ms)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (str && str->voice)
		AIL_set_sample_ms_position(reinterpret_cast<HSAMPLE>(str->voice), ms);
}

DXDEC void AILCALL AIL_stream_ms_position(HSTREAM st, S32 FAR *tot, S32 FAR *cur)
{
	OalStream *str = reinterpret_cast<OalStream *>(st);
	if (str && str->voice)
		AIL_sample_ms_position(reinterpret_cast<HSAMPLE>(str->voice), tot, cur);
}

DXDEC void AILCALL AIL_set_listener_3D_position(HDIGDRIVER d, F32 x, F32 y, F32 z)
{
	(void)d;
	lock();
	if (ensureContext())
		alListener3f(AL_POSITION, x, y, z);
	unlock();
}

DXDEC void AILCALL AIL_set_listener_3D_velocity_vector(HDIGDRIVER d, F32 x, F32 y, F32 z)
{
	(void)d;
	lock();
	if (ensureContext())
		alListener3f(AL_VELOCITY, x, y, z);
	unlock();
}

DXDEC void AILCALL AIL_set_listener_3D_orientation(HDIGDRIVER d, F32 fx, F32 fy, F32 fz, F32 ux, F32 uy, F32 uz)
{
	(void)d;
	lock();
	if (ensureContext())
	{
		ALfloat ori[6] = { fx, fy, fz, ux, uy, uz };
		alListenerfv(AL_ORIENTATION, ori);
	}
	unlock();
}

DXDEC S32 AILCALL AIL_digital_CPU_percent(HDIGDRIVER d)
{
	(void)d;
	return 0;
}

DXDEC S32 AILCALL AIL_digital_latency(HDIGDRIVER d)
{
	(void)d;
	return 0;
}

DXDEC void AILCALL AIL_digital_configuration(HDIGDRIVER d, S32 FAR *rate, S32 FAR *format, S32 FAR *channels)
{
	(void)d;
	if (rate)
		*rate = g_digRate;
	if (format)
		*format = g_digFormat;
	if (channels)
		*channels = (g_digFormat & DIG_F_STEREO_MASK) ? 2 : 1;
}

DXDEC void AILCALL AIL_set_room_type(HDIGDRIVER d, S32 env)
{
	(void)d;
	g_roomType = env;
}

DXDEC S32 AILCALL AIL_room_type(HDIGDRIVER d)
{
	(void)d;
	return g_roomType;
}

#endif // SWG_USE_OPENAL
