// =====================================================================
//
// ActionsTool.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// =====================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ActionsTool.h"
#include "ActionsTool.moc"

#include "fileInterface/StdioFile.h"

#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientObject/LODManager.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"

#include "ActionsEdit.h"
#include "ActionHack.h"
#include "BookmarkBrowser.h"
#include "BrushData.h"
#include "ConfigGodClient.h"
#include "FilterWindow.h"
#include "GameWindow.h"
#include "GodClientData.h"
#include "GodClientPerforce.h"
#include "GroupObjectWindow.h"
#include "IconLoader.h"
#include "ActionsWindow.h"
#include "MainFrame.h"
#include "ObjectEditor.h"
#include "ServerCommander.h"
#include "SystemMessageWidget.h"
#include "TreeBrowser.h"
#include "Unicode.h"
#include "sharedTerrain/TerrainObject.h"

#include <qdialog.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qinputdialog.h>
#include <qmessagebox.h>
#include <qworkspace.h>
#include <algorithm>

//----------------------------------------------------------------------
ActionsTool::ActionsTool()
: QObject(),
  Singleton<ActionsTool>(),
  m_snapToGrid(0),
  m_stackObject(0),
  m_saveAsBrush(0),
  m_createBrush(0),
  m_deleteBrush(0),
  m_pasteBrushHere(0),
  m_createPalette(0),
  m_deletePalette(0),
  m_getSphereTree(0),
  m_grabRelativeCoordinates(0),
	m_sendSystemMessage(0),
	m_renderRangeSettings(0)
{
	QWidget * const p = &MainFrame::getInstance();

	//create the actions
	m_snapToGrid              = new ActionHack("Snap To Grid...",           IL_PIXMAP(hi16_action_random_rotate), "&Snap To Grid...", 0, p, "snap_to_grid");
	m_stackObject             = new ActionHack("Stacker",                    IL_PIXMAP(hi16_action_random_rotate), "Stac&ker",          0, p, "stacker");
	m_saveAsBrush             = new ActionHack("Save As Brush",             IL_PIXMAP(hi16_action_editcopy),      "Save As &Brush",   0, p, "save_as_brush");
	m_createBrush             = new ActionHack("Create Brush",              IL_PIXMAP(hi16_action_editcopy),      "Create Brush",     0, p, "create_brush");
	m_deleteBrush             = new ActionHack("Delete Brush",              IL_PIXMAP(hi16_action_editdelete),    "Delete Brush",     0, p, "delete_brush");
	m_pasteBrushHere          = new ActionHack("Paste Brush Here",          IL_PIXMAP(hi16_action_editpaste),     "Paste Brush Here", 0, p, "paste_brush_here");
	m_createPalette           = new ActionHack("Create Palette",            IL_PIXMAP(hi16_action_editcopy),      "Create Palette",   0, p, "create_palette");
	m_deletePalette           = new ActionHack("Delete Palette",            IL_PIXMAP(hi16_action_editdelete),    "Delete Palette",   0, p, "delete_palette");
	m_getSphereTree           = new ActionHack("Get SphereTree Snapshot",   IL_PIXMAP(hi16_action_random_rotate), "Get SphereTree Snapshot",   0, p, "show_sphere_tree");
	m_grabRelativeCoordinates = new ActionHack("Grab Relative Coordinates", IL_PIXMAP(hi16_action_random_rotate), "Grab Relative Coordinates", 0, p, "grab_relative_coords");
	m_sendSystemMessage       = new ActionHack("Send a System Message",     IL_PIXMAP(hi16_action_editpaste),     "Send a System Message",     0, p, "send_system_message");
	m_renderRangeSettings     = new ActionHack("Render Range Settings",     IL_PIXMAP(hi16_action_gear),          "Render Range Settings...",  0, p, "render_range_settings");

	//connect them to slots
	IGNORE_RETURN(connect(m_snapToGrid,              SIGNAL(activated()), this, SLOT(onSnapToGrid())));
	IGNORE_RETURN(connect(m_stackObject,             SIGNAL(activated()), this, SLOT(onStackObject())));
	IGNORE_RETURN(connect(m_saveAsBrush,             SIGNAL(activated()), this, SLOT(onSaveAsBrush())));
	IGNORE_RETURN(connect(m_createBrush,             SIGNAL(activated()), this, SLOT(onCreateBrush())));
	IGNORE_RETURN(connect(m_deleteBrush,             SIGNAL(activated()), this, SLOT(onDeleteBrush())));
	IGNORE_RETURN(connect(m_pasteBrushHere,          SIGNAL(activated()), this, SLOT(onPasteBrushHere())));
	IGNORE_RETURN(connect(m_createPalette,           SIGNAL(activated()), this, SLOT(onCreatePalette())));
	IGNORE_RETURN(connect(m_deletePalette,           SIGNAL(activated()), this, SLOT(onDeletePalette())));
	IGNORE_RETURN(connect(m_getSphereTree,           SIGNAL(activated()), this, SLOT(onShowSphereTree())));
	IGNORE_RETURN(connect(m_grabRelativeCoordinates, SIGNAL(activated()), this, SLOT(onGrabRelativeCoordinates())));
	IGNORE_RETURN(connect(m_sendSystemMessage,       SIGNAL(activated()), this, SLOT(onSendSystemMessage())));
	IGNORE_RETURN(connect(m_renderRangeSettings,     SIGNAL(activated()), this, SLOT(onRenderRangeSettings())));
}

//----------------------------------------------------------------------

ActionsTool::~ActionsTool()
{
	m_snapToGrid              = 0;
	m_stackObject             = 0;
	m_saveAsBrush             = 0;
	m_createBrush             = 0;
	m_deleteBrush             = 0;
	m_createPalette           = 0;
	m_deletePalette           = 0;
	m_getSphereTree           = 0;
	m_grabRelativeCoordinates = 0;
	m_sendSystemMessage       = 0;
	m_renderRangeSettings     = 0;
}

//----------------------------------------------------------------------

void ActionsTool::onSaveAsBrush() const
{
	bool ok = false;
	QString qName = (QInputDialog::getText(tr("Brush Creation"), tr("Please name the brush"), QLineEdit::Normal, QString::null, &ok));
	if(!qName.isNull())
	{
		std::string name = qName.latin1();
		if(ok && !name.empty())
			GodClientData::getInstance().saveCurrentSelectionAsBrush(name);
	}
}

//----------------------------------------------------------------------

void ActionsTool::onCreateBrush() const
{
}

//----------------------------------------------------------------------

void ActionsTool::onDeleteBrush() const
{
}

//----------------------------------------------------------------------

void ActionsTool::onPasteBrushHere() const
{
	//set the clipboard with the current brush
	GodClientData::getInstance().setCurrentBrush(*BrushData::getInstance().getSelectedBrush());

	//get the avatar's location, to use as the paste location
	GroundScene* const gs = dynamic_cast<GroundScene*>(Game::getScene());
	if(gs == 0)
		return;
	const Object* player    = gs->getPlayer();
	if(!player)
		return;
	const Vector& playerLocW = player->getPosition_w();

	//set the paste location to be the avatar
	GodClientData::getInstance().setPasteLocation(playerLocW);

	//now paste that brush into the world as usual
	ActionsEdit::getInstance().pasteBrush->doActivate();
}

//----------------------------------------------------------------------
/**
 * Create a new palette.  It must be named before objects can go in it.
 */
void ActionsTool::onCreatePalette() const
{
	bool ok = false;
	QString qName = (QInputDialog::getText(tr("Palette Creation"), tr("Please name the palette"), QLineEdit::Normal, QString::null, &ok));
	if(!qName.isNull())
	{
		std::string name = qName.latin1();
		if(ok && !name.empty())
			GodClientData::getInstance().addPalette(name);
	}
}

//----------------------------------------------------------------------

/**
 * Delete the currently selected palette
 */
void ActionsTool::onDeletePalette() const
{
	QMessageBox mb("Palette Deletion", 
	               "Really delete this palette?", 
	               QMessageBox::NoIcon, 
	               QMessageBox::Yes | QMessageBox::Default, 
	               QMessageBox::No  | QMessageBox::Escape, 
	               QMessageBox::NoButton);

	//TODO finalize this code
	if(mb.exec() == QMessageBox::Yes)
		GodClientData::getInstance().deletePalette("test");
}

//-----------------------------------------------------------------

/**
 * Get a snapshot of the current sphere tree.
 */
void ActionsTool::onShowSphereTree() const
{
	GodClientData::getInstance().getSphereTreeSnapshot();
}

//-----------------------------------------------------------------

/**
 * Show a dialog that allows setting the snap to grid parameters.
 */
void ActionsTool::onSnapToGrid() const
{
	GodClientData::getInstance().snapToGridDlg();
}

//-----------------------------------------------------------------

void ActionsTool::onStackObject() const
{
	MainFrame::getInstance().m_stackToolDock->show();
	ActionsWindow::getInstance().m_stackTool->setOn(true);
}

//-----------------------------------------------------------------

/**
 * Store the coordinates for use in building scripts
 */
void ActionsTool::onGrabRelativeCoordinates() const
{
}

//-----------------------------------------------------------------

/**
 */
void ActionsTool::onSendSystemMessage() const
{
	SystemMessageWidget* w = new SystemMessageWidget(MainFrame::getInstance().getWorkspace(), "Send System Message");
	w->show();
}

//-----------------------------------------------------------------

namespace
{
	void applyObjectUpdateRangeCap(float capMeters)
	{
		if (capMeters <= 0.f)
			return;

		const int kMaxLevels = 8;
		float distances[kMaxLevels];
		float maxDistance = 0.f;

		for (int i = 0; i < kMaxLevels; ++i)
		{
			distances[i] = LODManager::getLODDistance(i);
			maxDistance = std::max(maxDistance, distances[i]);
		}

		if (maxDistance <= 0.f || maxDistance <= capMeters)
			return;

		const float scale = capMeters / maxDistance;
		for (int i = 0; i < kMaxLevels; ++i)
			LODManager::setLODDistance(i, std::max(1.f, distances[i] * scale));
	}

	class RenderRangeSettingsDialog : public QDialog
	{
	public:
		explicit RenderRangeSettingsDialog(QWidget *parent)
		: QDialog(parent, "RenderRangeSettingsDialog", true)
		{
			setCaption("Render Range / Visibility Settings");

			QGridLayout *layout = new QGridLayout(this, 11, 2, 8, 6);

			NOT_NULL(layout);
			layout->addWidget(new QLabel("Terrain update range (LOD threshold):", this), 0, 0);
			layout->addWidget(new QLabel("Terrain local visibility cutoff (high LOD range):", this), 1, 0);
			layout->addWidget(new QLabel("Local visibility cutoff (dynamic flora near):", this), 2, 0);
			layout->addWidget(new QLabel("Hard-render cutoff (static flora):", this), 3, 0);
			layout->addWidget(new QLabel("Object update range LOD1 distance:", this), 4, 0);
			layout->addWidget(new QLabel("Object update range LOD2 distance:", this), 5, 0);
			layout->addWidget(new QLabel("Object update range LOD3 distance:", this), 6, 0);
			layout->addWidget(new QLabel("Object local visibility (min screen coverage):", this), 7, 0);
			layout->addWidget(new QLabel("GodClient object update range cap (meters):", this), 8, 0);
			layout->addWidget(new QLabel("Enable runtime culling:", this), 9, 0);

			m_lodThreshold = new QSlider(Qt::Horizontal, this);
			m_highLodThreshold = new QSlider(Qt::Horizontal, this);
			m_dynamicNearDistance = new QSlider(Qt::Horizontal, this);
			m_staticDistance = new QSlider(Qt::Horizontal, this);
			m_objectLodDistance1 = new QSlider(Qt::Horizontal, this);
			m_objectLodDistance2 = new QSlider(Qt::Horizontal, this);
			m_objectLodDistance3 = new QSlider(Qt::Horizontal, this);
			m_objectMinimumCoverage = new QSlider(Qt::Horizontal, this);
			m_objectUpdateRangeCap = new QSlider(Qt::Horizontal, this);
			m_runtimeCullEnabled = new QCheckBox(this);

			m_lodThreshold->setMinValue(1);       m_lodThreshold->setMaxValue(static_cast<int>(ClientProceduralTerrainAppearance::getMaximumThreshold()));
			m_highLodThreshold->setMinValue(1);   m_highLodThreshold->setMaxValue(static_cast<int>(ClientProceduralTerrainAppearance::getMaximumThresholdHigh()));
			m_dynamicNearDistance->setMinValue(4); m_dynamicNearDistance->setMaxValue(512);
			m_staticDistance->setMinValue(4);      m_staticDistance->setMaxValue(1024);
			m_objectLodDistance1->setMinValue(8);  m_objectLodDistance1->setMaxValue(16384);
			m_objectLodDistance2->setMinValue(16); m_objectLodDistance2->setMaxValue(32768);
			m_objectLodDistance3->setMinValue(32); m_objectLodDistance3->setMaxValue(65536);
			// coverage is stored as [0.0 .. 0.25], slider scaled by 10000 for precision.
			m_objectMinimumCoverage->setMinValue(0); m_objectMinimumCoverage->setMaxValue(2500);
			m_objectUpdateRangeCap->setMinValue(0);  m_objectUpdateRangeCap->setMaxValue(65536);

			layout->addWidget(m_lodThreshold, 0, 1);
			layout->addWidget(m_highLodThreshold, 1, 1);
			layout->addWidget(m_dynamicNearDistance, 2, 1);
			layout->addWidget(m_staticDistance, 3, 1);
			layout->addWidget(m_objectLodDistance1, 4, 1);
			layout->addWidget(m_objectLodDistance2, 5, 1);
			layout->addWidget(m_objectLodDistance3, 6, 1);
			layout->addWidget(m_objectMinimumCoverage, 7, 1);
			layout->addWidget(m_objectUpdateRangeCap, 8, 1);
			layout->addWidget(m_runtimeCullEnabled, 9, 1);

			QPushButton *applyButton = new QPushButton("Apply", this);
			QPushButton *cancelButton = new QPushButton("Cancel", this);
			layout->addWidget(applyButton, 10, 0);
			layout->addWidget(cancelButton, 10, 1);

			IGNORE_RETURN(connect(applyButton, SIGNAL(clicked()), this, SLOT(accept())));
			IGNORE_RETURN(connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject())));
		}

		void setValues(float lodThreshold, float highLodThreshold, float dynamicNearDistance, float staticDistance, float objectLodDistance1, float objectLodDistance2, float objectLodDistance3, float objectMinimumCoverage, float objectUpdateRangeCap)
		{
			m_lodThreshold->setValue(static_cast<int>(lodThreshold));
			m_highLodThreshold->setValue(static_cast<int>(highLodThreshold));
			m_dynamicNearDistance->setValue(static_cast<int>(dynamicNearDistance));
			m_staticDistance->setValue(static_cast<int>(staticDistance));
			m_objectLodDistance1->setValue(static_cast<int>(objectLodDistance1));
			m_objectLodDistance2->setValue(static_cast<int>(objectLodDistance2));
			m_objectLodDistance3->setValue(static_cast<int>(objectLodDistance3));
			m_objectMinimumCoverage->setValue(static_cast<int>(objectMinimumCoverage * 10000.f));
			m_objectUpdateRangeCap->setValue(static_cast<int>(objectUpdateRangeCap));
			m_runtimeCullEnabled->setChecked(objectMinimumCoverage > 0.f);
		}

		bool getValues(float &lodThreshold, float &highLodThreshold, float &dynamicNearDistance, float &staticDistance, float &objectLodDistance1, float &objectLodDistance2, float &objectLodDistance3, float &objectMinimumCoverage, float &objectUpdateRangeCap) const
		{
			lodThreshold = static_cast<float>(m_lodThreshold->value());
			highLodThreshold = static_cast<float>(m_highLodThreshold->value());
			dynamicNearDistance = static_cast<float>(m_dynamicNearDistance->value());
			staticDistance = static_cast<float>(m_staticDistance->value());
			objectLodDistance1 = static_cast<float>(m_objectLodDistance1->value());
			objectLodDistance2 = static_cast<float>(m_objectLodDistance2->value());
			objectLodDistance3 = static_cast<float>(m_objectLodDistance3->value());
			objectMinimumCoverage = m_runtimeCullEnabled->isChecked() ? (static_cast<float>(m_objectMinimumCoverage->value()) / 10000.f) : 0.f;
			objectUpdateRangeCap = static_cast<float>(m_objectUpdateRangeCap->value());
			return true;
		}

	private:
		QSlider *m_lodThreshold;
		QSlider *m_highLodThreshold;
		QSlider *m_dynamicNearDistance;
		QSlider *m_staticDistance;
		QSlider *m_objectLodDistance1;
		QSlider *m_objectLodDistance2;
		QSlider *m_objectLodDistance3;
		QSlider *m_objectMinimumCoverage;
		QSlider *m_objectUpdateRangeCap;
		QCheckBox *m_runtimeCullEnabled;
	};
}

void ActionsTool::onRenderRangeSettings() const
{
	RenderRangeSettingsDialog dlg(&MainFrame::getInstance());
	dlg.setValues(
		TerrainObject::getLevelOfDetailThreshold(),
		TerrainObject::getHighLevelOfDetailThreshold(),
		ClientProceduralTerrainAppearance::getDynamicNearFloraDistance(),
		ClientProceduralTerrainAppearance::getStaticNonCollidableFloraDistance(),
		LODManager::getLODDistance(0),
		LODManager::getLODDistance(1),
		LODManager::getLODDistance(2),
		LODManager::getMinimumCoverage(),
		static_cast<float>(ConfigGodClient::getObjectUpdateRangeCap())
	);

	if (dlg.exec() != QDialog::Accepted)
		return;

	float lodThreshold = 0.f;
	float highLodThreshold = 0.f;
	float dynamicNearDistance = 0.f;
	float staticDistance = 0.f;
	float objectLodDistance1 = 0.f;
	float objectLodDistance2 = 0.f;
	float objectLodDistance3 = 0.f;
	float objectMinimumCoverage = 0.f;
	float objectUpdateRangeCap = 0.f;
	if (!dlg.getValues(lodThreshold, highLodThreshold, dynamicNearDistance, staticDistance, objectLodDistance1, objectLodDistance2, objectLodDistance3, objectMinimumCoverage, objectUpdateRangeCap))
	{
		return;
	}

	lodThreshold = std::max(1.f, std::min(lodThreshold, ClientProceduralTerrainAppearance::getMaximumThreshold()));
	highLodThreshold = std::max(1.f, std::min(highLodThreshold, ClientProceduralTerrainAppearance::getMaximumThresholdHigh()));
	dynamicNearDistance = std::max(4.f, std::min(dynamicNearDistance, 512.f));
	staticDistance = std::max(4.f, std::min(staticDistance, 1024.f));
	objectLodDistance1 = std::max(8.f, std::min(objectLodDistance1, 16384.f));
	objectLodDistance2 = std::max(16.f, std::min(objectLodDistance2, 32768.f));
	objectLodDistance3 = std::max(32.f, std::min(objectLodDistance3, 65536.f));
	objectMinimumCoverage = std::max(0.f, std::min(objectMinimumCoverage, 0.25f));
	objectUpdateRangeCap = std::max(0.f, std::min(objectUpdateRangeCap, 65536.f));

	TerrainObject::setLevelOfDetailThreshold(lodThreshold);
	TerrainObject::setHighLevelOfDetailThreshold(highLodThreshold);
	ClientProceduralTerrainAppearance::setDynamicNearFloraDistance(dynamicNearDistance);
	ClientProceduralTerrainAppearance::setStaticNonCollidableFloraDistance(staticDistance);
	LODManager::setLODDistance(0, objectLodDistance1);
	LODManager::setLODDistance(1, objectLodDistance2);
	LODManager::setLODDistance(2, objectLodDistance3);
	LODManager::setMinimumCoverage(objectMinimumCoverage);
	applyObjectUpdateRangeCap(objectUpdateRangeCap);
}

//-----------------------------------------------------------------

