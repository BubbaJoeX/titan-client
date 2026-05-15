// ======================================================================
//
// FloraFamilyEditorWindow.cpp
// copyright (c) 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "FloraFamilyEditorWindow.h"
#include "FloraFamilyEditorWindow.moc"

#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "sharedMath/PackedRgb.h"
#include "sharedTerrain/FloraGroup.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <qcheckbox.h>
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
	FloraFamilyEditorWindow* s_floraFamilyEditorWindow = 0;
}

// ======================================================================

void FloraFamilyEditorWindow::showSingleton(QWidget* parent)
{
	if (!s_floraFamilyEditorWindow)
		s_floraFamilyEditorWindow = new FloraFamilyEditorWindow(parent);

	s_floraFamilyEditorWindow->reloadFromTerrain();
	s_floraFamilyEditorWindow->show();
	s_floraFamilyEditorWindow->raise();
	s_floraFamilyEditorWindow->setActiveWindow();
}

// ======================================================================

FloraFamilyEditorWindow::FloraFamilyEditorWindow(QWidget* parent, const char* name)
: QDialog(parent, name, false),
  m_familyList(0),
  m_idLabel(0),
  m_nameLabel(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_densityEdit(0),
  m_floatsCheck(0),
  m_childList(0),
  m_applyButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Flora families");
	resize(620, 480);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edit density, floating placement, and preview color. Child appearances are read-only (edit .trn externally).",
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

	grid->addWidget(new QLabel("Density:", fieldsFrame), 3, 0);
	m_densityEdit = new QLineEdit(fieldsFrame);
	m_densityEdit->setText("0.5");
	grid->addMultiCellWidget(m_densityEdit, 3, 1, 3, 3);

	m_floatsCheck = new QCheckBox("Allow floating placement", fieldsFrame);
	grid->addMultiCellWidget(m_floatsCheck, 4, 0, 4, 3);

	grid->addWidget(new QLabel("Children:", fieldsFrame), 5, 0);
	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Appearance");
	m_childList->addColumn("Weight");
	m_childList->addColumn("Sway");
	m_childList->addColumn("Displacement");
	m_childList->addColumn("Period");
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

FloraFamilyEditorWindow::~FloraFamilyEditorWindow()
{
	if (s_floraFamilyEditorWindow == this)
		s_floraFamilyEditorWindow = 0;
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::reloadFromTerrain()
{
	rebuildFamilyList();
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::rebuildFamilyList()
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

	FloraGroup const& fg = gen->getFloraGroup();
	int const n = fg.getNumberOfFamilies();
	for (int i = 0; i < n; ++i)
	{
		int const fid = fg.getFamilyId(i);
		m_listFamilyIds.push_back(fid);
		char const* nm = fg.getFamilyName(fid);
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

int FloraFamilyEditorWindow::selectedFamilyId() const
{
	if (!m_familyList || m_listFamilyIds.empty())
		return -1;
	int const row = m_familyList->currentItem();
	if (row < 0 || row >= static_cast<int>(m_listFamilyIds.size()))
		return -1;
	return m_listFamilyIds[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::rebuildChildList(int familyId)
{
	if (!m_childList)
		return;

	m_childList->clear();

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getFloraGroup().hasFamily(familyId))
		return;

	FloraGroup const& fg = gen->getFloraGroup();
	int const nc = fg.getFamilyNumberOfChildren(familyId);
	for (int i = 0; i < nc; ++i)
	{
		FloraGroup::FamilyChildData const& fcd = fg.getFamilyChild(familyId, i);
		QString w, sway, disp, per;
		w.sprintf("%.2f", fcd.weight);
		sway = fcd.shouldSway ? QString::fromLatin1("yes") : QString::fromLatin1("no");
		disp.sprintf("%.2f", fcd.displacement);
		per.sprintf("%.2f", fcd.period);
		QString const ap = fcd.appearanceTemplateName ? QString::fromLatin1(fcd.appearanceTemplateName) : QString::fromLatin1("");
		(void)new QListViewItem(m_childList, ap, w, sway, disp, per);
	}
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::loadFieldsForFamily(int familyId)
{
	m_loadingFields = true;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getFloraGroup().hasFamily(familyId))
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
		if (m_densityEdit)
			m_densityEdit->setText("0.5");
		if (m_floatsCheck)
			m_floatsCheck->setChecked(false);
		m_loadingFields = false;
		return;
	}

	FloraGroup const& fg = gen->getFloraGroup();

	if (m_idLabel)
	{
		QString s;
		s.sprintf("%d", familyId);
		m_idLabel->setText(s);
	}

	char const* nm = fg.getFamilyName(familyId);
	if (m_nameLabel)
		m_nameLabel->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

	PackedRgb const pr = fg.getFamilyColor(familyId);
	if (m_redSpin)
		m_redSpin->setValue(pr.r);
	if (m_greenSpin)
		m_greenSpin->setValue(pr.g);
	if (m_blueSpin)
		m_blueSpin->setValue(pr.b);

	float const d = fg.getFamilyDensity(familyId);
	if (m_densityEdit)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.5f", d);
		m_densityEdit->setText(QString::fromLatin1(buf));
	}

	if (m_floatsCheck)
		m_floatsCheck->setChecked(fg.getFamilyFloats(familyId));

	rebuildChildList(familyId);

	m_loadingFields = false;
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::commitFloraGroupToTerrain()
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
		dock->refreshFloraFamilyComboFromGenerator();
	}
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onFamilyListSelectionChanged()
{
	if (m_loadingFields)
		return;
	loadFieldsForFamily(selectedFamilyId());
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onApplyEdits()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "No terrain generator."));
		return;
	}

	FloraGroup& fg = gen->getFloraGroup();
	if (!fg.hasFamily(fid))
		return;

	bool ok = false;
	float density = m_densityEdit ? m_densityEdit->text().toFloat(&ok) : 0.5f;
	if (!ok)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "Density must be a number."));
		return;
	}
	if (density < 0.f || density > 1.f)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "Density should be between 0 and 1."));
		return;
	}

	PackedRgb rgb(
		static_cast<uint8>(m_redSpin ? m_redSpin->value() : 0),
		static_cast<uint8>(m_greenSpin ? m_greenSpin->value() : 0),
		static_cast<uint8>(m_blueSpin ? m_blueSpin->value() : 0));

	fg.setFamilyDensity(fid, density);
	fg.setFamilyFloats(fid, m_floatsCheck ? m_floatsCheck->isChecked() : false);
	fg.setFamilyColor(fid, rgb);

	int const sel = m_familyList ? m_familyList->currentItem() : 0;
	commitFloraGroupToTerrain();
	rebuildFamilyList();
	int const listCount = m_familyList ? static_cast<int>(m_familyList->count()) : 0;
	if (m_familyList && sel >= 0 && sel < listCount)
	{
		m_familyList->setCurrentItem(sel);
		onFamilyListSelectionChanged();
	}

	MainFrame::getInstance().textToConsole("Flora family updated.");
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onClose()
{
	hide();
}
