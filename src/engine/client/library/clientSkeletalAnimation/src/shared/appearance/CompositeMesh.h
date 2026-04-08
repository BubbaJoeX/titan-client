// ==================================================================
//
// CompositeMesh.h
// copyright 2001 Sony Online Entertainment
// 
// ==================================================================

#ifndef COMPOSITE_MESH_H
#define COMPOSITE_MESH_H

// ==================================================================
// forward declarations

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Tag.h"

class Appearance;
class Camera;
class CustomizationData;
class MeshConstructionHelper;
class MeshGenerator;
class Object;
class ShaderPrimitive;
class ShaderTemplate;
class Skeleton;
class Texture;
class TextureRendererTemplate;
class Transform;
class TransformNameMap;
class Vector;

// ==================================================================

class CompositeMesh
{
public:

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	struct TextureShaderMap
	{
		int m_textureRendererIndex;
		int m_shaderIndex;
		Tag m_shaderTextureTag;
	};

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	typedef stdvector<ShaderPrimitive*>::fwd  ShaderPrimitiveVector;

	/// Optional: after each mesh generator adds primitives to a composite, apply per-wearable runtime textures (e.g. remote/tailor PNG).
	typedef void (*ApplyRemoteTextureToCompositePrimitiveCallback)(Object const *wearableObject, ShaderPrimitive *primitive);

public:

	static void install();

	static void setApplyRemoteTextureToCompositePrimitiveCallback(ApplyRemoteTextureToCompositePrimitiveCallback callback);
	static void clearApplyRemoteTextureToCompositePrimitiveCallback();

public:

	CompositeMesh();
	~CompositeMesh();

	// mesh generator management
	void                      addMeshGenerator(const MeshGenerator *meshGenerator, CustomizationData *customizationData, Object const *sourceObject = 0);
	void                      removeMeshGenerator(const MeshGenerator *meshGenerator);
	void                      removeAllMeshGenerators();

	int                       getMeshGeneratorCount() const;
	void                      getMeshGenerator(int index, int *layer, const MeshGenerator **meshGenerator) const;

	// mesh construction
	void                      applySkeletonModifications(Skeleton &skeleton) const;
	void                      addShaderPrimitives(Appearance &appearance, int lodIndex, const TransformNameMap &transformNameMap, ShaderPrimitiveVector &shaderPrimitives, bool restrictHologramShaderToOwnerMeshGenerators = false) const;

private:

	struct GeneratorLayer;
	struct GeneratorContainer;

private:

	static void remove();

private:

	GeneratorContainer *m_meshGenerators;

private:

	// disable these
	CompositeMesh(const CompositeMesh&);
	const CompositeMesh &operator =(const CompositeMesh&);

};

// Applies MAIN texture (and optional scroll) to a composite primitive when it is a SoftwareBlendSkeletalShaderPrimitive.
// Implemented in clientSkeletalAnimation so clientGame does not include private skeletal headers.
void CompositeMeshApplyRemoteMainTextureToPrimitive(ShaderPrimitive *primitive, Texture const *texture, float scrollU, float scrollV);

// ==================================================================

#endif
