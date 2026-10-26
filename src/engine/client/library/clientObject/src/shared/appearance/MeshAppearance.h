// ======================================================================
//
// MeshAppearance.h
// Portions Copyright 1999 Bootprint Entertainment
// Portions Copyright 2000-2001 Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_MeshAppearance_H
#define INCLUDED_MeshAppearance_H

// ======================================================================

class MeshAppearanceTemplate;
class NetworkId;
class ShaderPrimitiveSet;
class StaticIndexBuffer;
class Transform;

#include "sharedObject/Appearance.h"

#include <vector>

struct PortalVolumeCut
{
	float minI;
	float maxI;
	float minJ;
	float maxJ;
	float inwardDepth;
	float outwardDepth;
};

struct PendingPortalCut
{
	Transform doorWorld;
	PortalVolumeCut volume;
};

struct RegisteredCellPortalCut
{
	Transform doorTransform_cell;
	PortalVolumeCut volume;
};

// ======================================================================

class MeshAppearance: public Appearance
{
	friend class MeshAppearanceTemplate;

public:

	static void install ();
	static void setCollideAgainstAllGeometry (bool collideAgainstAllGeometry);
	static void setFadeInEnabled(bool enabled);

	static void clearBuildingCellPortalCuts(NetworkId const & buildingId);
	static void setBuildingCellPortalCuts(NetworkId const & buildingId, int cellIndex, std::vector<RegisteredCellPortalCut> const & cuts);
	static void setActivePortalCutBuildingId(NetworkId const & buildingId);
	static void clearActivePortalCutBuildingId();

public:

	MeshAppearance(const MeshAppearanceTemplate *newAppearanceTemplate);
	virtual ~MeshAppearance();

	virtual bool                isLoaded() const;

	virtual void                addToWorld();
	virtual void                removeFromWorld();

	virtual void                setAlpha(bool opaqueEnabled, float opaqueAlpha, bool alphaEnabled, float alphaAlpha);

	virtual float               alter(float time);
	virtual const Sphere       &getSphere() const;
	virtual bool collide(Vector const & start_o, Vector const & end_o, CollideParameters const & collideParameters, CollisionInfo & result) const;
	virtual bool implementsCollide() const;

	virtual void                setCustomizationData(CustomizationData *customizationData);
	virtual void                addCustomizationVariables(CustomizationData &customizationData) const;

	virtual void                render() const;
	virtual void                objectListCameraRender() const;
	virtual void drawDebugShapes (DebugShapeRenderer* renderer) const;

	const ShaderPrimitiveSet   *getShaderPrimitiveSet() const;

	virtual void                getMeshGeometryForCollision(IndexedTriangleList & out) const;

	virtual void hideShaderPrimitivesInBox(AxialBox const & box_o);
	virtual bool hideShaderPrimitivesInPortalVolume(Transform const & door_o, PortalVolumeCut const & volume);
	virtual bool hideShaderPrimitivesInPortalVolumeWorld(Transform const & door_w, Transform const & meshToWorld, PortalVolumeCut const & volume);
	virtual void clearHiddenShaderPrimitives();

	void addPendingPortalCut(Transform const & doorWorld, PortalVolumeCut const & volume);
	void clearPendingPortalCuts();
	bool hasPendingPortalCuts() const;
	bool hasAnyHiddenShaderPrimitive() const;
	void applyPendingPortalCuts();
	int  applyPortalVolumeCut(Transform const & doorWorld, PortalVolumeCut const & volume);
	int  applyPortalVolumeBoxFallback(Transform const & doorMesh, PortalVolumeCut const & volume);

	virtual void                setTexture(Tag tag, const Texture &texture);
	virtual void                setTextureScroll(Tag tag, float scrollU, float scrollV);
	virtual void renderShadowBlob (const Vector& position_o, float radius) const;
	virtual void renderReticle (const Vector& position_o, float radius) const;
	virtual AxialBox const getTangibleExtent() const;

#ifdef _DEBUG
	virtual int  getPolygonCount () const;
	virtual void debugDump(std::string &result, int indentLevel) const;
#endif

private:

	// disable these routines
	MeshAppearance();
	MeshAppearance(const MeshAppearance &);
	MeshAppearance &operator =(const MeshAppearance &);

	const MeshAppearanceTemplate *getMeshAppearanceTemplate() const;
	void                          create();
	virtual DPVS::Object         *getDpvsObject() const;

protected:

	ShaderPrimitiveSet *m_shaderPrimitiveSet;
	DPVS::Object       *m_dpvsObject;

	bool                m_needsSetTexture;
	bool                m_obeyOpaqueAlpha;
	bool                m_obeyAlphaAlpha;
	Tag                 m_setTextureTag;
	const Texture*      m_setTextureTexture;
	float               m_opaqueAlpha;
	float               m_alphaAlpha;

	CustomizationData  *m_customizationData;

	bool mutable m_lastCouldRenderShadow;
	bool mutable m_lastCouldRenderReticles;
	std::vector<bool> m_hiddenShaderPrimitiveMask;
	std::vector<StaticIndexBuffer *> m_portalCutIndexBuffers;
	std::vector<PendingPortalCut> m_pendingPortalCuts;

	void clearPortalCutIndexBuffers();
};

// ======================================================================

inline const ShaderPrimitiveSet *MeshAppearance::getShaderPrimitiveSet() const
{
	return m_shaderPrimitiveSet;
}

// ======================================================================

#endif
