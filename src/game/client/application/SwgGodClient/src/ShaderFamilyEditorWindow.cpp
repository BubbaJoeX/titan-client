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
#include "MainFrame.h"
#include "TerrainDock.h"

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
#include <qspinbox.h>

#include <cstdio>

namespace
{
	ShaderFamilyEditorWindow* s_shaderFamilyEditorWindow = 0;
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
  m_nameLabel(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_surfacePropertiesEdit(0),
  m_featherClampEdit(0),
  m_childList(0),
  m_applyButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Shader families");
	resize(560, 460);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Inspect shader template children and edit surface properties, feather clamp, and preview color on the live generator.",
		this);
	mainLayout->addWidget(hint);

	m_familyList = new QListBox(this);
	m_familyList->setColumnMode(QListBox::FitToWidth);
	mainLayout->addWidget(m_familyList, 1);

	QFrame* const fieldsFrame = new QFrame(this);
	fieldsFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QGridLayout* const grid = new QGridLayout(fieldsFrame, 6, 4, 6, 6);

	m_idLabel = new QLabel("(no selection)", fieldsFrame);
	grid->addWidget(new QLabel("Family id:", fieldsFrame), 0, 0);
	grid->addMultiCellWidget(m_idLabel, 0, 0, 1, 3);

	grid->addWidget(new QLabel("Name:", fieldsFrame), 1, 0);
	m_nameLabel = new QLabel("", fieldsFrame);
	grid->addMultiCellWidget(m_nameLabel, 1, 1, 1, 3);

	grid->addWidget(new QLabel("Color (RGB):", fieldsFrame), 2, 0);
	m_redSpin = new QSpinBox(fieldsFrame);
	m_greenSpin = new QSpinBox(fieldsFrame);
	m_blueSpin = new QSpinBox(fieldsFrame);
	m_redSpin->setMinValue(0);
	m_redSpin->setMaxValue(255);
	m_greenSpin->setMinValue(0);
	m_greenSpin->setMaxValue(255);
	m_blueSpin->setMinValue(0);
	m_blueSpin->setMaxValue(255);
	QHBoxLayout* const rgbRow = new QHBoxLayout(0, 0, 4);
	rgbRow->addWidget(m_redSpin);
	rgbRow->addWidget(m_greenSpin);
	rgbRow->addWidget(m_blueSpin);
	grid->addLayout(rgbRow, 2, 1);

	grid->addWidget(new QLabel("Surface properties:", fieldsFrame), 3, 0);
	m_surfacePropertiesEdit = new QLineEdit(fieldsFrame);
	grid->addMultiCellWidget(m_surfacePropertiesEdit, 3, 1, 3, 3);

	grid->addWidget(new QLabel("Feather clamp:", fieldsFrame), 4, 0);
	m_featherClampEdit = new QLineEdit(fieldsFrame);
	m_featherClampEdit->setText("1.0");
	grid->addMultiCellWidget(m_featherClampEdit, 4, 1, 4, 3);

	grid->addWidget(new QLabel("Children:", fieldsFrame), 5, 0);
	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Shader template");
	m_childList->addColumn("Weight");
	m_childList->setSorting(-1);
	grid->addMultiCellWidget(m_childList, 5, 1, 5, 3);

	mainLayout->addWidget(fieldsFrame);

	QHBoxLayout* const buttonRow = new QHBoxLayout(0, 0, 6);
	m_applyButton = new QPushButton("Apply", this);
	m_closeButton = new QPushButton("Close", this);
	buttonRow->addWidget(m_applyButton);
	buttonRow->addStretch(1);
	buttonRow->addWidget(m_closeButton);
	mainLayout->addLayout(buttonRow);

	IGNORE_RETURN(connect(m_familyList, SIGNAL(selectionChanged()), this, SLOT(onFamilyListSelectionChanged())));
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
		if (m_nameLabel)
			m_nameLabel->setText("");
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
	if (m_nameLabel)
		m_nameLabel->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

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

void ShaderFamilyEditorWindow::onClose()
{
	hide();
}
