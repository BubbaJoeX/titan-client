//===================================================================
//
// TerrainModificationHelper.cpp
// copyright 2001, sony online entertainment
//
//
//===================================================================

#include "sharedTerrain/FirstSharedTerrain.h"
#include "sharedTerrain/TerrainModificationHelper.h"

#include "sharedTerrain/AffectorHeight.h"
#include "sharedFile/Iff.h"

//===================================================================

void TerrainModificationHelper::setHeight (TerrainGenerator::Layer* layer, float height)
{
	layer->setModificationHeight (height);

	{
		int i;
		for (i = 0; i < layer->getNumberOfAffectors (); ++i)
			if (layer->getAffector (i)->isActive () && layer->getAffector (i)->getType () == TGAT_heightConstant)
				safe_cast<AffectorHeightConstant*> (layer->getAffector (i))->setHeight (height);
	}

	{
		int i;
		for (i = 0; i < layer->getNumberOfLayers (); ++i)
		{
			TerrainGenerator::Layer* const sub = layer->getLayer (i);
			if (sub && sub->isActive ())
				setHeight (sub, height);
		}
	}
}

//-------------------------------------------------------------------

void TerrainModificationHelper::setPosition (TerrainGenerator::Layer* layer, const Vector2d& position)
{
	{
		int i;
		for (i = 0; i < layer->getNumberOfBoundaries (); ++i)
			if (layer->getBoundary (i)->isActive ())
				layer->getBoundary (i)->setCenter (position);
	}

	{
		int i;
		for (i = 0; i < layer->getNumberOfLayers (); i++)
		{
			TerrainGenerator::Layer* const sub = layer->getLayer (i);
			if (sub && sub->isActive ())
				setPosition (sub, position);
		}
	}
}

//-------------------------------------------------------------------

void TerrainModificationHelper::setRotation (TerrainGenerator::Layer* layer, float angle)
{
	{
		int i;
		for (i = 0; i < layer->getNumberOfBoundaries (); ++i)
			if (layer->getBoundary (i)->isActive ())
				layer->getBoundary (i)->setRotation (angle);
	}

	{
		int i;
		for (i = 0; i < layer->getNumberOfLayers (); i++)
		{
			TerrainGenerator::Layer* const sub = layer->getLayer (i);
			if (sub && sub->isActive ())
				setRotation (sub, angle);
		}
	}
}

//-------------------------------------------------------------------

TerrainGenerator::Layer* TerrainModificationHelper::importLayer (const char* filename)
{
	Iff iff;
	if (iff.open (filename, true))
	{
		ShaderGroup* shaderGroup = new ShaderGroup;
		shaderGroup->load (iff);
		delete shaderGroup;

		FloraGroup* floraGroup = new FloraGroup;
		floraGroup->load (iff);
		delete floraGroup;

		RadialGroup* radialGroup = new RadialGroup;
		radialGroup->load (iff);
		delete radialGroup;

		EnvironmentGroup* environmentGroup = new EnvironmentGroup;
		environmentGroup->load (iff);
		delete environmentGroup;

		FractalGroup* fractalGroup = new FractalGroup;
		fractalGroup->load (iff);
		delete fractalGroup;

		TerrainGenerator::Layer* const newLayer = new TerrainGenerator::Layer;
		newLayer->load (iff, 0);

		return newLayer;
	}

	return 0;
}

//===================================================================
