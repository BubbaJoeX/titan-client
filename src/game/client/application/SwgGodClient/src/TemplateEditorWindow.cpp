// ======================================================================
//
// TemplateEditorWindow.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "TemplateEditorWindow.h"
#include "TemplateEditorWindow.moc"

#include "ActionHack.h"
#include "ActionsScript.h"
#include "ActionsWindow.h"
#include "MainFrame.h"

#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/MemoryBlockManagedObject.h"
#include "sharedFile/TreeFile.h"

#include "_precompile.h"
#include "UIManager.h"
#include "UIPage.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"

#include <qcombobox.h>
#include <qdragobject.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qframe.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qsplitter.h>
#include <qtextedit.h>
#include <qdialog.h>

#include <cstdio>
#include <fstream>
#include <io.h>
#include <string>

// ======================================================================

namespace
{
	bool readTextFile(const std::string & path, std::string & out)
	{
		out.clear();
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return false;

		std::string line;
		while (std::getline(file, line))
		{
			out += line;
			out += "\n";
		}

		return true;
	}

	bool hasSuffix(const std::string & value, const char * suffix)
	{
		const size_t suffixLen = strlen(suffix);
		return value.size() >= suffixLen && value.compare(value.size() - suffixLen, suffixLen, suffix) == 0;
	}

	std::string trim(const std::string & in)
	{
		size_t b = 0;
		size_t e = in.size();
		while (b < e && (in[b] == ' ' || in[b] == '\t' || in[b] == '\r' || in[b] == '\n'))
			++b;
		while (e > b && (in[e - 1] == ' ' || in[e - 1] == '\t' || in[e - 1] == '\r' || in[e - 1] == '\n'))
			--e;
		return in.substr(b, e - b);
	}

	bool extractQuotedPair(const std::string & source, const std::string & field, std::string & outFile, std::string & outKey)
	{
		const std::string token = field + " = ";
		const std::string::size_type p = source.find(token);
		if (p == std::string::npos)
			return false;

		const std::string::size_type q1 = source.find('"', p + token.size());
		if (q1 == std::string::npos)
			return false;
		const std::string::size_type q2 = source.find('"', q1 + 1);
		if (q2 == std::string::npos)
			return false;
		const std::string::size_type q3 = source.find('"', q2 + 1);
		if (q3 == std::string::npos)
			return false;
		const std::string::size_type q4 = source.find('"', q3 + 1);
		if (q4 == std::string::npos)
			return false;

		outFile = source.substr(q1 + 1, q2 - q1 - 1);
		outKey  = source.substr(q3 + 1, q4 - q3 - 1);
		return true;
	}

	void replaceOrAppendQuotedPair(std::string & source, const std::string & field, const std::string & file, const std::string & key)
	{
		const std::string replacement = field + " = \"" + file + "\" \"" + key + "\"";
		const std::string token = field + " = ";
		const std::string::size_type p = source.find(token);
		if (p == std::string::npos)
		{
			source += "\n";
			source += replacement;
			source += "\n";
			return;
		}

		const std::string::size_type lineEnd = source.find('\n', p);
		if (lineEnd == std::string::npos)
			source.replace(p, source.size() - p, replacement);
		else
			source.replace(p, lineEnd - p, replacement);
	}

	bool extractSimpleAssignment(const std::string & source, const std::string & field, std::string & outValue)
	{
		const std::string token = field + " = ";
		const std::string::size_type p = source.find(token);
		if (p == std::string::npos)
			return false;

		const std::string::size_type start = p + token.size();
		const std::string::size_type end = source.find('\n', start);
		outValue = trim(end == std::string::npos ? source.substr(start) : source.substr(start, end - start));
		return true;
	}

	void replaceOrAppendSimpleAssignment(std::string & source, const std::string & field, const std::string & value)
	{
		const std::string replacement = field + " = " + value;
		const std::string token = field + " = ";
		const std::string::size_type p = source.find(token);
		if (p == std::string::npos)
		{
			source += "\n";
			source += replacement;
			source += "\n";
			return;
		}

		const std::string::size_type lineEnd = source.find('\n', p);
		if (lineEnd == std::string::npos)
			source.replace(p, source.size() - p, replacement);
		else
			source.replace(p, lineEnd - p, replacement);
	}

	std::string makeStringFilePath(const std::string & fileField)
	{
		if (fileField.empty())
			return std::string();
		if (fileField.find(".stf") != std::string::npos)
			return fileField;
		if (fileField.find("dsrc/") == 0 || fileField.find("D:/") == 0 || fileField.find("d:\\") == 0)
			return fileField;
		return "dsrc/sku.0/sys.shared/compiled/game/string/en/" + fileField + ".stf";
	}

	std::string escapeForCommand(const std::string & value)
	{
		std::string out;
		for (size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] == '"')
				out += "\\\"";
			else
				out += value[i];
		}
		return out;
	}

	void addComboIfMissing(QComboBox * combo, const std::string & value)
	{
		if (!combo || value.empty())
			return;
		for (int i = 0; i < combo->count(); ++i)
		{
			if (combo->text(i) == value.c_str())
				return;
		}
		combo->insertItem(value.c_str());
	}

	void collectAppearanceFiles(const std::string & dir, std::vector<std::string> & out)
	{
		_finddata_t fd;
		std::string pattern = dir + "/*";
		intptr_t handle = _findfirst(pattern.c_str(), &fd);
		if (handle == -1)
			return;

		do
		{
			if (fd.name[0] == '.')
				continue;

			std::string path = dir + "/" + fd.name;

			if (fd.attrib & _A_SUBDIR)
			{
				collectAppearanceFiles(path, out);
			}
			else
			{
				const char * ext = strrchr(fd.name, '.');
				if (ext && (!_stricmp(ext, ".sat") || !_stricmp(ext, ".apt") || !_stricmp(ext, ".lmg") || !_stricmp(ext, ".msh") || !_stricmp(ext, ".pob")))
				{
					out.push_back(path);
				}
			}
		}
		while (_findnext(handle, &fd) == 0);
		_findclose(handle);
	}

	std::string toLowerStr(const std::string & s)
	{
		std::string r = s;
		for (size_t i = 0; i < r.size(); ++i)
		{
			if (r[i] >= 'A' && r[i] <= 'Z')
				r[i] = r[i] - 'A' + 'a';
		}
		return r;
	}
}

// ======================================================================

TemplateEditorWindow::TemplateEditorWindow(QWidget * parent, const char * name)
: QDialog(parent, name, false),
  m_serverEditor(0),
  m_sharedEditor(0),
  m_stringEditor(0),
  m_serverPathEdit(0),
  m_sharedPathEdit(0),
  m_stringPathEdit(0),
  m_saveButton(0),
  m_compileButton(0),
  m_newButton(0),
  m_browseServerButton(0),
  m_browseSharedButton(0),
  m_browseStringButton(0),
  m_applyStructuredButton(0),
  m_refreshStringsButton(0),
  m_scriptToolboxButton(0),
  m_previewButton(0),
  m_templateTypeCombo(0),
  m_containerTypeCombo(0),
  m_appearanceCombo(0),
  m_appearanceFilterEdit(0),
  m_statusLabel(0),
  m_appearancePreviewLabel(0),
  m_appearanceViewer(0),
  m_previewObject(0),
  m_viewerVisible(false),
  m_objectNameFileEdit(0),
  m_objectNameKeyEdit(0),
  m_objectNameValueEdit(0),
  m_detailedDescFileEdit(0),
  m_detailedDescKeyEdit(0),
  m_detailedDescValueEdit(0),
  m_lookAtFileEdit(0),
  m_lookAtKeyEdit(0),
  m_lookAtValueEdit(0),
  m_currentServerPath(),
  m_currentSharedPath(),
  m_currentStringPath(),
  m_hasLoadedTemplate(false)
{
	setCaption("Template Editor");
	resize(1300, 900);
	setAcceptDrops(true);

	QVBoxLayout * mainLayout = new QVBoxLayout(this, 6, 6);

	QHBoxLayout * newLayout = new QHBoxLayout(0, 0, 4);
	QLabel * typeLabel = new QLabel("Type:", this);
	m_templateTypeCombo = new QComboBox(this);
	m_newButton = new QPushButton("New Template", this);
	m_scriptToolboxButton = new QPushButton("Script Toolbox", this);
	m_previewButton = new QPushButton("Preview Appearance", this);
	newLayout->addWidget(typeLabel);
	newLayout->addWidget(m_templateTypeCombo);
	newLayout->addWidget(m_newButton);
	newLayout->addWidget(m_scriptToolboxButton);
	newLayout->addWidget(m_previewButton);
	newLayout->addStretch(1);
	mainLayout->addLayout(newLayout);

	loadKnownTemplateTypesFromTdf();
	if (m_templateTypeCombo->count() == 0)
	{
		m_templateTypeCombo->insertItem("tangible");
		m_templateTypeCombo->insertItem("creature");
		m_templateTypeCombo->insertItem("weapon");
	}

	QSplitter * mainSplitter = new QSplitter(Qt::Horizontal, this);
	mainLayout->addWidget(mainSplitter, 1);

	// Left pane: server/shared editors + robust string controls
	QWidget * leftPane = new QWidget(mainSplitter);
	QVBoxLayout * leftLayout = new QVBoxLayout(leftPane, 4, 4);

	QSplitter * topSplitter = new QSplitter(Qt::Horizontal, leftPane);
	leftLayout->addWidget(topSplitter, 1);

	QWidget * serverPane = new QWidget(topSplitter);
	QVBoxLayout * serverLayout = new QVBoxLayout(serverPane, 2, 2);
	serverLayout->addWidget(new QLabel("Server Template (.tpf)", serverPane));
	QHBoxLayout * serverPathLayout = new QHBoxLayout(0, 0, 2);
	m_serverPathEdit = new QLineEdit(serverPane);
	m_browseServerButton = new QPushButton("...", serverPane);
	m_browseServerButton->setMaximumWidth(30);
	serverPathLayout->addWidget(m_serverPathEdit, 1);
	serverPathLayout->addWidget(m_browseServerButton);
	serverLayout->addLayout(serverPathLayout);
	m_serverEditor = new QTextEdit(serverPane);
	m_serverEditor->setTextFormat(Qt::PlainText);
	m_serverEditor->setFont(QFont("Courier New", 9));
	serverLayout->addWidget(m_serverEditor, 1);

	QWidget * sharedPane = new QWidget(topSplitter);
	QVBoxLayout * sharedLayout = new QVBoxLayout(sharedPane, 2, 2);
	sharedLayout->addWidget(new QLabel("Shared Template (.tpf)", sharedPane));
	QHBoxLayout * sharedPathLayout = new QHBoxLayout(0, 0, 2);
	m_sharedPathEdit = new QLineEdit(sharedPane);
	m_browseSharedButton = new QPushButton("...", sharedPane);
	m_browseSharedButton->setMaximumWidth(30);
	sharedPathLayout->addWidget(m_sharedPathEdit, 1);
	sharedPathLayout->addWidget(m_browseSharedButton);
	sharedLayout->addLayout(sharedPathLayout);
	m_sharedEditor = new QTextEdit(sharedPane);
	m_sharedEditor->setTextFormat(Qt::PlainText);
	m_sharedEditor->setFont(QFont("Courier New", 9));
	sharedLayout->addWidget(m_sharedEditor, 1);

	// Bottom robust string section
	QGroupBox * stringGroup = new QGroupBox(1, Qt::Horizontal, "String File + Structured Fields", leftPane);
	leftLayout->addWidget(stringGroup);

	QWidget * stringWidget = new QWidget(stringGroup);
	QVBoxLayout * stringLayout = new QVBoxLayout(stringWidget, 2, 2);

	QHBoxLayout * stringPathLayout = new QHBoxLayout(0, 0, 2);
	m_stringPathEdit = new QLineEdit(stringWidget);
	m_browseStringButton = new QPushButton("...", stringWidget);
	m_browseStringButton->setMaximumWidth(30);
	stringPathLayout->addWidget(m_stringPathEdit, 1);
	stringPathLayout->addWidget(m_browseStringButton);
	stringLayout->addLayout(stringPathLayout);

	QGridLayout * grid = new QGridLayout(4, 4, 2);
	grid->addWidget(new QLabel("Field", stringWidget), 0, 0);
	grid->addWidget(new QLabel("File", stringWidget), 0, 1);
	grid->addWidget(new QLabel("Key", stringWidget), 0, 2);
	grid->addWidget(new QLabel("Value", stringWidget), 0, 3);

	grid->addWidget(new QLabel("objectName", stringWidget), 1, 0);
	m_objectNameFileEdit = new QLineEdit(stringWidget);
	m_objectNameKeyEdit = new QLineEdit(stringWidget);
	m_objectNameValueEdit = new QLineEdit(stringWidget);
	grid->addWidget(m_objectNameFileEdit, 1, 1);
	grid->addWidget(m_objectNameKeyEdit, 1, 2);
	grid->addWidget(m_objectNameValueEdit, 1, 3);

	grid->addWidget(new QLabel("detailedDescription", stringWidget), 2, 0);
	m_detailedDescFileEdit = new QLineEdit(stringWidget);
	m_detailedDescKeyEdit = new QLineEdit(stringWidget);
	m_detailedDescValueEdit = new QLineEdit(stringWidget);
	grid->addWidget(m_detailedDescFileEdit, 2, 1);
	grid->addWidget(m_detailedDescKeyEdit, 2, 2);
	grid->addWidget(m_detailedDescValueEdit, 2, 3);

	grid->addWidget(new QLabel("lookAtText", stringWidget), 3, 0);
	m_lookAtFileEdit = new QLineEdit(stringWidget);
	m_lookAtKeyEdit = new QLineEdit(stringWidget);
	m_lookAtValueEdit = new QLineEdit(stringWidget);
	grid->addWidget(m_lookAtFileEdit, 3, 1);
	grid->addWidget(m_lookAtKeyEdit, 3, 2);
	grid->addWidget(m_lookAtValueEdit, 3, 3);

	stringLayout->addLayout(grid);

	QHBoxLayout * structuredButtons = new QHBoxLayout(0, 0, 4);
	m_applyStructuredButton = new QPushButton("Apply to Template", stringWidget);
	m_refreshStringsButton = new QPushButton("Load Values from STF", stringWidget);
	structuredButtons->addWidget(m_applyStructuredButton);
	structuredButtons->addWidget(m_refreshStringsButton);
	structuredButtons->addStretch(1);
	stringLayout->addLayout(structuredButtons);

	m_stringEditor = new QTextEdit(stringWidget);
	m_stringEditor->setTextFormat(Qt::PlainText);
	m_stringEditor->setFont(QFont("Courier New", 8));
	m_stringEditor->setMaximumHeight(140);
	stringLayout->addWidget(m_stringEditor);

	// Right pane: preview/settings
	QWidget * rightPane = new QWidget(mainSplitter);
	QVBoxLayout * rightLayout = new QVBoxLayout(rightPane, 4, 4);

	QGroupBox * templateMeta = new QGroupBox(1, Qt::Horizontal, "Template Metadata", rightPane);
	rightLayout->addWidget(templateMeta);
	QWidget * metaWidget = new QWidget(templateMeta);
	QVBoxLayout * metaInnerLayout = new QVBoxLayout(metaWidget, 4, 4);
	QGridLayout * metaGrid = new QGridLayout(metaInnerLayout, 2, 2, 2);
	metaGrid->addWidget(new QLabel("Container Type", metaWidget), 0, 0);
	m_containerTypeCombo = new QComboBox(metaWidget);
	addKnownContainerTypes();
	metaGrid->addWidget(m_containerTypeCombo, 0, 1);
	metaGrid->addWidget(new QLabel("Appearance", metaWidget), 1, 0);
	m_appearanceCombo = new QComboBox(metaWidget);
	m_appearanceCombo->setEditable(true);
	metaGrid->addWidget(m_appearanceCombo, 1, 1);

	QGroupBox * previewGroup = new QGroupBox(1, Qt::Horizontal, "Appearance Viewer", rightPane);
	rightLayout->addWidget(previewGroup, 1);
	QWidget * previewWidget = new QWidget(previewGroup);
	QVBoxLayout * previewLayout = new QVBoxLayout(previewWidget, 4, 4);

	QLabel * filterLabel = new QLabel("Search:", previewWidget);
	m_appearanceFilterEdit = new QLineEdit(previewWidget);
	QHBoxLayout * filterLayout = new QHBoxLayout(0, 0, 4);
	filterLayout->addWidget(filterLabel);
	filterLayout->addWidget(m_appearanceFilterEdit, 1);
	QPushButton * browseAppearanceButton = new QPushButton("...", previewWidget);
	browseAppearanceButton->setMaximumWidth(30);
	filterLayout->addWidget(browseAppearanceButton);
	previewLayout->addLayout(filterLayout);

	m_appearancePreviewLabel = new QLabel("Select an appearance and click Preview.", previewWidget);
	m_appearancePreviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_appearancePreviewLabel->setMinimumHeight(60);
	m_appearancePreviewLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	previewLayout->addWidget(m_appearancePreviewLabel, 1);

	QHBoxLayout * previewButtonLayout = new QHBoxLayout(0, 0, 4);
	QPushButton * previewBtn = new QPushButton("Preview in Game", previewWidget);
	previewButtonLayout->addWidget(previewBtn);
	previewButtonLayout->addStretch(1);
	previewLayout->addLayout(previewButtonLayout);

	IGNORE_RETURN(connect(previewBtn, SIGNAL(clicked()), this, SLOT(onPreviewAppearance())));
	IGNORE_RETURN(connect(browseAppearanceButton, SIGNAL(clicked()), this, SLOT(onBrowseAppearance())));
	IGNORE_RETURN(connect(m_appearanceFilterEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onAppearanceFilterChanged(const QString &))));

	rightLayout->addStretch(1);

	m_statusLabel = new QLabel("Load a .tpf/.iff file or create a new template.", this);
	mainLayout->addWidget(m_statusLabel);

	QHBoxLayout * buttonLayout = new QHBoxLayout(0, 0, 6);
	buttonLayout->addStretch(1);
	m_saveButton = new QPushButton("Save TPF + STF", this);
	m_compileButton = new QPushButton("Compile", this);
	m_compileButton->setEnabled(false);
	buttonLayout->addWidget(m_saveButton);
	buttonLayout->addWidget(m_compileButton);
	mainLayout->addLayout(buttonLayout);

	IGNORE_RETURN(connect(m_saveButton, SIGNAL(clicked()), this, SLOT(onSave())));
	IGNORE_RETURN(connect(m_compileButton, SIGNAL(clicked()), this, SLOT(onCompile())));
	IGNORE_RETURN(connect(m_browseServerButton, SIGNAL(clicked()), this, SLOT(onBrowseServerPath())));
	IGNORE_RETURN(connect(m_browseSharedButton, SIGNAL(clicked()), this, SLOT(onBrowseSharedPath())));
	IGNORE_RETURN(connect(m_browseStringButton, SIGNAL(clicked()), this, SLOT(onBrowseStringPath())));
	IGNORE_RETURN(connect(m_newButton, SIGNAL(clicked()), this, SLOT(onNewTemplate())));
	IGNORE_RETURN(connect(m_scriptToolboxButton, SIGNAL(clicked()), this, SLOT(onOpenScriptToolbox())));
	IGNORE_RETURN(connect(m_previewButton, SIGNAL(clicked()), this, SLOT(onPreviewAppearance())));
	IGNORE_RETURN(connect(m_applyStructuredButton, SIGNAL(clicked()), this, SLOT(onApplyStructuredFields())));
	IGNORE_RETURN(connect(m_refreshStringsButton, SIGNAL(clicked()), this, SLOT(onRefreshStringValues())));
	IGNORE_RETURN(connect(m_appearanceCombo, SIGNAL(activated(const QString &)), this, SLOT(onAppearanceChanged(const QString &))));
	IGNORE_RETURN(connect(m_appearanceCombo, SIGNAL(textChanged(const QString &)), this, SLOT(onAppearanceChanged(const QString &))));
}

// ----------------------------------------------------------------------

TemplateEditorWindow::~TemplateEditorWindow()
{
	hideAppearanceViewer();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::dragEnterEvent(QDragEnterEvent * event)
{
	if (QTextDrag::canDecode(event))
		event->accept();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::dropEvent(QDropEvent * event)
{
	QString text;
	if (!QTextDrag::decode(event, text))
		return;

	if (text == ActionsScript::DragMessages::SCRIPT_DRAGGED)
	{
		std::string scriptPath;
		if (ActionsScript::getInstance().getSelectedScript(scriptPath))
		{
			appendScriptToServerTemplate(scriptPath);
			m_statusLabel->setText(("Script dropped into template: " + scriptPath).c_str());
			event->accept();
		}
	}
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onOpenScriptToolbox()
{
	// Use the same action path as the Window->Tree Browser toggle,
	// so script dragging uses existing safe Tree Browser flow.
	if (ActionsWindow::getInstance().m_treeBrowser)
		ActionsWindow::getInstance().m_treeBrowser->setOn(true);

	MainFrame::getInstance().textToConsole("TemplateEditor: Script Toolbox opened (Tree Browser).");
	raise();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onBrowseAppearance()
{
	QString path = QFileDialog::getOpenFileName(
		QString::null,
		"Appearance Files (*.apt *.sat *.lmg *.msh *.pob);;IFF Files (*.iff);;All Files (*)",
		this,
		"browseAppearance",
		"Select Appearance File");

	if (path.isEmpty())
		return;

	std::string relative = path.latin1();
	for (size_t i = 0; i < relative.size(); ++i)
	{
		if (relative[i] == '\\')
			relative[i] = '/';
	}

	std::string::size_type appPos = relative.find("appearance/");
	if (appPos != std::string::npos)
		relative = relative.substr(appPos);

	addComboIfMissing(m_appearanceCombo, relative);
	m_appearanceCombo->setCurrentText(relative.c_str());
	updateAppearancePreviewText();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::hideEvent(QHideEvent * event)
{
	UNREF(event);
	hideAppearanceViewer();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::showAppearanceViewer(const std::string & appearancePath)
{
	hideAppearanceViewer();

	if (appearancePath.empty())
		return;

	if (!TreeFile::exists(appearancePath.c_str()))
	{
		m_appearancePreviewLabel->setText(("Appearance not found in TreeFile:\n" + appearancePath).c_str());
		return;
	}

	Appearance * const appearance = AppearanceTemplateList::createAppearance(appearancePath.c_str());
	if (!appearance)
	{
		m_appearancePreviewLabel->setText(("Failed to load appearance:\n" + appearancePath).c_str());
		return;
	}

	m_previewObject = new MemoryBlockManagedObject;
	m_previewObject->setAppearance(appearance);

	UIPage * const root = UIManager::gUIManager().GetRootPage();
	if (!root)
	{
		delete m_previewObject;
		m_previewObject = 0;
		m_appearancePreviewLabel->setText("UI system not available.");
		return;
	}

	m_appearanceViewer = new CuiWidget3dObjectListViewer;
	m_appearanceViewer->SetName("TemplateEditorViewer");

	m_appearanceViewer->SetParent(root);
	m_appearanceViewer->Link();

	const long rootW = root->GetWidth();
	const long rootH = root->GetHeight();
	const long viewerW = 300;
	const long viewerH = 300;
	m_appearanceViewer->SetLocation(UIPoint(rootW - viewerW - 10, 10));
	m_appearanceViewer->SetSize(UISize(viewerW, viewerH));

	m_appearanceViewer->SetBackgroundColor(UIColor(0, 0, 0));
	m_appearanceViewer->SetBackgroundOpacity(0.85f);
	m_appearanceViewer->SetVisible(true);

	m_appearanceViewer->addObject(*m_previewObject);
	m_appearanceViewer->setAlterObjects(true);
	m_appearanceViewer->setPaused(false);
	m_appearanceViewer->setCameraLookAtCenter(true);
	m_appearanceViewer->setCameraForceTarget(true);
	m_appearanceViewer->recomputeZoom();
	m_appearanceViewer->setCameraForceTarget(false);
	m_appearanceViewer->setViewDirty(true);
	m_appearanceViewer->setRotateSpeed(0.5f);
	m_appearanceViewer->setDragYawOk(true);
	m_appearanceViewer->setDragPitchOk(true);
	m_appearanceViewer->setCameraLodBias(3.0f);
	m_appearanceViewer->setCameraLodBiasOverride(true);
	m_appearanceViewer->setFitDistanceFactor(1.5f);

	m_viewerVisible = true;

	m_appearancePreviewLabel->setText(("Previewing:\n" + appearancePath + "\n\nDrag in the game viewport to rotate.").c_str());
	m_statusLabel->setText(("Preview: " + appearancePath).c_str());
	MainFrame::getInstance().textToConsole(("TemplateEditor: Previewing " + appearancePath).c_str());
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::hideAppearanceViewer()
{
	if (m_appearanceViewer)
	{
		m_appearanceViewer->clearObjects();
		m_appearanceViewer->SetVisible(false);

		UIPage * const root = UIManager::gUIManager().GetRootPage();
		if (root)
			root->RemoveChild(m_appearanceViewer);

		// RemoveChild detaches refcount; viewer deletes itself when it reaches 0
		m_appearanceViewer = 0;
	}

	if (m_previewObject)
	{
		delete m_previewObject;
		m_previewObject = 0;
	}

	m_viewerVisible = false;
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onPreviewAppearance()
{
	if (m_viewerVisible)
	{
		hideAppearanceViewer();
		m_appearancePreviewLabel->setText("Preview hidden.");
		m_statusLabel->setText("Preview hidden.");
		return;
	}

	const std::string appearance = m_appearanceCombo->currentText().latin1();
	if (appearance.empty())
	{
		QMessageBox::information(this, "Preview", "No appearance selected.\n\nType or select an appearance path first.");
		return;
	}

	showAppearanceViewer(appearance);
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::populateAppearanceFilter(const std::string & filter)
{
	m_appearanceCombo->clear();

	if (m_allAppearances.empty())
	{
		collectAppearanceFiles("appearance", m_allAppearances);

		if (m_allAppearances.empty())
		{
			collectAppearanceFiles("../../appearance", m_allAppearances);
		}
	}

	const std::string lowerFilter = toLowerStr(filter);
	const int maxResults = 200;
	int count = 0;

	for (size_t j = 0; j < m_allAppearances.size() && count < maxResults; ++j)
	{
		if (lowerFilter.empty() || toLowerStr(m_allAppearances[j]).find(lowerFilter) != std::string::npos)
		{
			m_appearanceCombo->insertItem(m_allAppearances[j].c_str());
			++count;
		}
	}
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onAppearanceFilterChanged(const QString & text)
{
	populateAppearanceFilter(text.latin1());
}

// ----------------------------------------------------------------------

std::string TemplateEditorWindow::convertCompiledPathToSource(const std::string & path) const
{
	std::string result = path;
	std::string::size_type dataPos = result.find("/data/");
	if (dataPos == std::string::npos)
		dataPos = result.find("data/");
	if (dataPos != std::string::npos)
	{
		size_t start = (result[dataPos] == '/') ? dataPos + 1 : dataPos;
		result.replace(start, 4, "dsrc");
	}
	std::string::size_type compiledPos = result.find("/compiled/");
	if (compiledPos != std::string::npos)
		result.erase(compiledPos, 9);
	if (hasSuffix(result, ".iff"))
		result.replace(result.size() - 4, 4, ".tpf");
	return result;
}

// ----------------------------------------------------------------------

std::string TemplateEditorWindow::getSharedCounterpartFromServerPath(const std::string & serverPath) const
{
	std::string sharedPath = serverPath;
	std::string::size_type pos = sharedPath.find("sys.server");
	if (pos != std::string::npos)
		sharedPath.replace(pos, 10, "sys.shared");

	// Ensure shared filename prefix: .../foo.tpf -> .../shared_foo.tpf
	std::string normalized = sharedPath;
	for (size_t i = 0; i < normalized.size(); ++i)
	{
		if (normalized[i] == '\\')
			normalized[i] = '/';
	}
	std::string::size_type slash = normalized.find_last_of('/');
	std::string::size_type dot = normalized.rfind(".tpf");
	if (slash != std::string::npos && dot != std::string::npos && dot > slash + 1)
	{
		const std::string filename = normalized.substr(slash + 1, dot - (slash + 1));
		if (filename.find("shared_") != 0)
			normalized.insert(slash + 1, "shared_");
	}
	return normalized;
}

// ----------------------------------------------------------------------

std::string TemplateEditorWindow::getServerCounterpartFromSharedPath(const std::string & sharedPath) const
{
	std::string serverPath = sharedPath;
	std::string::size_type pos = serverPath.find("sys.shared");
	if (pos != std::string::npos)
		serverPath.replace(pos, 10, "sys.server");

	for (size_t i = 0; i < serverPath.size(); ++i)
	{
		if (serverPath[i] == '\\')
			serverPath[i] = '/';
	}
	std::string::size_type slash = serverPath.find_last_of('/');
	std::string::size_type dot = serverPath.rfind(".tpf");
	if (slash != std::string::npos && dot != std::string::npos && dot > slash + 1)
	{
		const std::string filename = serverPath.substr(slash + 1, dot - (slash + 1));
		if (filename.find("shared_") == 0)
		{
			serverPath.erase(slash + 1, 7);
		}
	}
	return serverPath;
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::parseTemplateIdentityFromPath(const std::string & path, std::string & outType, std::string & outName) const
{
	outType.clear();
	outName.clear();
	std::string p = path;
	for (size_t i = 0; i < p.size(); ++i)
	{
		if (p[i] == '\\')
			p[i] = '/';
	}
	std::string::size_type objectPos = p.find("/object/");
	if (objectPos == std::string::npos)
		return false;
	std::string tail = p.substr(objectPos + 8);
	std::string::size_type slash = tail.find('/');
	if (slash == std::string::npos)
		return false;
	outType = tail.substr(0, slash);
	std::string filename = tail.substr(slash + 1);
	std::string::size_type ext = filename.rfind('.');
	outName = (ext == std::string::npos) ? filename : filename.substr(0, ext);
	if (outName.find("shared_") == 0)
		outName = outName.substr(7);
	return !outType.empty() && !outName.empty();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::loadTemplate(const std::string & inputPath)
{
	std::string tpfPath = inputPath;
	if (hasSuffix(tpfPath, ".iff"))
		tpfPath = convertCompiledPathToSource(tpfPath);

	std::string contents;
	if (!readTextFile(tpfPath, contents))
	{
		// .iff open-as-new fallback
		std::string type;
		std::string name;
		if (!parseTemplateIdentityFromPath(tpfPath, type, name))
		{
			QMessageBox::warning(this, "Template Editor", QString("Could not open: %1").arg(tpfPath.c_str()));
			return;
		}

		newTemplate();
		m_templateTypeCombo->setCurrentText(type.c_str());
		m_serverEditor->setText(getBoilerplate(type, name, false).c_str());
		m_sharedEditor->setText(getBoilerplate(type, name, true).c_str());

		m_serverPathEdit->setText(("dsrc/sku.0/sys.server/compiled/game/object/" + type + "/" + name + ".tpf").c_str());
		m_sharedPathEdit->setText(("dsrc/sku.0/sys.shared/compiled/game/object/" + type + "/shared_" + name + ".tpf").c_str());
		m_stringPathEdit->setText(("dsrc/sku.0/sys.shared/compiled/game/string/en/object/" + type + "/" + name + ".stf").c_str());

		m_currentServerPath = m_serverPathEdit->text().latin1();
		m_currentSharedPath = m_sharedPathEdit->text().latin1();
		m_currentStringPath = m_stringPathEdit->text().latin1();

		parseSharedTemplateIntoStructuredFields();
		refreshStringValuesFromTool();

		m_hasLoadedTemplate = true;
		m_compileButton->setEnabled(true);
		m_statusLabel->setText(("Opened as new from compiled template path: " + tpfPath).c_str());
		return;
	}

	if (tpfPath.find("sys.server") != std::string::npos)
	{
		m_serverEditor->setText(contents.c_str());
		m_serverPathEdit->setText(tpfPath.c_str());
		m_currentServerPath = tpfPath;

		std::string sharedPath = getSharedCounterpartFromServerPath(tpfPath);
		m_sharedPathEdit->setText(sharedPath.c_str());

		std::string sharedContents;
		if (readTextFile(sharedPath, sharedContents))
		{
			m_sharedEditor->setText(sharedContents.c_str());
			m_currentSharedPath = sharedPath;
		}
	}
	else if (tpfPath.find("sys.shared") != std::string::npos)
	{
		m_sharedEditor->setText(contents.c_str());
		m_sharedPathEdit->setText(tpfPath.c_str());
		m_currentSharedPath = tpfPath;

		std::string serverPath = getServerCounterpartFromSharedPath(tpfPath);
		m_serverPathEdit->setText(serverPath.c_str());

		std::string serverContents;
		if (readTextFile(serverPath, serverContents))
		{
			m_serverEditor->setText(serverContents.c_str());
			m_currentServerPath = serverPath;
		}
	}
	else
	{
		m_serverEditor->setText(contents.c_str());
		m_serverPathEdit->setText(tpfPath.c_str());
		m_currentServerPath = tpfPath;
	}

	parseSharedTemplateIntoStructuredFields();
	refreshStringValuesFromTool();
	updateAppearancePreviewText();

	m_hasLoadedTemplate = true;
	m_compileButton->setEnabled(true);
	m_statusLabel->setText(QString("Loaded: %1").arg(tpfPath.c_str()));
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::newTemplate()
{
	m_serverEditor->clear();
	m_sharedEditor->clear();
	m_stringEditor->clear();
	m_serverPathEdit->clear();
	m_sharedPathEdit->clear();
	m_stringPathEdit->clear();

	m_objectNameFileEdit->clear();
	m_objectNameKeyEdit->clear();
	m_objectNameValueEdit->clear();
	m_detailedDescFileEdit->clear();
	m_detailedDescKeyEdit->clear();
	m_detailedDescValueEdit->clear();
	m_lookAtFileEdit->clear();
	m_lookAtKeyEdit->clear();
	m_lookAtValueEdit->clear();
	m_appearanceCombo->setCurrentText("");
	m_containerTypeCombo->setCurrentText("CT_none");

	m_currentServerPath.clear();
	m_currentSharedPath.clear();
	m_currentStringPath.clear();
	m_hasLoadedTemplate = false;
	m_compileButton->setEnabled(false);
	m_statusLabel->setText("New template - select a type and enter content.");
	updateAppearancePreviewText();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onNewTemplate()
{
	std::string templateType = m_templateTypeCombo->currentText().latin1();
	const std::string templateName = "new_" + templateType;

	newTemplate();

	m_serverEditor->setText(getBoilerplate(templateType, templateName, false).c_str());
	m_sharedEditor->setText(getBoilerplate(templateType, templateName, true).c_str());

	m_serverPathEdit->setText(("dsrc/sku.0/sys.server/compiled/game/object/" + templateType + "/" + templateName + ".tpf").c_str());
	m_sharedPathEdit->setText(("dsrc/sku.0/sys.shared/compiled/game/object/" + templateType + "/shared_" + templateName + ".tpf").c_str());
	m_stringPathEdit->setText(("dsrc/sku.0/sys.shared/compiled/game/string/en/object/" + templateType + "/" + templateName + ".stf").c_str());

	parseSharedTemplateIntoStructuredFields();
	refreshStringValuesFromTool();

	m_hasLoadedTemplate = true;
	m_compileButton->setEnabled(true);
	m_statusLabel->setText(QString("New %1 template created.").arg(templateType.c_str()));
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::addKnownContainerTypes()
{
	m_containerTypeCombo->insertItem("CT_none");
	m_containerTypeCombo->insertItem("CT_slotted");
	m_containerTypeCombo->insertItem("CT_volume");
	m_containerTypeCombo->insertItem("CT_volumeIntangible");
	m_containerTypeCombo->insertItem("CT_volumeGeneric");
	m_containerTypeCombo->insertItem("CT_ridable");
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::loadKnownTemplateTypesFromTdf()
{
	const char * pattern = "dsrc/sku.0/sys.shared/compiled/game/object/*.tdf";
	_finddata_t fd;
	intptr_t handle = _findfirst(pattern, &fd);
	if (handle == -1)
		return;

	do
	{
		std::string n = fd.name;
		if (hasSuffix(n, ".tdf"))
		{
			n = n.substr(0, n.size() - 4);
			if (n != "object_template" && n.find("base") != 0)
				addComboIfMissing(m_templateTypeCombo, n);
		}
	}
	while (_findnext(handle, &fd) == 0);

	_findclose(handle);
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::parseSharedTemplateIntoStructuredFields()
{
	const std::string shared = m_sharedEditor->text().latin1();

	std::string file, key;
	if (extractQuotedPair(shared, "objectName", file, key))
	{
		m_objectNameFileEdit->setText(file.c_str());
		m_objectNameKeyEdit->setText(key.c_str());
	}
	if (extractQuotedPair(shared, "detailedDescription", file, key))
	{
		m_detailedDescFileEdit->setText(file.c_str());
		m_detailedDescKeyEdit->setText(key.c_str());
	}
	if (extractQuotedPair(shared, "lookAtText", file, key))
	{
		m_lookAtFileEdit->setText(file.c_str());
		m_lookAtKeyEdit->setText(key.c_str());
	}

	std::string appearance;
	if (extractSimpleAssignment(shared, "appearanceFilename", appearance))
	{
		if (appearance.size() > 1 && appearance[0] == '"' && appearance[appearance.size() - 1] == '"')
			appearance = appearance.substr(1, appearance.size() - 2);
		addComboIfMissing(m_appearanceCombo, appearance);
		m_appearanceCombo->setCurrentText(appearance.c_str());
	}

	std::string containerType;
	if (extractSimpleAssignment(shared, "containerType", containerType))
		m_containerTypeCombo->setCurrentText(containerType.c_str());
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::applyStructuredFieldsToSharedTemplate()
{
	std::string shared = m_sharedEditor->text().latin1();

	replaceOrAppendQuotedPair(shared, "objectName", m_objectNameFileEdit->text().latin1(), m_objectNameKeyEdit->text().latin1());
	replaceOrAppendQuotedPair(shared, "detailedDescription", m_detailedDescFileEdit->text().latin1(), m_detailedDescKeyEdit->text().latin1());
	replaceOrAppendQuotedPair(shared, "lookAtText", m_lookAtFileEdit->text().latin1(), m_lookAtKeyEdit->text().latin1());

	const std::string appearance = m_appearanceCombo->currentText().latin1();
	replaceOrAppendSimpleAssignment(shared, "appearanceFilename", "\"" + appearance + "\"");
	replaceOrAppendSimpleAssignment(shared, "containerType", m_containerTypeCombo->currentText().latin1());

	m_sharedEditor->setText(shared.c_str());
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onApplyStructuredFields()
{
	applyStructuredFieldsToSharedTemplate();
	m_statusLabel->setText("Structured fields applied to shared template text.");
	updateAppearancePreviewText();
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::runLocalizationTool(const std::string & filePath, const std::string & args, std::string & output, int & resultCode) const
{
	output.clear();

	std::string command = "LocalizationToolCon_d.exe \"" + filePath + "\" " + args + " 2>&1";
	FILE * pipe = _popen(command.c_str(), "r");
	if (!pipe)
	{
		command = "LocalizationToolCon.exe \"" + filePath + "\" " + args + " 2>&1";
		pipe = _popen(command.c_str(), "r");
	}
	if (!pipe)
	{
		resultCode = -999;
		output = "Failed to execute LocalizationToolCon.";
		return false;
	}

	char buffer[512];
	while (fgets(buffer, sizeof(buffer), pipe))
		output += buffer;

	resultCode = _pclose(pipe);
	return true;
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::findStringValueWithTool(const std::string & file, const std::string & key, std::string & outValue, std::string & error)
{
	outValue.clear();
	error.clear();
	if (file.empty() || key.empty())
		return false;

	const std::string stfPath = makeStringFilePath(file);
	std::string output;
	int rc = 0;
	if (!runLocalizationTool(stfPath, "-find \"" + escapeForCommand(key) + "\"", output, rc))
	{
		error = output;
		return false;
	}
	if (rc != 0)
	{
		error = output;
		return false;
	}

	// best-effort parse: grab last non-empty line, then consume token delimiter
	std::string lastLine;
	std::string current;
	for (size_t i = 0; i < output.size(); ++i)
	{
		if (output[i] == '\n')
		{
			if (!trim(current).empty())
				lastLine = trim(current);
			current.clear();
		}
		else if (output[i] != '\r')
		{
			current += output[i];
		}
	}
	if (!trim(current).empty())
		lastLine = trim(current);

	if (lastLine.empty())
		return false;

	std::string::size_type pos = lastLine.find(key);
	if (pos != std::string::npos)
	{
		std::string value = trim(lastLine.substr(pos + key.size()));
		if (!value.empty() && (value[0] == ':' || value[0] == '=' || value[0] == '-'))
			value = trim(value.substr(1));
		outValue = value;
	}
	else
	{
		outValue = lastLine;
	}

	return true;
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::setStringValueWithTool(const std::string & file, const std::string & key, const std::string & value, std::string & error)
{
	error.clear();
	if (file.empty() || key.empty())
		return true;

	const std::string stfPath = makeStringFilePath(file);
	std::string output;
	int rc = 0;
	const std::string args = "-set \"" + escapeForCommand(key) + "\" \"" + escapeForCommand(value) + "\"";
	if (!runLocalizationTool(stfPath, args, output, rc))
	{
		error = output;
		return false;
	}
	if (rc != 0)
	{
		error = output;
		return false;
	}
	return true;
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::refreshStringValuesFromTool()
{
	std::string error;
	std::string value;

	if (findStringValueWithTool(m_objectNameFileEdit->text().latin1(), m_objectNameKeyEdit->text().latin1(), value, error))
		m_objectNameValueEdit->setText(value.c_str());
	if (findStringValueWithTool(m_detailedDescFileEdit->text().latin1(), m_detailedDescKeyEdit->text().latin1(), value, error))
		m_detailedDescValueEdit->setText(value.c_str());
	if (findStringValueWithTool(m_lookAtFileEdit->text().latin1(), m_lookAtKeyEdit->text().latin1(), value, error))
		m_lookAtValueEdit->setText(value.c_str());

	const std::string stf = m_stringPathEdit->text().latin1();
	if (!stf.empty())
	{
		std::string output;
		int rc = 0;
		if (runLocalizationTool(stf, "-list", output, rc))
			m_stringEditor->setText(output.c_str());
	}
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onRefreshStringValues()
{
	refreshStringValuesFromTool();
	m_statusLabel->setText("String values refreshed from LocalizationToolCon.");
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::updateAppearancePreviewText()
{
	const std::string appearance = m_appearanceCombo->currentText().latin1();
	std::string text;
	if (appearance.empty())
	{
		text = "No appearance selected.\nUse search or browse to find one.";
	}
	else
	{
		text = "Appearance: " + appearance + "\n";

		if (hasSuffix(appearance, ".sat"))
			text += "Type: Skeletal Appearance Template\n";
		else if (hasSuffix(appearance, ".apt"))
			text += "Type: Appearance Template\n";
		else if (hasSuffix(appearance, ".lmg"))
			text += "Type: LOD Mesh Generator\n";
		else if (hasSuffix(appearance, ".msh"))
			text += "Type: Mesh\n";
		else if (hasSuffix(appearance, ".pob"))
			text += "Type: Portal Object\n";
		else
			text += "Type: Unknown\n";

		if (m_viewerVisible)
			text += "\nCurrently previewing in game viewport.";
		else
			text += "\nClick 'Preview in Game' to render.";
	}
	m_appearancePreviewLabel->setText(text.c_str());

	if (m_viewerVisible)
		showAppearanceViewer(appearance);
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onAppearanceChanged(const QString &)
{
	updateAppearancePreviewText();
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::appendScriptToServerTemplate(const std::string & classPath)
{
	std::string server = m_serverEditor->text().latin1();
	if (server.find(classPath) != std::string::npos)
		return;

	const std::string oneLine = "scripts = []";
	std::string::size_type pos = server.find(oneLine);
	if (pos != std::string::npos)
	{
		server.replace(pos, oneLine.size(), "scripts = [\"" + classPath + "\"]");
	}
	else
	{
		pos = server.find("scripts = [");
		if (pos != std::string::npos)
		{
			std::string::size_type close = server.find(']', pos);
			if (close != std::string::npos)
			{
				if (close > pos + 11 && server[close - 1] != '[')
					server.insert(close, ", \"" + classPath + "\"");
				else
					server.insert(close, "\"" + classPath + "\"");
			}
		}
		else
		{
			server += "\n";
			server += "scripts = [\"";
			server += classPath;
			server += "\"]\n";
		}
	}

	m_serverEditor->setText(server.c_str());
}

// ----------------------------------------------------------------------

std::string TemplateEditorWindow::getBoilerplate(const std::string & templateType, const std::string & templateName, bool isShared) const
{
	std::string result;

	if (isShared)
	{
		result += "@base object/" + templateType + "/base/shared_" + templateType + "_default.iff\n\n";
		result += "@class object_template_version [id] 0\n\n";
		result += "objectName = \"object/" + templateType + "/" + templateName + "\" \"" + templateName + "\"\n";
		result += "detailedDescription = \"object/" + templateType + "/" + templateName + "\" \"" + templateName + "_d\"\n";
		result += "lookAtText = \"object/" + templateType + "/" + templateName + "\" \"" + templateName + "_l\"\n\n";
		result += "appearanceFilename = \"\"\n";
		result += "containerType = CT_none\n";
		result += "containerVolumeLimit = 0\n";
	}
	else
	{
		result += "@base object/" + templateType + "/base/" + templateType + "_default.iff\n\n";
		result += "@class object_template_version [id] 0\n\n";
		result += "// Server-side template for " + templateName + "\n";
		result += "scripts = []\n";
		result += "objvars = []\n";
	}

	return result;
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onSave()
{
	applyStructuredFieldsToSharedTemplate();

	int confirm1 = QMessageBox::question(this, "Save Template",
		"Are you sure you want to save this template and string values?",
		QMessageBox::Yes, QMessageBox::No);
	if (confirm1 != QMessageBox::Yes)
		return;

	bool saved = false;

	std::string serverPath = m_serverPathEdit->text().latin1();
	if (!serverPath.empty())
	{
		std::string contents = m_serverEditor->text().latin1();
		if (saveToFile(serverPath, contents))
		{
			m_currentServerPath = serverPath;
			saved = true;
		}
	}

	std::string sharedPath = m_sharedPathEdit->text().latin1();
	if (!sharedPath.empty())
	{
		std::string contents = m_sharedEditor->text().latin1();
		if (saveToFile(sharedPath, contents))
		{
			m_currentSharedPath = sharedPath;
			saved = true;
		}
	}

	// Persist localized values via LocalizationToolCon
	std::string err;
	bool stringsOk = true;
	stringsOk = stringsOk && setStringValueWithTool(m_objectNameFileEdit->text().latin1(), m_objectNameKeyEdit->text().latin1(), m_objectNameValueEdit->text().latin1(), err);
	stringsOk = stringsOk && setStringValueWithTool(m_detailedDescFileEdit->text().latin1(), m_detailedDescKeyEdit->text().latin1(), m_detailedDescValueEdit->text().latin1(), err);
	stringsOk = stringsOk && setStringValueWithTool(m_lookAtFileEdit->text().latin1(), m_lookAtKeyEdit->text().latin1(), m_lookAtValueEdit->text().latin1(), err);

	if (!stringsOk)
	{
		QMessageBox::warning(this, "Template Editor", QString("String update failed:\n%1").arg(err.c_str()));
	}

	if (saved)
	{
		m_hasLoadedTemplate = true;
		m_compileButton->setEnabled(true);
		m_statusLabel->setText(stringsOk ? "Template and string values saved." : "Template saved, but string update had errors.");
		emit statusMessage("Template saved.");
	}
	else
	{
		m_statusLabel->setText("No template files were saved. Check paths.");
	}
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onCompile()
{
	if (!m_hasLoadedTemplate)
	{
		QMessageBox::warning(this, "Template Editor", "No template loaded to compile.");
		return;
	}

	std::string tpfPath;
	if (!m_currentServerPath.empty())
		tpfPath = m_currentServerPath;
	else if (!m_currentSharedPath.empty())
		tpfPath = m_currentSharedPath;
	else
		tpfPath = m_serverPathEdit->text().latin1();

	if (tpfPath.empty())
	{
		QMessageBox::warning(this, "Template Editor", "No template path available for compilation.");
		return;
	}

	m_statusLabel->setText(QString("Compiling: %1").arg(tpfPath.c_str()));

	if (runTemplateCompiler(tpfPath))
	{
		m_statusLabel->setText("Compilation successful.");
		emit statusMessage("Template compiled successfully.");
		MainFrame::getInstance().textToConsole(("TemplateEditor: Compiled " + tpfPath).c_str());
	}
	else
	{
		m_statusLabel->setText("Compilation failed. Check console for details.");
		QMessageBox::warning(this, "Template Editor", "TemplateCompiler failed. Check the console output.");
	}
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onBrowseServerPath()
{
	QString path = QFileDialog::getOpenFileName(QString::null, "Template Files (*.tpf *.iff);;All Files (*)", this, "browseServer", "Open Server Template");
	if (!path.isEmpty())
		loadTemplate(path.latin1());
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onBrowseSharedPath()
{
	QString path = QFileDialog::getOpenFileName(QString::null, "Template Files (*.tpf *.iff);;All Files (*)", this, "browseShared", "Open Shared Template");
	if (!path.isEmpty())
		loadTemplate(path.latin1());
}

// ----------------------------------------------------------------------

void TemplateEditorWindow::onBrowseStringPath()
{
	QString path = QFileDialog::getOpenFileName(QString::null, "String Files (*.stf);;All Files (*)", this, "browseString", "Open String File");
	if (!path.isEmpty())
	{
		m_stringPathEdit->setText(path);
		m_currentStringPath = path.latin1();
		onRefreshStringValues();
	}
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::saveToFile(const std::string & path, const std::string & contents)
{
	FILE * fp = fopen(path.c_str(), "w");
	if (!fp)
		return false;
	fwrite(contents.c_str(), 1, contents.size(), fp);
	fclose(fp);
	return true;
}

// ----------------------------------------------------------------------

bool TemplateEditorWindow::runTemplateCompiler(const std::string & tpfPath)
{
	char command[2048];
	snprintf(command, sizeof(command), "TemplateCompiler -compile \"%s\"", tpfPath.c_str());
	int result = system(command);
	return (result == 0);
}

// ======================================================================
