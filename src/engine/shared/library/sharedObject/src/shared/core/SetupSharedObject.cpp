// ======================================================================
//
// SetupSharedObject.cpp
// copyright 1998 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedObject/FirstSharedObject.h"
#include "sharedObject/SetupSharedObject.h"

#include "sharedCollision/ExtentList.h"
#include "sharedObject/AlterScheduler.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/ArrangementDescriptorList.h"
#include "sharedObject/BasicRangedIntCustomizationVariable.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/ConfigSharedObject.h"
#include "sharedObject/ContainedByProperty.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/CustomizationIdManager.h"
#include "sharedObject/MemoryBlockManagedObject.h"
#include "sharedObject/MovementTable.h"
#include "sharedObject/NoRenderAppearanceTemplate.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/PaletteColorCustomizationVariable.h"
#include "sharedObject/PortalPropertyTemplate.h"
#include "sharedObject/ScheduleData.h"
#include "sharedObject/SlotDescriptorList.h"
#include "sharedObject/SlotIdManager.h"
#include "sharedObject/SlottedContainmentProperty.h"
#include "sharedObject/TrackingDynamics.h"
#include "sharedObject/VolumeContainmentProperty.h"
#include "sharedDebug/InstallTimer.h"

#include <string>
#if defined(_WIN32)
#include <windows.h>
#define SETUP_SO_TRACE(msg) do { ::OutputDebugStringA("[Titan] " msg "\r\n"); } while (0)
#else
#define SETUP_SO_TRACE(msg) do { } while (0)
#endif

// ======================================================================
// class SetupSharedObject::Data
// ======================================================================

SetupSharedObject::Data::Data()
:	version(0),
	useContainers(false),
	slotDefinitionFilename(new std::string("")),
	loadAssociatedHardpointNames(false),
	useMovementTable(false),
	movementStateTableFilename(new std::string("")),
	useTimedAppearanceTemplates(false),
	ensureDefaultAppearanceExists(true),
	customizationIdManagerFilename(0),
	objectsAlterChildrenAndContents(true),
	loadObjectTemplateCrcStringTable(true),
	pobEjectionTransformFilename(NULL)
{
}

// ----------------------------------------------------------------------

SetupSharedObject::Data::~Data()
{
	delete customizationIdManagerFilename;
	delete movementStateTableFilename;
	delete slotDefinitionFilename;
}

// ======================================================================
// class SetupSharedObject
// ======================================================================
/**
 * Install sharedObject.
 *
 * The settings in the Data structure will determine which subsystems
 * get initialized.
 */

void SetupSharedObject::install(const Data &data)
{
	InstallTimer const installTimer("SetupSharedObject::install");
	SETUP_SO_TRACE("SetupSharedObject::install: enter");

	DEBUG_FATAL(data.version != DATA_VERSION, ("SetupSharedObject::install wrong version %d/%d", data.version, DATA_VERSION));

	ConfigSharedObject::install();

	ScheduleData::install();

	Appearance::install();
	AppearanceTemplateList::install(data.useTimedAppearanceTemplates, data.ensureDefaultAppearanceExists);
	NoRenderAppearanceTemplate::install();

	// this is needed by plug-ins that aren't using the rest of the 3d system
	ExtentList::install();
	SETUP_SO_TRACE("SetupSharedObject::install: after ExtentList, before ObjectTemplateList::install");
	ObjectTemplateList::install(data.loadObjectTemplateCrcStringTable);
	SETUP_SO_TRACE("SetupSharedObject::install: after ObjectTemplateList::install");
	SETUP_SO_TRACE("SetupSharedObject::install: before ObjectTemplate::install");
	ObjectTemplate::install();
	SETUP_SO_TRACE("SetupSharedObject::install: after ObjectTemplate::install");
	SETUP_SO_TRACE("SetupSharedObject::install: before Object::install");
	Object::install(data.objectsAlterChildrenAndContents);
	SETUP_SO_TRACE("SetupSharedObject::install: after Object::install");
	MemoryBlockManagedObject::install();
	CellProperty::install();
	AppearanceTemplate::install();
	ContainedByProperty::install();
	SlottedContainmentProperty::install();
	VolumeContainmentProperty::install();

	CustomizationData::install();
	BasicRangedIntCustomizationVariable::install();
	PaletteColorCustomizationVariable::install();
	PortalPropertyTemplate::install(data.pobEjectionTransformFilename);

	// Dynamics.
	TrackingDynamics::install();

	// install container-related systems
	if (data.useContainers)
	{
		DEBUG_FATAL(data.slotDefinitionFilename->empty(), ("must specify a slotDefinitionFilename if you're using containers\n"));

		SETUP_SO_TRACE("SetupSharedObject::install: before SlotIdManager::install (slot_definitions.iff, etc.)");
		SlotIdManager::install(*data.slotDefinitionFilename, data.loadAssociatedHardpointNames);
		SETUP_SO_TRACE("SetupSharedObject::install: after SlotIdManager, before SlotDescriptorList");
		SlotDescriptorList::install();
		ArrangementDescriptorList::install();
		SETUP_SO_TRACE("SetupSharedObject::install: after container slot lists");
	}

	// install movement table
	if (data.useMovementTable)
	{
		DEBUG_FATAL(data.movementStateTableFilename->empty(), ("must specify a movementStateTableFilename if using the movement table\n"));
		SETUP_SO_TRACE("SetupSharedObject::install: before MovementTable::install (movementstates.iff)");
		MovementTable::install(*data.movementStateTableFilename);
		SETUP_SO_TRACE("SetupSharedObject::install: after MovementTable::install");
	}

	AlterScheduler::install();

	// Optionally install customization id manager
	if (data.customizationIdManagerFilename)
	{
		SETUP_SO_TRACE("SetupSharedObject::install: before CustomizationIdManager::install");
		CustomizationIdManager::install(data.customizationIdManagerFilename->c_str());
		SETUP_SO_TRACE("SetupSharedObject::install: after CustomizationIdManager::install");
	}
	SETUP_SO_TRACE("SetupSharedObject::install: leave");
}

// ----------------------------------------------------------------------

void SetupSharedObject::setupDefaultGameData(Data &data)
{
	data.version = DATA_VERSION;
}

// ----------------------------------------------------------------------

void SetupSharedObject::setupDefaultConsoleData(Data &data)
{
	data.version = DATA_VERSION;
	data.loadObjectTemplateCrcStringTable = false;
}

// ----------------------------------------------------------------------

void SetupSharedObject::setupDefaultMFCData(Data &data)
{
	data.version = DATA_VERSION;
}

// ----------------------------------------------------------------------

void SetupSharedObject::addSlotIdManagerData(Data &data, bool loadAssociatedHardpointNames)
{
	data.useContainers                = true;
	*data.slotDefinitionFilename      = "abstract/slot/slot_definition/slot_definitions.iff";
	data.loadAssociatedHardpointNames = loadAssociatedHardpointNames;
}

// ----------------------------------------------------------------------

void SetupSharedObject::addMovementTableData(Data &data)
{
	data.useMovementTable            = true;
	*data.movementStateTableFilename = "datatables/movement/movementstates.iff";
}

// ----------------------------------------------------------------------

void SetupSharedObject::addCustomizationSupportData(Data &data)
{
	IS_NULL(data.customizationIdManagerFilename);
	data.customizationIdManagerFilename = new std::string("customization/customization_id_manager.iff");
}

// ----------------------------------------------------------------------

void SetupSharedObject::addPobEjectionTransformData(Data &data)
{
	IS_NULL(data.pobEjectionTransformFilename);
	data.pobEjectionTransformFilename = "datatables/pob/pob_ejection_point.iff";
}

// ======================================================================

