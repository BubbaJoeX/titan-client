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
#include "GodClientVirtualPath.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "ConfigGodClient.h"
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
#include <qsizepolicy.h>
#include <qspinbox.h>

#include <cstdio>

namespace
{
	FloraFamilyEditorWindow* s_floraFamilyEditorWindow = 0;

	static int floraFindUnusedFamilyId(TerrainGenerator& gen)
	{
		for (int id = 1; id <= 255; ++id)
			if (!gen.getFloraGroup().hasFamily(id))
				return id;
		return -1;
	}
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
  m_nameEdit(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_densityEdit(0),
  m_floatsCheck(0),
  m_childList(0),
  m_applyButton(0),
  m_addFamilyButton(0),
  m_removeFamilyButton(0),
  m_addChildAppearanceButton(0),
  m_removeChildButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Flora families");
	resize(660, 520);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edit families and child appearances (.apt /.sat /.prt resolved like the offline terrain editor — pick files under client data when possible).",
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
		QHBoxLayout* const densityRow = new QHBoxLayout(0, 0, 8);
		densityRow->addWidget(new QLabel("Density:", fieldsFrame));
		m_densityEdit = new QLineEdit(fieldsFrame);
		m_densityEdit->setMinimumWidth(120);
		m_densityEdit->setText("0.5");
		densityRow->addWidget(m_densityEdit, 1);
		detailLayout->addLayout(densityRow);
	}

	m_floatsCheck = new QCheckBox("Allow floating placement", fieldsFrame);
	detailLayout->addWidget(m_floatsCheck);

	detailLayout->addWidget(new QLabel("Children:", fieldsFrame));

	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Appearance");
	m_childList->addColumn("Weight");
	m_childList->addColumn("Sway");
	m_childList->addColumn("Displacement");
	m_childList->addColumn("Period");
	m_childList->setSorting(-1);
	m_childList->setMinimumHeight(170);
	m_childList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	detailLayout->addWidget(m_childList, 1);

	QHBoxLayout* const childBtns = new QHBoxLayout(0, 0, 8);
	m_addChildAppearanceButton = new QPushButton("Add appearance...", fieldsFrame);
	m_removeChildButton = new QPushButton("Remove selected child", fieldsFrame);
	childBtns->addWidget(m_addChildAppearanceButton);
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
	IGNORE_RETURN(connect(m_addChildAppearanceButton, SIGNAL(clicked()), this, SLOT(onAddChildAppearance())));
	IGNORE_RETURN(connect(m_removeChildButton, SIGNAL(clicked()), this, SLOT(onRemoveChild())));
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
		if (m_nameEdit)
			m_nameEdit->setText("");
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
	if (m_nameEdit)
		m_nameEdit->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

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

	QString const fname = m_nameEdit ? m_nameEdit->text().stripWhiteSpace() : QString::null;
	if (fname.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "Family name must not be empty."));
		return;
	}
	fg.setFamilyName(fid, fname.latin1());

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

void FloraFamilyEditorWindow::onAddFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "No terrain generator."));
		return;
	}

	int const newId = floraFindUnusedFamilyId(*gen);
	if (newId < 0)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "No free family ids (1-255)."));
		return;
	}

	char defaultName[64];
	snprintf(defaultName, sizeof(defaultName), "flora_%d", newId);

	FloraGroup& fg = gen->getFloraGroup();
	fg.addFamily(newId, defaultName, PackedRgb(128, 128, 128));
	fg.setFamilyDensity(newId, 0.5f);

	commitFloraGroupToTerrain();
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
	MainFrame::getInstance().textToConsole("Added flora family.");
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onRemoveFamily()
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
		"Flora families",
		"Remove this flora family from the live generator?",
		QMessageBox::Yes,
		QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	gen->getFloraGroup().removeFamily(fid);
	commitFloraGroupToTerrain();
	rebuildFamilyList();
	MainFrame::getInstance().textToConsole("Removed flora family.");
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onAddChildAppearance()
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
	QString const apt = godClientOpenAsset(this, "Assign appearance (.apt /.sat /.prt)",
		QString::fromLatin1("Appearances (*.apt *.sat *.prt);;All (*.*)"),
		dir);
	if (apt.isEmpty())
		return;

	FloraGroup& fg = gen->getFloraGroup();
	if (!fg.hasFamily(fid))
		return;

	int const nc = fg.getFamilyNumberOfChildren(fid);
	FloraGroup::FamilyChildData fcd;

	if (nc > 0)
	{
		FloraGroup::FamilyChildData const& ref = fg.getFamilyChild(fid, nc - 1);
		fcd.weight = ref.weight;
		fcd.shouldSway = ref.shouldSway;
		fcd.period = ref.period;
		fcd.displacement = ref.displacement;
		fcd.alignToTerrain = ref.alignToTerrain;
		fcd.shouldScale = ref.shouldScale;
		fcd.minimumScale = ref.minimumScale;
		fcd.maximumScale = ref.maximumScale;
	}
	else
	{
		fcd = FloraGroup::FamilyChildData();
		fcd.weight = 1.f;
	}

	fcd.familyId = fid;

	QString aptLine = apt.stripWhiteSpace();
	fcd.appearanceTemplateName = aptLine.latin1();

	fg.addChild(fcd);
	commitFloraGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Added flora child appearance.");
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onRemoveChild()
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

	QString const apt = cur->text(0).stripWhiteSpace();
	if (apt.isEmpty())
		return;

	FloraGroup& fg = gen->getFloraGroup();
	if (!fg.hasFamily(fid))
		return;

	FloraGroup::FamilyChildData rm;
	rm.familyId = fid;
	QString aptHeld = apt;
	rm.appearanceTemplateName = aptHeld.latin1();
	fg.removeChild(rm);
	commitFloraGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Removed flora child appearance.");
}

// ----------------------------------------------------------------------

void FloraFamilyEditorWindow::onClose()
{
	hide();
}
