//===================================================================
//
// SwgCuiStructurePlacement.h
// asommers
//
// copyright 2002, sony online entertainment
//
//===================================================================

#ifndef INCLUDED_SwgCuiStructurePlacement_H
#define INCLUDED_SwgCuiStructurePlacement_H

//===================================================================

#include "UIEventCallback.h"
#include "clientUserInterface/CuiMediator.h"
#include "sharedObject/LotType.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkId.h"

class Object;
class ObjectTemplate;
class StructureFootprint;
class UIButton;
class UIText;

//===================================================================

class SwgCuiStructurePlacement : public UIEventCallback, public CuiMediator
{
public:

	explicit SwgCuiStructurePlacement (UIPage & page);

	virtual bool OnMessage (UIWidget* context, const UIMessage& msg);

	virtual void performActivate   ();
	virtual void performDeactivate ();

	bool         setData (const NetworkId& deedNetworkId, const char* structureSharedObjectTemplateName);
	bool         setData (const NetworkId& deedNetworkId, const char* structureSharedObjectTemplateName, float claimFootprintRadiusMeters);

private:

	static void visualHelpRefresh (void * context);

	void cleanup ();
	void layoutOrientationHelpText ();
	void updateOrientationHelpText ();
	void installGlobalRenderSkipForPlacement ();
	void restoreGlobalRenderSkipFromPlacement ();

	virtual ~SwgCuiStructurePlacement ();
	SwgCuiStructurePlacement (const SwgCuiStructurePlacement&);
	SwgCuiStructurePlacement& operator= (const SwgCuiStructurePlacement&);

	char*                     m_structureSharedObjectTemplateName;
	NetworkId                 m_deedNetworkId;
	int                       m_oldView;
	const ObjectTemplate*     m_structureObjectTemplate;
	const StructureFootprint* m_structureFootprint;
	Object*                   m_structureObject;
	RotationType              m_rotationType;
	UIText*                   m_orientationHelpText;
	float                     m_claimFootprintRadiusMeters;
	bool                      m_isClaimPlacement;
};

//===================================================================

#endif
