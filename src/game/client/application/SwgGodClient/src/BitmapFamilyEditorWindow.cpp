// ======================================================================
//
// BitmapFamilyEditorWindow.cpp
// copyright (c) 2026 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "BitmapFamilyEditorWindow.h"
#include "BitmapFamilyEditorWindow.moc"

#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "sharedTerrain/BitmapGroup.h"
#include "sharedTerrain/TerrainGenerator.h"

#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistbox.h>
#include <qmessagebox.h>
#include <qpushbutton.h>

namespace
{
	BitmapFamilyEditorWindow* s_bitmapFamilyEditorWindow = 0;
}

// ======================================================================

void BitmapFamilyEditorWindow::showSingleton(QWidget* parent)
{
	if (!s_bitmapFamilyEditorWindow)
		s_bitmapFamilyEditorWindow = new BitmapFamilyEditorWindow(parent);

	s_bitmapFamilyEditorWindow->reloadFromTerrain();
	s_bitmapFamilyEditorWindow->show();
	s_bitmapFamilyEditorWindow->raise();
	s_bitmapFamilyEditorWindow->setActiveWindow();
}

// ======================================================================

BitmapFamilyEditorWindow::BitmapFamilyEditorWindow(QWidget* parent, const char* name)
: QDialog(parent, name, false),
  m_familyList(0),
  m_idLabel(0),
  m_displayNameEdit(0),
  m_tgaBasenameEdit(0),
  m_hintLabel(0),
  m_reloadTgaButton(0),
  m_applyButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Bitmap stamp families");
	resize(480, 380);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Rename stamp families and reload greyscale .tga sources from disk (same rules as TerrainEditor: terrain/<basename>.tga, 8-bit).",
		this);
	mainLayout->addWidget(hint);

	m_familyList = new QListBox(this);
	m_familyList->setColumnMode(QListBox::FitToWidth);
	mainLayout->addWidget(m_familyList, 1);

	QFrame* const fieldsFrame = new QFrame(this);
	fieldsFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QGridLayout* const grid = new QGridLayout(fieldsFrame, 4, 2, 6, 6);

	m_idLabel = new QLabel("(no selection)", fieldsFrame);
	grid->addWidget(new QLabel("Family id:", fieldsFrame), 0, 0);
	grid->addWidget(m_idLabel, 0, 1);

	grid->addWidget(new QLabel("Display name:", fieldsFrame), 1, 0);
	m_displayNameEdit = new QLineEdit(fieldsFrame);
	grid->addWidget(m_displayNameEdit, 1, 1);

	grid->addWidget(new QLabel("TGA basename:", fieldsFrame), 2, 0);
	m_tgaBasenameEdit = new QLineEdit(fieldsFrame);
	grid->addWidget(m_tgaBasenameEdit, 2, 1);

	m_hintLabel = new QLabel("Example: my_mask loads terrain/my_mask.tga", fieldsFrame);
	grid->addMultiCellWidget(m_hintLabel, 3, 0, 3, 1);

	mainLayout->addWidget(fieldsFrame);

	QHBoxLayout* const buttonRow = new QHBoxLayout(0, 0, 6);
	m_reloadTgaButton = new QPushButton("Reload TGA", this);
	m_applyButton = new QPushButton("Apply name", this);
	m_closeButton = new QPushButton("Close", this);
	buttonRow->addWidget(m_reloadTgaButton);
	buttonRow->addWidget(m_applyButton);
	buttonRow->addStretch(1);
	buttonRow->addWidget(m_closeButton);
	mainLayout->addLayout(buttonRow);

	IGNORE_RETURN(connect(m_familyList, SIGNAL(selectionChanged()), this, SLOT(onFamilyListSelectionChanged())));
	IGNORE_RETURN(connect(m_reloadTgaButton, SIGNAL(clicked()), this, SLOT(onReloadTgaClicked())));
	IGNORE_RETURN(connect(m_applyButton, SIGNAL(clicked()), this, SLOT(onApplyEdits())));
	IGNORE_RETURN(connect(m_closeButton, SIGNAL(clicked()), this, SLOT(onClose())));
}

// ----------------------------------------------------------------------

BitmapFamilyEditorWindow::~BitmapFamilyEditorWindow()
{
	if (s_bitmapFamilyEditorWindow == this)
		s_bitmapFamilyEditorWindow = 0;
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::reloadFromTerrain()
{
	rebuildFamilyList();
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::rebuildFamilyList()
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

	BitmapGroup const& bg = gen->getBitmapGroup();
	int const n = bg.getNumberOfFamilies();
	for (int i = 0; i < n; ++i)
	{
		int const fid = bg.getFamilyId(i);
		m_listFamilyIds.push_back(fid);
		char const* nm = bg.getFamilyName(fid);
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

int BitmapFamilyEditorWindow::selectedFamilyId() const
{
	if (!m_familyList || m_listFamilyIds.empty())
		return -1;
	int const row = m_familyList->currentItem();
	if (row < 0 || row >= static_cast<int>(m_listFamilyIds.size()))
		return -1;
	return m_listFamilyIds[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::loadFieldsForFamily(int familyId)
{
	m_loadingFields = true;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen || familyId < 0 || !gen->getBitmapGroup().hasFamily(familyId))
	{
		if (m_idLabel)
			m_idLabel->setText("(invalid)");
		if (m_displayNameEdit)
			m_displayNameEdit->setText("");
		if (m_tgaBasenameEdit)
			m_tgaBasenameEdit->setText("");
		m_loadingFields = false;
		return;
	}

	BitmapGroup const& bg = gen->getBitmapGroup();

	if (m_idLabel)
	{
		QString s;
		s.sprintf("%d", familyId);
		m_idLabel->setText(s);
	}

	char const* nm = bg.getFamilyName(familyId);
	if (m_displayNameEdit)
		m_displayNameEdit->setText(nm ? QString::fromLatin1(nm) : QString::fromLatin1(""));

	if (m_tgaBasenameEdit)
		m_tgaBasenameEdit->setText("");

	m_loadingFields = false;
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::commitBitmapGroupToTerrain(int familyId)
{
	if (TerrainDock* const dock = MainFrame::getInstance().getTerrainDock())
	{
		dock->markLiveTerrainModified();
		dock->terrainGeneratorLiveCommit();
		dock->refreshBitmapStampComboFromGenerator();
		dock->reloadBitmapStampPreviewIfCurrentFamily(familyId);
	}
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onFamilyListSelectionChanged()
{
	if (m_loadingFields)
		return;
	loadFieldsForFamily(selectedFamilyId());
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onApplyEdits()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "No terrain generator."));
		return;
	}

	BitmapGroup& bg = gen->getBitmapGroup();
	if (!bg.hasFamily(fid))
		return;

	QString const disp = m_displayNameEdit ? m_displayNameEdit->text().stripWhiteSpace() : QString::null;
	if (disp.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "Display name must not be empty."));
		return;
	}

	bg.setFamilyName(fid, disp.latin1());

	int const sel = m_familyList ? m_familyList->currentItem() : 0;
	commitBitmapGroupToTerrain(fid);
	rebuildFamilyList();
	int const listCount = m_familyList ? static_cast<int>(m_familyList->count()) : 0;
	if (m_familyList && sel >= 0 && sel < listCount)
	{
		m_familyList->setCurrentItem(sel);
		onFamilyListSelectionChanged();
	}

	MainFrame::getInstance().textToConsole("Bitmap stamp family name updated.");
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onReloadTgaClicked()
{
	int const fid = selectedFamilyId();
	if (fid < 0)
		return;

	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "No terrain generator."));
		return;
	}

	BitmapGroup& bg = gen->getBitmapGroup();
	if (!bg.hasFamily(fid))
		return;

	QString const base = m_tgaBasenameEdit ? m_tgaBasenameEdit->text().stripWhiteSpace() : QString::null;
	if (base.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "Enter a TGA basename (no path, no extension)."));
		return;
	}

	bg.loadFamilyBitmap(fid, base.latin1());
	commitBitmapGroupToTerrain(fid);
	MainFrame::getInstance().textToConsole("Reloaded bitmap stamp TGA.");
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onClose()
{
	hide();
}
