// ======================================================================
//
// EnvironmentEditorWindow.cpp
// copyright (c) 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "EnvironmentEditorWindow.h"
#include "EnvironmentEditorWindow.moc"

#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "sharedMath/PackedRgb.h"
#include "sharedTerrain/EnvironmentGroup.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>

#include <cstdio>

namespace
{
	EnvironmentEditorWindow* s_environmentEditorWindow = 0;
}

// ======================================================================

void EnvironmentEditorWindow::showSingleton(QWidget* parent)
{
	if (!s_environmentEditorWindow)
		s_environmentEditorWindow = new EnvironmentEditorWindow(parent);

	s_environmentEditorWindow->reloadFromTerrain();
	s_environmentEditorWindow->show();
	s_environmentEditorWindow->raise();
	s_environmentEditorWindow->setActiveWindow();
}

// ======================================================================

EnvironmentEditorWindow::EnvironmentEditorWindow(QWidget* parent, const char* name)
: QDialog(parent, name, false),
  m_familyList(0),
  m_idLabel(0),
  m_nameEdit(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_featherClampEdit(0),
  m_applyButton(0),
  m_addButton(0),
  m_removeButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Environment families");
	resize(520, 420);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edits apply to the live terrain generator (save the .trn from the terrain dock to persist).",
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
	m_nameEdit = new QLineEdit(fieldsFrame);
	grid->addMultiCellWidget(m_nameEdit, 1, 1, 1, 3);

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

	grid->addWidget(new QLabel("Feather clamp:", fieldsFrame), 3, 0);
	m_featherClampEdit = new QLineEdit(fieldsFrame);
	m_featherClampEdit->setText("1.0");
	grid->addMultiCellWidget(m_featherClampEdit, 3, 1, 3, 3);

	mainLayout->addWidget(fieldsFrame);

	QHBoxLayout* const buttonRow = new QHBoxLayout(0, 0, 6);
	m_applyButton = new QPushButton("Apply row", this);
	m_addButton = new QPushButton("Add family", this);
	m_removeButton = new QPushButton("Remove family", this);
	m_closeButton = new QPushButton("Close", this);
	buttonRow->addWidget(m_applyButton);
	buttonRow->addWidget(m_addButton);
	buttonRow->addWidget(m_removeButton);
	buttonRow->addStretch(1);
	buttonRow->addWidget(m_closeButton);
	mainLayout->addLayout(buttonRow);

	IGNORE_RETURN(connect(m_familyList, SIGNAL(selectionChanged()), this, SLOT(onFamilyListSelectionChanged())));
	IGNORE_RETURN(connect(m_applyButton, SIGNAL(clicked()), this, SLOT(onApplyEdits())));
	IGNORE_RETURN(connect(m_addButton, SIGNAL(clicked()), this, SLOT(onAddFamily())));
	IGNORE_RETURN(connect(m_removeButton, SIGNAL(clicked()), this, SLOT(onRemoveFamily())));
	IGNORE_RETURN(connect(m_closeButton, SIGNAL(clicked()), this, SLOT(onClose())));
}

// ----------------------------------------------------------------------

EnvironmentEditorWindow::~EnvironmentEditorWindow()
{
	if (s_environmentEditorWindow == this)
		s_environmentEditorWindow = 0;
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::reloadFromTerrain()
{
	rebuildFamilyList();
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::rebuildFamilyList()
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

	EnvironmentGroup& eg = gen->getEnvironmentGroup();
	int const n = eg.getNumberOfFamilies();
	for (int i = 0; i < n; ++i)
	{
		int const fid = eg.getFamilyId(i);
		m_listFamilyIds.push_back(fid);
		char const* nm = eg.getFamilyName(fid);
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

int EnvironmentEditorWindow::selectedFamilyId() const
{
	if (!m_familyList || m_listFamilyIds.empty())
		return -1;
	int const row = m_familyList->currentItem();
	if (row < 0 || row >= static_cast<int>(m_listFamilyIds.size()))
		return -1;
	return m_listFamilyIds[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::loadFieldsForFamily(int familyId)
{
	m_loadingFields = true;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getEnvironmentGroup().hasFamily(familyId))
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
		if (m_featherClampEdit)
			m_featherClampEdit->setText("1.0");
		m_loadingFields = false;
		return;
	}

	EnvironmentGroup const& eg = gen->getEnvironmentGroup();

	if (m_idLabel)
	{
		QString s;
		s.sprintf("%d", familyId);
		m_idLabel->setText(s);
	}

	char const* nm = eg.getFamilyName(familyId);
	if (m_nameEdit)
		m_nameEdit->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

	PackedRgb const pr = eg.getFamilyColor(familyId);
	if (m_redSpin)
		m_redSpin->setValue(pr.r);
	if (m_greenSpin)
		m_greenSpin->setValue(pr.g);
	if (m_blueSpin)
		m_blueSpin->setValue(pr.b);

	float const fc = eg.getFamilyFeatherClamp(familyId);
	if (m_featherClampEdit)
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%.4f", fc);
		m_featherClampEdit->setText(QString::fromLatin1(buf));
	}

	m_loadingFields = false;
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::commitEnvironmentGroupToTerrain()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	gen->prepare();

	if (TerrainDock* const dock = MainFrame::getInstance().getTerrainDock())
	{
		dock->markLiveTerrainModified();
		dock->terrainGeneratorLiveCommit();
		dock->refreshEnvironmentFamilyComboFromGenerator();
	}
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::onFamilyListSelectionChanged()
{
	if (m_loadingFields)
		return;
	loadFieldsForFamily(selectedFamilyId());
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::onApplyEdits()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "No terrain generator."));
		return;
	}

	EnvironmentGroup& eg = gen->getEnvironmentGroup();
	if (!eg.hasFamily(fid))
		return;

	QString const name = m_nameEdit ? m_nameEdit->text().stripWhiteSpace() : QString::null;
	if (name.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Name must not be empty."));
		return;
	}

	bool ok = false;
	float featherClamp = m_featherClampEdit ? m_featherClampEdit->text().toFloat(&ok) : 1.f;
	if (!ok)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Feather clamp must be a number."));
		return;
	}
	if (featherClamp < 0.f || featherClamp > 1.f)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Feather clamp should be between 0 and 1."));
		return;
	}

	PackedRgb rgb(
		static_cast<uint8>(m_redSpin ? m_redSpin->value() : 0),
		static_cast<uint8>(m_greenSpin ? m_greenSpin->value() : 0),
		static_cast<uint8>(m_blueSpin ? m_blueSpin->value() : 0));

	eg.setFamilyName(fid, name.latin1());
	eg.setFamilyColor(fid, rgb);
	eg.setFamilyFeatherClamp(fid, featherClamp);

	int const sel = m_familyList ? m_familyList->currentItem() : 0;
	commitEnvironmentGroupToTerrain();
	rebuildFamilyList();
	int const listCount = m_familyList ? static_cast<int>(m_familyList->count()) : 0;
	if (m_familyList && sel >= 0 && sel < listCount)
	{
		m_familyList->setCurrentItem(sel);
		onFamilyListSelectionChanged();
	}

	MainFrame::getInstance().textToConsole("Environment family updated.");
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::onAddFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "No terrain generator."));
		return;
	}

	EnvironmentGroup& eg = gen->getEnvironmentGroup();

	int newId = 1;
	for (; newId <= 255; ++newId)
	{
		if (!eg.hasFamily(newId))
			break;
	}
	if (newId > 255)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "No free family ids (1-255)."));
		return;
	}

	char defaultName[64];
	snprintf(defaultName, sizeof(defaultName), "environment_%d", newId);
	eg.addFamily(newId, defaultName, PackedRgb(128, 128, 128));

	commitEnvironmentGroupToTerrain();
	rebuildFamilyList();

	for (size_t i = 0; i < m_listFamilyIds.size(); ++i)
	{
		if (m_listFamilyIds[i] == newId && m_familyList)
		{
			m_familyList->setCurrentItem(static_cast<int>(i));
			break;
		}
	}
	onFamilyListSelectionChanged();
	MainFrame::getInstance().textToConsole("Added environment family.");
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::onRemoveFamily()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
		return;

	int const answer = QMessageBox::question(
		this,
		"Environment",
		"Remove this environment family from the live generator?",
		QMessageBox::Yes,
		QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	gen->getEnvironmentGroup().removeFamily(fid);
	commitEnvironmentGroupToTerrain();
	rebuildFamilyList();
	MainFrame::getInstance().textToConsole("Removed environment family.");
}

// ----------------------------------------------------------------------

void EnvironmentEditorWindow::onClose()
{
	hide();
}
