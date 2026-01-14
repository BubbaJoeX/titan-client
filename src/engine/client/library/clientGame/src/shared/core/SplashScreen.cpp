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
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"

// ======================================================================

namespace SplashScreenNamespace
{
	bool ms_installed = false;
	bool ms_active = false;
	bool ms_dismissed = false;
	
	Texture const * ms_texture = nullptr;
	StaticShader * ms_shader = nullptr;
	
	char const * const cms_defaultSplashTexture = "texture/ui/loading/large/large_load_watto.dds";
	char const * const cms_splashShader = "shader/ui_splash.sht";
	
	void renderFullscreenQuad();
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
	
	// Load splash texture
	char const * const splashTexture = ConfigFile::getKeyString("ClientGame", "splashTexture", cms_defaultSplashTexture);
	ms_texture = TextureList::fetch(splashTexture);
	
	// Fetch shader and set the texture on it
	Shader const * baseShader = ShaderTemplateList::fetchShader("shader/2d_texture.sht");
	if (baseShader)
	{
		//ms_shader = baseShader->prepareToView()->asStaticShader();
		if (ms_shader && ms_texture)
		{
			ms_shader->setTexture(TAG(M,A,I,N), *ms_texture);
		}
		baseShader->release();
	}
	
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

void SplashScreen::render()
{
	if (!ms_active || ms_dismissed || !ms_texture || !ms_shader)
		return;
	
	// Clear screen to black first
	Graphics::clearViewport(true, 0x00000000, true, 1.0f, true, 0);
	
	// Set up rendering state
	Graphics::setStaticShader(*ms_shader);
	
	// Draw fullscreen quad
	renderFullscreenQuad();
	
	// Present to screen
	Graphics::present();
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

void SplashScreenNamespace::renderFullscreenQuad()
{
	// Get screen dimensions
	int const screenWidth = Graphics::getCurrentRenderTargetWidth();
	int const screenHeight = Graphics::getCurrentRenderTargetHeight();
	
	// Calculate texture dimensions to maintain aspect ratio
	int const textureWidth = ms_texture->getWidth();
	int const textureHeight = ms_texture->getHeight();
	
	float const screenAspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
	float const textureAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
	
	float x0, y0, x1, y1;
	
	if (textureAspect > screenAspect)
	{
		// Texture is wider - fit to width, letterbox top/bottom
		float const scaledHeight = screenWidth / textureAspect;
		x0 = 0.0f;
		x1 = static_cast<float>(screenWidth);
		y0 = (screenHeight - scaledHeight) / 2.0f;
		y1 = y0 + scaledHeight;
	}
	else
	{
		// Texture is taller - fit to height, pillarbox sides
		float const scaledWidth = screenHeight * textureAspect;
		y0 = 0.0f;
		y1 = static_cast<float>(screenHeight);
		x0 = (screenWidth - scaledWidth) / 2.0f;
		x1 = x0 + scaledWidth;
	}
	
	// Create vertex buffer for fullscreen quad
	VertexBufferFormat format;
	format.setPosition();
	format.setTransformed();
	format.setNumberOfTextureCoordinateSets(1);
	format.setTextureCoordinateSetDimension(0, 2);
	
	DynamicVertexBuffer vertexBuffer(format);
	vertexBuffer.lock(4);
	
	VertexBufferWriteIterator v = vertexBuffer.begin();
	
	// Top-left
	v.setPosition(x0 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 0.0f, 0.0f);
	++v;
	
	// Top-right
	v.setPosition(x1 - 0.5f, y0 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 1.0f, 0.0f);
	++v;
	
	// Bottom-left
	v.setPosition(x0 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 0.0f, 1.0f);
	++v;
	
	// Bottom-right
	v.setPosition(x1 - 0.5f, y1 - 0.5f, 0.0f);
	v.setOoz(1.0f);
	v.setTextureCoordinates(0, 1.0f, 1.0f);
	++v;
	
	vertexBuffer.unlock();
	
	Graphics::setVertexBuffer(vertexBuffer);
	Graphics::drawTriangleStrip();
}

// ======================================================================
