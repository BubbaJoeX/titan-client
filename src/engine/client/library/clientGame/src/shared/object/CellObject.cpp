// ======================================================================
//
// CellObject.cpp
// Copyright 2001 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/CellObject.h"

#include "clientGame/ClientObjectTemplate.h"
#include "clientGame/ConfigClientGame.h"
#include "clientGame/ShipObject.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/RenderWorld.h"
#include "sharedMath/VectorArgb.h"
#include "clientObject/InteriorEnvironmentBlock.h"
#include "clientObject/InteriorEnvironmentBlockManager.h"
#include "sharedCollision/Floor.h"
#include "sharedCollision/FloorMesh.h"
#include "sharedCollision/FloorTri.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedFoundation/DebugInfoManager.h"
#include "sharedFoundation/Production.h"
#include "sharedMath/IndexedTriangleList.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/ContainedByProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/Portal.h"
#include "sharedObject/PortalProperty.h"
#include "sharedObject/PortalPropertyTemplate.h"
#include "sharedObject/PortalPropertyTemplateList.h"

#include <algorithm>
#include <set>

// ======================================================================

namespace CellObjectNamespace
{
	Light *createLight(const PortalPropertyTemplateCellLight &lightData);
	void swapBuildingToDarkPob(CellProperty * cellProperty);

	std::set<const PortalProperty *> ms_swappedBuildings;
	std::string const & ms_debugInfoSectionName = "CellObject";
}
using namespace CellObjectNamespace;

// ======================================================================

void CellObject::Callbacks::PublicChanged::modified(CellObject & /*target*/, const bool & /*oldValue*/, const bool & /*newValue*/, bool /*local*/)
{
}

// ======================================================================

CellObject::CellObject(const SharedObjectTemplate *newTemplate)
: ClientObject(newTemplate, ClientWorld::getIntangibleNotification()),
	m_isPublic(true),
	m_cellNumber(-1),
	m_cellLabel(),
	m_labelLocationOffset(),
	m_accessAllowed(true),
	m_radarShape (0),
	m_radarEdges (0),
	m_radarPortalEdges (0),
	m_cellLights(),
	m_hasCustomLighting(false)
{
	addSharedVariable(m_isPublic);
	addSharedVariable(m_cellNumber);
	addSharedVariable_np(m_cellLabel);
	addSharedVariable_np(m_labelLocationOffset);

	m_isPublic.setSourceObject(this);

	setDebugName("PortalProperty cell object");
	RenderWorld::addCellNotifications(*this);

	CellProperty *cellProperty = new CellProperty(*this);
	addProperty(*cellProperty);
}

// ----------------------------------------------------------------------

CellObject::~CellObject()
{
	//-- This must be the first line in the destructor to invalidate any watchers watching this object
	nullWatchers();

	delete m_radarShape;
	m_radarShape = 0;

	delete m_radarEdges;
	m_radarEdges = 0;
}

// ----------------------------------------------------------------------

Light *CellObjectNamespace::createLight(const PortalPropertyTemplateCellLight &lightData)
{
	Light *light = NULL;

	switch (lightData.type)
	{
		case PortalPropertyTemplateCellLight::T_ambient:
			light = new Light(Light::T_ambient, lightData.diffuseColor);
			break;

		case PortalPropertyTemplateCellLight::T_parallel:
			light = new Light(Light::T_parallel, lightData.diffuseColor);
			light->setTransform_o2p(lightData.transform);
			light->setSpecularColor(lightData.specularColor);
			break;

		case PortalPropertyTemplateCellLight::T_point:
			light = new Light(Light::T_point, lightData.diffuseColor);
			light->setTransform_o2p(lightData.transform);
			light->setSpecularColor(lightData.specularColor);
			light->setConstantAttenuation(lightData.constantAttenuation);
			light->setLinearAttenuation(lightData.linearAttenuation);
			light->setQuadraticAttenuation(lightData.quadraticAttenuation);
			break;

		default:
			DEBUG_FATAL(true, ("unknown light type"));
	}

	return light;
}

// ----------------------------------------------------------------------

void CellObjectNamespace::swapBuildingToDarkPob(CellProperty * cellProperty)
{
	if (!cellProperty)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: cellProperty is NULL\n"));
		return;
	}

	const PortalProperty * portalProperty = cellProperty->getPortalProperty();
	if (!portalProperty)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: portalProperty is NULL\n"));
		return;
	}

	if (ms_swappedBuildings.find(portalProperty) != ms_swappedBuildings.end())
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: building already swapped, skipping\n"));
		return;
	}

	const char * pobName = portalProperty->getPobName();
	if (!pobName || !*pobName)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: pobName is NULL or empty\n"));
		return;
	}

	REPORT_LOG(true, ("swapBuildingToDarkPob: pobName = [%s]\n", pobName));

	std::string pobStr(pobName);
	const std::string::size_type dotPos = pobStr.rfind(".pob");
	if (dotPos == std::string::npos)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: no .pob extension found\n"));
		return;
	}

	if (pobStr.find("_dark.pob") != std::string::npos)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: already a _dark pob\n"));
		return;
	}

	std::string darkPobStr = pobStr.substr(0, dotPos) + "_dark.pob";
	REPORT_LOG(true, ("swapBuildingToDarkPob: looking for [%s]\n", darkPobStr.c_str()));

	if (!TreeFile::exists(darkPobStr.c_str()))
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: _dark.pob does NOT exist in TreeFile; using dynamic interior lighting (no baked pob swap)\n"));
		// Same bookkeeping as a successful swap: avoid retrying per-cell custom-light initialization on this portal.
		ms_swappedBuildings.insert(portalProperty);
		return;
	}

	REPORT_LOG(true, ("swapBuildingToDarkPob: _dark.pob EXISTS, fetching template\n"));

	const PortalPropertyTemplate * darkTemplate = PortalPropertyTemplateList::fetch(CrcLowerString(darkPobStr.c_str()));
	if (!darkTemplate)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: failed to fetch dark template\n"));
		return;
	}

	PortalProperty * mutablePortalProperty = const_cast<PortalProperty *>(portalProperty);
	Object const & portalOwner = mutablePortalProperty->getOwner();
	ClientObject const * const portalOwnerClient = portalOwner.asClientObject();
	bool const portalOwnerIsShip = portalOwnerClient && portalOwnerClient->asShipObject();

	const int numCells = mutablePortalProperty->getNumberOfCells();
	const int darkNumCells = darkTemplate->getNumberOfCells();
	REPORT_LOG(true, ("swapBuildingToDarkPob: numCells=%d darkNumCells=%d\n", numCells, darkNumCells));

	for (int i = 1; i < numCells && i < darkNumCells; ++i)
	{
		CellProperty * cell = mutablePortalProperty->getCell(i);
		if (!cell)
		{
			REPORT_LOG(true, ("swapBuildingToDarkPob: cell %d is NULL\n", i));
			continue;
		}

		Object * appearanceObj = cell->getAppearanceObject();
		if (!appearanceObj)
		{
			REPORT_LOG(true, ("swapBuildingToDarkPob: cell %d appearanceObj is NULL\n", i));
			continue;
		}

		const char * darkAppName = darkTemplate->getCell(i).getAppearanceName();
		if (darkAppName && *darkAppName)
		{
			REPORT_LOG(true, ("swapBuildingToDarkPob: cell %d swapping to [%s]\n", i, darkAppName));
			Appearance * const darkAppearance = AppearanceTemplateList::createAppearance(darkAppName);
			if (darkAppearance)
			{
				darkAppearance->setShadowBlobAllowed();
				appearanceObj->setAppearance(darkAppearance);
			}
			else
			{
				REPORT_LOG(true, ("swapBuildingToDarkPob: cell %d failed to create appearance [%s]\n", i, darkAppName));
			}
		}
	}

	// Housing structures swap exterior mesh at lights-off (dark pob). Ship hull appearance must stay the flyable mesh.
	const char * darkExteriorName = darkTemplate->getExteriorAppearanceName();
	if (!portalOwnerIsShip && darkExteriorName && *darkExteriorName)
	{
		REPORT_LOG(true, ("swapBuildingToDarkPob: swapping exterior to [%s]\n", darkExteriorName));
		Object & buildingObj = mutablePortalProperty->getOwner();
		Appearance * const darkExterior = AppearanceTemplateList::createAppearance(darkExteriorName);
		if (darkExterior)
		{
			darkExterior->setShadowBlobAllowed();
			buildingObj.setAppearance(darkExterior);
		}
	}

	ms_swappedBuildings.insert(portalProperty);

	darkTemplate->release();
	REPORT_LOG(true, ("swapBuildingToDarkPob: COMPLETE\n"));
}

// ----------------------------------------------------------------------

void CellObject::endBaselines()
{
	ClientObject::endBaselines();

	DEBUG_FATAL(m_cellNumber.get() < 1, ("Invalid cell number"));
	ContainedByProperty *containedByProperty = getContainedByProperty();
	NetworkId networkId = containedByProperty->getContainedByNetworkId();
	Object *container = containedByProperty->getContainedBy();
	FATAL(container == NULL, ("CellObject container is NULL, cell=%s, container=%s", getNetworkId().getValueString().c_str(), networkId.getValueString().c_str()));
	PortalProperty *portalProperty = container->getPortalProperty();
	FATAL(portalProperty == NULL, ("CellObject's container's portal property is NULL"));

	// Grafted dynamic-bunker cells may arrive before the graft descriptor message.
	// Skip load here; DynamicBunkerClient will call cellLoaded once the donor POB is registered.
	if (m_cellNumber.get() >= portalProperty->getBaseTemplateCellCount() && !portalProperty->isGraftedCell(m_cellNumber.get()))
	{
		WARNING(true, ("CellObject::endBaselines - deferring grafted cell %d until DynamicBunker graft arrives", m_cellNumber.get()));
		return;
	}

	if (!portalProperty->getCell(m_cellNumber.get()))
		portalProperty->cellLoaded(m_cellNumber.get(), *this, true);

	if (ConfigClientGame::getAmbientLightInCells())
	{
		Light *light = new Light(Light::T_ambient, VectorArgb::solidWhite);
		light->attachToObject_p(this, true);
		m_cellLights.push_back(light);
	}

	PortalPropertyTemplateCell::LightList const *lightList = portalProperty->getCellTemplate(m_cellNumber.get()).getLightList();
	if (lightList)
	{
		PortalPropertyTemplateCell::LightList::const_iterator const iEnd = lightList->end();
		for (PortalPropertyTemplateCell::LightList::const_iterator i = lightList->begin(); i != iEnd; ++i)
		{
			const PortalPropertyTemplateCellLight &lightData = *i;

			{
				Light *light = createLight(lightData);
				light->setAffectsShadersWithPrecalculatedVertexLighting(false);
				light->attachToObject_p(this, true);
				m_cellLights.push_back(light);
			}	

			if (lightData.specularColor.r != 0.0f || lightData.specularColor.g != 0.0f || lightData.specularColor.b != 0.0f)
			{
				Light *light = createLight(lightData);
				light->setDiffuseColor(VectorArgb::solidBlack);
				light->setAffectsShadersWithoutPrecalculatedVertexLighting(false);
				light->attachToObject_p(this, true);
				m_cellLights.push_back(light);
			}
		}
	}

	//-- build the radar shape
	{
		const CellProperty* const cellProperty = getCellProperty ();
		if (cellProperty)
		{
			const Floor* const floor = cellProperty->getFloor ();
			if (floor)
			{
				const FloorMesh* const floorMesh = floor->getFloorMesh ();
				if (floorMesh)
				{
					std::vector<Vector> vertices;
					floorMesh->getVertices (vertices);

					std::vector<int> indices;
					floorMesh->getIndices (indices);

					m_radarShape = new IndexedTriangleList;
					m_radarShape->addIndexedTriangleList (&vertices [0], static_cast<int> (vertices.size ()), &indices [0], static_cast<int> (indices.size ()));
				}
			}
		}
	}

	//-- build the radar edges
	{
		const CellProperty* const cellProperty = getCellProperty ();
		if (cellProperty)
		{
			const Floor* const floor = cellProperty->getFloor ();
			if (floor)
			{
				const FloorMesh* const floorMesh = floor->getFloorMesh ();
				if (floorMesh)
				{
					m_radarEdges = new std::vector<Vector>;

					int i;
					for (i = 0; i < floorMesh->getTriCount (); ++i)
					{
						const FloorTri& floorTri = floorMesh->getFloorTri (i);

						int j;
						for (j = 0; j < 3; ++j)
						{
							if (!floorTri.isCrossable (j) && floorTri.getPortalId (j) == -1)
							{
								const Vector& v0 = floorMesh->getVertex (floorTri.getCornerIndex (j));
								m_radarEdges->push_back (v0);

								const Vector& v1 = floorMesh->getVertex (floorTri.getCornerIndex (j + 1));
								m_radarEdges->push_back (v1);
							}
						}
					}
				}
			}
		}
	}

	//-- build the radar portal edges
	{
		const CellProperty* const cellProperty = getCellProperty ();
		if (cellProperty)
		{
			int i;
			for (i = 0; i < cellProperty->getNumberOfPortalObjects (); ++i)
			{
				const CellProperty::PortalObjectEntry& portalObjectEntry = cellProperty->getPortalObject (i);

				uint j;
				for (j = 0; j < portalObjectEntry.portalList->size (); ++j)
				{
					const Portal* const portal = (*portalObjectEntry.portalList) [j];
					if (portal && portal->getNeighbor () && portal->getNeighbor ()->getParentCell () == CellProperty::getWorldCellProperty ())
					{
						IndexedTriangleList const & portalGeometry = portal->getGeometry ();
						std::vector<Vector> const & vertices = portalGeometry.getVertices();
						std::vector<int> const & indices = portalGeometry.getIndices();
						uint const numberOfFaces = indices.size() / 3;

						if (!m_radarPortalEdges)
							m_radarPortalEdges = new std::vector<Vector>;

						//-- we found a portal to the outside world
						for (uint faceIndex = 0, index = 0; faceIndex < numberOfFaces; ++faceIndex)
						{
							Vector const & v0 = vertices[indices[index++]];
							Vector const & v1 = vertices[indices[index++]];
							Vector const & v2 = vertices[indices[index++]];

							m_radarPortalEdges->push_back(v0);
							m_radarPortalEdges->push_back(v1);
							m_radarPortalEdges->push_back(v1);
							m_radarPortalEdges->push_back(v2);
							m_radarPortalEdges->push_back(v2);
							m_radarPortalEdges->push_back(v0);
						}
					}
				}
			}	
		}
	}

	//-- hook up the cell property's environment texture
	{
		CellProperty* const cellProperty = getCellProperty ();
		const char* const cellName = cellProperty->getCellName ();
		const PortalProperty* const portalProperty = cellProperty->getPortalProperty ();
		const char* const pobShortName = portalProperty->getPobShortName ();

		const InteriorEnvironmentBlock* const interiorEnvironmentBlock = NON_NULL (InteriorEnvironmentBlockManager::getEnvironmentBlock (pobShortName, cellName));
		if (interiorEnvironmentBlock)
		{
			cellProperty->setEnvironmentTexture (interiorEnvironmentBlock->getEnvironmentTexture ());
			cellProperty->setFogEnabled (interiorEnvironmentBlock->getFogEnabled ());
			cellProperty->setFogColor (interiorEnvironmentBlock->getFogColor ());
			cellProperty->setFogDensity (interiorEnvironmentBlock->getFogDensity ());
		}
	}
}

// ----------------------------------------------------------------------

void CellObject::setAccessAllowed(bool allowed)
{
	m_accessAllowed = allowed;
	// this is now handled by a auto delta variable
	// TODO : remove the set and dependent messages completely
}

// ----------------------------------------------------------------------

const IndexedTriangleList* CellObject::getRadarShape () const
{
	return m_radarShape;
}

// ----------------------------------------------------------------------

const std::vector<Vector>* CellObject::getRadarEdges () const
{
	return m_radarEdges;
}

// ----------------------------------------------------------------------

const std::vector<Vector>* CellObject::getRadarPortalEdges () const
{
	return m_radarPortalEdges;
}

// ----------------------------------------------------------------------

void CellObject::setCellLightColor(float r, float g, float b, float brightness)
{
	// Volume lighting path (engine): CellProperty RGB drives Graphics::setOverrideFullAmbient inside ShaderPrimitiveSorter::pushCell,
	// precalc shaders get dynamic-light routing while override is on, LightManager fills uplight from the same RGB, and
	// Direct3d9_StaticShaderData tints material channels. Template lights below stay numerically consistent with that tint.

	REPORT_LOG(true, ("setCellLightColor: r=%.2f g=%.2f b=%.2f brightness=%.2f\n", r, g, b, brightness));

	for (std::vector<Light *>::iterator it = m_cellLights.begin(); it != m_cellLights.end(); ++it)
	{
		Light * const light = *it;
		if (light)
		{
			light->removeFromWorld();
			delete light;
		}
	}
	m_cellLights.clear();

	bool const firstCustomLightingUpdate = !m_hasCustomLighting;
	m_hasCustomLighting = true;

	// The original remote device uses normalized RGB, but accept legacy 0..255 payload values without changing the wire format.
	float const inputMaximum = std::max (r, std::max (g, b));
	float const rgbDivisor = inputMaximum > 1.5f ? 255.f : 1.f;
	float const appliedBrightness = clamp (0.f, brightness, ConfigClientGame::getRemoteCellLightingBrightnessCap ());
	const float fr = clamp (0.f, (r / rgbDivisor) * appliedBrightness, 1.f);
	const float fg = clamp (0.f, (g / rgbDivisor) * appliedBrightness, 1.f);
	const float fb = clamp (0.f, (b / rgbDivisor) * appliedBrightness, 1.f);

	CellProperty * const cellProperty = getCellProperty();
	PortalProperty * portalProperty = NULL;

	if (cellProperty)
	{
		cellProperty->setCustomLightingOverride(true, fr, fg, fb);
		portalProperty = const_cast<PortalProperty *>(cellProperty->getPortalProperty());
		if (firstCustomLightingUpdate && ConfigClientGame::getRemoteCellLightingUseDarkPob ())
			swapBuildingToDarkPob(cellProperty);
	}

	REPORT_LOG (true, ("setCellLightColor: applied normalized RGB=(%.3f,%.3f,%.3f), brightness=%.3f (cap %.3f)\n",
		fr, fg, fb, appliedBrightness, ConfigClientGame::getRemoteCellLightingBrightnessCap ()));

	if (ConfigClientGame::getAmbientLightInCells())
	{
		Light * const ambientLight = new Light(Light::T_ambient, VectorArgb(1.f, fr, fg, fb));
		ambientLight->attachToObject_p(this, true);
		m_cellLights.push_back(ambientLight);
	}

	// Recreate the original POB lights at their positions and preserve vanilla shader routing:
	// diffuse lights affect dynamic meshes; a separate black-diffuse light carries specular to prelit meshes.
	if (portalProperty)
	{
		const PortalPropertyTemplate & templ = portalProperty->getPortalPropertyTemplate();
		const int cellIndex = m_cellNumber.get();

		if (cellIndex >= 0 && cellIndex < templ.getNumberOfCells())
		{
			PortalPropertyTemplateCell::LightList const * lightList = templ.getCell(cellIndex).getLightList();
			if (lightList)
			{
				for (PortalPropertyTemplateCell::LightList::const_iterator i = lightList->begin(); i != lightList->end(); ++i)
				{
					const PortalPropertyTemplateCellLight & lightData = *i;

					VectorArgb const tintedDiffuse(
						lightData.diffuseColor.a,
						lightData.diffuseColor.r * fr,
						lightData.diffuseColor.g * fg,
						lightData.diffuseColor.b * fb
					);
					VectorArgb const tintedSpecular(
						lightData.specularColor.a,
						lightData.specularColor.r * fr,
						lightData.specularColor.g * fg,
						lightData.specularColor.b * fb
					);

					Light * const diffuseLight = createLight(lightData);
					if (diffuseLight)
					{
						diffuseLight->setDiffuseColor(tintedDiffuse);
						diffuseLight->setSpecularColor(tintedSpecular);
						diffuseLight->setAffectsShadersWithPrecalculatedVertexLighting(false);
						diffuseLight->attachToObject_p(this, true);
						m_cellLights.push_back(diffuseLight);
					}

					if (tintedSpecular.r != 0.f || tintedSpecular.g != 0.f || tintedSpecular.b != 0.f)
					{
						Light * const specularLight = createLight(lightData);
						if (specularLight)
						{
							specularLight->setDiffuseColor(VectorArgb::solidBlack);
							specularLight->setSpecularColor(tintedSpecular);
							specularLight->setAffectsShadersWithoutPrecalculatedVertexLighting(false);
							specularLight->attachToObject_p(this, true);
							m_cellLights.push_back(specularLight);
						}
					}
				}
			}
		}
	}
}

bool CellObject::hasCustomLighting() const
{
	return m_hasCustomLighting;
}

//-----------------------------------------------------------------------

void CellObject::getObjectInfo(std::map<std::string, std::map<std::string, Unicode::String> > & propertyMap) const
{
#if PRODUCTION == 0
/**
  When adding a variable to this class, please add it here.  Variable that aren't easily displayable are still listed, for tracking purposes.
*/

/**
	Don't compile in production build because this maps human-readable values to data members and makes hacking easier
*/
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "CellNumber", m_cellNumber.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "AccessAllowed", m_isPublic.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "CellLabel", m_cellLabel.get());
	DebugInfoManager::addProperty(propertyMap, ms_debugInfoSectionName, "LabelLocationOffset", m_labelLocationOffset.get());

//	IndexedTriangleList*             m_radarShape;
//	stdvector<Vector>::fwd*          m_radarEdges;
//	stdvector<Vector>::fwd*          m_radarPortalEdges;

	ClientObject::getObjectInfo(propertyMap);

#else
	UNREF(propertyMap);
#endif
}

// ----------------------------------------------------------------------

bool CellObject::getAccessAllowed() const
{
	return (m_isPublic.get() && m_accessAllowed) || m_accessAllowed;
}

// ----------------------------------------------------------------------

CellObject * CellObject::getCellObject(NetworkId const & networkId)
{
	return CellObject::asCellObject(NetworkIdManager::getObjectById(networkId));
}

// ----------------------------------------------------------------------

CellObject * CellObject::asCellObject(Object * object)
{
	ClientObject * const clientObject = (object != NULL) ? object->asClientObject() : NULL;
	CellObject * const cellObject = ( clientObject != NULL) ? clientObject->asCellObject() : NULL;

	return cellObject;
}

// ----------------------------------------------------------------------

CellObject const * CellObject::asCellObject(Object const * object)
{
	ClientObject const * const clientObject = (object != NULL) ? object->asClientObject() : NULL;
	CellObject const * const cellObject = (clientObject != NULL) ? clientObject->asCellObject() : NULL;

	return cellObject;
}
 
// ----------------------------------------------------------------------

CellObject * CellObject::asCellObject()
{
	return this;
}

// ----------------------------------------------------------------------

CellObject const * CellObject::asCellObject() const
{
	return this;
}
// ======================================================================
