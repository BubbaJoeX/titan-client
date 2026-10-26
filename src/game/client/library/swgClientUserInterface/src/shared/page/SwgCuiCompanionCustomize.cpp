// ======================================================================

//

// SwgCuiCompanionCustomize.cpp

// copyright (c) 2001 Sony Online Entertainment

//

// ======================================================================



#include "swgClientUserInterface/FirstSwgClientUserInterface.h"

#include "swgClientUserInterface/SwgCuiCompanionCustomize.h"

#include "swgClientUserInterface/SwgCuiAvatarCustomizationBase_CustomizationGroup.h"



#include "clientGame/ClientCommandQueue.h"

#include "clientGame/CreatureObject.h"

#include "clientGame/Game.h"

#include "clientGame/PlayerObject.h"

#include "clientGame/TangibleObject.h"

#include "clientUserInterface/CuiColorPicker.h"

#include "clientUserInterface/CuiWidget3dObjectListViewer.h"

#include "sharedFoundation/Crc.h"

#include "sharedFoundation/CrcLowerString.h"

#include "sharedGame/CustomizationManager.h"

#include "sharedGame/SharedImageDesignerManager.h"

#include "sharedMath/PaletteArgb.h"

#include "sharedObject/CustomizationData.h"

#include "sharedObject/NetworkIdManager.h"

#include "sharedObject/PaletteColorCustomizationVariable.h"

#include "sharedObject/RangedIntCustomizationVariable.h"

#include "swgClientUserInterface/SwgCuiAvatarCreationHelper.h"

#include "swgClientUserInterface/SwgCuiMediatorTypes.h"



#include "UIButton.h"

#include "UIBaseObject.h"
#include "UIComposite.h"
#include "UIWidget.h"

#include "UIPage.h"

#include "UIVolumePage.h"



#include <sstream>



// ======================================================================



namespace SwgCuiCompanionCustomizeNamespace

{

	uint32 const cms_companionSaveCustomization = Crc::normalizeAndCalculate("companionSaveCustomization");

	int const cms_companionHairSkillMod = 13;



	CreatureObject * getActiveStoryCompanionCreature()

	{

		PlayerObject const * const playerObject = Game::getPlayerObject();

		if (!playerObject || playerObject->getCompanionPetStanceUi() < 0)

			return NULL;



		NetworkId const & petId = playerObject->getPetId();

		if (!petId.isValid())

			return NULL;



		Object * const petObject = NetworkIdManager::getObjectById(petId);

		if (!petObject)

			return NULL;



		ClientObject * const clientObject = petObject->asClientObject();

		if (!clientObject)

			return NULL;



		return clientObject->asCreatureObject();

	}



	struct CollectCustomizationContext

	{

		std::ostringstream params;

		bool first;

	};



	void collectCustomizationCallback(std::string const & fullVariablePathName, CustomizationVariable const * customizationVariable, void * context)

	{

		CollectCustomizationContext * const ctx = NON_NULL(static_cast<CollectCustomizationContext *>(context));

		PaletteColorCustomizationVariable const * const paletteVar = dynamic_cast<PaletteColorCustomizationVariable const *>(customizationVariable);

		RangedIntCustomizationVariable const * const rangedVar = paletteVar ? static_cast<RangedIntCustomizationVariable const *>(paletteVar) : dynamic_cast<RangedIntCustomizationVariable const *>(customizationVariable);

		if (!rangedVar || !rangedVar->doesVariablePersist())

			return;



		if (!ctx->first)

			ctx->params << ";";

		ctx->first = false;

		ctx->params << fullVariablePathName << "=" << rangedVar->getValue();

	}



	void collectHairCustomizationCallback(std::string const & fullVariablePathName, CustomizationVariable const * customizationVariable, void * context)

	{

		CollectCustomizationContext * const ctx = NON_NULL(static_cast<CollectCustomizationContext *>(context));

		PaletteColorCustomizationVariable const * const paletteVar = dynamic_cast<PaletteColorCustomizationVariable const *>(customizationVariable);

		RangedIntCustomizationVariable const * const rangedVar = paletteVar ? static_cast<RangedIntCustomizationVariable const *>(paletteVar) : dynamic_cast<RangedIntCustomizationVariable const *>(customizationVariable);

		if (!rangedVar || !rangedVar->doesVariablePersist())

			return;



		if (!ctx->first)

			ctx->params << ";";

		ctx->first = false;

		ctx->params << "hair/" << fullVariablePathName << "=" << rangedVar->getValue();

	}



	std::string buildCompanionCustomizationParams(CreatureObject & creature)

	{

		std::ostringstream params;

		bool first = true;



		CustomizationData const * const cdata = creature.fetchCustomizationData();

		if (cdata)

		{

			CollectCustomizationContext ctx;

			ctx.first = true;

			cdata->iterateOverConstVariables(collectCustomizationCallback, &ctx, false);

			cdata->release();

			params << ctx.params.str();

			if (!ctx.first)

				first = false;

		}



		ClientObject const * const hairObj = creature.getHairObject();

		TangibleObject const * const hair = hairObj ? hairObj->asTangibleObject() : NULL;

		if (!first)

			params << ";";

		if (hair)

		{

			std::string const hairTemplate = CustomizationManager::convertSharedHairTemplateNameToServerHairTemplateName(hair->getTemplateName());

			params << "hairTemplate=" << hairTemplate;



			CustomizationData const * const hairData = hair->fetchCustomizationData();

			if (hairData)

			{

				CollectCustomizationContext hairCtx;

				hairCtx.first = true;

				hairData->iterateOverConstVariables(collectHairCustomizationCallback, &hairCtx, false);

				hairData->release();

				if (!hairCtx.first)

					params << ";" << hairCtx.params.str();

			}

		}

		else

		{

			params << "hairTemplate=";

		}



		return params.str();

	}



	std::string mapNpcBaseToPlayerCustomizationSpecies(std::string const & speciesGender)

	{

		std::string mapped = speciesGender;

		std::string::size_type basePos = mapped.find("_base_");

		if (basePos != std::string::npos)

			mapped.erase(basePos, strlen("_base"));

		return mapped;

	}

}



using namespace SwgCuiCompanionCustomizeNamespace;



// ----------------------------------------------------------------------



SwgCuiCompanionCustomize::SwgCuiCompanionCustomize(UIPage & page)

: SwgCuiAvatarCustomizationBase("SwgCuiCompanionCustomize", page),

  m_buttonCommit(0),

  m_buttonCancel(0),

  m_buttonRevert(0),

  m_hairPageOverlay(0),

  m_previewCompanion(0)

{

	getCodeDataObject (TUIButton, m_buttonCommit, "buttonCommit");

	getCodeDataObject (TUIButton, m_buttonCancel, "buttonCancel");

	getCodeDataObject (TUIButton, m_buttonRevert, "buttonRevert");

	getCodeDataObject (TUIPage, m_hairPageOverlay, "overlayhairpage");



	registerMediatorObject (*m_buttonCommit, true);

	registerMediatorObject (*m_buttonCancel, true);

	registerMediatorObject (*m_buttonRevert, true);



	setYawOnCustomizationSelection(false);

	hideImageDesignerSessionWidgets();



	if (m_hairPageOverlay)

		m_hairPageOverlay->SetVisible(false);

}



//-----------------------------------------------------------------------



SwgCuiCompanionCustomize::~SwgCuiCompanionCustomize()

{

	if (m_previewCompanion)

	{

		delete m_previewCompanion;

		m_previewCompanion = 0;

	}



	m_buttonCommit = 0;

	m_buttonCancel = 0;

	m_buttonRevert = 0;

	m_hairPageOverlay = 0;

}



//-----------------------------------------------------------------------



void SwgCuiCompanionCustomize::hideImageDesignerSessionWidgets() const

{

	static char const * const widgetsToHide[] =

	{

		"all.comp.right.timeRemaining",

		"all.comp.right.costtextbox",

		"all.comp.right.offeredMoney",

		"all.comp.right.target.advanced",

		"all.comp.viewer.label",

		"all.comp.right.target.advanced.holoemoteall",

		"all.comp.right.target.advanced.holoemotebeehive",

		"all.comp.right.target.advanced.holoemotebrainstorm",

		"all.comp.right.target.advanced.holoemotebubblehead",

		"all.comp.right.target.advanced.holoemotebullhorns",

		"all.comp.right.target.advanced.holoemotebutterflies",

		"all.comp.right.target.advanced.holoemotechampagne",

		"all.comp.right.target.advanced.holoemotehaunted",

		"all.comp.right.target.advanced.holoemotehearts",

		"all.comp.right.target.advanced.holoemotehologlitter",

		"all.comp.right.target.advanced.holoemoteholonotes",

		"all.comp.right.target.advanced.holoemoteimperial",

		"all.comp.right.target.advanced.holoemotenone",

		"all.comp.right.target.advanced.holoemotephonytail",

		"all.comp.right.target.advanced.holoemoterebel",

		"all.comp.right.target.advanced.holoemotesparky",

		"all.comp.right.target.advanced.holoemotetechnokitty",

		0

	};



	for (int i = 0; widgetsToHide[i]; ++i)

	{

		UIWidget * const widget = safe_cast<UIWidget *>(getPage().GetObjectFromPath(widgetsToHide[i], TUIWidget));

		if (widget)

			widget->SetVisible(false);

	}

}



//-----------------------------------------------------------------------



void SwgCuiCompanionCustomize::performActivate()

{

	SwgCuiAvatarCustomizationBase::performActivate();



	setPointerInputActive(true);

	setKeyboardInputActive(true);

	setInputToggleActive(false);

	hideImageDesignerSessionWidgets();

	if (m_buttonCommit)
		m_buttonCommit->SetEnabled(true);



	CreatureObject * const companion = getActiveStoryCompanionCreature();

	if (!companion)

	{

		closeNextFrame();

		return;

	}



	if (m_previewCompanion)

	{

		delete m_previewCompanion;

		m_previewCompanion = 0;

	}

	m_previewCompanion = SwgCuiAvatarCreationHelper::duplicateCreatureWithClothesAndCustomization(*companion, false);

	if (!m_previewCompanion)

	{

		closeNextFrame();

		return;

	}



	setObject(m_previewCompanion, cms_companionHairSkillMod);



	std::string const currentGroup = getGroup();

	CustomizationGroup const * const hairGroup = findHairCustomizationGroup();

	if (hairGroup)

	{

		setGroup(Unicode::wideToNarrow(hairGroup->name));

		setGroup(currentGroup);

	}

}



//-----------------------------------------------------------------------



void SwgCuiCompanionCustomize::performDeactivate()

{

	setPointerInputActive  (false);

	setKeyboardInputActive (false);

	setInputToggleActive   (true);



	setObject(0, 0);



	if (m_previewCompanion)

	{

		delete m_previewCompanion;

		m_previewCompanion = 0;

	}



	SwgCuiAvatarCustomizationBase::performDeactivate();

}



//-----------------------------------------------------------------------



void SwgCuiCompanionCustomize::OnButtonPressed(UIWidget *context)

{

	SwgCuiAvatarCustomizationBase::OnButtonPressed(context);



	if (context == m_buttonCommit)

	{

		CreatureObject * const preview = getObject();

		if (preview)

		{

			std::string const params = buildCompanionCustomizationParams(*preview);

			if (!params.empty())

				IGNORE_RETURN(ClientCommandQueue::enqueueCommand(cms_companionSaveCustomization, NetworkId::cms_invalid, Unicode::narrowToWide(params)));

		}

		CreatureObject * const liveCompanion = getActiveStoryCompanionCreature();

		if (liveCompanion)

			liveCompanion->setDupedCreaturesDirty(true);

		setObject(0, 0);

		closeNextFrame();

	}

	else if (context == m_buttonCancel || context == m_buttonRevert)

	{

		setObject(0, 0);

		closeNextFrame();

	}

}



//----------------------------------------------------------------------



void SwgCuiCompanionCustomize::updateSelectedHairColorState()

{

	SwgCuiAvatarCustomizationBase::updateSelectedHairColorState();

	setColorPickerColumnAndMaxIndexes();

}



//----------------------------------------------------------------------



void SwgCuiCompanionCustomize::setGroup(std::string const & groupName)

{

	SwgCuiAvatarCustomizationBase::setGroup(groupName);

	setColorPickerColumnAndMaxIndexes();



	if (m_hairPageOverlay)

	{

		CustomizationGroup const * const hairGroup = findHairCustomizationGroup();

		if (hairGroup && Unicode::caseInsensitiveCompare(hairGroup->name, groupName))

			m_hairPageOverlay->SetVisible(false);

		else

			m_hairPageOverlay->SetVisible(true);

	}

}



//----------------------------------------------------------------------



void SwgCuiCompanionCustomize::setColorPickerColumnAndMaxIndexes() const

{

	if(m_colorPicker[0])

	{

		PaletteArgb const * const palette = m_colorPicker[0]->getPalette();

		if(palette)

		{

			SharedImageDesignerManager::PaletteValues const paletteValues = SharedImageDesignerManager::getPaletteValuesForPaletteAndLevel(palette, SharedImageDesignerManager::IDPL_NOT_A_DESIGNER);

			m_colorPicker[0]->setForceColumns(paletteValues.columns);

			m_colorPicker[0]->setMaximumPaletteIndex(paletteValues.maxIndex);

			m_colorPicker[0]->reset();

		}

	}



	if(m_colorPicker[1])

	{

		PaletteArgb const * const palette = m_colorPicker[1]->getPalette();

		if(palette)

		{

			SharedImageDesignerManager::PaletteValues const paletteValues = SharedImageDesignerManager::getPaletteValuesForPaletteAndLevel(palette, SharedImageDesignerManager::IDPL_NOT_A_DESIGNER);

			m_colorPicker[1]->setForceColumns(paletteValues.columns);

			m_colorPicker[1]->setMaximumPaletteIndex(paletteValues.maxIndex);

			m_colorPicker[1]->reset();

		}

	}

}



//-----------------------------------------------------------------------



std::string SwgCuiCompanionCustomize::getCustomizationSpeciesGender(ClientObject const & object) const

{

	return mapNpcBaseToPlayerCustomizationSpecies(SwgCuiAvatarCustomizationBase::getCustomizationSpeciesGender(object));

}



std::string SwgCuiCompanionCustomize::getHairStyleTemplateName(ClientObject const & object) const

{

	std::string const speciesGender = getCustomizationSpeciesGender(object);

	if (speciesGender.empty())

		return SwgCuiAvatarCustomizationBase::getHairStyleTemplateName(object);

	return std::string("object/creature/player/shared_") + speciesGender + ".iff";

}



// ======================================================================

