// ======================================================================
//
// ActionsFileControl.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ActionsFileControl.h"
#include "ActionsFileControl.moc"

#include "ActionHack.h"
#include "BuildoutEditorWindow.h"
#include "ConfigGodClient.h"
#include "DatatableEditorWindow.h"
#include "FileServerTreeWindow.h"
#include "IconLoader.h"
#include "MainFrame.h"
#include "TemplateEditorWindow.h"

#include "FileControlClient.h"

#include "sharedFoundation/ConfigFile.h"

#include <qfiledialog.h>
#include <qmessagebox.h>

// ======================================================================

ActionsFileControl::ActionsFileControl()
: QObject(),
  Singleton<ActionsFileControl>(),
  m_sendAsset(0),
  m_reloadAsset(0),
  m_retrieveAsset(0),
  m_broadcastUpdate(0),
  m_updateDbTemplates(0),
  m_verifyAsset(0),
  m_flush(0),
  m_openFileServerTree(0),
  m_openTemplateEditor(0),
  m_openDatatableEditor(0),
  m_openBuildoutEditor(0),
  m_selectedPath(),
  m_connected(false),
  m_lifecycleStage(LS_IDLE)
{
	QWidget * const p = &MainFrame::getInstance();

	m_sendAsset         = new ActionHack("Send Asset",           IL_PIXMAP(hi16_action_finish),      "&Send Asset",           0, p, "fc_send");
	m_reloadAsset       = new ActionHack("Reload Asset",         IL_PIXMAP(hi16_action_reload),      "&Reload Asset",         0, p, "fc_reload");
	m_retrieveAsset     = new ActionHack("Retrieve Asset",       IL_PIXMAP(hi16_action_revert),      "Re&trieve Asset",       0, p, "fc_retrieve");
	m_broadcastUpdate   = new ActionHack("Broadcast Update",     IL_PIXMAP(hi16_action_reload),      "&Broadcast Update",     0, p, "fc_broadcast");
	m_updateDbTemplates = new ActionHack("Update DB Templates",  IL_PIXMAP(hi16_action_gear),        "&Update DB Templates",  0, p, "fc_updatedb");
	m_verifyAsset       = new ActionHack("Verify Asset",         IL_PIXMAP(hi16_action_edit),        "&Verify Asset",         0, p, "fc_verify");
	m_flush             = new ActionHack("Flush All",            IL_PIXMAP(hi16_action_reload),      "&Flush All",            0, p, "fc_flush");
	m_openFileServerTree  = new ActionHack("File Server Tree",   IL_PIXMAP(hi16_action_window_new),  "File Server &Tree",     0, p, "fc_tree");
	m_openTemplateEditor  = new ActionHack("Template Editor",    IL_PIXMAP(hi16_action_window_new),  "Te&mplate Editor",      0, p, "fc_template_editor");
	m_openDatatableEditor = new ActionHack("Datatable Editor",   IL_PIXMAP(hi16_action_window_new),  "&Datatable Editor",     0, p, "fc_datatable_editor");
	m_openBuildoutEditor  = new ActionHack("Buildout Manager",   IL_PIXMAP(hi16_action_window_new),  "B&uildout Manager",     0, p, "fc_buildout_manager");

	IGNORE_RETURN(connect(m_sendAsset,         SIGNAL(activated()), this, SLOT(onSendAsset())));
	IGNORE_RETURN(connect(m_reloadAsset,       SIGNAL(activated()), this, SLOT(onReloadAsset())));
	IGNORE_RETURN(connect(m_retrieveAsset,     SIGNAL(activated()), this, SLOT(onRetrieveAsset())));
	IGNORE_RETURN(connect(m_broadcastUpdate,   SIGNAL(activated()), this, SLOT(onBroadcastUpdate())));
	IGNORE_RETURN(connect(m_updateDbTemplates, SIGNAL(activated()), this, SLOT(onUpdateDbTemplates())));
	IGNORE_RETURN(connect(m_verifyAsset,       SIGNAL(activated()), this, SLOT(onVerifyAsset())));
	IGNORE_RETURN(connect(m_flush,             SIGNAL(activated()), this, SLOT(onFlush())));
	IGNORE_RETURN(connect(m_openFileServerTree,  SIGNAL(activated()), this, SLOT(onOpenFileServerTree())));
	IGNORE_RETURN(connect(m_openTemplateEditor,  SIGNAL(activated()), this, SLOT(onOpenTemplateEditor())));
	IGNORE_RETURN(connect(m_openDatatableEditor, SIGNAL(activated()), this, SLOT(onOpenDatatableEditor())));
	IGNORE_RETURN(connect(m_openBuildoutEditor,  SIGNAL(activated()), this, SLOT(onOpenBuildoutEditor())));

	m_sendAsset->setEnabled(false);
	m_reloadAsset->setEnabled(false);
	m_retrieveAsset->setEnabled(false);
	m_verifyAsset->setEnabled(false);
}

// ----------------------------------------------------------------------

ActionsFileControl::~ActionsFileControl()
{
	m_sendAsset         = 0;
	m_reloadAsset       = 0;
	m_retrieveAsset     = 0;
	m_broadcastUpdate   = 0;
	m_updateDbTemplates = 0;
	m_verifyAsset       = 0;
	m_flush             = 0;
	m_openFileServerTree  = 0;
	m_openTemplateEditor  = 0;
	m_openDatatableEditor = 0;
	m_openBuildoutEditor  = 0;
}

// ----------------------------------------------------------------------

void ActionsFileControl::setLifecycleStage(LifecycleStage stage)
{
	m_lifecycleStage = stage;
	MainFrame::getInstance().textToConsole(
		(std::string("FileControl [") + getLifecycleStageText() + "]").c_str());
}

// ----------------------------------------------------------------------

const char * ActionsFileControl::getLifecycleStageText() const
{
	switch (m_lifecycleStage)
	{
	case LS_IDLE:         return "idle";
	case LS_UPLOADING:    return "uploading";
	case LS_STORED:       return "stored";
	case LS_REBUILDING:   return "rebuilding";
	case LS_RELOADING:    return "reloading";
	case LS_DISTRIBUTING: return "distributing";
	case LS_CONFIRMED:    return "confirmed";
	case LS_FAILED:       return "FAILED";
	default:              return "unknown";
	}
}

// ----------------------------------------------------------------------

void ActionsFileControl::logLifecycle(const char * action, const std::string & path, bool success)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "FileControl: %s %s -> %s",
		action, path.c_str(), success ? "OK" : "FAILED");
	MainFrame::getInstance().textToConsole(buf);
	emit fileControlStatusMessage(buf);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onSelectedPathChanged(const std::string & path)
{
	m_selectedPath = path;
	bool hasPath = !path.empty();
	m_sendAsset->setEnabled(hasPath);
	m_reloadAsset->setEnabled(hasPath);
	m_retrieveAsset->setEnabled(hasPath);
	m_verifyAsset->setEnabled(hasPath);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onSendAsset()
{
	if (m_selectedPath.empty())
	{
		QMessageBox::warning(&MainFrame::getInstance(), "File Control", "No asset path selected.");
		return;
	}

	setLifecycleStage(LS_UPLOADING);

	bool success = FileControlClient::requestSendAsset(m_selectedPath);

	if (success)
	{
		setLifecycleStage(LS_STORED);
		setLifecycleStage(LS_DISTRIBUTING);
		setLifecycleStage(LS_CONFIRMED);
	}
	else
	{
		setLifecycleStage(LS_FAILED);
	}

	logLifecycle("Send", m_selectedPath, success);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onReloadAsset()
{
	if (m_selectedPath.empty())
	{
		QMessageBox::warning(&MainFrame::getInstance(), "File Control", "No asset path selected.");
		return;
	}

	setLifecycleStage(LS_RELOADING);

	bool success = FileControlClient::requestReloadAsset(m_selectedPath);

	if (success)
		setLifecycleStage(LS_CONFIRMED);
	else
		setLifecycleStage(LS_FAILED);

	logLifecycle("Reload", m_selectedPath, success);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onRetrieveAsset()
{
	if (m_selectedPath.empty())
	{
		QMessageBox::warning(&MainFrame::getInstance(), "File Control", "No asset path selected.");
		return;
	}

	setLifecycleStage(LS_UPLOADING);

	std::vector<unsigned char> data;
	bool success = FileControlClient::requestRetrieveAsset(m_selectedPath, data);

	if (success)
	{
		setLifecycleStage(LS_STORED);
		setLifecycleStage(LS_CONFIRMED);

		char buf[256];
		snprintf(buf, sizeof(buf), "FileControl: Retrieved %s (%d bytes)",
			m_selectedPath.c_str(), static_cast<int>(data.size()));
		MainFrame::getInstance().textToConsole(buf);
	}
	else
	{
		setLifecycleStage(LS_FAILED);
	}

	logLifecycle("Retrieve", m_selectedPath, success);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onBroadcastUpdate()
{
	setLifecycleStage(LS_DISTRIBUTING);

	bool success = FileControlClient::requestBroadcastUpdate();

	if (success)
		setLifecycleStage(LS_CONFIRMED);
	else
		setLifecycleStage(LS_FAILED);

	logLifecycle("Broadcast", "", success);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onUpdateDbTemplates()
{
	int result = QMessageBox::question(&MainFrame::getInstance(), "File Control",
		"Run 'ant process_templates' on the server?\nThis may take some time.",
		QMessageBox::Yes, QMessageBox::No);

	if (result == QMessageBox::Yes)
	{
		setLifecycleStage(LS_REBUILDING);

		bool success = FileControlClient::requestUpdateDbTemplates();

		if (success)
			setLifecycleStage(LS_CONFIRMED);
		else
			setLifecycleStage(LS_FAILED);

		logLifecycle("UpdateDbTemplates", "", success);
	}
}

// ----------------------------------------------------------------------

void ActionsFileControl::onVerifyAsset()
{
	if (m_selectedPath.empty())
	{
		QMessageBox::warning(&MainFrame::getInstance(), "File Control", "No asset path selected.");
		return;
	}

	unsigned long size = 0;
	unsigned long crc = 0;
	bool success = FileControlClient::requestVerifyAsset(m_selectedPath, size, crc);

	if (success)
	{
		char buf[512];
		snprintf(buf, sizeof(buf),
			"Asset: %s\nSize: %lu bytes\nCRC: 0x%08lx",
			m_selectedPath.c_str(), size, crc);
		QMessageBox::information(&MainFrame::getInstance(), "Asset Verification", buf);
	}
	else
	{
		QMessageBox::warning(&MainFrame::getInstance(), "File Control",
			("Asset not found on server: " + m_selectedPath).c_str());
	}

	logLifecycle("Verify", m_selectedPath, success);
}

// ----------------------------------------------------------------------

void ActionsFileControl::onFlush()
{
	int result = QMessageBox::question(&MainFrame::getInstance(), "File Control",
		"Flush and reload ALL tracked assets on the server?\nThis will force a complete reload.",
		QMessageBox::Yes, QMessageBox::No);

	if (result == QMessageBox::Yes)
	{
		setLifecycleStage(LS_RELOADING);

		bool success = FileControlClient::requestFlush();

		if (success)
			setLifecycleStage(LS_CONFIRMED);
		else
			setLifecycleStage(LS_FAILED);

		logLifecycle("Flush", "", success);
	}
}

// ----------------------------------------------------------------------

void ActionsFileControl::onOpenFileServerTree()
{
	MainFrame & mf = MainFrame::getInstance();
	if (mf.m_fileServerTreeDock)
	{
		mf.m_fileServerTreeDock->show();
		if (mf.m_fileServerTree)
			mf.m_fileServerTree->refreshTree();
	}
	mf.textToConsole("FileControl: Opening File Server Tree");
}

// ----------------------------------------------------------------------

std::string ActionsFileControl::convertCompiledPathToSource(const std::string & compiledPath)
{
	std::string result = compiledPath;

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

	std::string::size_type extPos = result.rfind(".iff");
	if (extPos != std::string::npos && extPos == result.size() - 4)
		result.replace(extPos, 4, ".tpf");

	return result;
}

// ----------------------------------------------------------------------

void ActionsFileControl::onOpenTemplateEditor()
{
	MainFrame & mf = MainFrame::getInstance();
	if (!mf.m_templateEditor)
		return;

	if (!m_selectedPath.empty())
	{
		std::string srcPath = convertCompiledPathToSource(m_selectedPath);
		mf.m_templateEditor->loadTemplate(srcPath);
		mf.textToConsole(("FileControl: Opening Template Editor -> " + srcPath).c_str());
	}
	else
	{
		mf.textToConsole("FileControl: Opening Template Editor");
	}

	mf.m_templateEditor->show();
}

// ----------------------------------------------------------------------

void ActionsFileControl::onOpenDatatableEditor()
{
	MainFrame & mf = MainFrame::getInstance();
	if (mf.m_datatableEditor)
		mf.m_datatableEditor->show();
	mf.textToConsole("FileControl: Opening Datatable Editor");
}

// ----------------------------------------------------------------------

void ActionsFileControl::onOpenBuildoutEditor()
{
	MainFrame & mf = MainFrame::getInstance();
	if (mf.m_buildoutEditor)
		mf.m_buildoutEditor->show();
	mf.textToConsole("FileControl: Opening Buildout Manager");
}

// ======================================================================
