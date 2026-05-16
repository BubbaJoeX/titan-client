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
#include "GodClientVirtualPath.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "ConfigGodClient.h"
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
#include <qsizepolicy.h>
#include <qspinbox.h>

#include <cstdio>

namespace
{
	RadialFamilyEditorWindow* s_radialFamilyEditorWindow = 0;

	static int radialFindUnusedFamilyId(TerrainGenerator& gen)
	{
		for (int id = 1; id <= 255; ++id)
			if (!gen.getRadialGroup().hasFamily(id))
				return id;
		return -1;
	}
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
  m_nameEdit(0),
  m_redSpin(0),
  m_greenSpin(0),
  m_blueSpin(0),
  m_densityEdit(0),
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
	setCaption("Radial (dynamic flora) families");
	resize(760, 520);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Edit radial families (.trn-facing parameters and shader children). Shader children use the same TerrainGenerator rules as TerrainEditor.",
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

	detailLayout->addWidget(new QLabel("Children:", fieldsFrame));

	m_childList = new QListView(fieldsFrame);
	m_childList->addColumn("Shader");
	m_childList->addColumn("Wt");
	m_childList->addColumn("Dist");
	m_childList->addColumn("W min/max");
	m_childList->addColumn("H min/max");
	m_childList->addColumn("Details");
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
	if (m_nameEdit)
		m_nameEdit->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

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

	QString const fname = m_nameEdit ? m_nameEdit->text().stripWhiteSpace() : QString::null;
	if (fname.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "Family name must not be empty."));
		return;
	}
	rg.setFamilyName(fid, fname.latin1());

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

void RadialFamilyEditorWindow::onAddFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "No terrain generator."));
		return;
	}

	int const newId = radialFindUnusedFamilyId(*gen);
	if (newId < 0)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "No free family ids (1-255)."));
		return;
	}

	char defaultName[64];
	snprintf(defaultName, sizeof(defaultName), "radial_%d", newId);

	RadialGroup& rg = gen->getRadialGroup();
	rg.addFamily(newId, defaultName, PackedRgb(144, 144, 160));
	rg.setFamilyDensity(newId, 0.5f);

	commitRadialGroupToTerrain();
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
	MainFrame::getInstance().textToConsole("Added radial family.");
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onRemoveFamily()
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
		"Radial families",
		"Remove this radial family from the live generator?",
		QMessageBox::Yes,
		QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	gen->getRadialGroup().removeFamily(fid);
	commitRadialGroupToTerrain();
	rebuildFamilyList();
	MainFrame::getInstance().textToConsole("Removed radial family.");
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onAddChildShader()
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
	QString const sht = godClientOpenAsset(this, "Assign radial shader (.sht)",
		QString::fromLatin1("Shaders (*.sht);;All (*.*)"),
		dir);
	if (sht.isEmpty())
		return;

	RadialGroup& rg = gen->getRadialGroup();
	if (!rg.hasFamily(fid))
		return;

	RadialGroup::FamilyChildData fcd;

	int const nc = rg.getFamilyNumberOfChildren(fid);
	if (nc > 0)
		fcd = rg.getFamilyChild(fid, nc - 1);
	else
	{
		fcd = RadialGroup::FamilyChildData();
		fcd.weight = 1.f;
		fcd.distance = 64.f;
		fcd.minWidth = 8.f;
		fcd.maxWidth = 24.f;
		fcd.minHeight = 8.f;
		fcd.maxHeight = 24.f;
		fcd.maintainAspectRatio = true;
		fcd.period = 0.1f;
		fcd.displacement = 0.02f;
		fcd.shouldSway = false;
		fcd.alignToTerrain = true;
		fcd.createPlus = false;
	}

	fcd.familyId = fid;
	QString heldSh = sht.stripWhiteSpace();
	fcd.shaderTemplateName = heldSh.latin1();

	rg.addChild(fcd);
	commitRadialGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Added radial shader child.");
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onRemoveChild()
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

	RadialGroup& rg = gen->getRadialGroup();
	if (!rg.hasFamily(fid))
		return;

	RadialGroup::FamilyChildData rm;
	rm.familyId = fid;
	QString shHeld = shName;
	rm.shaderTemplateName = shHeld.latin1();
	rg.removeChild(rm);
	commitRadialGroupToTerrain();
	rebuildChildList(fid);
	MainFrame::getInstance().textToConsole("Removed radial shader child.");
}

// ----------------------------------------------------------------------

void RadialFamilyEditorWindow::onClose()
{
	hide();
}
