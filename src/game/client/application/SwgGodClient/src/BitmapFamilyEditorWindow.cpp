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
#include "GodClientVirtualPath.h"
#include "MainFrame.h"
#include "TerrainDock.h"

#include "ConfigGodClient.h"
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

	static QString bitmapTerrainBasenameUi(char const* stored)
	{
		if (!stored || !stored[0])
			return QString();
		QString s = QString::fromLatin1(stored);
		QString const low = s.lower();
		if (low.endsWith(QString::fromLatin1(".tga")))
			s = s.left(static_cast<int>(s.length()) - 4);
		QString const prefix = QString::fromLatin1("terrain/");
		if (low.startsWith(prefix))
			s = s.mid(static_cast<int>(prefix.length()));
		return s;
	}
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
  m_addFamilyButton(0),
  m_removeFamilyButton(0),
  m_browseTgaBasenameButton(0),
  m_reloadTgaButton(0),
  m_applyButton(0),
  m_closeButton(0),
  m_listFamilyIds(),
  m_loadingFields(false)
{
	setCaption("Bitmap stamp families");
	resize(520, 400);

	QVBoxLayout* const mainLayout = new QVBoxLayout(this, 8, 6);

	QLabel* const hint = new QLabel(
		"Rename stamp families (display name). TGA basename is terrain/<basename>.tga — browse .tga from client data or paste a basename.",
		this);
	mainLayout->addWidget(hint);

	m_familyList = new QListBox(this);
	m_familyList->setColumnMode(QListBox::FitToWidth);
	mainLayout->addWidget(m_familyList);

	QFrame* const fieldsFrame = new QFrame(this);
	fieldsFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	QGridLayout* const grid = new QGridLayout(fieldsFrame, 4, 3, 6, 6);

	m_idLabel = new QLabel("(no selection)", fieldsFrame);
	grid->addWidget(new QLabel("Family id:", fieldsFrame), 0, 0);
	grid->addMultiCellWidget(m_idLabel, 0, 1, 1, 2);

	grid->addWidget(new QLabel("Display name:", fieldsFrame), 1, 0);
	m_displayNameEdit = new QLineEdit(fieldsFrame);
	grid->addMultiCellWidget(m_displayNameEdit, 1, 1, 1, 2);

	grid->addWidget(new QLabel("TGA basename:", fieldsFrame), 2, 0);
	m_tgaBasenameEdit = new QLineEdit(fieldsFrame);
	m_browseTgaBasenameButton = new QPushButton("Browse...", fieldsFrame);
	QHBoxLayout* const basenameRow = new QHBoxLayout(0, 0, 4);
	basenameRow->addWidget(m_tgaBasenameEdit, 1);
	basenameRow->addWidget(m_browseTgaBasenameButton);
	grid->addMultiCellLayout(basenameRow, 2, 1, 1, 2);

	m_hintLabel = new QLabel("Examples: my_mask loads terrain/my_mask.tga — pick greyscale PF_w_8 .tga from client data.", fieldsFrame);
	grid->addMultiCellWidget(m_hintLabel, 3, 0, 1, 3);

	mainLayout->addWidget(fieldsFrame);

	mainLayout->setStretchFactor(m_familyList, 1);
	mainLayout->setStretchFactor(fieldsFrame, 2);

	QHBoxLayout* const buttonRow = new QHBoxLayout(0, 0, 6);
	m_addFamilyButton = new QPushButton("Add family", this);
	m_removeFamilyButton = new QPushButton("Remove family", this);
	m_reloadTgaButton = new QPushButton("Reload TGA", this);
	m_applyButton = new QPushButton("Apply name", this);
	m_closeButton = new QPushButton("Close", this);
	buttonRow->addWidget(m_addFamilyButton);
	buttonRow->addWidget(m_removeFamilyButton);
	buttonRow->addWidget(m_reloadTgaButton);
	buttonRow->addWidget(m_applyButton);
	buttonRow->addStretch(1);
	buttonRow->addWidget(m_closeButton);
	mainLayout->addLayout(buttonRow);

	IGNORE_RETURN(connect(m_familyList, SIGNAL(selectionChanged()), this, SLOT(onFamilyListSelectionChanged())));
	IGNORE_RETURN(connect(m_addFamilyButton, SIGNAL(clicked()), this, SLOT(onAddFamily())));
	IGNORE_RETURN(connect(m_removeFamilyButton, SIGNAL(clicked()), this, SLOT(onRemoveFamily())));
	IGNORE_RETURN(connect(m_browseTgaBasenameButton, SIGNAL(clicked()), this, SLOT(onBrowseTgaBasename())));
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
		m_tgaBasenameEdit->setText(bitmapTerrainBasenameUi(bg.getFamilyBitmapBasename(familyId)));
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

void BitmapFamilyEditorWindow::onAddFamily()
{
	TerrainGenerator* const gen = GodClientTerrainEditor::isInstalled()
		? GodClientTerrainEditor::getInstance().getTerrainGenerator()
		: 0;
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "No terrain generator."));
		return;
	}

	BitmapGroup& bg = gen->getBitmapGroup();

	int newId = 1;
	while (bg.hasFamily(newId))
		++newId;

	QString nameBase;
	nameBase.sprintf("stamp_%d", newId);
	QString name = nameBase;
	int unusedId = 0;
	for (int tag = 2; bg.findFamily(name.latin1(), unusedId) && tag < 100002; ++tag)
		name = QString("%1_%2").arg(nameBase).arg(tag);

	bg.addFamily(newId, name.latin1(), "");

	commitBitmapGroupToTerrain(newId);
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
	MainFrame::getInstance().textToConsole("Added bitmap stamp family.");
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onRemoveFamily()
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
		"Bitmap families",
		"Remove this bitmap stamp family from the live generator?",
		QMessageBox::Yes,
		QMessageBox::No);
	if (answer != QMessageBox::Yes)
		return;

	gen->getBitmapGroup().removeFamily(fid);
	commitBitmapGroupToTerrain(-1);
	rebuildFamilyList();
	MainFrame::getInstance().textToConsole("Removed bitmap stamp family.");
}

// ----------------------------------------------------------------------

void BitmapFamilyEditorWindow::onBrowseTgaBasename()
{
	QString const dir = QString::fromLatin1(ConfigGodClient::getData().localClientDataPath);
	QString const pick = godClientOpenAsset(this, "Greyscale terrain stamp .tga",
		QString::fromLatin1("TGA images (*.tga);;All (*.*)"),
		dir);
	if (pick.isEmpty())
		return;

	QString const trimmed = bitmapTerrainBasenameUi(pick.stripWhiteSpace().latin1());
	if (!trimmed.isEmpty() && m_tgaBasenameEdit)
		m_tgaBasenameEdit->setText(trimmed);
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
