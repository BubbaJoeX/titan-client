// ======================================================================
//
// MeshAppearance.cpp
// Portions Copyright 1999 Bootprint Entertainment
// Portions Copyright 2000-2001 Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "clientObject/FirstClientObject.h"
#include "clientObject/MeshAppearance.h"

#include "clientGraphics/Camera.h"
#include "clientGraphics/DebugPrimitive.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/Shader.h"
#include "clientGraphics/ShaderPrimitiveSet.h"
#include "clientGraphics/ShaderPrimitiveSetTemplate.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticShaderTemplate.h"
#include "clientGraphics/Texture.h"
#include "clientObject/MeshAppearanceTemplate.h"
#include "clientObject/DetailAppearance.h"
#include "clientObject/ShadowBlobManager.h"
#include "clientObject/ShadowManager.h"
#include "clientObject/ReticleManager.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedCollision/Extent.h"
#include "sharedCollision/ExtentList.h"
#include "sharedMath/Transform.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/VertexBufferIterator.h"
#include "sharedDebug/DebugFlags.h"

#include <vector>
#include <cmath>
#include "sharedDebug/Profiler.h"
#include "sharedMath/DebugShapeRenderer.h"
#include "sharedMath/AxialBox.h"
#include "sharedMath/IndexedTriangleList.h"
#include "sharedMath/Plane.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/CustomizationData.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/Object.h"
#include "sharedObject/PortalProperty.h"

#include "dpvsObject.hpp"
#include "dpvsModel.hpp"

#include <algorithm>
#include <map>
#include <stdio.h>
#include <string>
#include <vector>

// ======================================================================

namespace
{
	/// Prefer drawable vertex/index buffers (actual rendered mesh faces) over authored collision triangle lists from .msh.
	/// When \a walkAndBlockingCollision is true, include every drawable primitive so collision matches what is drawn.
	bool appendRenderableMeshTrianglesFromTemplate(ShaderPrimitiveSetTemplate const *const spsTemplate, IndexedTriangleList &out, bool walkAndBlockingCollision)
	{
		if (!spsTemplate)
			return false;

		size_t const beforeVerts = out.getVertices().size();
		bool const collideAll = ShaderPrimitiveSetTemplate::getCollideAgainstAllGeometry();

		int const numPrimitives = spsTemplate->getNumberOfShaderPrimitiveTemplates();
		for (int i = 0; i < numPrimitives; ++i)
		{
			ShaderPrimitiveSetTemplate::LocalShaderPrimitiveTemplate const &prim = spsTemplate->getShaderPrimitiveTemplate(i);
			ShaderTemplate const *const shaderTemplate = prim.getShaderTemplate();
			if (!walkAndBlockingCollision && !collideAll && shaderTemplate && !shaderTemplate->isCollidable())
				continue;

			StaticVertexBuffer const *const vb = prim.getVertexBuffer();
			StaticIndexBuffer const *const ib = prim.getIndexBuffer();
			if (!vb || !ib)
				continue;

			int const nv = vb->getNumberOfVertices();
			int const ni = ib->getNumberOfIndices();
			if (nv < 3 || ni < 3 || (ni % 3) != 0)
				continue;

			std::vector<Vector> verts(static_cast<size_t>(nv));
			vb->lockReadOnly();
			{
				VertexBufferReadIterator it = vb->beginReadOnly();
				for (int v = 0; v < nv; ++v, ++it)
					verts[static_cast<size_t>(v)] = it.getPosition();
			}
			vb->unlock();

			std::vector<int> indices(static_cast<size_t>(ni));
			ib->lockReadOnly();
			{
				Index const *const begin = ib->beginReadOnly();
				for (int ii = 0; ii < ni; ++ii)
				{
					int const idx = static_cast<int>(begin[ii]);
					if (idx < 0 || idx >= nv)
					{
						indices.clear();
						break;
					}
					indices[static_cast<size_t>(ii)] = idx;
				}
			}
			ib->unlock();

			if (indices.size() != static_cast<size_t>(ni))
				continue;

			out.addIndexedTriangleList(&verts[0], nv, &indices[0], ni);
		}

		return out.getVertices().size() > beforeVerts;
	}
}

namespace MeshAppearanceNamespace
{
	bool ms_fadeInEnabled = true;

	typedef std::map<int, std::vector<RegisteredCellPortalCut> > CellPortalCutMap;
	typedef std::map<NetworkId, CellPortalCutMap> BuildingCellPortalCutMap;

	BuildingCellPortalCutMap s_buildingCellPortalCuts;
	NetworkId s_activePortalCutBuildingId;

	bool buildingHasRegisteredPortalCuts(NetworkId const & buildingId)
	{
		if (!buildingId.isValid())
			return false;

		BuildingCellPortalCutMap::const_iterator buildingIt = s_buildingCellPortalCuts.find(buildingId);
		return buildingIt != s_buildingCellPortalCuts.end() && !buildingIt->second.empty();
	}

	CellProperty const * resolveMeshOwnerCell(Object const * meshOwner)
	{
		if (!meshOwner)
			return 0;

		CellProperty const * cell = meshOwner->getCellProperty();
		if (cell && !cell->isWorldCell())
			return cell;

		cell = meshOwner->getParentCell();
		if (cell && !cell->isWorldCell())
			return cell;

		for (Object const * current = meshOwner->getAttachedTo(); current; current = current->getAttachedTo())
		{
			cell = current->getCellProperty();
			if (cell && !cell->isWorldCell())
				return cell;

			cell = current->getParentCell();
			if (cell && !cell->isWorldCell())
				return cell;
		}

		return 0;
	}

	NetworkId resolveMeshOwnerBuildingId(Object const * meshOwner)
	{
		CellProperty const * const cell = resolveMeshOwnerCell(meshOwner);
		if (cell && cell->getPortalProperty())
			return cell->getPortalProperty()->getOwner().getNetworkId();

		for (Object const * current = meshOwner; current; current = current->getAttachedTo())
		{
			PortalProperty const * const portalProperty = current->getPortalProperty();
			if (portalProperty)
				return portalProperty->getOwner().getNetworkId();

			CellProperty const * const parentCell = current->getParentCell();
			if (parentCell && !parentCell->isWorldCell() && parentCell->getPortalProperty())
				return parentCell->getPortalProperty()->getOwner().getNetworkId();
		}

		if (s_activePortalCutBuildingId.isValid())
			return s_activePortalCutBuildingId;

		return NetworkId();
	}

	struct MeshLocalPortalCut
	{
		Transform doorMesh;
		Transform doorWorld;
		bool hasDoorWorld;
		PortalVolumeCut volume;
	};

	void appendMeshLocalCut(
		std::vector<MeshLocalPortalCut> & cuts,
		Transform const & doorMesh,
		PortalVolumeCut const & volume,
		Transform const * doorWorld = 0)
	{
		MeshLocalPortalCut cut;
		cut.doorMesh = doorMesh;
		cut.hasDoorWorld = doorWorld != 0;
		if (cut.hasDoorWorld)
			cut.doorWorld = *doorWorld;
		cut.volume = volume;
		cuts.push_back(cut);
	}

	void appendRegisteredCutsForBuilding(
		NetworkId const & buildingId,
		int cellIndexFilter,
		Transform const & meshToWorld,
		std::vector<MeshLocalPortalCut> & outCuts)
	{
		BuildingCellPortalCutMap::const_iterator buildingIt = s_buildingCellPortalCuts.find(buildingId);
		if (buildingIt == s_buildingCellPortalCuts.end())
			return;

		Object const * const buildingObject = NetworkIdManager::getObjectById(buildingId);
		PortalProperty const * const portalProperty = buildingObject ? buildingObject->getPortalProperty() : 0;
		if (!portalProperty)
			return;

		Transform worldToMesh;
		worldToMesh.invert(meshToWorld);

		for (CellPortalCutMap::const_iterator cellIt = buildingIt->second.begin(); cellIt != buildingIt->second.end(); ++cellIt)
		{
			if (cellIndexFilter >= 1 && cellIt->first != cellIndexFilter)
				continue;

			CellProperty const * const cutCellProperty = portalProperty->getCell(cellIt->first);
			if (!cutCellProperty)
				continue;

			Transform const cellToWorld = cutCellProperty->getOwner().getTransform_o2w();
			for (size_t cutIndex = 0; cutIndex < cellIt->second.size(); ++cutIndex)
			{
				RegisteredCellPortalCut const & registered = cellIt->second[cutIndex];
				Transform doorWorld;
				doorWorld.multiply(cellToWorld, registered.doorTransform_cell);
				Transform doorMesh;
				doorMesh.multiply(worldToMesh, doorWorld);
				appendMeshLocalCut(outCuts, doorMesh, registered.volume, &doorWorld);
			}
		}
	}

	void gatherMeshLocalPortalCuts(
		Object const * meshOwner,
		Transform const & meshToWorld,
		std::vector<PendingPortalCut> const & pendingCuts,
		std::vector<MeshLocalPortalCut> & outCuts)
	{
		Transform worldToMesh;
		worldToMesh.invert(meshToWorld);

		for (size_t i = 0; i < pendingCuts.size(); ++i)
		{
			PendingPortalCut const & pending = pendingCuts[i];
			Transform doorMesh;
			doorMesh.multiply(worldToMesh, pending.doorWorld);
			appendMeshLocalCut(outCuts, doorMesh, pending.volume, &pending.doorWorld);
		}

		NetworkId const buildingId = resolveMeshOwnerBuildingId(meshOwner);
		if (!buildingId.isValid())
			return;

		int cellIndexFilter = -1;
		CellProperty const * const meshCell = resolveMeshOwnerCell(meshOwner);
		if (meshCell)
			cellIndexFilter = meshCell->getCellIndex();

		appendRegisteredCutsForBuilding(buildingId, cellIndexFilter, meshToWorld, outCuts);
	}
}

using namespace MeshAppearanceNamespace;

// ======================================================================

void MeshAppearance::install ()
{
}

// ----------------------------------------------------------------------

void MeshAppearance::setCollideAgainstAllGeometry(bool const collideAgainstAllGeometry)
{
	ShaderPrimitiveSetTemplate::setCollideAgainstAllGeometry(collideAgainstAllGeometry);
}

// ----------------------------------------------------------------------

void MeshAppearance::setFadeInEnabled(bool enabled)
{
	ms_fadeInEnabled = enabled;
}

// ----------------------------------------------------------------------

void MeshAppearance::clearBuildingCellPortalCuts(NetworkId const & buildingId)
{
	s_buildingCellPortalCuts.erase(buildingId);
}

// ----------------------------------------------------------------------

void MeshAppearance::setBuildingCellPortalCuts(NetworkId const & buildingId, int cellIndex, std::vector<RegisteredCellPortalCut> const & cuts)
{
	if (!buildingId.isValid() || cellIndex < 1)
		return;

	if (cuts.empty())
	{
		BuildingCellPortalCutMap::iterator buildingIt = s_buildingCellPortalCuts.find(buildingId);
		if (buildingIt != s_buildingCellPortalCuts.end())
		{
			buildingIt->second.erase(cellIndex);
			if (buildingIt->second.empty())
				s_buildingCellPortalCuts.erase(buildingIt);
		}
		return;
	}

	s_buildingCellPortalCuts[buildingId][cellIndex] = cuts;
}

// ----------------------------------------------------------------------

void MeshAppearance::setActivePortalCutBuildingId(NetworkId const & buildingId)
{
	s_activePortalCutBuildingId = buildingId;
}

// ----------------------------------------------------------------------

void MeshAppearance::clearActivePortalCutBuildingId()
{
	s_activePortalCutBuildingId = NetworkId();
}

// ======================================================================

const MeshAppearanceTemplate *MeshAppearance::getMeshAppearanceTemplate() const
{
	return safe_cast<const MeshAppearanceTemplate *>(getAppearanceTemplate());
}

// ----------------------------------------------------------------------

MeshAppearance::MeshAppearance(const MeshAppearanceTemplate *meshAppearanceTemplate)
:	Appearance(NON_NULL(meshAppearanceTemplate)),
	m_shaderPrimitiveSet(NULL),
	m_dpvsObject(NULL),
	m_needsSetTexture(false),
	m_obeyOpaqueAlpha(false),
	m_obeyAlphaAlpha(false),
	m_setTextureTag(),
	m_setTextureTexture(0),
	m_opaqueAlpha(1.0f),
	m_alphaAlpha(1.0f),
	m_customizationData(0),
	m_lastCouldRenderShadow(false),
	m_lastCouldRenderReticles(false)
{
	{
		DPVS::Model *const testModel = RenderWorld::fetchDefaultModel();
		m_dpvsObject = RenderWorld::createObject(this, testModel);
		testModel->release();
	}
	create();

	meshAppearanceTemplate->meshAppearanceCreated();
}

// ----------------------------------------------------------------------

MeshAppearance::~MeshAppearance()
{
	if (m_customizationData)
	{
		m_customizationData->release();
		m_customizationData = NULL;
	}

	if (!m_shaderPrimitiveSet)
		getMeshAppearanceTemplate()->removeAsynchronouslyLoadedAppearance(this);

	m_dpvsObject->release();
	m_dpvsObject = NULL;

	delete m_shaderPrimitiveSet;
	m_shaderPrimitiveSet = NULL;

	clearPortalCutIndexBuffers();

	//-- if m_needsSetTexture is true, we were never able to set the texture, so release it
	if (m_needsSetTexture && m_setTextureTexture)
	{
		m_setTextureTexture->release();
		m_setTextureTexture = 0;
	}

	const_cast<MeshAppearanceTemplate *>(getMeshAppearanceTemplate())->meshAppearanceDestroyed();
}

// ----------------------------------------------------------------------

bool MeshAppearance::isLoaded() const
{
	return m_shaderPrimitiveSet != NULL;
}

// ----------------------------------------------------------------------

void MeshAppearance::create()
{
	const MeshAppearanceTemplate *meshAppearanceTemplate = getMeshAppearanceTemplate();

	if (meshAppearanceTemplate->isLoaded())
	{
		m_shaderPrimitiveSet = meshAppearanceTemplate->getShaderPrimitiveSetTemplate()->createShaderPrimitiveSet(*this);
		m_dpvsObject->setTestModel(meshAppearanceTemplate->getDpvsTestShape());
		setExtent(ExtentList::fetch(meshAppearanceTemplate->getExtent()));

		m_shaderPrimitiveSet->setCustomizationData(m_customizationData);
	}
}

// ----------------------------------------------------------------------

float MeshAppearance::alter(float time)
{
	float alterResult;

	if (m_shaderPrimitiveSet)
	{
		Object const * const owner = getOwner();
		if (owner && !MeshAppearanceNamespace::s_buildingCellPortalCuts.empty())
		{
			NetworkId const buildingId = MeshAppearanceNamespace::resolveMeshOwnerBuildingId(owner);
			if (MeshAppearanceNamespace::buildingHasRegisteredPortalCuts(buildingId))
			{
				bool hasCutState = hasAnyHiddenShaderPrimitive();
				if (!hasCutState)
				{
					for (size_t i = 0; i < m_portalCutIndexBuffers.size(); ++i)
					{
						if (m_portalCutIndexBuffers[i])
						{
							hasCutState = true;
							break;
						}
					}
				}

				if (!hasCutState)
					applyPendingPortalCuts();
			}
		}

		alterResult = m_shaderPrimitiveSet->alter(time);

		if (m_setTextureTexture && m_needsSetTexture)
		{
			m_needsSetTexture = false;
			m_shaderPrimitiveSet->setTexture(m_setTextureTag, *m_setTextureTexture);
			m_setTextureTexture->release();
			m_setTextureTexture = 0;
		}
	}
	else
	{
		//-- if the appearance doesn't have a shader primitive set, that means it is being
		//   asynchronously loaded.  in this case, we need to return alter next frame until
		//   the object has finished loading
		alterResult = AlterResult::cms_alterNextFrame;
	}

	// alpha fade in a mesh if it's the root appearance on an object
	const Object *owner = getOwner();
	if (m_obeyOpaqueAlpha && m_obeyAlphaAlpha && owner && owner->getAppearance() == this)
	{
		if (isLoaded())
		{
			m_opaqueAlpha += time;
			m_alphaAlpha += time;
			if (!ms_fadeInEnabled || m_opaqueAlpha >= 1.0f)
			{
				m_obeyOpaqueAlpha = false;
				m_obeyAlphaAlpha = false;
				m_opaqueAlpha = 0.0f;
				m_alphaAlpha = 0.0f;
			}
		}

		alterResult = AlterResult::cms_alterNextFrame;
	}

	return alterResult;
}

// ----------------------------------------------------------------------

DPVS::Object *MeshAppearance::getDpvsObject() const
{
	return m_dpvsObject;
}

// ----------------------------------------------------------------------

void MeshAppearance::addToWorld()
{
	// check if we should alpha fade this object in
	m_obeyOpaqueAlpha = ms_fadeInEnabled;
	m_obeyAlphaAlpha = ms_fadeInEnabled;
	m_opaqueAlpha = 0.0f;
	m_alphaAlpha = 0.0f;

	Appearance::addToWorld();
}

// ----------------------------------------------------------------------

void MeshAppearance::removeFromWorld()
{
	Appearance::removeFromWorld();
	m_obeyOpaqueAlpha = false;
	m_obeyAlphaAlpha = false;
	m_opaqueAlpha = 0.0f;
	m_alphaAlpha = 0.0f;
}

// ----------------------------------------------------------------------

void MeshAppearance::setAlpha(bool const opaqueEnabled, float const opaqueAlpha, bool const alphaEnabled, float const alphaAlpha)
{
	m_obeyOpaqueAlpha = opaqueEnabled;
	m_obeyAlphaAlpha = alphaEnabled;
	m_opaqueAlpha = opaqueAlpha;
	m_alphaAlpha = alphaAlpha;
}

// ----------------------------------------------------------------------

void MeshAppearance::objectListCameraRender() const
{
	const_cast<MeshAppearance *>(this)->applyPendingPortalCuts();
	// Same issue as DetailAppearance::objectListCameraRender(): world fade-in alpha
	// state must not affect UI object-list camera previews (examine, inventory icons).
	const_cast<MeshAppearance *>(this)->setAlpha(false, 0.0f, false, 0.0f);
	Appearance::objectListCameraRender();
}

// ----------------------------------------------------------------------

void MeshAppearance::render() const
{
	NP_PROFILER_AUTO_BLOCK_DEFINE("MeshAppearance::render");

	const_cast<MeshAppearance *>(this)->applyPendingPortalCuts();

	if (m_shaderPrimitiveSet)
	{
		std::vector<bool> const * skipMask = m_hiddenShaderPrimitiveMask.empty() ? 0 : &m_hiddenShaderPrimitiveMask;
		if (m_obeyOpaqueAlpha || m_obeyAlphaAlpha)
			m_shaderPrimitiveSet->addWithAlphaFadeOpacity(m_obeyOpaqueAlpha, m_opaqueAlpha, m_obeyAlphaAlpha, m_alphaAlpha, skipMask);
		else
		{
			m_shaderPrimitiveSet->add(skipMask);

			//-- queue shadow
			if (m_shaderPrimitiveSet->getNumberOfShaders())
			{
				const Object *owner=getOwner();
				if (owner)
				{
					getMeshAppearanceTemplate()->renderVolumetricShadow(this, m_shaderPrimitiveSet->getShader(0)->usesVertexShader());
				}
			}
		}

		if (getShadowBlobAllowed())
		{
			bool const couldRenderShadow = ShadowManager::getEnabled() && ShadowManager::getAllowed() && ShadowManager::getSkeletalShadowsSimple();
			bool const couldRenderReticle = ReticleManager::getEnabled();

			if (couldRenderShadow)
			{
				if (m_opaqueAlpha == 0.f)
					ShadowBlobManager::renderShadowBlobs(*this);

				m_lastCouldRenderShadow = true;
			}

			if(couldRenderReticle)
			{
				if(m_opaqueAlpha == 0.f)
					ReticleManager::renderReticles(*this);

				m_lastCouldRenderReticles = true;
			}

			if(!couldRenderShadow && !couldRenderReticle && (m_lastCouldRenderReticles || m_lastCouldRenderShadow))
			{				
				getMeshAppearanceTemplate()->clearUpwardIndices();
			}
			m_lastCouldRenderShadow = couldRenderShadow;
			m_lastCouldRenderReticles = couldRenderReticle;
		}
	}

	Appearance::render();
}

// ----------------------------------------------------------------------

const Sphere &MeshAppearance::getSphere() const
{
	return getMeshAppearanceTemplate()->getSphere();
}

// ----------------------------------------------------------------------

void MeshAppearance::setCustomizationData(CustomizationData *customizationData)
{
	//-- Hang on to customization data.
	if (customizationData)
		customizationData->fetch();

	if (m_customizationData)
		m_customizationData->release();

	m_customizationData = customizationData;

	if (m_shaderPrimitiveSet)
		m_shaderPrimitiveSet->setCustomizationData(customizationData);
}

// ----------------------------------------------------------------------

void MeshAppearance::addCustomizationVariables(CustomizationData &customizationData) const
{
	if (m_shaderPrimitiveSet)
		m_shaderPrimitiveSet->addCustomizationVariables(customizationData);
}

// ----------------------------------------------------------------------

bool MeshAppearance::collide(Vector const & start_o, Vector const & end_o, CollideParameters const & collideParameters, CollisionInfo & result) const
{
	//-- Collide w/ extent only
	if (collideParameters.getQuality() == CollideParameters::Q_low)
	{
		Extent const * const extent = getExtent();
		if (extent)
		{
			Vector normal;
			float time;
			if (extent->intersect(start_o, end_o, &normal, &time))
			{
				result.setPoint(Vector::linearInterpolate(start_o, end_o, time));
				result.setNormal(normal);
				result.setTime(time);

				return true;
			}
		}

		return false;
	}

	//-- All other collide qualities do collision with actual mesh
	if (m_shaderPrimitiveSet && getExtent() && getExtent()->intersect(start_o, end_o))
	{
		std::vector<bool> const * skipMask = m_hiddenShaderPrimitiveMask.empty() ? 0 : &m_hiddenShaderPrimitiveMask;
		return m_shaderPrimitiveSet->collide(start_o, end_o, collideParameters, result, skipMask);
	}

	return false;
}

// ----------------------------------------------------------------------

bool MeshAppearance::implementsCollide() const
{
	return true;
}

// ----------------------------------------------------------------------

void MeshAppearance::setTexture(const Tag tag, const Texture &texture)
{
	m_needsSetTexture = true;
	m_setTextureTag = tag;
	m_setTextureTexture = &texture;
	m_setTextureTexture->fetch();
}

// ----------------------------------------------------------------------

void MeshAppearance::setTextureScroll(const Tag tag, float scrollU, float scrollV)
{
	if (m_shaderPrimitiveSet)
	{
		m_shaderPrimitiveSet->setTextureScroll(tag, scrollU, scrollV, 0.0f, 0.0f);
	}
}

// ----------------------------------------------------------------------

void MeshAppearance::renderShadowBlob(Vector const & position_o, float const radius) const
{
	getMeshAppearanceTemplate()->renderSimpleShadow(position_o, radius);
}

// ----------------------------------------------------------------------

void MeshAppearance::renderReticle(Vector const & position_o, float const radius) const
{
	getMeshAppearanceTemplate()->renderReticle(position_o, radius);
}

// ----------------------------------------------------------------------

void MeshAppearance::getMeshGeometryForCollision(IndexedTriangleList & out) const
{
	const MeshAppearanceTemplate *const template_ = getMeshAppearanceTemplate();
	if (!template_ || !template_->isLoaded())
		return;

	ShaderPrimitiveSetTemplate const *const spsTemplate = template_->getShaderPrimitiveSetTemplate();
	if (!spsTemplate)
		return;

	// Source of truth: drawable mesh (vertex + index buffers). Matches on-screen geometry for walk/blocking collision.
	if (appendRenderableMeshTrianglesFromTemplate(spsTemplate, out, true))
		return;

	// Fallback when render buffers are unavailable: legacy authored collision chunks from .msh (collision IndexedTriangleList).
	int const numPrimitives = spsTemplate->getNumberOfShaderPrimitiveTemplates();
	for (int i = 0; i < numPrimitives; ++i)
	{
		ShaderPrimitiveSetTemplate::LocalShaderPrimitiveTemplate const & primTemplate = spsTemplate->getShaderPrimitiveTemplate(i);
		IndexedTriangleList const *const collisionList = primTemplate.getCollisionIndexedTriangleList();
		if (!collisionList)
			continue;

		stdvector<Vector>::fwd const & verts = collisionList->getVertices();
		stdvector<int>::fwd const & inds = collisionList->getIndices();
		if (verts.empty() || inds.empty())
			continue;

		int const vcount = static_cast<int>(verts.size());
		bool indicesOk = true;
		for (size_t ii = 0; ii < inds.size(); ++ii)
		{
			int const idx = inds[ii];
			if (idx < 0 || idx >= vcount)
			{
				indicesOk = false;
				break;
			}
		}
		if (!indicesOk)
			continue;

		out.addIndexedTriangleList(&verts[0], static_cast<int>(verts.size()), &inds[0], static_cast<int>(inds.size()));
	}
}

// ----------------------------------------------------------------------

AxialBox const MeshAppearance::getTangibleExtent() const
{
	if (m_shaderPrimitiveSet)
	{
		Extent const * const extent = getExtent();
		if (extent)
			return extent->getBoundingBox();
	}

	return AxialBox();
}

// ----------------------------------------------------------------------

void MeshAppearance::clearHiddenShaderPrimitives()
{
	m_hiddenShaderPrimitiveMask.clear();
	clearPendingPortalCuts();
}

// ----------------------------------------------------------------------

void MeshAppearance::clearPortalCutIndexBuffers()
{
	if (m_shaderPrimitiveSet)
		m_shaderPrimitiveSet->clearRuntimeIndexBufferOverrides();

	for (size_t i = 0; i < m_portalCutIndexBuffers.size(); ++i)
	{
		delete m_portalCutIndexBuffers[i];
		m_portalCutIndexBuffers[i] = 0;
	}
	m_portalCutIndexBuffers.clear();
}

// ----------------------------------------------------------------------

namespace
{
	bool boxesOverlap(AxialBox const & a, AxialBox const & b)
	{
		Vector const aMin = a.getMin();
		Vector const aMax = a.getMax();
		Vector const bMin = b.getMin();
		Vector const bMax = b.getMax();
		return aMin.x <= bMax.x && aMax.x >= bMin.x
			&& aMin.y <= bMax.y && aMax.y >= bMin.y
			&& aMin.z <= bMax.z && aMax.z >= bMin.z;
	}
}

namespace MeshAppearanceNamespace
{
	bool vertexInPortalVolume(Vector const & vertex_o, Transform const & door_o, PortalVolumeCut const & volume)
	{
		Vector const doorPos = door_o.getPosition_p();
		Vector const axisI = door_o.getLocalFrameI_p();
		Vector const axisJ = door_o.getLocalFrameJ_p();
		Vector const axisK = door_o.getLocalFrameK_p();

		Vector const delta = vertex_o - doorPos;
		float const i = delta.dot(axisI);
		float const j = delta.dot(axisJ);
		float const k = delta.dot(axisK);

		return i >= volume.minI && i <= volume.maxI
			&& j >= volume.minJ && j <= volume.maxJ
			&& k >= -volume.outwardDepth && k <= volume.inwardDepth;
	}

	void computeVertexDoorLocal(Vector const & vertex_o, Transform const & door_o, float & outI, float & outJ, float & outK)
	{
		Vector const doorPos = door_o.getPosition_p();
		Vector const delta = vertex_o - doorPos;
		outI = delta.dot(door_o.getLocalFrameI_p());
		outJ = delta.dot(door_o.getLocalFrameJ_p());
		outK = delta.dot(door_o.getLocalFrameK_p());
	}

	bool vertexInPortalVolumeWorld(Vector const & vertex_w, Transform const & door_w, PortalVolumeCut const & volume)
	{
		Vector const delta = vertex_w - door_w.getPosition_p();
		float const i = delta.dot(door_w.getLocalFrameI_p());
		float const j = delta.dot(door_w.getLocalFrameJ_p());
		float const k = delta.dot(door_w.getLocalFrameK_p());

		return i >= volume.minI && i <= volume.maxI
			&& j >= volume.minJ && j <= volume.maxJ
			&& k >= -volume.outwardDepth && k <= volume.inwardDepth;
	}

	void accumulateDoorLocalBounds(
		Vector const & vertex_w,
		Transform const & door_w,
		float & minI,
		float & maxI,
		float & minJ,
		float & maxJ,
		float & minK,
		float & maxK)
	{
		Vector const delta = vertex_w - door_w.getPosition_p();
		float const i = delta.dot(door_w.getLocalFrameI_p());
		float const j = delta.dot(door_w.getLocalFrameJ_p());
		float const k = delta.dot(door_w.getLocalFrameK_p());

		minI = std::min(minI, i);
		maxI = std::max(maxI, i);
		minJ = std::min(minJ, j);
		maxJ = std::max(maxJ, j);
		minK = std::min(minK, k);
		maxK = std::max(maxK, k);
	}

	bool triangleIjOverlapOverlapsPortalDepthSlop(
		float minI,
		float maxI,
		float minJ,
		float maxJ,
		float minK,
		float maxK,
		PortalVolumeCut const & volume,
		float kSlop,
		float ijSlop = 0.25f)
	{
		return minI <= volume.maxI + ijSlop && maxI >= volume.minI - ijSlop
			&& minJ <= volume.maxJ + ijSlop && maxJ >= volume.minJ - ijSlop
			&& minK <= volume.inwardDepth + kSlop && maxK >= -volume.outwardDepth - kSlop;
	}

	AxialBox buildPortalVolumeWorldAxialBox(Transform const & door_w, PortalVolumeCut const & volume, float kSlop)
	{
		Vector const doorPos = door_w.getPosition_p();
		Vector const axisI = door_w.getLocalFrameI_p();
		Vector const axisJ = door_w.getLocalFrameJ_p();
		Vector const axisK = door_w.getLocalFrameK_p();

		AxialBox box;
		for (int si = 0; si <= 1; ++si)
		{
			float const i = si == 0 ? volume.minI : volume.maxI;
			for (int sj = 0; sj <= 1; ++sj)
			{
				float const j = sj == 0 ? volume.minJ : volume.maxJ;
				for (int sk = 0; sk <= 1; ++sk)
				{
					float const k = sk == 0 ? -volume.outwardDepth - kSlop : volume.inwardDepth + kSlop;
					box.add(doorPos + axisI * i + axisJ * j + axisK * k);
				}
			}
		}
		return box;
	}

	bool vertexInAxialBox(Vector const & v, AxialBox const & box)
	{
		Vector const bMin = box.getMin();
		Vector const bMax = box.getMax();
		return v.x >= bMin.x && v.x <= bMax.x
			&& v.y >= bMin.y && v.y <= bMax.y
			&& v.z >= bMin.z && v.z <= bMax.z;
	}

	bool triangleOverlapsAxialBox(Vector const & w0, Vector const & w1, Vector const & w2, AxialBox const & box)
	{
		if (vertexInAxialBox(w0, box) || vertexInAxialBox(w1, box) || vertexInAxialBox(w2, box))
			return true;

		AxialBox triBox;
		triBox.add(w0);
		triBox.add(w1);
		triBox.add(w2);
		return boxesOverlap(triBox, box);
	}

	void accumulatePortalVolumeBounds(
		Vector const & vertex_o,
		Transform const & door_o,
		float & minI,
		float & maxI,
		float & minJ,
		float & maxJ,
		float & minK,
		float & maxK)
	{
		Vector const delta = vertex_o - door_o.getPosition_p();
		float const i = delta.dot(door_o.getLocalFrameI_p());
		float const j = delta.dot(door_o.getLocalFrameJ_p());
		float const k = delta.dot(door_o.getLocalFrameK_p());

		minI = std::min(minI, i);
		maxI = std::max(maxI, i);
		minJ = std::min(minJ, j);
		maxJ = std::max(maxJ, j);
		minK = std::min(minK, k);
		maxK = std::max(maxK, k);
	}

	bool triangleOverlapsPortalVolumeWorld(
		Vector const & v0_local,
		Vector const & v1_local,
		Vector const & v2_local,
		Transform const & door_w,
		Transform const & meshToWorld,
		PortalVolumeCut const & volume)
	{
		Vector const w0 = meshToWorld.rotateTranslate_l2p(v0_local);
		Vector const w1 = meshToWorld.rotateTranslate_l2p(v1_local);
		Vector const w2 = meshToWorld.rotateTranslate_l2p(v2_local);

		if (vertexInPortalVolumeWorld(w0, door_w, volume)
			|| vertexInPortalVolumeWorld(w1, door_w, volume)
			|| vertexInPortalVolumeWorld(w2, door_w, volume))
			return true;

		float minI = volume.maxI;
		float maxI = volume.minI;
		float minJ = volume.maxJ;
		float maxJ = volume.minJ;
		float minK = volume.inwardDepth;
		float maxK = -volume.outwardDepth;

		accumulateDoorLocalBounds(w0, door_w, minI, maxI, minJ, maxJ, minK, maxK);
		accumulateDoorLocalBounds(w1, door_w, minI, maxI, minJ, maxJ, minK, maxK);
		accumulateDoorLocalBounds(w2, door_w, minI, maxI, minJ, maxJ, minK, maxK);

		float const kSlop = 2.50f;
		if (triangleIjOverlapOverlapsPortalDepthSlop(minI, maxI, minJ, maxJ, minK, maxK, volume, kSlop))
			return true;

		AxialBox const worldBox = buildPortalVolumeWorldAxialBox(door_w, volume, kSlop);
		return triangleOverlapsAxialBox(w0, w1, w2, worldBox);
	}

	bool shouldCutTriangleForPortalVolumeWorld(
		Vector const & v0_local,
		Vector const & v1_local,
		Vector const & v2_local,
		Transform const & door_w,
		Transform const & meshToWorld,
		PortalVolumeCut const & volume)
	{
		return triangleOverlapsPortalVolumeWorld(v0_local, v1_local, v2_local, door_w, meshToWorld, volume);
	}

	bool triangleOverlapsPortalVolume(
		Vector const & v0,
		Vector const & v1,
		Vector const & v2,
		Transform const & door_o,
		PortalVolumeCut const & volume)
	{
		if (vertexInPortalVolume(v0, door_o, volume)
			|| vertexInPortalVolume(v1, door_o, volume)
			|| vertexInPortalVolume(v2, door_o, volume))
			return true;

		float minI = volume.maxI;
		float maxI = volume.minI;
		float minJ = volume.maxJ;
		float maxJ = volume.minJ;
		float minK = volume.inwardDepth;
		float maxK = -volume.outwardDepth;

		accumulatePortalVolumeBounds(v0, door_o, minI, maxI, minJ, maxJ, minK, maxK);
		accumulatePortalVolumeBounds(v1, door_o, minI, maxI, minJ, maxJ, minK, maxK);
		accumulatePortalVolumeBounds(v2, door_o, minI, maxI, minJ, maxJ, minK, maxK);

		float const kSlop = 2.0f;
		return triangleIjOverlapOverlapsPortalDepthSlop(minI, maxI, minJ, maxJ, minK, maxK, volume, kSlop);
	}

	bool shouldCutTriangleForPortalVolume(
		Vector const & v0,
		Vector const & v1,
		Vector const & v2,
		Transform const & door_o,
		PortalVolumeCut const & volume,
		Vector const & /*wallNormal_o*/)
	{
		return triangleOverlapsPortalVolume(v0, v1, v2, door_o, volume);
	}

	AxialBox buildPortalVolumeAxialBox(Transform const & door_o, PortalVolumeCut const & volume)
	{
		Vector const doorPos = door_o.getPosition_p();
		Vector const axisI = door_o.getLocalFrameI_p();
		Vector const axisJ = door_o.getLocalFrameJ_p();
		Vector const axisK = door_o.getLocalFrameK_p();

		AxialBox box;
		for (int si = 0; si <= 1; ++si)
		{
			float const i = si == 0 ? volume.minI : volume.maxI;
			for (int sj = 0; sj <= 1; ++sj)
			{
				float const j = sj == 0 ? volume.minJ : volume.maxJ;
				for (int sk = 0; sk <= 1; ++sk)
				{
					float const k = sk == 0 ? -volume.outwardDepth : volume.inwardDepth;
					box.add(doorPos + axisI * i + axisJ * j + axisK * k);
				}
			}
		}
		return box;
	}

	bool vertexInDoorOpening(
		float i,
		float j,
		float k,
		PortalVolumeCut const & volume,
		float planeSlop,
		float ijSlop)
	{
		return i >= volume.minI - ijSlop && i <= volume.maxI + ijSlop
			&& j >= volume.minJ - ijSlop && j <= volume.maxJ + ijSlop
			&& fabsf(k) <= planeSlop;
	}

	bool shouldCutTriangleAggressiveForPortalVolume(
		Vector const & v0,
		Vector const & v1,
		Vector const & v2,
		Transform const & door_o,
		PortalVolumeCut const & volume)
	{
		float i0 = 0.0f, j0 = 0.0f, k0 = 0.0f;
		float i1 = 0.0f, j1 = 0.0f, k1 = 0.0f;
		float i2 = 0.0f, j2 = 0.0f, k2 = 0.0f;
		computeVertexDoorLocal(v0, door_o, i0, j0, k0);
		computeVertexDoorLocal(v1, door_o, i1, j1, k1);
		computeVertexDoorLocal(v2, door_o, i2, j2, k2);

		float const planeSlop = 0.50f;
		float const ijSlop = 0.15f;

		if (vertexInDoorOpening(i0, j0, k0, volume, planeSlop, ijSlop)
			|| vertexInDoorOpening(i1, j1, k1, volume, planeSlop, ijSlop)
			|| vertexInDoorOpening(i2, j2, k2, volume, planeSlop, ijSlop))
			return true;

		float minI = std::min(i0, std::min(i1, i2));
		float maxI = std::max(i0, std::max(i1, i2));
		float minJ = std::min(j0, std::min(j1, j2));
		float maxJ = std::max(j0, std::max(j1, j2));
		float minK = std::min(k0, std::min(k1, k2));
		float maxK = std::max(k0, std::max(k1, k2));

		if (minK <= planeSlop && maxK >= -planeSlop
			&& minI <= volume.maxI + ijSlop && maxI >= volume.minI - ijSlop
			&& minJ <= volume.maxJ + ijSlop && maxJ >= volume.minJ - ijSlop)
			return true;

		if (shouldCutTriangleForPortalVolume(v0, v1, v2, door_o, volume, Vector::zero))
			return true;

		PortalVolumeCut expanded = volume;
		expanded.minI -= 0.25f;
		expanded.maxI += 0.25f;
		expanded.minJ -= 0.20f;
		expanded.maxJ += 0.20f;
		expanded.inwardDepth += 0.75f;
		expanded.outwardDepth += 0.75f;

		if (shouldCutTriangleForPortalVolume(v0, v1, v2, door_o, expanded, Vector::zero))
			return true;

		AxialBox const box = buildPortalVolumeAxialBox(door_o, expanded);
		if (vertexInAxialBox(v0, box) || vertexInAxialBox(v1, box) || vertexInAxialBox(v2, box))
			return true;

		AxialBox triBox;
		triBox.add(v0);
		triBox.add(v1);
		triBox.add(v2);
		return boxesOverlap(triBox, box);
	}

	bool shouldCutTriangleForPortalWall(
		Vector const & v0,
		Vector const & v1,
		Vector const & v2,
		Transform const & door_o,
		PortalVolumeCut const & volume)
	{
		if (vertexInPortalVolume(v0, door_o, volume)
			|| vertexInPortalVolume(v1, door_o, volume)
			|| vertexInPortalVolume(v2, door_o, volume))
			return true;

		float i0 = 0.0f, j0 = 0.0f, k0 = 0.0f;
		float i1 = 0.0f, j1 = 0.0f, k1 = 0.0f;
		float i2 = 0.0f, j2 = 0.0f, k2 = 0.0f;
		computeVertexDoorLocal(v0, door_o, i0, j0, k0);
		computeVertexDoorLocal(v1, door_o, i1, j1, k1);
		computeVertexDoorLocal(v2, door_o, i2, j2, k2);

		float const planeSlop = 0.35f;
		float const ijSlop = 0.12f;

		if (vertexInDoorOpening(i0, j0, k0, volume, planeSlop, ijSlop)
			|| vertexInDoorOpening(i1, j1, k1, volume, planeSlop, ijSlop)
			|| vertexInDoorOpening(i2, j2, k2, volume, planeSlop, ijSlop))
			return true;

		float const minI = std::min(i0, std::min(i1, i2));
		float const maxI = std::max(i0, std::max(i1, i2));
		float const minJ = std::min(j0, std::min(j1, j2));
		float const maxJ = std::max(j0, std::max(j1, j2));
		float const minK = std::min(k0, std::min(k1, k2));
		float const maxK = std::max(k0, std::max(k1, k2));

		return minK <= planeSlop && maxK >= -planeSlop
			&& minI <= volume.maxI + ijSlop && maxI >= volume.minI - ijSlop
			&& minJ <= volume.maxJ + ijSlop && maxJ >= volume.minJ - ijSlop;
	}
}

namespace
{
	int applyPortalTriangleCutsToMesh(
		MeshAppearance & meshApp,
		ShaderPrimitiveSet * shaderPrimitiveSet,
		Transform const & meshToWorld,
		Vector const & meshScale,
		std::vector<MeshLocalPortalCut> const & meshLocalCuts,
		std::vector<StaticIndexBuffer *> & portalCutIndexBuffers)
	{
		UNREF(meshApp);
		UNREF(meshScale);
		if (!shaderPrimitiveSet || meshLocalCuts.empty())
			return 0;

		int trianglesRemoved = 0;
		int const shaderCount = shaderPrimitiveSet->getNumberOfShaders();

		for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
		{
			StaticVertexBuffer const * const vb = shaderPrimitiveSet->getVertexBuffer(shaderIndex);
			StaticIndexBuffer const * const ib = shaderPrimitiveSet->getIndexBuffer(shaderIndex);
			if (!vb || !ib)
				continue;

			int const indexCount = ib->getNumberOfIndices();
			if (indexCount < 3)
				continue;

			std::vector<Index> keptIndices;
			keptIndices.reserve(static_cast<size_t>(indexCount));

			vb->lockReadOnly();
			ib->lockReadOnly();
			{
				VertexBufferReadIterator vit = vb->beginReadOnly();
				Index const * indices = ib->beginReadOnly();
				for (int tri = 0; tri < indexCount; tri += 3)
				{
					Index const i0 = indices[tri];
					Index const i1 = indices[tri + 1];
					Index const i2 = indices[tri + 2];

					VertexBufferReadIterator it0 = vit;
					it0 += i0;
					VertexBufferReadIterator it1 = vit;
					it1 += i1;
					VertexBufferReadIterator it2 = vit;
					it2 += i2;

					Vector const v0 = it0.getPosition();
					Vector const v1 = it1.getPosition();
					Vector const v2 = it2.getPosition();

					bool cutTriangle = false;
					for (size_t cutIndex = 0; cutIndex < meshLocalCuts.size(); ++cutIndex)
					{
						MeshLocalPortalCut const & cut = meshLocalCuts[cutIndex];
						if (cut.hasDoorWorld
							&& MeshAppearanceNamespace::shouldCutTriangleForPortalVolumeWorld(
								v0, v1, v2, cut.doorWorld, meshToWorld, cut.volume))
						{
							cutTriangle = true;
							break;
						}

						if (MeshAppearanceNamespace::shouldCutTriangleForPortalWall(
								v0, v1, v2, cut.doorMesh, cut.volume))
						{
							cutTriangle = true;
							break;
						}

						if (MeshAppearanceNamespace::shouldCutTriangleAggressiveForPortalVolume(
								v0, v1, v2, cut.doorMesh, cut.volume))
						{
							cutTriangle = true;
							break;
						}
					}

					if (cutTriangle)
						continue;

					keptIndices.push_back(i0);
					keptIndices.push_back(i1);
					keptIndices.push_back(i2);
				}
			}
			ib->unlock();
			vb->unlock();

			if (static_cast<int>(keptIndices.size()) == indexCount)
				continue;

			if (keptIndices.empty())
				continue;

			int const removedCount = indexCount - static_cast<int>(keptIndices.size());
			trianglesRemoved += removedCount;

			StaticIndexBuffer * const cutIb = new StaticIndexBuffer(static_cast<int>(keptIndices.size()));
			cutIb->lock();
			{
				Index * out = cutIb->begin();
				for (size_t ki = 0; ki < keptIndices.size(); ++ki)
					*(out++) = keptIndices[ki];
			}
			cutIb->unlock();

			portalCutIndexBuffers[static_cast<size_t>(shaderIndex)] = cutIb;
			shaderPrimitiveSet->setRuntimeIndexBufferOverride(shaderIndex, cutIb);
		}

		return trianglesRemoved;
	}

	int applyPortalCutBoxFallbacksToMesh(
		MeshAppearance & meshApp,
		Transform const & meshToWorld,
		std::vector<MeshLocalPortalCut> const & meshLocalCuts)
	{
		if (meshLocalCuts.empty())
			return 0;

		for (size_t cutIndex = 0; cutIndex < meshLocalCuts.size(); ++cutIndex)
		{
			MeshLocalPortalCut const & cut = meshLocalCuts[cutIndex];
			if (!cut.hasDoorWorld)
				continue;

			PortalVolumeCut expanded = cut.volume;
			expanded.minI -= 0.35f;
			expanded.maxI += 0.35f;
			expanded.minJ -= 0.25f;
			expanded.maxJ += 0.25f;
			expanded.inwardDepth += 2.0f;
			expanded.outwardDepth += 2.0f;

			if (meshApp.hideShaderPrimitivesInPortalVolumeWorld(cut.doorWorld, meshToWorld, expanded))
				return 1;
		}

		return 0;
	}

	void syncMeshPortalCutTransform(MeshAppearance & meshApp)
	{
		Object const * const meshOwner = meshApp.getOwner();
		if (!meshOwner)
			return;

		meshApp.setTransform_w(meshOwner->getTransform_o2w());
	}
}

// ----------------------------------------------------------------------

void MeshAppearance::addPendingPortalCut(Transform const & doorWorld, PortalVolumeCut const & volume)
{
	PendingPortalCut cut;
	cut.doorWorld = doorWorld;
	cut.volume = volume;
	m_pendingPortalCuts.push_back(cut);
}

// ----------------------------------------------------------------------

void MeshAppearance::clearPendingPortalCuts()
{
	m_pendingPortalCuts.clear();
	clearPortalCutIndexBuffers();
	m_hiddenShaderPrimitiveMask.clear();
}

// ----------------------------------------------------------------------

bool MeshAppearance::hasPendingPortalCuts() const
{
	return !m_pendingPortalCuts.empty();
}

// ----------------------------------------------------------------------

bool MeshAppearance::hasAnyHiddenShaderPrimitive() const
{
	for (size_t i = 0; i < m_hiddenShaderPrimitiveMask.size(); ++i)
		if (m_hiddenShaderPrimitiveMask[i])
			return true;
	return false;
}

// ----------------------------------------------------------------------

void MeshAppearance::applyPendingPortalCuts()
{
	if (!m_shaderPrimitiveSet)
		return;

	//-- render() calls this for every mesh every frame: with no cut queued on this mesh and none registered for
	//   any building there is nothing to gather, so skip the transform inversion and owner/cell walk below
	if (m_pendingPortalCuts.empty() && s_buildingCellPortalCuts.empty())
		return;

	syncMeshPortalCutTransform(*this);

	Transform const meshToWorld = getTransform_w();
	Vector meshScale(1.0f, 1.0f, 1.0f);
	Object const * const meshOwner = getOwner();
	if (meshOwner)
		meshScale = meshOwner->getScale();
	else
		meshScale = getScale();

	std::vector<MeshLocalPortalCut> meshLocalCuts;
	gatherMeshLocalPortalCuts(meshOwner, meshToWorld, m_pendingPortalCuts, meshLocalCuts);
	if (meshLocalCuts.empty())
		return;

	clearPortalCutIndexBuffers();

	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);
	m_portalCutIndexBuffers.assign(static_cast<size_t>(shaderCount), static_cast<StaticIndexBuffer *>(0));

	int trianglesRemoved = applyPortalTriangleCutsToMesh(
		*this, m_shaderPrimitiveSet, meshToWorld, meshScale, meshLocalCuts, m_portalCutIndexBuffers);

	if (trianglesRemoved <= 0)
	{
		for (size_t cutIndex = 0; cutIndex < meshLocalCuts.size(); ++cutIndex)
		{
			MeshLocalPortalCut const & cut = meshLocalCuts[cutIndex];
			if (!cut.hasDoorWorld)
				continue;
			if (hideShaderPrimitivesInPortalVolumeWorld(cut.doorWorld, meshToWorld, cut.volume))
				trianglesRemoved = 1;
		}

		if (trianglesRemoved <= 0)
			trianglesRemoved = applyPortalCutBoxFallbacksToMesh(*this, meshToWorld, meshLocalCuts);
	}

	UNREF(trianglesRemoved);
}

// ----------------------------------------------------------------------

int MeshAppearance::applyPortalVolumeCut(Transform const & doorWorld, PortalVolumeCut const & volume)
{
	if (!m_shaderPrimitiveSet)
		return 0;

	syncMeshPortalCutTransform(*this);

	Transform const meshToWorld = getTransform_w();
	Vector meshScale(1.0f, 1.0f, 1.0f);
	Object const * const meshOwner = getOwner();
	if (meshOwner)
		meshScale = meshOwner->getScale();
	else
		meshScale = getScale();

	clearPendingPortalCuts();
	addPendingPortalCut(doorWorld, volume);

	std::vector<MeshLocalPortalCut> meshLocalCuts;
	gatherMeshLocalPortalCuts(meshOwner, meshToWorld, m_pendingPortalCuts, meshLocalCuts);
	if (meshLocalCuts.empty())
		return 0;

	clearPortalCutIndexBuffers();

	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);
	m_portalCutIndexBuffers.assign(static_cast<size_t>(shaderCount), static_cast<StaticIndexBuffer *>(0));

	int trianglesRemoved = applyPortalTriangleCutsToMesh(
		*this, m_shaderPrimitiveSet, meshToWorld, meshScale, meshLocalCuts, m_portalCutIndexBuffers);

	if (trianglesRemoved <= 0)
	{
		for (size_t cutIndex = 0; cutIndex < meshLocalCuts.size(); ++cutIndex)
		{
			MeshLocalPortalCut const & cut = meshLocalCuts[cutIndex];
			if (!cut.hasDoorWorld)
				continue;
			if (hideShaderPrimitivesInPortalVolumeWorld(cut.doorWorld, meshToWorld, cut.volume))
				trianglesRemoved = 1;
		}
	}

	return trianglesRemoved;
}

// ----------------------------------------------------------------------

namespace
{
	bool shaderPrimitiveIsRoomScale(
		StaticVertexBuffer const * vb,
		Transform const & doorMesh,
		PortalVolumeCut const & volume)
	{
		if (!vb)
			return true;

		AxialBox const portalBox = MeshAppearanceNamespace::buildPortalVolumeAxialBox(doorMesh, volume);
		Vector const portalSize = portalBox.getMax() - portalBox.getMin();

		AxialBox primitiveBox;
		int const nv = vb->getNumberOfVertices();
		vb->lockReadOnly();
		{
			VertexBufferReadIterator it = vb->beginReadOnly();
			for (int v = 0; v < nv; ++v, ++it)
				primitiveBox.add(it.getPosition());
		}
		vb->unlock();

		Vector const primitiveSize = primitiveBox.getMax() - primitiveBox.getMin();
		float const maxPortalHoriz = std::max(portalSize.x, portalSize.y);
		float const maxPrimHoriz = std::max(primitiveSize.x, std::max(primitiveSize.y, primitiveSize.z));

		return maxPrimHoriz > maxPortalHoriz * 2.5f;
	}
}

int MeshAppearance::applyPortalVolumeBoxFallback(
	Transform const & doorMesh,
	PortalVolumeCut const & volume)
{
	if (!m_shaderPrimitiveSet)
		return 0;

	int hiddenCount = 0;
	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	if (static_cast<int>(m_hiddenShaderPrimitiveMask.size()) != shaderCount)
		m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);

	for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		if (m_hiddenShaderPrimitiveMask[static_cast<size_t>(shaderIndex)])
			continue;

		StaticVertexBuffer const * const vb = m_shaderPrimitiveSet->getVertexBuffer(shaderIndex);
		if (!vb || shaderPrimitiveIsRoomScale(vb, doorMesh, volume))
			continue;

		int insideCount = 0;
		int const nv = vb->getNumberOfVertices();
		vb->lockReadOnly();
		{
			VertexBufferReadIterator it = vb->beginReadOnly();
			for (int v = 0; v < nv; ++v, ++it)
			{
				if (MeshAppearanceNamespace::vertexInPortalVolume(it.getPosition(), doorMesh, volume))
					++insideCount;
			}
		}
		vb->unlock();

		if (insideCount <= 0)
			continue;

		m_hiddenShaderPrimitiveMask[static_cast<size_t>(shaderIndex)] = true;
		++hiddenCount;
	}

	return hiddenCount;
}

// ----------------------------------------------------------------------

void MeshAppearance::hideShaderPrimitivesInBox(AxialBox const & box_o)
{
	if (!m_shaderPrimitiveSet)
		return;

	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	if (static_cast<int>(m_hiddenShaderPrimitiveMask.size()) != shaderCount)
		m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);

	for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		if (m_hiddenShaderPrimitiveMask[static_cast<size_t>(shaderIndex)])
			continue;

		StaticVertexBuffer const * const vb = m_shaderPrimitiveSet->getVertexBuffer(shaderIndex);
		if (!vb)
			continue;

		int const nv = vb->getNumberOfVertices();
		if (nv <= 0)
			continue;

		AxialBox primitiveBox;
		vb->lockReadOnly();
		{
			VertexBufferReadIterator it = vb->beginReadOnly();
			for (int v = 0; v < nv; ++v, ++it)
				primitiveBox.add(it.getPosition());
		}
		vb->unlock();

		if (boxesOverlap(box_o, primitiveBox))
			m_hiddenShaderPrimitiveMask[static_cast<size_t>(shaderIndex)] = true;
	}
}

// ----------------------------------------------------------------------

bool MeshAppearance::hideShaderPrimitivesInPortalVolumeWorld(Transform const & door_w, Transform const & meshToWorld, PortalVolumeCut const & volume)
{
	if (!m_shaderPrimitiveSet)
		return false;

	clearPortalCutIndexBuffers();

	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);
	m_portalCutIndexBuffers.assign(static_cast<size_t>(shaderCount), static_cast<StaticIndexBuffer *>(0));

	bool anyIndexBufferOverride = false;

	for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		StaticVertexBuffer const * const vb = m_shaderPrimitiveSet->getVertexBuffer(shaderIndex);
		StaticIndexBuffer const * const ib = m_shaderPrimitiveSet->getIndexBuffer(shaderIndex);
		if (!vb || !ib)
			continue;

		int const indexCount = ib->getNumberOfIndices();
		if (indexCount < 3)
			continue;

		std::vector<Index> keptIndices;
		keptIndices.reserve(static_cast<size_t>(indexCount));

		vb->lockReadOnly();
		ib->lockReadOnly();
		{
			VertexBufferReadIterator vit = vb->beginReadOnly();
			Index const * indices = ib->beginReadOnly();
			for (int tri = 0; tri < indexCount; tri += 3)
			{
				Index const i0 = indices[tri];
				Index const i1 = indices[tri + 1];
				Index const i2 = indices[tri + 2];

				VertexBufferReadIterator it0 = vit;
				it0 += i0;
				VertexBufferReadIterator it1 = vit;
				it1 += i1;
				VertexBufferReadIterator it2 = vit;
				it2 += i2;

				Vector const v0 = it0.getPosition();
				Vector const v1 = it1.getPosition();
				Vector const v2 = it2.getPosition();

				if (MeshAppearanceNamespace::shouldCutTriangleForPortalVolumeWorld(v0, v1, v2, door_w, meshToWorld, volume))
					continue;

				keptIndices.push_back(i0);
				keptIndices.push_back(i1);
				keptIndices.push_back(i2);
			}
		}
		ib->unlock();
		vb->unlock();

		if (static_cast<int>(keptIndices.size()) == indexCount)
			continue;

		if (keptIndices.empty())
			continue;

		StaticIndexBuffer * const cutIb = new StaticIndexBuffer(static_cast<int>(keptIndices.size()));
		cutIb->lock();
		{
			Index * out = cutIb->begin();
			for (size_t ki = 0; ki < keptIndices.size(); ++ki)
				*(out++) = keptIndices[ki];
		}
		cutIb->unlock();

		m_portalCutIndexBuffers[static_cast<size_t>(shaderIndex)] = cutIb;
		m_shaderPrimitiveSet->setRuntimeIndexBufferOverride(shaderIndex, cutIb);
		anyIndexBufferOverride = true;
	}

	return anyIndexBufferOverride;
}

// ----------------------------------------------------------------------

bool MeshAppearance::hideShaderPrimitivesInPortalVolume(Transform const & door_o, PortalVolumeCut const & volume)
{
	if (!m_shaderPrimitiveSet)
		return false;

	clearPortalCutIndexBuffers();

	int const shaderCount = m_shaderPrimitiveSet->getNumberOfShaders();
	m_hiddenShaderPrimitiveMask.assign(static_cast<size_t>(shaderCount), false);
	m_portalCutIndexBuffers.assign(static_cast<size_t>(shaderCount), static_cast<StaticIndexBuffer *>(0));

	bool anyTriangleCut = false;
	Vector const wallNormal_o = door_o.getLocalFrameK_p();

	for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
	{
		StaticVertexBuffer const * const vb = m_shaderPrimitiveSet->getVertexBuffer(shaderIndex);
		StaticIndexBuffer const * const ib = m_shaderPrimitiveSet->getIndexBuffer(shaderIndex);
		if (!vb || !ib)
			continue;

		int const indexCount = ib->getNumberOfIndices();
		if (indexCount < 3)
			continue;

		std::vector<Index> keptIndices;
		keptIndices.reserve(static_cast<size_t>(indexCount));

		vb->lockReadOnly();
		ib->lockReadOnly();
		{
			VertexBufferReadIterator vit = vb->beginReadOnly();
			Index const * indices = ib->beginReadOnly();
			for (int tri = 0; tri < indexCount; tri += 3)
			{
				Index const i0 = indices[tri];
				Index const i1 = indices[tri + 1];
				Index const i2 = indices[tri + 2];

				VertexBufferReadIterator it0 = vit;
				it0 += i0;
				VertexBufferReadIterator it1 = vit;
				it1 += i1;
				VertexBufferReadIterator it2 = vit;
				it2 += i2;

				Vector const v0 = it0.getPosition();
				Vector const v1 = it1.getPosition();
				Vector const v2 = it2.getPosition();

				if (MeshAppearanceNamespace::shouldCutTriangleForPortalVolume(v0, v1, v2, door_o, volume, wallNormal_o))
				{
					anyTriangleCut = true;
					continue;
				}

				keptIndices.push_back(i0);
				keptIndices.push_back(i1);
				keptIndices.push_back(i2);
			}
		}
		ib->unlock();
		vb->unlock();

		if (static_cast<int>(keptIndices.size()) == indexCount)
			continue;

		if (keptIndices.empty())
			continue;

		anyTriangleCut = true;

		StaticIndexBuffer * const cutIb = new StaticIndexBuffer(static_cast<int>(keptIndices.size()));
		cutIb->lock();
		{
			Index * out = cutIb->begin();
			for (size_t ki = 0; ki < keptIndices.size(); ++ki)
				*(out++) = keptIndices[ki];
		}
		cutIb->unlock();

		m_portalCutIndexBuffers[static_cast<size_t>(shaderIndex)] = cutIb;
		m_shaderPrimitiveSet->setRuntimeIndexBufferOverride(shaderIndex, cutIb);
	}

	return anyTriangleCut;
}

// ======================================================================

void MeshAppearance::drawDebugShapes (DebugShapeRenderer* renderer) const
{
	if (!renderer)
		return;

#ifdef _DEBUG
	Appearance::drawDebugShapes (renderer);
#endif
}

// ======================================================================

#ifdef _DEBUG

int MeshAppearance::getPolygonCount () const
{
	int total = 0;

	for (int i = 0; i < m_shaderPrimitiveSet->getNumberOfShaders(); ++i)
		total += m_shaderPrimitiveSet->getIndexBuffer (i)->getNumberOfIndices () / 3;

	return total;
}

static void indent (std::string &result, const int indentLevel)
{
	int i;
	for (i = 0; i < indentLevel; ++i)
		result += "  ";
}

void MeshAppearance::debugDump(std::string &result, const int indentLevel) const
{
	char buffer [1024];

	sprintf (buffer, "MeshAppearance %s\r\n", getAppearanceTemplate ()->getName ());
	indent (result, indentLevel);
	result += buffer;

	sprintf (buffer, "%i shader primitives\r\n", m_shaderPrimitiveSet->getNumberOfShaders ());
	indent (result, indentLevel);
	result += buffer;

	int i;
	for (i = 0; i < m_shaderPrimitiveSet->getNumberOfShaders(); ++i)
	{
		const ShaderTemplate* const shaderTemplate = m_shaderPrimitiveSet->getShaderTemplate(i);

		sprintf (buffer, "shader %s\r\n", shaderTemplate->getName ().getString ());
		indent (result, indentLevel + 1);
		result += buffer;

		const StaticShaderTemplate* const staticShaderTemplate = dynamic_cast<const StaticShaderTemplate *>(shaderTemplate);

		sprintf (buffer, "isStatic=%s\r\n", staticShaderTemplate ? "yes" : "no");
		indent (result, indentLevel + 2);
		result += buffer;

		sprintf (buffer, "isOpaque=%s\r\n", shaderTemplate->isOpaqueSolid () ? "yes" : "no");
		indent (result, indentLevel + 2);
		result += buffer;

		sprintf (buffer, "isCollidable=%s\r\n", shaderTemplate->isCollidable () ? "yes" : "no");
		indent (result, indentLevel + 2);
		result += buffer;

		if (staticShaderTemplate)
		{
			sprintf (buffer, "numberOfPasses=%i\r\n", static_cast<const StaticShader*> (m_shaderPrimitiveSet->getShader (i))->getNumberOfPasses ());
			indent (result, indentLevel + 2);
			result += buffer;
		}

		const Shader* const shader = m_shaderPrimitiveSet->getShader (i);
		const StaticShader* const staticShader = shader->getStaticShader ();
		if (staticShader)
		{
			typedef std::vector<Tag> TagList;
			TagList tagList;

			staticShader->getTextureTags (tagList);
			TagList::const_iterator end = tagList.end ();
			for (TagList::const_iterator iter = tagList.begin (); iter != end; ++iter)
			{
				const Texture* texture = 0;
				if (staticShader->getTexture (*iter, texture) && texture)
				{
					sprintf (buffer, "texture=%s  %i x %i\r\n", texture->getName (), texture->getWidth (), texture->getHeight ());
					indent (result, indentLevel + 2);
					result += buffer;
				}
			}
		}

		const StaticVertexBuffer* const vertexBuffer = m_shaderPrimitiveSet->getVertexBuffer(i);
		const StaticIndexBuffer* const indexBuffer  = m_shaderPrimitiveSet->getIndexBuffer(i);

		sprintf (buffer, "%i vertices, %i triangles\r\n", vertexBuffer->getNumberOfVertices (), indexBuffer->getNumberOfIndices () / 3);
		indent (result, indentLevel + 2);
		result += buffer;
	}
}

#endif

// ======================================================================

