// ======================================================================
//
// SplashScreen.cpp
// copyright 2024 SWG Titan
//
// Displays a splash screen image during client startup
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/SplashScreen.h"

#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ShaderTemplate.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Os.h"
#include "sharedMath/VectorArgb.h"

#include <algorithm>

// ======================================================================

namespace SplashScreenNamespace
{
	bool ms_installed = false;
	bool ms_active = false;
	bool ms_dismissed = false;
	
	Texture const * ms_texture = nullptr;
	StaticShader * ms_shader = nullptr;
	bool ms_useColorFallback = false;
	/// Larger edge of splash content fits within this fraction of the matching viewport edge (0–1].
	float ms_splashMaxScreenFraction = 0.5f;

	char const * const cms_defaultSplashTexture = "texture/loading/large/large_load_watto.dds";
	// Same template path as CuiLayer_TextureCanvas (texture tag MAIN); 2d_texture uses different stage names.
	char const * const cms_defaultSplashShaderTemplate = "shader/uicanvas_filtered.sht";
	uint32 const cms_splashBackdropArgb = 0xff0c0e12;

	// Aspect used for solid-color splash when no texture (16:ish).
	int const cms_fallbackSplashWidth = 1024;
	int const cms_fallbackSplashHeight = 576;

	void computeCenteredSplashRect(int screenWidth, int screenHeight, int contentWidth, int contentHeight, float maxFraction, float &outX0, float &outY0, float &outX1, float &outY1);
	void renderTexturedSplashQuad(float x0, float y0, float x1, float y1);
	void renderColorFallbackQuad(float x0, float y0, float x1, float y1);
}

using namespace SplashScreenNamespace;

// ======================================================================

void SplashScreen::install()
{
	if (ms_installed)
		return;
		
	ms_installed = true;
	ms_active = true;
	ms_dismissed = false;
	ms_useColorFallback = false;

	ms_splashMaxScreenFraction = ConfigFile::getKeyFloat("ClientGame", "splashMaxScreenFraction", 0.5f);
	if (ms_splashMaxScreenFraction <= 0.f || ms_splashMaxScreenFraction > 1.f)
		ms_splashMaxScreenFraction = 0.5f;

	// Load splash texture
	char const * const splashTexture = ConfigFile::getKeyString("ClientGame", "splashTexture", cms_defaultSplashTexture);
	ms_texture = TextureList::fetch(splashTexture);
	if (!ms_texture)
		WARNING(true, ("SplashScreen: missing texture [%s]; using solid color until login.", splashTexture));

	char const * const shaderTemplateName = ConfigFile::getKeyString("ClientGame", "splashShader", cms_defaultSplashShaderTemplate);
	ShaderTemplate const * const shaderTemplate = ShaderTemplateList::fetch(shaderTemplateName);
	if (shaderTemplate)
	{
		ms_shader = dynamic_cast<StaticShader *>(shaderTemplate->fetchModifiableShader());
		shaderTemplate->release();
	}
	if (!ms_shader)
		WARNING(true, ("SplashScreen: could not load shader template [%s]; using solid color until login.", shaderTemplateName));
	else if (ms_texture)
		ms_shader->setTexture(TAG(M,A,I,N), *ms_texture);

	ms_useColorFallback = (!ms_texture || !ms_shader || !ms_shader->isValid());
	if (ms_useColorFallback && ms_shader && !ms_shader->isValid())
		WARNING(true, ("SplashScreen: shader not ready for draw after setup; using solid color."));

	ExitChain::add(SplashScreen::remove, "SplashScreen::remove");
}

// ----------------------------------------------------------------------

void SplashScreen::remove()
{
	if (!ms_installed)
		return;
		
	if (ms_texture)
	{
		ms_texture->release();
		ms_texture = nullptr;
	}
	
	if (ms_shader)
	{
		ms_shader->release();
		ms_shader = nullptr;
	}
	
	ms_installed = false;
	ms_active = false;
}

// ----------------------------------------------------------------------

void SplashScreen::pump()
{
	if (!ms_active || ms_dismissed)
		return;
	if (!Os::update())
		return;
	render();
}

// ----------------------------------------------------------------------

void SplashScreen::preloadConfiguredAssets()
{
	if (!ms_active || ms_dismissed)
		return;
	if (!ConfigFile::getKeyBool("ClientGame", "splashPreloadEnabled", true))
		return;

	for (int i = 0;; ++i)
	{
		char const * const path = ConfigFile::getKeyString("ClientGame", "splashPreload", i, nullptr);
		if (!path || !*path)
			break;
		Texture const * const t = TextureList::fetch(path);
		if (t)
			t->release();
		pump();
	}
}

// ----------------------------------------------------------------------

void SplashScreen::render()
{
	if (!ms_active || ms_dismissed)
		return;

	Graphics::beginScene();

	int const vw = Graphics::getCurrentRenderTargetWidth();
	int const vh = Graphics::getCurrentRenderTargetHeight();
	float sx0, sy0, sx1, sy1;

	if (ms_useColorFallback)
	{
		computeCenteredSplashRect(vw, vh, cms_fallbackSplashWidth, cms_fallbackSplashHeight, ms_splashMaxScreenFraction, sx0, sy0, sx1, sy1);
		Graphics::clearViewport(true, cms_splashBackdropArgb, true, 1.0f, true, 0);
		Graphics::setStaticShader(ShaderTemplateList::get2dVertexColorStaticShader());
		renderColorFallbackQuad(sx0, sy0, sx1, sy1);
	}
	else
	{
		computeCenteredSplashRect(vw, vh, ms_texture->getWidth(), ms_texture->getHeight(), ms_splashMaxScreenFraction, sx0, sy0, sx1, sy1);
		Graphics::clearViewport(true, cms_splashBackdropArgb, true, 1.0f, true, 0);
		Graphics::setStaticShader(*ms_shader);
		renderTexturedSplashQuad(sx0, sy0, sx1, sy1);
	}

	Graphics::endScene();
	static_cast<void>(Graphics::present());
}

// ----------------------------------------------------------------------

void SplashScreen::dismiss()
{
	ms_dismissed = true;
	ms_active = false;
}

// ----------------------------------------------------------------------

bool SplashScreen::isActive()
{
	return ms_active && !ms_dismissed;
}

// ----------------------------------------------------------------------

void SplashScreenNamespace::computeCenteredSplashRect(int screenWidth, int screenHeight, int contentWidth, int contentHeight, float maxFraction, float &outX0, float &outY0, float &outX1, float &outY1)
{
	float const maxW = static_cast<float>(screenWidth) * maxFraction;
	float const maxH = static_cast<float>(screenHeight) * maxFraction;
	float const tw = (std::max)(1.f, static_cast<float>(contentWidth));
	float const th = (std::max)(1.f, static_cast<float>(contentHeight));
	float const scale = (std::min)(maxW / tw, maxH / th);
	float const drawW = tw * scale;
	float const drawH = th * scale;
	float const ox = (static_cast<float>(screenWidth) - drawW) * 0.5f;
	float const oy = (static_cast<float>(screenHeight) - drawH) * 0.5f;
	outX0 = ox;
	outY0 = oy;
	outX1 = ox + drawW;
	outY1 = oy + drawH;
}

// ----------------------------------------------------------------------

void SplashScreenNamespace::renderColorFallbackQuad(float x0, float y0, float x1, float y1)
{
	VertexBufferFormat format;
	format.setPosition();
	format.setTransformed();
	format.setColor0();

	DynamicVertexBuffer vertexBuffer(format);
	vertexBuffer.lock(4);

	VectorArgb const fill(255, 35, 42, 65);
	VertexBufferWriteIterator v = vertexBuffer.begin();
	// Triangle strip order matches CuiLayerRenderer (CW): TL, TR, BR, BL
	v.setPosition(x0 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setColor0(fill);
	++v;
	v.setPosition(x1 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setColor0(fill);
	++v;
	v.setPosition(x1 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setColor0(fill);
	++v;
	v.setPosition(x0 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setColor0(fill);
	++v;

	vertexBuffer.unlock();

	Graphics::setVertexBuffer(vertexBuffer);
	Graphics::drawTriangleStrip();
}

// ----------------------------------------------------------------------

void SplashScreenNamespace::renderTexturedSplashQuad(float x0, float y0, float x1, float y1)
{
	VertexBufferFormat format;
	format.setPosition();
	format.setTransformed();
	format.setNumberOfTextureCoordinateSets(1);
	format.setTextureCoordinateSetDimension(0, 2);

	DynamicVertexBuffer vertexBuffer(format);
	vertexBuffer.lock(4);

	VertexBufferWriteIterator v = vertexBuffer.begin();

	// Triangle strip order matches CuiLayerRenderer (CW): TL, TR, BR, BL
	v.setPosition(x0 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 0.0f, 0.0f);
	++v;
	v.setPosition(x1 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 1.0f, 0.0f);
	++v;
	v.setPosition(x1 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 1.0f, 1.0f);
	++v;
	v.setPosition(x0 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 0.0f, 1.0f);
	++v;

	vertexBuffer.unlock();

	Graphics::setVertexBuffer(vertexBuffer);
	Graphics::drawTriangleStrip();
}

// ======================================================================
