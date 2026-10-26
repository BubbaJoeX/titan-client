// ======================================================================
//
// ShaderPrimitiveSet.h
// Copyright 2000-01, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ShaderPrimitiveSet_H
#define INCLUDED_ShaderPrimitiveSet_H

// ======================================================================

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Tag.h"
#include "sharedMath/Vector.h"

#include <vector>

// ======================================================================
class Appearance;
class CollideParameters;
class CollisionInfo;
class CustomizationData;
class Iff;
class Shader;
class ShaderPrimitiveSetTemplate;
class ShaderTemplate;
class StaticIndexBuffer;
class StaticVertexBuffer;
class Texture;

// ======================================================================

class ShaderPrimitiveSet
{
	friend class ShaderPrimitiveSetTemplate;

public:

	static void install();

	// Screen surfaces: while the filter is on, setTexture / setTextureScroll only touch shader primitives whose
	// shader name contains "screen" or "canvas" (a video screen inside a themed frame, a painting's canvas), and
	// count them; probe mode only counts. Off by default.
	static void beginScreenSurfaceFilter(bool probeOnly);
	static int  endScreenSurfaceFilter();
	static bool isScreenSurfaceShaderName(char const *shaderName);
	// setTexture / setTextureScroll of MAIN on a set that has screen surfaces change only those (applies also to the
	// deferred MeshAppearance::setTexture, which reaches the set at render time).
	bool        hasScreenSurfaces() const;

public:

	~ShaderPrimitiveSet();

	float alter(float elapsedTime);
	void add();
	void addWithAlphaFadeOpacity(bool opaqueEnabled, float opaqueOpacity, bool alphaEnabled, float alphaOpacity);

	void setCustomizationData(CustomizationData *customizationData);
	void addCustomizationVariables(CustomizationData &customizationData);

	int                       getNumberOfShaders () const;
	const Shader*             getShader (int shaderIndex) const;
	const ShaderTemplate*     getShaderTemplate (int shaderIndex) const;
	const StaticVertexBuffer* getVertexBuffer (int shaderIndex) const;
	const StaticIndexBuffer*  getIndexBuffer (int shaderIndex) const;

	void                      getCostEstimate(int &numberOfVertices, int &numberOfTriangles, float &complexity) const;

	void                      setTexture(Tag tag, const Texture &texture);
	void                      setTextureScroll(Tag tag, float u1, float v1, float u2, float v2);

	void                      add(std::vector<bool> const * skipMask);
	void                      addWithAlphaFadeOpacity(bool opaqueEnabled, float opaqueOpacity, bool alphaEnabled, float alphaOpacity, std::vector<bool> const * skipMask);

	void                      setRuntimeIndexBufferOverride(int shaderIndex, StaticIndexBuffer const * indexBuffer);
	void                      clearRuntimeIndexBufferOverrides();
	StaticIndexBuffer const * getRuntimeIndexBufferOverride(int shaderIndex) const;

	bool collide(Vector const & start_o, Vector const & end_o, CollideParameters const & collideParameters, CollisionInfo & result) const;
	bool collide(Vector const & start_o, Vector const & end_o, CollideParameters const & collideParameters, CollisionInfo & result, std::vector<bool> const * skipMask) const;

private:

	ShaderPrimitiveSet(const Appearance &owner, const ShaderPrimitiveSetTemplate &shaderPrimitiveSetTemplate);

public:
	class LocalShaderPrimitive;

private:
	typedef stdvector<LocalShaderPrimitive *>::fwd Primitives;

private:

	Primitives 	     *m_primitives;
	Vector            m_localCameraPos;
	std::vector<StaticIndexBuffer const *> m_runtimeIndexBufferOverrides;

private:

	// Disabled
	ShaderPrimitiveSet();
	ShaderPrimitiveSet(const ShaderPrimitiveSet &);
	ShaderPrimitiveSet &operator =(const ShaderPrimitiveSet &);
};

// ======================================================================

#endif
