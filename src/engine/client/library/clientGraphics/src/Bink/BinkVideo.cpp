// ======================================================================
// BinkVideo.cpp — play cinematic files through libVLC (LGPL). Keep class
// name and entry points; RAD Bink is not used. Ship libvlc.dll, libvlccore.dll, plugins/ (VideoLAN 3.x).
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/BinkVideo.h"
#include "clientGraphics/VlcModule.h"

#include "sharedFile/TreeFile.h"
#include "fileInterface/AbstractFile.h"
#include "sharedMath/Vector.h"
#include "sharedMath/Transform.h"
#include "sharedSynchronization/RecursiveMutex.h"

#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/TextureList.h"
#include "clientGraphics/Texture.def"
#include "clientGraphics/Texture.h"
#include "clientGraphics/VertexBufferFormat.h"
#include "clientGraphics/VertexBufferIterator.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include <d3d9.h>
#include <windows.h>

// Bink copy flag values (kept for call sites; pixels are always RV32 from VLC).
static const unsigned BINKSURFACE32A  = 5u;
static const unsigned BINKSURFACE565  = 10u;
static const unsigned BINKSURFACE5551 = 8u;
static const unsigned BINKCOPYALL       = 0x80000000u;

// ======================================================================

struct VlcVideoContext
{
	libvlc_media_player_t *  player;
	libvlc_media_t *         media;
	std::string              tempPath;
	RecursiveMutex            frameLock;
	std::vector<uint8_t>      pixels; // BGRA / RV32
	int                       width;
	int                       height;
	int                       pitch;
	uint64_t                  displayCount;
	ULONGLONG                 lastFrameTick;
	bool                      ended;
	bool                      hasFormat;
};

// ----- libvlc vout callbacks -------------------------------------------------

static unsigned vlc_format_setup (void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines)
{
	if (!opaque || !*opaque || !chroma || !width || !height || !pitches || !lines)
		return 0u;
	VlcVideoContext *c = reinterpret_cast<VlcVideoContext *>(*opaque);
	if (!c)
		return 0u;
	if (!*width || !*height)
		return 0u;
	memcpy(chroma, "RV32", 4u);
	c->width  = (int)(*width);
	c->height = (int)(*height);
	c->pitch  = (int)(*width * 4u);
	*pitches  = (unsigned)c->pitch;
	*lines    = *height;
	c->pixels.assign((size_t)(c->pitch * c->height), 0);
	c->hasFormat = true;
	return 1u;
}

static void vlc_format_cleanup (void *opaque)
{
	VlcVideoContext *c = reinterpret_cast<VlcVideoContext *>(opaque);
	(void)c;
}

static void *vlc_lock (void *opaque, void **planes)
{
	VlcVideoContext *c = reinterpret_cast<VlcVideoContext *>(opaque);
	if (!c || !c->hasFormat)
	{
		*planes = 0;
		return 0;
	}
	c->frameLock.enter();
	*planes = c->pixels.empty() ? 0 : c->pixels.data();
	return 0;
}

static void vlc_unlock (void *opaque, void *picture, void *const *planes)
{
	VlcVideoContext *c = reinterpret_cast<VlcVideoContext *>(opaque);
	(void)picture; (void)planes;
	if (c)
		c->frameLock.leave();
}

static void vlc_display (void *opaque, void *picture)
{
	(void)picture;
	VlcVideoContext *c = reinterpret_cast<VlcVideoContext *>(opaque);
	if (c)
	{
		++c->displayCount;
		c->lastFrameTick = GetTickCount64();
	}
}

// ======================================================================

namespace BinkVideoNamespace
{
	bool install(void *hMilesDigitalDriver);
	void remove();

	static void _onDeviceLost();
	static void _onDeviceRestored();

	bool                 s_installed         = false;
	libvlc_instance_t *  s_instance         = 0;
	void  *              s_miles            = 0;
	bool                 s_dynamicTextures = false;

	// Blit helper (unchanged)
	class TextureBlit
	{
	public:
		enum { MAX_WIDTH=1024, MAX_HEIGHT=1024 };
		static bool construct();
		static void destroy();
		static void doFrame(BinkVideo *video);
		static void draw(BinkVideo *video, int screenX, int screenY, int screenCX, int screenCY);
		static Texture *            s_dynamicTexture;
		static Shader *             s_videoBlitShader;
		static VertexBufferFormat   s_vertexFormat;
		static DynamicVertexBuffer *s_vertexBuffer;
		static GlMatrix4x4          s_projectionMatrix;
	};
	Texture *            TextureBlit::s_dynamicTexture;
	Shader *             TextureBlit::s_videoBlitShader;
	VertexBufferFormat   TextureBlit::s_vertexFormat;
	DynamicVertexBuffer *TextureBlit::s_vertexBuffer;
	GlMatrix4x4          TextureBlit::s_projectionMatrix;
}
using namespace BinkVideoNamespace;

// ----- export TreeFile to temp (VLC decodes from a real file path) -----------

static bool treeExportToTemp (const char *name, std::string &outPath, std::string *errOut)
{
	AbstractFile *f = TreeFile::open(name, AbstractFile::PriorityAudioVideo, true);
	if (!f)
	{
		if (errOut) *errOut = "open";
		return false;
	}
	const int len = f->length();
	if (len <= 0)
	{
		f->close();
		delete f;
		if (errOut) *errOut = "length";
		return false;
	}
	unsigned char *data = f->readEntireFileAndClose();
	if (!data)
	{
		if (errOut) *errOut = "read";
		return false;
	}

	char base[MAX_PATH+8];
	if (!GetTempPathA((DWORD)sizeof(base), base))
	{
		delete [] data;
		if (errOut) *errOut = "temp";
		return false;
	}
	char temp[MAX_PATH+8];
	if (!GetTempFileNameA(base, "swg", 0, temp))
	{
		delete [] data;
		if (errOut) *errOut = "tmpname";
		return false;
	}
	// try to preserve an extension for demuxer heuristics
	std::string ext;
	const char *dot = strrchr(name, '.');
	if (dot) ext = dot; else ext = ".bin";
	std::string finalPath = std::string(temp) + ext;
	if (!MoveFileA(temp, finalPath.c_str()))
		finalPath = temp;

	FILE *w = 0;
	if (fopen_s(&w, finalPath.c_str(), "wb") != 0 || !w)
	{
		delete [] data;
		if (errOut) *errOut = "fopen_w";
		return false;
	}
	const size_t wr = fwrite(data, 1, (size_t)len, w);
	fclose(w);
	delete [] data;
	if (wr != (size_t)len)
	{
		DeleteFileA(finalPath.c_str());
		if (errOut) *errOut = "fwrite";
		return false;
	}
	outPath = finalPath;
	return true;
}

// ======================================================================
bool BinkVideoNamespace::install (void *hMilesDigitalDriver)
{
	if (s_installed)
	{
		WARNING(true, ("Nested video installs are not supported.\n"));
		return false;
	}
	if (!vlcLoad(0))
	{
		WARNING(true, ("libVLC (libvlc.dll) failed to load. Place VideoLAN 3.x 64-bit DLLs and plugins/ next to the game executable (same folder as SwgTitan)."));
		return false;
	}
	char modDir[MAX_PATH];
	if (!vlcGetModuleDirectory(modDir, sizeof(modDir)))
		return false;
	static std::string s_pluginArg;
	s_pluginArg = std::string("--plugin-path=") + modDir + "\\plugins";
	const char *argv[] = { "titan", "--quiet", "--no-video-title-show", s_pluginArg.c_str() };
	s_instance = g_vlc.f_libvlc_new(4, argv);
	if (!s_instance)
	{
		const char *e = g_vlc.f_libvlc_errmsg ? g_vlc.f_libvlc_errmsg() : "";
		WARNING(true, ("libvlc_new failed: %s", e));
		vlcUnload();
		return false;
	}
	s_miles = hMilesDigitalDriver;
	Graphics::addDeviceLostCallback(_onDeviceLost);
	Graphics::addDeviceRestoredCallback(_onDeviceRestored);
	_onDeviceRestored();
	s_installed = true;
	return true;
}

void BinkVideoNamespace::remove()
{
	if (!s_installed)
	{
		WARNING(true, ("remove() without install.\n"));
		return;
	}
	_onDeviceLost();
	Graphics::removeDeviceLostCallback(_onDeviceLost);
	Graphics::removeDeviceRestoredCallback(_onDeviceRestored);
	if (s_dynamicTextures)
	{
		TextureBlit::destroy();
		s_dynamicTextures = false;
	}
	if (s_instance)
	{
		g_vlc.f_libvlc_release(s_instance);
		s_instance = 0;
	}
	s_miles = 0;
	vlcUnload();
	s_installed = false;
}

void BinkVideoNamespace::_onDeviceLost()
{
	if (s_dynamicTextures)
	{
		TextureBlit::destroy();
		s_dynamicTextures = false;
	}
}

void BinkVideoNamespace::_onDeviceRestored()
{
	s_dynamicTextures = TextureBlit::construct();
}

// ----- Texture blit (same as legacy) ----------------------------------------

bool BinkVideoNamespace::TextureBlit::construct()
{
	if (!Graphics::supportsDynamicTextures())
		return false;
	TextureFormat formats[] = { TF_ARGB_8888 };
	s_dynamicTexture  = TextureList::fetch(TCF_dynamic, MAX_WIDTH, MAX_HEIGHT, 1, formats, 1);
	if (!s_dynamicTexture) return false;
	s_videoBlitShader = ShaderTemplateList::fetchModifiableShader("shader/video_blit.sht");
	if (!s_videoBlitShader)
	{
		s_dynamicTexture->release();
		s_dynamicTexture = 0;
		return false;
	}
	safe_cast<StaticShader *>(s_videoBlitShader)->setTexture(TAG(M,A,I,N), *s_dynamicTexture);
	s_vertexFormat.setPosition();
	s_vertexFormat.setColor0(true);
	s_vertexFormat.setNumberOfTextureCoordinateSets(1);
	s_vertexFormat.setTextureCoordinateSetDimension(0, 2);
	s_vertexBuffer = new DynamicVertexBuffer(s_vertexFormat);
	s_projectionMatrix.matrix[0][0] = 0.f; s_projectionMatrix.matrix[0][1] = 0.f; s_projectionMatrix.matrix[0][2] = 0.f; s_projectionMatrix.matrix[0][3] = 0.f;
	s_projectionMatrix.matrix[1][0] = 0.f; s_projectionMatrix.matrix[1][1] = 0.f; s_projectionMatrix.matrix[1][2] = 0.f; s_projectionMatrix.matrix[1][3] = 0.f;
	s_projectionMatrix.matrix[2][0] = 0.f; s_projectionMatrix.matrix[2][1] = 0.f; s_projectionMatrix.matrix[2][2] = 1.f; s_projectionMatrix.matrix[2][3] = 0.f;
	s_projectionMatrix.matrix[3][0] = 0.f; s_projectionMatrix.matrix[3][1] = 0.f; s_projectionMatrix.matrix[3][2] = 0.f; s_projectionMatrix.matrix[3][3] = 1.f;
	return true;
}

void BinkVideoNamespace::TextureBlit::destroy()
{
	if (s_vertexBuffer) { delete s_vertexBuffer; s_vertexBuffer=0; }
	if (s_videoBlitShader) { s_videoBlitShader->release(); s_videoBlitShader=0; }
	if (s_dynamicTexture)  { s_dynamicTexture->release();  s_dynamicTexture=0; }
}

void BinkVideoNamespace::TextureBlit::doFrame(BinkVideo *video)
{
	const TextureFormat format = s_dynamicTexture->getNativeFormat();
	Texture::LockData ldata(format,0,0,0,s_dynamicTexture->getWidth(),s_dynamicTexture->getHeight(),true);
	s_dynamicTexture->lock(ldata);
	if (ldata.getPixelData())
	{
		uint8 *dest = (uint8 *)ldata.getPixelData();
		video->copyToBuffer(dest, ldata.getPitch(), ldata.getHeight(),0,0, BINKSURFACE32A|BINKCOPYALL);
	}
	s_dynamicTexture->unlock(ldata);
}

void BinkVideoNamespace::TextureBlit::draw (BinkVideo *video, int screenX, int screenY, int screenCX, int screenCY)
{
	int width  = Graphics::getCurrentRenderTargetWidth();
	int height = Graphics::getCurrentRenderTargetHeight();
	Graphics::setViewport(0,0,width,height);
	const GlCullMode preCull = Graphics::getCullMode();
	Graphics::setCullMode(GCM_none);
	const GlFillMode preFill = Graphics::getFillMode();
	Graphics::setFillMode(GFM_solid);
	Graphics::setScissorRect(false,0,0,0,0);
	Graphics::setObjectToWorldTransformAndScale(Transform::identity, Vector::xyz111);
	Graphics::setWorldToCameraTransform(Transform::identity, Vector::zero);
	const float oodx = 1.0f/float(width);
	s_projectionMatrix.matrix[0][0] =  2.f * oodx;
	s_projectionMatrix.matrix[0][3]   = -2.f * float(0) * oodx - 1.f;
	const float oody = 1.0f/float(height);
	s_projectionMatrix.matrix[1][1] = -2.f * oody;
	s_projectionMatrix.matrix[1][3]  =  2.f * float(0) * oody + 1.f;
	Graphics::setProjectionMatrix(s_projectionMatrix);
	const float videoX0   = 0.5f/float(MAX_WIDTH);
	const float videoY0   = 0.5f/float(MAX_HEIGHT);
	const float vW  = float(video->getWidth())/float(MAX_WIDTH);
	const float vH  = float(video->getHeight())/float(MAX_HEIGHT);
	s_vertexBuffer->lock(4);
	{
		VertexBufferWriteIterator vb = s_vertexBuffer->begin();
		vb.setPosition((float)screenX,(float)screenY,1);   vb.setColor0(0xffffffff); vb.setTextureCoordinates(0,videoX0,videoY0);   ++vb;
		vb.setPosition((float)screenX,(float)(screenY+screenCY),1); vb.setColor0(0xffffffff); vb.setTextureCoordinates(0,videoX0,vH);  ++vb;
		vb.setPosition((float)(screenX+screenCX),(float)(screenY+screenCY),1); vb.setColor0(0xffffffff); vb.setTextureCoordinates(0,vW,vH);  ++vb;
		vb.setPosition((float)(screenX+screenCX),(float)screenY,1);  vb.setColor0(0xffffffff); vb.setTextureCoordinates(0,vW,videoY0);  ++vb;
	}
	s_vertexBuffer->unlock();
	Graphics::setVertexBuffer(*s_vertexBuffer);
	Graphics::setStaticShader(s_videoBlitShader->prepareToView(),0);
	Graphics::drawTriangleFan(0,2);
	Graphics::setCullMode(preCull);
	Graphics::setFillMode(preFill);
}

// ----- BinkVideo --------------------------------------------------------------

BinkVideo *BinkVideo::newBinkVideo (const char *name)
{
	if (!BinkVideoNamespace::s_instance)
	{
		WARNING(true, ("BinkVideo: VL install missing — call BinkVideoNamespace::install first.\n"));
		return 0;
	}
	std::string filePath;
	if (!treeExportToTemp(name, filePath, 0))
		return 0;
	libvlc_media_t *md = g_vlc.f_libvlc_media_new_path(BinkVideoNamespace::s_instance, filePath.c_str());
	if (!md)
	{
		DeleteFileA(filePath.c_str());
		return 0;
	}
	g_vlc.f_libvlc_media_add_option(md, ":file-caching=300");
	libvlc_media_player_t *pl = g_vlc.f_libvlc_media_player_new(BinkVideoNamespace::s_instance);
	if (!pl)
	{
		g_vlc.f_libvlc_media_release(md);
		DeleteFileA(filePath.c_str());
		return 0;
	}
	g_vlc.f_libvlc_media_player_set_media(pl, md);
	VlcVideoContext *ctx    = new VlcVideoContext();
	ctx->player              = pl;
	ctx->media               = md;
	ctx->tempPath            = filePath;
	ctx->width=ctx->height=ctx->pitch=0;
	ctx->displayCount=0; ctx->lastFrameTick=0; ctx->ended=false; ctx->hasFormat=false;
	g_vlc.f_libvlc_video_set_callbacks(pl, vlc_lock, vlc_unlock, vlc_display, ctx);
	g_vlc.f_libvlc_video_set_format_callbacks(pl, vlc_format_setup, vlc_format_cleanup);
	if (BinkVideoNamespace::s_miles && g_vlc.f_libvlc_audio_set_mute)
		g_vlc.f_libvlc_audio_set_mute(pl, 1);
	if (g_vlc.f_libvlc_media_player_play(pl) != 0)
	{
		g_vlc.f_libvlc_media_player_release(pl);
		g_vlc.f_libvlc_media_release(md);
		if (!ctx->tempPath.empty()) DeleteFileA(ctx->tempPath.c_str());
		delete ctx;
		return 0;
	}
	return new BinkVideo(name, ctx);
}

BinkVideo::BinkVideo (const char *name, VlcVideoContext *ctx) :
	Video(name),
	m_ctx(ctx),
	m_loopCount(0),
	m_didFrame(false),
	m_nextFrame(false)
{
}

BinkVideo::~BinkVideo()
{
	if (m_ctx)
	{
		if (m_ctx->player) { g_vlc.f_libvlc_media_player_stop(m_ctx->player); g_vlc.f_libvlc_media_player_release(m_ctx->player); }
		if (m_ctx->media)  { g_vlc.f_libvlc_media_release(m_ctx->media); }
		if (!m_ctx->tempPath.empty()) DeleteFileA(m_ctx->tempPath.c_str());
		delete m_ctx; m_ctx = 0;
	}
}

int  BinkVideo::getWidth()  const { return m_ctx && m_ctx->hasFormat ? m_ctx->width  : 0; }
int  BinkVideo::getHeight() const { return m_ctx && m_ctx->hasFormat ? m_ctx->height : 0; }
int  BinkVideo::getLoopCount() const { return m_loopCount; }
bool BinkVideo::canStretchBlt() const { return s_dynamicTextures; }

bool BinkVideo::pause (bool p)
{
	if (!m_ctx || !m_ctx->player) return false;
	if (p) g_vlc.f_libvlc_media_player_pause(m_ctx->player);
	else
	{
		(void)g_vlc.f_libvlc_media_player_play(m_ctx->player);
	}
	return p;
}
void  BinkVideo::goTo (unsigned frame, unsigned) { (void)frame; }
bool  BinkVideo::setVideoOnOff (bool) { return true; }
bool  BinkVideo::setSoundOnOff (bool) { return true; }
void  BinkVideo::setVolume (unsigned, int v)
{
	if (!m_ctx || !m_ctx->player || !g_vlc.f_libvlc_audio_set_volume) return;
	int p = 0;
	if (v <= 0) p = 0; else if (v >= 32768) p = 100; else p = (v * 100) / 32768;
	if (g_vlc.f_libvlc_audio_set_mute) g_vlc.f_libvlc_audio_set_mute(m_ctx->player, p==0?1:0);
	g_vlc.f_libvlc_audio_set_volume(m_ctx->player, p);
}
void  BinkVideo::setPan (unsigned, int) {}
unsigned BinkVideo::getKeyFrame (unsigned, unsigned) { return 0; }

bool BinkVideo::doFrame() { return false; }
void BinkVideo::nextFrame() {}
bool BinkVideo::shouldSkip() { return false; }
bool BinkVideo::wait()
{
	if (!m_ctx) return true;
	ULONGLONG t = GetTickCount64();
	if (m_ctx->lastFrameTick == 0) return true;
	if (t - m_ctx->lastFrameTick < 15u) return true; // ~60 Hz max pace
	return false;
}

void BinkVideo::service()
{
	if (!m_ctx || !m_ctx->player) return;
	if (!g_vlc.f_libvlc_media_player_get_state) return;
	const libvlc_state_t st = g_vlc.f_libvlc_media_player_get_state(m_ctx->player);
	if (st == libvlc_Ended)
	{
		if (getLooping())
		{
			if (g_vlc.f_libvlc_media_player_set_time) g_vlc.f_libvlc_media_player_set_time(m_ctx->player, 0);
			g_vlc.f_libvlc_media_player_play(m_ctx->player);
			++m_loopCount;
		}
		else
		{
			if (!m_ctx->ended)
			{
				m_ctx->ended = true;
				m_loopCount  = 1;
			}
		}
	}
}

static uint16 r32t565 (uint32 c)
{
	const uint8 r = (uint8)(c >> 16);
	const uint8 g = (uint8)(c >> 8);
	const uint8 b = (uint8)(c);
	return (uint16)(((r>>3)<<11) | ((g>>2)<<5) | (b>>3));
}
static uint16 r32t5551 (uint32 c)
{
	const uint8 r = (uint8)(c >> 16);
	const uint8 g = (uint8)(c >> 8);
	const uint8 b = (uint8)(c);
	return (uint16)(1u | (uint16((r>>3)&31)<<11) | (uint16((g>>3)&31)<<6) | (uint16((b>>3)&31)<<1));
}

bool BinkVideo::copyToBuffer (void *d, int destYStride, unsigned destH, unsigned destX, unsigned destY, unsigned flg)
{
	(void)destH; (void)destX; (void)destY;
	if (!m_ctx || !d || !m_ctx->hasFormat) return false;
	m_ctx->frameLock.enter();
	if (m_ctx->pixels.empty() || m_ctx->width <= 0 || m_ctx->height <= 0)
	{
		m_ctx->frameLock.leave();
		return true;
	}
	const unsigned srf = (flg & 15u);
	const int w  = m_ctx->width;
	const int h  = m_ctx->height;
	const int sp = m_ctx->pitch;
	const uint8 *src = m_ctx->pixels.data();
	if (srf == (BINKSURFACE32A & 15u) || srf == 3u || srf == 5u)
	{
		for (int y=0;y<h;++y)
		{
			memcpy((uint8*)d + (size_t)y * (size_t)destYStride, src + (size_t)y * (size_t)sp, (size_t)w*4u);
		}
	}
	else if (srf == (BINKSURFACE565 & 15u) || srf==10u)
	{
		for (int y=0;y<h;++y)
		{
			uint16 *row = (uint16 *)((uint8*)d + (size_t)y * (size_t)destYStride);
			const uint32 *sL = (const uint32 *)(src + (size_t)y * (size_t)sp);
			for (int x=0;x<w;++x) row[x] = r32t565(sL[x]);
		}
	}
	else if (srf == (BINKSURFACE5551 & 15u) || srf==8u)
	{
		for (int y=0;y<h;++y)
		{
			uint16 *row = (uint16 *)((uint8*)d + (size_t)y * (size_t)destYStride);
			const uint32 *sL = (const uint32 *)(src + (size_t)y * (size_t)sp);
			for (int x=0;x<w;++x) row[x] = r32t5551(sL[x]);
		}
	}
	else
	{
		for (int y=0;y<h;++y)
			memcpy((uint8*)d + (size_t)y * (size_t)destYStride, src + (size_t)y * (size_t)sp, (size_t)w*4u);
	}
	m_ctx->frameLock.leave();
	return false;
}

bool BinkVideo::copyToBufferRect (void *d, int destYStride, unsigned destH, unsigned destX, unsigned destY, unsigned srcX, unsigned srcY, unsigned srcW, unsigned srcH, unsigned binkCopyFlags)
{
	(void)srcX; (void)srcY; (void)srcW; (void)srcH;
	return copyToBuffer(d, destYStride, destH, destX, destY, binkCopyFlags);
}
int    BinkVideo::getRects (unsigned) { return 0; }
void   BinkVideo::getFrameBuffersInfo (void *) {}
void   BinkVideo::registerFrameBuffers (const void *) {}
void   BinkVideo::getSummary (void *) {}
void   BinkVideo::getRealtime (void *, unsigned) {}
bool   BinkVideo::controlBackgroundIO (unsigned) { return false; }
unsigned BinkVideo::getTrackID (unsigned) { return 0; }
unsigned BinkVideo::DX9SurfaceType (IDirect3DSurface9 *) { return 0; }

bool BinkVideo::_isFirstFrame() const
{
	if (!m_ctx) return true;
	return m_ctx->displayCount == 0;
}
bool BinkVideo::_isFinished() const
{
	if (!m_ctx) return true;
	if (!getLooping() && m_loopCount) return true;
	return m_ctx->ended;
}
bool BinkVideo::_isPlaying() const
{
	return !_isFirstFrame() && !_isFinished();
}

void BinkVideo::_doFrame()
{
	doFrame();
	m_didFrame  = true;
	m_nextFrame = true;
}

void BinkVideo::_nextFrame()
{
	if (m_nextFrame)
	{
		m_nextFrame = false;
	}
}

bool BinkVideo::performDrawing (int screenX, int screenY, int screenCX, int screenCY)
{
	if (!s_dynamicTextures || !m_ctx) return false;
	if (_isPlaying()) service();
	else if (_isFirstFrame() && !m_didFrame) { _doFrame(); _nextFrame(); }
	if (m_didFrame) { TextureBlit::doFrame(this); m_didFrame = false; }
	if (screenCX<0) screenCX = m_ctx? m_ctx->width:0;
	if (screenCY<0) screenCY = m_ctx? m_ctx->height:0;
	TextureBlit::draw(this, screenX, screenY, screenCX, screenCY);
	return true;
}

bool BinkVideo::performBlitting (int screenX, int screenY)
{
	if (s_dynamicTextures || !m_ctx) return false;
	if (_isPlaying()) service();
	else if (_isFirstFrame() && !m_didFrame) { _doFrame(); _nextFrame(); }
	Gl_rect lockRect; lockRect.x0 = screenX; lockRect.y0 = screenY;
	lockRect.x1 = screenX + (m_ctx?m_ctx->width:0);
	lockRect.y1 = screenY + (m_ctx?m_ctx->height:0);
	Gl_pixelRect pixels;
	if (Graphics::lockBackBuffer(pixels,0))
	{
		unsigned pfmt=0; int bpp=0;
		if (pixels.colorBits==24 && pixels.alphaBits==8) { pfmt = BINKSURFACE32A; bpp=4; }
		else if (pixels.colorBits==16 && pixels.alphaBits==0) { pfmt = BINKSURFACE565; bpp=2; }
		else if (pixels.colorBits==15 && pixels.alphaBits==1) { pfmt = BINKSURFACE5551; bpp=2; }
		if (bpp>0) {
			uint8 *dp = (uint8 *)(pixels.pixels) + (size_t)screenY * (size_t)pixels.pitch + (size_t)screenX * (size_t)bpp;
			copyToBuffer(dp, pixels.pitch, m_ctx?m_ctx->height:0,0,0, pfmt|BINKCOPYALL);
		}
		Graphics::unlockBackBuffer();
	}
	m_didFrame = false;
	return true;
}
