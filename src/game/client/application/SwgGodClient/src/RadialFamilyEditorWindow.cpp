// ======================================================================
//
// RadialFamilyEditorWindow.cpp
// copyright (c) 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "RadialFamilyEditorWindow.h"
#include "RadialFamilyEditorWindow.moc"

#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "sharedMath/PackedRgb.h"
#include "sharedTerrain/RadialGroup.h"
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
	RadialFamilyEditorWindow* s_radialFamilyEditorWindow = 0;
}

// ======================================================================

void RadialFamilyEditorWindow::showSingleton(QWidget* parent)
{
	if (!s_radialFamilyEditorWindow)
		s_radialFamilyEditorWindow = new RadialFamilyEditorWindow(parent);

	s_radialFamilyEditorWindow->reloadFromTerrain();
	s_radialFamilyEditorWindow->show();
	s_radialFamilyEditorWindow->raise();
	s_radialFamilyEditorWindow->setActiveWindow();
}

// ======================================================================

RadialFamilyEditorWindow::RadialFamilyEditorWindow(QWidget* parent, const char* name)
: QDialog(parent, name, false),
  m_familyList(0),
  m_idLabel(0),
  m_nameLabel(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_densityEdit(0),
  m_childList(0),
  m_applyButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Radial (dynamic flora) families");
	resize(720, 480);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edit density and preview color. Child shader templates and sizing columns are read-only (edit .trn externally).",
		this);
	mainLayout->addWidget(hint);

	m_familyList = new QListBox(this);
	m_familyList->setColumnMode(QListBox::FitToWidth);
	mainLayout->addWidget(m_familyList, 1);

	QFrame* const fieldsFrame = new QFrame(this);
	fieldsFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QGridLayout* const grid = new QGridLayout(fieldsFrame, 5, 4, 6, 6);

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

	grid->addWidget(new QLabel("Children:", fieldsFrame), 4, 0);
	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Shader");
	m_childList->addColumn("Wt");
	m_childList->addColumn("Dist");
	m_childList->addColumn("W min/max");
	m_childList->addColumn("H min/max");
	m_childList->addColumn("Details");
	m_childList->setSorting(-1);
	grid->addMultiCellWidget(m_childList, 4, 1, 4, 3);

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

RadialFamilyEditorWindow::~RadialFamilyEditorWindow()
{
	if (s_radialFamilyEditorWindow == this)
		s_radialFamilyEditorWindow = 0;
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::reloadFromTerrain()
{
	rebuildFamilyList();
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::rebuildFamilyList()
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

	RadialGroup const& rg = gen->getRadialGroup();
	int const n = rg.getNumberOfFamilies();
	for (int i = 0; i < n; ++i)
	{
		int const fid = rg.getFamilyId(i);
		m_listFamilyIds.push_back(fid);
		char const* nm = rg.getFamilyName(fid);
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

int RadialFamilyEditorWindow::selectedFamilyId() const
{
	if (!m_familyList || m_listFamilyIds.empty())
		return -1;
	int const row = m_familyList->currentItem();
	if (row < 0 || row >= static_cast<int>(m_listFamilyIds.size()))
		return -1;
	return m_listFamilyIds[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::rebuildChildList(int familyId)
{
	if (!m_childList)
		return;

	m_childList->clear();

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getRadialGroup().hasFamily(familyId))
		return;

	RadialGroup const& rg = gen->getRadialGroup();
	int const nc = rg.getFamilyNumberOfChildren(familyId);
	for (int i = 0; i < nc; ++i)
	{
		RadialGroup::FamilyChildData const& fcd = rg.getFamilyChild(familyId, i);
		QString w, dist, wminmax, hminmax, details;
		w.sprintf("%.2f", fcd.weight);
		dist.sprintf("%.2f", fcd.distance);
		wminmax.sprintf("%.1f/%.1f", fcd.minWidth, fcd.maxWidth);
		hminmax.sprintf("%.1f/%.1f", fcd.minHeight, fcd.maxHeight);
		details.sprintf(
			"asp=%s sway=%s disp=%.2f per=%.2f",
			fcd.maintainAspectRatio ? "y" : "n",
			fcd.shouldSway ? "y" : "n",
			fcd.displacement,
			fcd.period);
		QString const sh = fcd.shaderTemplateName ? QString::fromLatin1(fcd.shaderTemplateName) : QString::fromLatin1("");
		(void)new QListViewItem(m_childList, sh, w, dist, wminmax, hminmax, details);
	}
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::loadFieldsForFamily(int familyId)
{
	m_loadingFields = true;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getRadialGroup().hasFamily(familyId))
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
		m_loadingFields = false;
		return;
	}

	RadialGroup const& rg = gen->getRadialGroup();

	if (m_idLabel)
	{
		QString s;
		s.sprintf("%d", familyId);
		m_idLabel->setText(s);
	}

	char const* nm = rg.getFamilyName(familyId);
	if (m_nameLabel)
		m_nameLabel->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

	PackedRgb const pr = rg.getFamilyColor(familyId);
	if (m_redSpin)
		m_redSpin->setValue(pr.r);
	if (m_greenSpin)
		m_greenSpin->setValue(pr.g);
	if (m_blueSpin)
		m_blueSpin->setValue(pr.b);

	float const d = rg.getFamilyDensity(familyId);
	if (m_densityEdit)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.5f", d);
		m_densityEdit->setText(QString::fromLatin1(buf));
	}

	rebuildChildList(familyId);

	m_loadingFields = false;
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::commitRadialGroupToTerrain()
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
		dock->refreshRadialFamilyComboFromGenerator();
	}
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onFamilyListSelectionChanged()
{
	if (m_loadingFields)
		return;
	loadFieldsForFamily(selectedFamilyId());
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onApplyEdits()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "No terrain generator."));
		return;
	}

	RadialGroup& rg = gen->getRadialGroup();
	if (!rg.hasFamily(fid))
		return;

	bool ok = false;
	float density = m_densityEdit ? m_densityEdit->text().toFloat(&ok) : 0.5f;
	if (!ok)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "Density must be a number."));
		return;
	}
	if (density < 0.f || density > 1.f)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "Density should be between 0 and 1."));
		return;
	}

	PackedRgb rgb(
		static_cast<uint8>(m_redSpin ? m_redSpin->value() : 0),
		static_cast<uint8>(m_greenSpin ? m_greenSpin->value() : 0),
		static_cast<uint8>(m_blueSpin ? m_blueSpin->value() : 0));

	rg.setFamilyDensity(fid, density);
	rg.setFamilyColor(fid, rgb);

	int const sel = m_familyList ? m_familyList->currentItem() : 0;
	commitRadialGroupToTerrain();
	rebuildFamilyList();
	int const listCount = m_familyList ? static_cast<int>(m_familyList->count()) : 0;
	if (m_familyList && sel >= 0 && sel < listCount)
	{
		m_familyList->setCurrentItem(sel);
		onFamilyListSelectionChanged();
	}

	MainFrame::getInstance().textToConsole("Radial family updated.");
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onClose()
{
	hide();
}
