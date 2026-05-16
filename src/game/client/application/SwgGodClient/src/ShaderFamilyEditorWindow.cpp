// ======================================================================
//
// ShaderFamilyEditorWindow.cpp
// copyright (c) 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ShaderFamilyEditorWindow.h"
#include "ShaderFamilyEditorWindow.moc"

#include "GodClientTerrainEditor.h"
#include "GodClientVirtualPath.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "ConfigGodClient.h"
#include "sharedMath/PackedRgb.h"
#include "sharedTerrain/ShaderGroup.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qlistview.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qsizepolicy.h>
#include <qspinbox.h>

#include <cstdio>

namespace
{
	ShaderFamilyEditorWindow* s_shaderFamilyEditorWindow = 0;

	static int shaderFindUnusedFamilyId(TerrainGenerator& gen)
	{
		for (int id = 1; id <= 255; ++id)
			if (!gen.getShaderGroup().hasFamily(id))
				return id;
		return -1;
	}
}

// ======================================================================

void ShaderFamilyEditorWindow::showSingleton(QWidget* parent)
{
	if (!s_shaderFamilyEditorWindow)
		s_shaderFamilyEditorWindow = new ShaderFamilyEditorWindow(parent);

	s_shaderFamilyEditorWindow->reloadFromTerrain();
	s_shaderFamilyEditorWindow->show();
	s_shaderFamilyEditorWindow->raise();
	s_shaderFamilyEditorWindow->setActiveWindow();
}

// ======================================================================

ShaderFamilyEditorWindow::ShaderFamilyEditorWindow(QWidget* parent, const char* name)
: QDialog(parent, name, false),
  m_familyList(0),
  m_idLabel(0),
  m_nameEdit(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_surfacePropertiesEdit(0),
  m_featherClampEdit(0),
  m_childList(0),
  m_applyButton(0),
  m_addFamilyButton(0),
  m_removeFamilyButton(0),
  m_addChildShaderButton(0),
  m_removeChildButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Shader families");
	resize(600, 500);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edit families, surface properties, feather clamp, color, and assign shader template children (.sht).",
		this);
	mainLayout->addWidget(hint);

	m_familyList = new QListBox(this);
	m_familyList->setColumnMode(QListBox::FitToWidth);
	mainLayout->addWidget(m_familyList);

	QFrame* const fieldsFrame = new QFrame(this);
	fieldsFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QVBoxLayout* const detailLayout = new QVBoxLayout(fieldsFrame, 8, 8);

	{
		QHBoxLayout* const idRow = new QHBoxLayout(0, 0, 8);
		idRow->addWidget(new QLabel("Family id:", fieldsFrame));
		m_idLabel = new QLabel("(no selection)", fieldsFrame);
		idRow->addWidget(m_idLabel, 1);
		detailLayout->addLayout(idRow);
	}

	{
		QHBoxLayout* const nameRow = new QHBoxLayout(0, 0, 8);
		nameRow->addWidget(new QLabel("Name:", fieldsFrame));
		m_nameEdit = new QLineEdit(fieldsFrame);
		nameRow->addWidget(m_nameEdit, 1);
		detailLayout->addLayout(nameRow);
	}

	{
		QHBoxLayout* const rgbRow = new QHBoxLayout(0, 0, 8);
		rgbRow->addWidget(new QLabel("Color (RGB):", fieldsFrame));
		m_redSpin = new QSpinBox(fieldsFrame);
		m_greenSpin = new QSpinBox(fieldsFrame);
		m_blueSpin = new QSpinBox(fieldsFrame);
		m_redSpin->setMinValue(0);
		m_redSpin->setMaxValue(255);
		m_greenSpin->setMinValue(0);
		m_greenSpin->setMaxValue(255);
		m_blueSpin->setMinValue(0);
		m_blueSpin->setMaxValue(255);
		rgbRow->addWidget(m_redSpin);
		rgbRow->addWidget(m_greenSpin);
		rgbRow->addWidget(m_blueSpin);
		rgbRow->addStretch(1);
		detailLayout->addLayout(rgbRow);
	}

	{
		QHBoxLayout* const surfRow = new QHBoxLayout(0, 0, 8);
		surfRow->addWidget(new QLabel("Surface properties:", fieldsFrame));
		m_surfacePropertiesEdit = new QLineEdit(fieldsFrame);
		surfRow->addWidget(m_surfacePropertiesEdit, 1);
		detailLayout->addLayout(surfRow);
	}

	{
		QHBoxLayout* const clampRow = new QHBoxLayout(0, 0, 8);
		clampRow->addWidget(new QLabel("Feather clamp:", fieldsFrame));
		m_featherClampEdit = new QLineEdit(fieldsFrame);
		m_featherClampEdit->setMinimumWidth(80);
		m_featherClampEdit->setText("1.0");
		clampRow->addWidget(m_featherClampEdit, 1);
		detailLayout->addLayout(clampRow);
	}

	detailLayout->addWidget(new QLabel("Children:", fieldsFrame));

	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Shader template");
	m_childList->addColumn("Weight");
	m_childList->setSorting(-1);
	m_childList->setMinimumHeight(170);
	m_childList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	detailLayout->addWidget(m_childList, 1);

	QHBoxLayout* const childBtns = new QHBoxLayout(0, 0, 8);
	m_addChildShaderButton = new QPushButton("Add shader...", fieldsFrame);
	m_removeChildButton = new QPushButton("Remove selected child", fieldsFrame);
	childBtns->addWidget(m_addChildShaderButton);
	childBtns->addWidget(m_removeChildButton);
	childBtns->addStretch(1);
	detailLayout->addLayout(childBtns);

	mainLayout->addWidget(fieldsFrame);
	mainLayout->setStretchFactor(m_familyList, 1);
	mainLayout->setStretchFactor(fieldsFrame, 2);

	QHBoxLayout* const buttonRow = new QHBoxLayout(0, 0, 6);
	m_addFamilyButton = new QPushButton("Add family", this);
	m_removeFamilyButton = new QPushButton("Remove family", this);
	m_applyButton = new QPushButton("Apply", this);
	m_closeButton = new QPushButton("Close", this);
	buttonRow->addWidget(m_addFamilyButton);
	buttonRow->addWidget(m_removeFamilyButton);
	buttonRow->addWidget(m_applyButton);
	buttonRow->addStretch(1);
	buttonRow->addWidget(m_closeButton);
	mainLayout->addLayout(buttonRow);

	IGNORE_RETURN(connect(m_familyList, SIGNAL(selectionChanged()), this, SLOT(onFamilyListSelectionChanged())));
	IGNORE_RETURN(connect(m_addFamilyButton, SIGNAL(clicked()), this, SLOT(onAddFamily())));
	IGNORE_RETURN(connect(m_removeFamilyButton, SIGNAL(clicked()), this, SLOT(onRemoveFamily())));
	IGNORE_RETURN(connect(m_addChildShaderButton, SIGNAL(clicked()), this, SLOT(onAddChildShader())));
	IGNORE_RETURN(connect(m_removeChildButton, SIGNAL(clicked()), this, SLOT(onRemoveChild())));
	IGNORE_RETURN(connect(m_applyButton, SIGNAL(clicked()), this, SLOT(onApplyEdits())));
	IGNORE_RETURN(connect(m_closeButton, SIGNAL(clicked()), this, SLOT(onClose())));
}

// ----------------------------------------------------------------------

ShaderFamilyEditorWindow::~ShaderFamilyEditorWindow()
{
	if (s_shaderFamilyEditorWindow == this)
		s_shaderFamilyEditorWindow = 0;
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::reloadFromTerrain()
{
	rebuildFamilyList();
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::rebuildFamilyList()
{
	m_listFamilyIds.clear();

	if (m_familyList)
		m_familyList->clear();

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		if (m_idLabel)
			m_idLabel->setText("(no terrain)");
		return;
	}

	ShaderGroup const& sg = gen->getShaderGroup();
	int const n = sg.getNumberOfFamilies();
	for (int i = 0; i < n; ++i)
	{
		int const fid = sg.getFamilyId(i);
		m_listFamilyIds.push_back(fid);
		char const* nm = sg.getFamilyName(fid);
		QString line;
		line.sprintf("%d  %s", fid, nm ? nm : "");
		if (m_familyList)
			m_familyList->insertItem(line);
	}

	if (m_familyList && m_familyList->count() > 0)
	{
		m_familyList->setCurrentItem(0);
		onFamilyListSelectionChanged();
	}
	else if (m_idLabel)
	{
		m_idLabel->setText("(no families)");
	}
}

// ----------------------------------------------------------------------

int ShaderFamilyEditorWindow::selectedFamilyId() const
{
	if (!m_familyList || m_listFamilyIds.empty())
		return -1;
	int const row = m_familyList->currentItem();
	if (row < 0 || row >= static_cast<int>(m_listFamilyIds.size()))
		return -1;
	return m_listFamilyIds[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::rebuildChildList(int familyId)
{
	if (!m_childList)
		return;

	m_childList->clear();

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getShaderGroup().hasFamily(familyId))
		return;

	ShaderGroup const& sg = gen->getShaderGroup();
	int const nc = sg.getFamilyNumberOfChildren(familyId);
	for (int i = 0; i < nc; ++i)
	{
		ShaderGroup::FamilyChildData const fcd(sg.getFamilyChild(familyId, i));
		QString w;
		w.sprintf("%.2f", fcd.weight);
		(void)new QListViewItem(m_childList, fcd.shaderTemplateName ? QString::fromLatin1(fcd.shaderTemplateName) : QString::fromLatin1(""), w);
	}
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::loadFieldsForFamily(int familyId)
{
	m_loadingFields = true;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getShaderGroup().hasFamily(familyId))
	{
		if (m_idLabel)
			m_idLabel->setText("(invalid)");
		if (m_nameEdit)
			m_nameEdit->setText("");
		if (m_redSpin)
			m_redSpin->setValue(0);
		if (m_greenSpin)
			m_greenSpin->setValue(0);
		if (m_blueSpin)
			m_blueSpin->setValue(0);
		if (m_surfacePropertiesEdit)
			m_surfacePropertiesEdit->setText("");
		if (m_featherClampEdit)
			m_featherClampEdit->setText("1.0");
		m_loadingFields = false;
		return;
	}

	ShaderGroup const& sg = gen->getShaderGroup();

	if (m_idLabel)
	{
		QString s;
		s.sprintf("%d", familyId);
		m_idLabel->setText(s);
	}

	char const* nm = sg.getFamilyName(familyId);
	if (m_nameEdit)
		m_nameEdit->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

	PackedRgb const pr = sg.getFamilyColor(familyId);
	if (m_redSpin)
		m_redSpin->setValue(pr.r);
	if (m_greenSpin)
		m_greenSpin->setValue(pr.g);
	if (m_blueSpin)
		m_blueSpin->setValue(pr.b);

	char const* surf = sg.getFamilySurfacePropertiesName(familyId);
	if (m_surfacePropertiesEdit)
		m_surfacePropertiesEdit->setText(surf ? QString::fromLatin1(surf) : QString::fromLatin1(""));

	float const fc = sg.getFamilyFeatherClamp(familyId);
	if (m_featherClampEdit)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.5f", fc);
		m_featherClampEdit->setText(QString::fromLatin1(buf));
	}

	rebuildChildList(familyId);

	m_loadingFields = false;
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::commitShaderGroupToTerrain()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	if (TerrainDock* const dock = MainFrame::getInstance().getTerrainDock())
	{
		dock->markLiveTerrainModified();
		dock->terrainGeneratorLiveCommit();
		dock->refreshShaderGroupUiFromGenerator();
	}
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onFamilyListSelectionChanged()
{
	if (m_loadingFields)
		return;
	loadFieldsForFamily(selectedFamilyId());
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onApplyEdits()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "No terrain generator."));
		return;
	}

	ShaderGroup& sg = gen->getShaderGroup();
	if (!sg.hasFamily(fid))
		return;

	QString const fname = m_nameEdit ? m_nameEdit->text().stripWhiteSpace() : QString::null;
	if (fname.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "Family name must not be empty."));
		return;
	}
	sg.setFamilyName(fid, fname.latin1());

	bool ok = false;
	float featherClamp = m_featherClampEdit ? m_featherClampEdit->text().toFloat(&ok) : 1.f;
	if (!ok)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "Feather clamp must be a number."));
		return;
	}
	if (featherClamp < 0.f || featherClamp > 1.f)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "Feather clamp should be between 0 and 1."));
		return;
	}

	PackedRgb rgb(
		static_cast<uint8>(m_redSpin ? m_redSpin->value() : 0),
		static_cast<uint8>(m_greenSpin ? m_greenSpin->value() : 0),
		static_cast<uint8>(m_blueSpin ? m_blueSpin->value() : 0));

	QString const surf = m_surfacePropertiesEdit ? m_surfacePropertiesEdit->text().stripWhiteSpace() : QString::null;
	sg.setFamilySurfacePropertiesName(fid, surf.latin1());
	sg.setFamilyFeatherClamp(fid, featherClamp);
	sg.setFamilyColor(fid, rgb);
	sg.loadSurfaceProperties();

	int const sel = m_familyList ? m_familyList->currentItem() : 0;
	commitShaderGroupToTerrain();
	rebuildFamilyList();
	int const listCount = m_familyList ? static_cast<int>(m_familyList->count()) : 0;
	if (m_familyList && sel >= 0 && sel < listCount)
	{
		m_familyList->setCurrentItem(sel);
		onFamilyListSelectionChanged();
	}

	MainFrame::getInstance().textToConsole("Shader family updated.");
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onAddFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "No terrain generator."));
		return;
	}

	int const newId = shaderFindUnusedFamilyId(*gen);
	if (newId < 0)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "No free family ids (1-255)."));
		return;
	}

	char defaultName[64];
	snprintf(defaultName, sizeof(defaultName), "shader_%d", newId);
	gen->getShaderGroup().addFamily(newId, defaultName, PackedRgb(160, 160, 160));
	gen->getShaderGroup().setFamilyFeatherClamp(newId, 1.f);

	commitShaderGroupToTerrain();
	rebuildFamilyList();

	for (size_t i = 0; i < m_listFamilyIds.size(); ++i)
	{
		if (static_cast<int>(m_listFamilyIds[i]) == newId && m_familyList)
		{
			m_familyList->setCurrentItem(static_cast<int>(i));
			break;
		}
	}
	onFamilyListSelectionChanged();
	MainFrame::getInstance().textToConsole("Added shader family.");
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onRemoveFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	int const answer = QMessageBox::question(
		this,
		"Shader families",
		"Remove this shader family from the live generator?",
		QMessageBox::Yes,
		QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	gen->getShaderGroup().removeFamily(fid);
	commitShaderGroupToTerrain();
	rebuildFamilyList();
	MainFrame::getInstance().textToConsole("Removed shader family.");
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onAddChildShader()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	QString const dir = QString::fromLatin1(ConfigGodClient::getData().localClientDataPath);
	QString const sht = godClientOpenAsset(this, "Assign shader template",
		QString::fromLatin1("Shaders (*.sht);;All (*.*)"),
		dir);
	if (sht.isEmpty())
		return;

	ShaderGroup& sg = gen->getShaderGroup();
	if (!sg.hasFamily(fid))
		return;

	ShaderGroup::FamilyChildData fcd;
	int const nc = sg.getFamilyNumberOfChildren(fid);
	fcd.familyId = fid;

	if (nc > 0)
	{
		ShaderGroup::FamilyChildData const prev = sg.getFamilyChild(fid, nc - 1);
		fcd.weight = prev.weight;
	}
	else
		fcd.weight = 1.f;

	QString held = sht.stripWhiteSpace();
	fcd.shaderTemplateName = held.latin1();

	sg.addChild(fcd);
	sg.loadSurfaceProperties();
	commitShaderGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Added shader family child.");
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onRemoveChild()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	QListViewItem* cur = m_childList ? m_childList->currentItem() : 0;
	if (!cur)
		return;

	QString const shName = cur->text(0).stripWhiteSpace();
	if (shName.isEmpty())
		return;

	ShaderGroup& sg = gen->getShaderGroup();
	if (!sg.hasFamily(fid))
		return;

	ShaderGroup::FamilyChildData rm;
	rm.familyId = fid;
	QString shHeld = shName;
	rm.shaderTemplateName = shHeld.latin1();
	sg.removeChild(rm);
	sg.loadSurfaceProperties();
	commitShaderGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Removed shader family child.");
}

// ----------------------------------------------------------------------

void ShaderFamilyEditorWindow::onClose()
{
	hide();
}
